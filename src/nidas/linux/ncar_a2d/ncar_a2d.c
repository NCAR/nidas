/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*-
 * vim: set shiftwidth=8 softtabstop=8 expandtab: */

/*
  ncar_a2d.c

  Linux driver for NCAR/EOL/RAF A/D card, adapted from the RTLinux
  a2d_driver code.

  Copyright 2007 UCAR, NCAR, All Rights Reserved
*/

#include <linux/autoconf.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>           /* has to be before <linux/cdev.h>! GRRR! */
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/slab.h>		/* kmalloc, kfree */

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <nidas/linux/util.h>
#include <nidas/linux/irigclock.h>
#include <nidas/linux/isa_bus.h>
#include <nidas/linux/SvnInfo.h>    // SVNREVISION

// #define DEBUG
#include <nidas/linux/klog.h>

#include "ncar_a2d_priv.h"

MODULE_AUTHOR("Chris Burghart <burghart@ucar.edu>");
MODULE_DESCRIPTION("NCAR A/D driver");
MODULE_LICENSE("GPL");

#ifdef USE_RESET_WORKER
/*
 * Tag to mark waiting for reset attempt.  Any value not likely to 
 * show up as a system error is fine here...
 */
static const int WAITING_FOR_RESET = 77777;

/* Number of reset attempts to try before giving up
 * and returning an error to the user via the poll
 * and read method.
 */
#define NUM_RESET_ATTEMPTS  5

#endif
//#define TEMPDEBUG

/* Selecting ioport address with SW22 on ncar A2D, for
 * both Vipers and Vulcans.
 * O=toward Outside edge of card, set in direction of arrow
 * on SW22, bit value=0.
 * I=toward Inside of card, bit value=1
 *
 * Note that the switch sequence here is backwards from
 * what is seen when looking at the edge of the card.
 * On the card the switches are numbered from 1-8, left to right.
 *
 * First card:
 * hex            3       a
 * switch#  8 7 6 5 4 3 2 1
 * setting  O O I I I O I O
 *
 * Second card:
 * hex            3       b
 * switch#  8 7 6 5 4 3 2 1
 * setting  O O I I I O I I
 *
 * Third card:
 * hex            3       c
 * switch#  8 7 6 5 4 3 2 1
 * setting  O O I I I I 0 0
 */

/* I/O port addresses of installed boards, 0=no board installed.
 * This is the address set in SW22 on the board.
 */
static int IoPort[MAX_A2D_BOARDS] = { 0x3a0, 0, 0, 0 };

/* 
 * Which A2D chip is the master? (-1 causes the first requested channel to
 * be the master)
 */
static int Master[MAX_A2D_BOARDS] = { -1, -1, -1, -1 };

static int nIoPort,nMaster;
#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(IoPort, int, &nIoPort, S_IRUGO);
module_param_array(Master, int, &nMaster, S_IRUGO);
#else
module_param_array(IoPort, int, nIoPort, S_IRUGO);
module_param_array(Master, int, nMaster, S_IRUGO);
#endif

MODULE_PARM_DESC(IoPort, "ISA port address of each board, e.g.: 0x3A0");
MODULE_PARM_DESC(Master,
                 "Master A/D for the board, default=first requested channel");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
#define mutex_init(x)               init_MUTEX(x)
#define mutex_lock_interruptible(x) ( down_interruptible(x) ? -ERESTARTSYS : 0)
#define mutex_unlock(x)             up(x)
#endif

/* number of A2D boards in system (number of non-zero ioport values) */
static int NumBoards = 0;

static struct A2DBoard *BoardInfo = 0;
#define BOARD_INDEX(boardptr) ((boardptr) - BoardInfo)

static struct workqueue_struct *work_queue = 0;

/* 
 * Address for a specific channel on a board: board base address + 2 * channel.
 * These addresses are always used for 16 bit transfers.
 */
static inline int CHAN_ADDR16(struct A2DBoard *brd, int channel)
{
        return (brd->base_addr16 + 2 * channel);
}

/*
 * prototypes
 */
static int stopBoard(struct A2DBoard *brd);
static ssize_t ncar_a2d_read(struct file *filp, char __user * buf,
                             size_t count, loff_t * f_pos);
static int ncar_a2d_open(struct inode *inode, struct file *filp);
static int ncar_a2d_release(struct inode *inode, struct file *filp);
static unsigned int ncar_a2d_poll(struct file *filp, poll_table * wait);
static long ncar_a2d_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
/*
 * Operations for the devices
 */
static struct file_operations ncar_a2d_fops = {
        .owner = THIS_MODULE,
        .read = ncar_a2d_read,
        .open = ncar_a2d_open,
        .unlocked_ioctl = ncar_a2d_ioctl,
        .release = ncar_a2d_release,
        .poll = ncar_a2d_poll,
        .llseek  = no_llseek,
};

/**
 * Info for A/D user devices
 */
#define DEVNAME_A2D "ncar_a2d"
static dev_t ncar_a2d_device = MKDEV(0, 0);
static struct cdev ncar_a2d_cdev;

// Clock/data line bits for i2c interface
static const unsigned long I2CSCL = 0x2;
static const unsigned long I2CSDA = 0x1;

/*-----------------------Utility------------------------------*/
// I2C serial bus control utilities

static inline void i2c_clock_hi(struct A2DBoard *brd)
{
        brd->i2c |= I2CSCL;     // Set clock bit hi
        outb(brd->i2c, brd->base_addr);
        udelay(1);
        return;
}

static inline void i2c_clock_lo(struct A2DBoard *brd)
{
        brd->i2c &= ~I2CSCL;    // Set clock bit low
        outb(brd->i2c, brd->base_addr);
        udelay(1);
        return;
}

static inline void i2c_data_hi(struct A2DBoard *brd)
{
        brd->i2c |= I2CSDA;     // Set data bit hi
        outb(brd->i2c, brd->base_addr);
        udelay(1);
        return;
}

static inline void i2c_data_lo(struct A2DBoard *brd)
{
        brd->i2c &= ~I2CSDA;    // Set data bit lo
        outb(brd->i2c, brd->base_addr);
        udelay(1);
        return;
}

/*
 * Send an I2C start sequence: data goes from hi to lo while clock is high.
 */
static inline void i2c_start_sequence(struct A2DBoard *brd)
{
        i2c_data_hi(brd);
        i2c_clock_hi(brd);
        i2c_data_lo(brd);
        i2c_clock_lo(brd);
}

/*
 * Send an I2C stop sequence: data goes from lo to hi while clock is high.
 */
static inline void i2c_stop_sequence(struct A2DBoard *brd)
{
        i2c_data_lo(brd);
        i2c_clock_hi(brd);
        i2c_data_hi(brd);
        // i2c_clock_lo(brd);
}

/*
 * Get the I2C acknowledge bit from the slave
 */
static inline int i2c_getAck(struct A2DBoard *brd)
{
        unsigned char ack = 0;
        i2c_clock_hi(brd);
        ack = inb(brd->base_addr) & 0x1;
        i2c_clock_lo(brd);
        if (ack != 0) {
                KLOG_NOTICE("%s: Incorrect I2C ACK from board!\n",
                            brd->deviceName);
                return 1;
        }
        return 0;
}

/*
 * Acknowledge with a one bit.
 */
static inline void i2c_putNoAck(struct A2DBoard *brd)
{
        i2c_data_hi(brd);
        i2c_clock_hi(brd);
        i2c_clock_lo(brd);  // 
}

/*
 * Acknowledge with a zero bit.
 */
static inline void i2c_putAck(struct A2DBoard *brd)
{
        i2c_data_lo(brd);
        i2c_clock_hi(brd);
        i2c_clock_lo(brd);  // 
        i2c_data_hi(brd);   // Can we still drive the data line after the clock, or is the LM92 chip driving?
                            // Spowart says that by setting the line high it effectively opens the circuit.
}

static inline unsigned char i2c_get_byte(struct A2DBoard *brd)
{
        unsigned char i;
        unsigned char byte = 0;
        unsigned char bit;

        for (i = 0; i < 8; i++) {
                // raise clock
                i2c_clock_hi(brd);
                // get data
                bit = inb(brd->base_addr) & 0x1;
                byte = (byte << 1) | bit;
                // lower clock
                i2c_clock_lo(brd);
        }

        // total: 2 * 8 = 16 I2C ops

        return byte;
}

static inline int i2c_put_byte(struct A2DBoard *brd, unsigned char byte)
{
        unsigned char i;

        for (i = 0; i < 8; i++) {
                // set data line to the high order bit
                if (byte & 0x80)
                        i2c_data_hi(brd);
                else
                        i2c_data_lo(brd);

                // raise clock
                i2c_clock_hi(brd);
                // lower clock
                i2c_clock_lo(brd);

                // shift by a bit
                byte <<= 1;
        }

        return i2c_getAck(brd);        // 2 I2C operations
        // total: 3 * 8 + 2 = 26 I2C ops
}


/*-----------------------Utility------------------------------*/
/**
 * Read on-board LM92 temperature sensor via i2c serial bus
 * The signed short returned is (16 * temperature in deg C)
 */

static short A2DTemp(struct A2DBoard *brd)
{
        // This takes 72 i2c operations to perform.
        // Using a delay of 1 usecs, this should take
        // approximately 72 usecs.

        unsigned char b0;
        unsigned char b1;
        unsigned char address = 0x48;   // I2C address of temperature register

        /*
         * The board will not return temperatures unless it's running...
         */
        if (!brd->busy)
                return -32768;

        /*
         * Enable access to the i2c chip
         */
        outb(A2DIO_A2DDATA, brd->cmd_addr);
        /*
         * Send I2C start sequence
         */
        i2c_start_sequence(brd);        // 4 operations

        /*
         * I2C address goes in bits 7-1, and we set bit 0 to indicate we want
         * to read.
         */
        b0 = (address << 1) | 1;
        if (i2c_put_byte(brd, b0)) {  // 26 operations
                b0 = 0;
                b1 = 0;
                goto bailout;
        }
        /*
         * Get the two data bytes
         */
        b0 = i2c_get_byte(brd); // 16 operations
        i2c_putAck(brd);        // 3 I2C operations
        b1 = i2c_get_byte(brd); // 16 operations
        i2c_putNoAck(brd);      // 3 I2C operations
bailout:
        /*
         * Send I2C stop sequence
         */
        i2c_stop_sequence(brd); // 4 operations

        /*
         * Disable access to the i2c chip
         */
        outb(A2DIO_FIFO, brd->cmd_addr);

        return (short) ((b0 << 8 | b1) >> 3);
}

/*-----------------------End I2C Utils -----------------------*/


/*-----------------------Utility------------------------------*/


/*
 * Wait up to maxmsecs milliseconds for the interrupt bit for the 
 * selected channel to be set.
 */
inline static int
waitForChannelInterrupt(struct A2DBoard *brd, int channel, int ncoef)
{
        unsigned char interrupts;
        unsigned char mask = (1 << channel);
        int ntry,NTRY=100;

        /*
         * On a Vulcan, if schedule() is called (rather than udelay() or msleep())
         * between checks of the interrupt bit:
         * For all but 6 or 7 times out of the 517 coefficient loads, the
         *      bit appeared in less than 5 interations of the loop.
         * On those 6 or 7 loads, the interrupt appeared after 5 iterations,
         *      except for the 517th time when it took 11 or 13 iterations.
         * On a Vulcan, the same results are seen with a call to udelay(1).
         * When using udelay(2), then only the 517th load required 5 or
         * more iterations, and it still required 10 or 11.
         *
         * The total time taken to download the 517 coefficients for a
         * channel ranged from 10 to 60 msec using schedule() and with udelay(1).
         * The same total time is seen on a Viper.
         *
         * If schedule_timeout(1) is used, the total time is over 5100 msecs
         * for one channel, which is too long.
         *
         * udelay() and repeatedly calling schedule() are both busy waits.
         * We'll use schedule().
         *
         * So generally the interrupt bit is seen within 5 iterations, or
         * 15 after the last coefficient.  However, on a Viper I saw a
         * case where increasing NTRY to 100 was necessary to get around
         * a recalcitrant bit for channel 0, coefficient 0.
         */

        for (ntry = 0; ntry < NTRY; ntry++) {
                if (!(ntry % 4)) outb(A2DIO_SYSCTL, brd->cmd_addr);
                interrupts = inb(brd->base_addr);
                if ((interrupts & mask) != 0) {
                        if (ntry > 5)
                                KLOG_DEBUG
                                    ("%s: interrupt bit set for channel %d, response=%#x, ntry=%d, ncoef=%d\n",
                                     brd->deviceName,channel, (int)interrupts,ntry,ncoef);
                        return 0;
                }
                else KLOG_DEBUG("%s: channel %d SYSCTL response =%#x, ntry=%d, ncoef=%d\n",
                                     brd->deviceName,channel,(int)interrupts,ntry,ncoef);
                schedule();
                // udelay(1);
                // set_current_state(TASK_INTERRUPTIBLE);
                // schedule_timeout(1);
        }
        KLOG_WARNING
            ("%s: interrupt bit not set for channel %d, ncoef=%d, response=%#x, ntry=%d\n",
             brd->deviceName,channel,ncoef,(int)interrupts,ntry);
        return -ETIMEDOUT;
}

// Read status of AD7725 A/D chip specified by channel 0-7
static unsigned short AD7725Status(struct A2DBoard *brd, int channel)
{
        outb(A2DIO_A2DSTAT + A2DIO_LBSD3, brd->cmd_addr);
        return (inw(CHAN_ADDR16(brd, channel)));
}

static void AD7725StatusAll(struct A2DBoard *brd)
{
        int i;
        for (i = 0; i < NUM_USABLE_NCAR_A2D_CHANNELS; i++)
                if (brd->gain[i] > 0)
                        brd->cur_status.goodval[i] = AD7725Status(brd, i);
                else brd->cur_status.goodval[i] = 0;
        for ( ; i < NUM_NCAR_A2D_CHANNELS; i++)
                        brd->cur_status.goodval[i] = 0;
        return;
}

/**
 * AD7725 sets some of the bits of the previous instruction in its
 * status register. For a given instruction, return the value
 * of those status bits, which gives some indication of the
 * success of the previous operation.
 */
static inline unsigned short
AD7725StatusInstrBits(unsigned short instr)
{
        unsigned short expected = 0;
        if (instr & 0x8000) expected |= A2DINSTREG15;
        if (instr & 0x2000) expected |= A2DINSTREG13;
        if (instr & 0x1000) expected |= A2DINSTREG12;
        if (instr & 0x0800) expected |= A2DINSTREG11;
        if (instr & 0x0040) expected |= A2DINSTREG06;
        if (instr & 0x0020) expected |= A2DINSTREG05;
        if (instr & 0x0010) expected |= A2DINSTREG04;
        if (instr & 0x0002) expected |= A2DINSTREG01;
        if (instr & 0x0001) expected |= A2DINSTREG00;
        return expected;
}
/*-----------------------Utility------------------------------*/
// A2DSetGain sets an A/D Channel gain selected by channel.
// TODO: update old DAC calculation in comment below...
// Allowable gain values are 1 <= A2DGain <= 25.5
//   in 255 gain steps.
// The gain is calculated from gaincode (0-255) as:
//   gain = 5*5.12*gaincode/256 = .1*gaincode.
// The gain can go down to .7 before the system has saturation problems
// Check this 

static int A2DSetGain(struct A2DBoard *brd, int channel)
{
        unsigned short gainCode = 0;

        if (channel < 0 || channel >= NUM_USABLE_NCAR_A2D_CHANNELS)
                return -EINVAL;

        // The new 12-bit DAC has lower input resistance (7K ohms as opposed
        // to 10K ohms for the 8-bit DAC). The gain is the ratio of the amplifier
        // feedback resistor to the input resistance. Therefore, for the same
        // gain codes, the new board will yield higher gains by a factor of
        // approximately 1.43. I believe you will want to divide the old gain
        // codes (0-255) by 1.43. The new gain code will be between
        // 0 - 4095. So, after you divide the old gain code by 1.43 you
        // will want to multiply by 16. This is the same as multiplying
        // by 16/1.43 = 11.2.
        if (brd->offset[channel]) {         // unipolar
                switch(brd->gain[channel]) {
                case 1:
                        gainCode = 0x1100 + channel;    //   0 to +20  ???
                        break;
                case 2:
                        gainCode = 0x4400 + channel;    //   0 to +10
                        break;
                case 4:
                        gainCode = 0x8800 + channel;    //   0 to +5
                        break;
                default:
                        // No need to set a gainCode here because unused
                        // channels are bipolar by default.
                        return -EINVAL;
                }
        } else {                            // bipolar
                switch(brd->gain[channel]) {
                default:
                        // Use lowest gain setting for un-expected inputs.
                case 1:
                        gainCode = 0x2200 + channel;    // -10 to +10
                        break;
                case 2:
                        gainCode = 0x4400 + channel;
                        break;
                case 4:
                        gainCode = 0x8800 + channel;
                        break;
                }
        }
        // 1.  Write (or set) D2A0. This is accomplished by writing to the A/D
        // with the lower four address bits (SA0-SA3) set to all "ones" and the
        // data bus to 0x03.
        //   KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_D2A0, brd->cmd_addr);
        outb(A2DIO_D2A0, brd->cmd_addr);
        msleep(10);
        // 2. Then write to the A/D card with lower address bits set to "zeros"
        // and data bus set to the gain value for the specific channel with the
        // upper data three bits equal to the channel address. The lower 12
        // bits are the gain code and data bit 12 is equal zero. So for channel
        // 0 write: (xxxxxxxxxxxx0000) where the x's are the gain code.
        KLOG_DEBUG
            ("%s: chn: %d   offset: %d   gain: %2d   outb( 0x%x, 0x%lx)\n",
             brd->deviceName,channel, brd->offset[channel], brd->gain[channel], gainCode,
             brd->base_addr);
        // KLOG_DEBUG("outb( 0x%x, 0x%x);\n", gainCode, brd->base_addr);
        outw(gainCode, brd->base_addr16);
        msleep(10);
        return 0;
}

/*-----------------------Utility------------------------------*/
// A2DSetMaster routes the interrupt signal from the target A/D chip to where????
static int A2DSetMaster(struct A2DBoard *brd, int channel)
{
        if (channel < 0 || channel >= NUM_USABLE_NCAR_A2D_CHANNELS) {
                KLOG_ERR("%s: bad master chip number: %d\n",brd->deviceName,channel);
                return -EINVAL;
        }

        KLOG_DEBUG("%s: A2DSetMaster, Master=%d\n", brd->deviceName,channel);
        outb(A2DIO_FIFOSTAT, brd->cmd_addr);
        outb((char) channel, brd->base_addr);
        return 0;
}

/*-----------------------Utility------------------------------*/
// Set a calibration voltage for all channels:
//
//  bits volts
//  0x00 gnd
//  0x01 open
//  0x03 +1
//  0x05 +5
//  0x09 -10
//  0x11 +10
//
static int CalVoltIsValid(int volt)
{
        int i, valid[] = { 0, 1, 5, -10, 10 };

        for (i = 0; i < 5; i++)
                if (volt == valid[i])
                        return 1;

        return 0;
}
static int CalVoltToBits(int volt)
{
        int i, valid[] = { 0,    1,    5,    -10,  10   };
        int     bits[] = { 0x00, 0x03, 0x05, 0x09, 0x11 };

        for (i = 0; i < 5; i++)
                if (volt == valid[i])
                        return bits[i];

        return 0x01;  // default is open
}
static void UnSetVcal(struct A2DBoard *brd)
{
        outb(A2DIO_D2A2, brd->cmd_addr);

        // Write cal voltage code for an open state
        outw(0x01, brd->base_addr16);
}
static void SetVcal(struct A2DBoard *brd)
{
        KLOG_DEBUG("%s: brd->cal.vcal: %d\n", brd->deviceName, brd->cal.vcal);

        // Unset before each cal voltage change to avoid shorting
        UnSetVcal(brd);

        // Point to the calibration DAC channel
        outb(A2DIO_D2A2, brd->cmd_addr);

        // Write cal voltage code
        outw(CalVoltToBits(brd->cal.vcal) & 0x1f, brd->base_addr16);
}

/*-----------------------Utility------------------------------*/
// Switch inputs specified by Chans bits to calibration mode.
// Cal bits are in lower byte
//
//      8 bit selection of which channel is using a cal voltage
//     /|
// 0xXXYY
//   \|
//    8 bit selection of which channel is offset (bipolar)
//
static void SetCal(struct A2DBoard *brd)
{
        unsigned short OffChans = 0;
        unsigned short CalChans = 0;
        int i;

        KLOG_DEBUG("%s: brd->OffCal: 0x%04x\n", brd->deviceName, brd->OffCal);

        // Change the calset array of bools into a byte
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                OffChans >>= 1;
                CalChans >>= 1;
                if (brd->offset[i] != 0)
                        OffChans += 0x80;
                if (brd->cal.calset[i] != 0)
                        CalChans += 0x80;
        }
        // Point at the system control input channel
        outb(A2DIO_SYSCTL, brd->cmd_addr);

        // Set the appropriate bits in OffCal
        brd->OffCal = (OffChans << 8) & 0xFF00;
        brd->OffCal |= CalChans;
        brd->OffCal = ~(brd->OffCal) & 0xFFFF;  // invert bits

        // Send OffCal word to system control word
        outw(brd->OffCal, brd->base_addr16);
}

/*-----------------------Utility------------------------------*/
// Switch channels specified by Chans bits to offset mode.
// Offset bits are in upper byte
//
// 0xXX00
//   \|
//    8 bit selection of which channel is offset (unipolar = high)
//
static void SetOffset(struct A2DBoard *brd)
{
        unsigned short OffChans = 0;
        int i;

        KLOG_DEBUG("%s: brd->OffCal: 0x%04x\n", brd->deviceName, brd->OffCal);

        // Change the offset array of bools into a byte
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                OffChans >>= 1;
                if (brd->offset[i] != 0)
                        OffChans += 0x80;
        }
        // Point at the system control input channel
        outb(A2DIO_SYSCTL, brd->cmd_addr);

        // Set the appropriate bits in OffCal
        brd->OffCal = (OffChans << 8) & 0xFF00;
        brd->OffCal = ~(brd->OffCal) & 0xFFFF;  // invert bits

        // Send OffCal word to system control word
        outw(brd->OffCal, brd->base_addr16);
}

/*-----------------------Utility------------------------------*/
// Set A2D SYNC flip/flop.  This stops the A/D's until cleared
// under program control or by a positive 1PPS transition
// 1PPS enable must be asserted in order to sync on 1PPS

static void A2DSetSYNC(struct A2DBoard *brd)
{
        outb(A2DIO_FIFO, brd->cmd_addr);

        brd->FIFOCtl |= A2DSYNC;        // Ensure that SYNC bit in FIFOCtl is set.

        // Cycle the sync clock while keeping SYNC bit high
        outb(brd->FIFOCtl, brd->base_addr);
        outb(brd->FIFOCtl | A2DSYNCCK, brd->base_addr);
        outb(brd->FIFOCtl, brd->base_addr);
        return;
}

/*-----------------------Utility------------------------------*/
// Clear the SYNC flag

static void A2DClearSYNC(struct A2DBoard *brd)
{
        outb(A2DIO_FIFO, brd->cmd_addr);

        brd->FIFOCtl &= ~A2DSYNC;       // Ensure that SYNC bit in FIFOCtl is cleared.

        // Cycle the sync clock while keeping sync lowthe SYNC data line
        outb(brd->FIFOCtl, brd->base_addr);
        outb(brd->FIFOCtl | A2DSYNCCK, brd->base_addr);
        outb(brd->FIFOCtl, brd->base_addr);
        return;
}

/*-----------------------Utility------------------------------*/
// Enable 1PPS sync

static void A2DEnable1PPS(struct A2DBoard *brd)
{
        // Point at the FIFO control byte
        outb(A2DIO_FIFO, brd->cmd_addr);

        // Set the 1PPS enable bit
        outb(brd->FIFOCtl | A2D1PPSEBL, brd->base_addr);

        return;
}

/*-----------------------Utility------------------------------*/
// Clear (reset) the data FIFO

static void A2DClearFIFO(struct A2DBoard *brd)
{
        // Point to FIFO control byte
        outb(A2DIO_FIFO, brd->cmd_addr);

        brd->FIFOCtl &= ~FIFOCLR;       // Ensure that FIFOCLR bit is not set in FIFOCtl

        outb(brd->FIFOCtl, brd->base_addr);
        outb(brd->FIFOCtl | FIFOCLR, brd->base_addr);   // cycle bit 0 to clear FIFO
        outb(brd->FIFOCtl, brd->base_addr);

        return;
}

/*-----------------------Utility------------------------------*/
/**
 * Return the status from the card.  The upper 10 bits of the status 
 * are the card serial number, and the lower six bits are:
 * 
 * 0x20: PRESYNC        Presync bit (NOT USED)
 * 0x10: INV1PPS        Inverted 1 PPS pulse
 * 0x08: FIFONOTFULL    FIFO not full
 * 0x04: FIFONOTEMPTY   FIFO not empty
 * 0x02: FIFOAFAE       FIFO contains < 1024 words or > 3072 words
 * 0x01: FIFOHF         FIFO half full
 */
static inline unsigned short A2DBoardStatus(struct A2DBoard *brd)
{
        outb(A2DIO_FIFOSTAT, brd->cmd_addr);
        return inw(brd->base_addr16);
}

/*-----------------------Utility------------------------------*/
/**
 * Get the FIFO fill level:
 * @return 0: empty
 *         1: not empty but less than or = 1/4 full
 *         2: more than 1/4 but less than 1/2
 *         3: more than or = 1/2 but less than 3/4
 *         4: more than or = 3/4 but not totally full
 *         5: full
 */
static inline int getA2DFIFOLevel(struct A2DBoard *brd)
{
        unsigned short stat = A2DBoardStatus(brd);

        // If FIFONOTFULL is 0, fifo IS full
        if ((stat & FIFONOTFULL) == 0)
                return 5;

        // If FIFONOTEMPTY is 0, fifo IS empty
        else if ((stat & FIFONOTEMPTY) == 0)
                return 0;

        // Figure out which 1/4 of the 1024 FIFO words we're filled to

        // bit 0, 0x1, set when FIFO >= half full
        // bit 1, 0x2, either almost full (>=3/4) or almost empty (<=1/4).
        switch (stat & 0x03)    // Switch on stat's 2 LSB's
        {
        case 3:                // almost full/empty, half full (>=3/4 to <4/4)
                return 4;
        case 2:                // almost full/empty, not half full (empty to <=1/4)
                return 1;
        case 1:                // not almost full/empty, half full (>=2/4 to <3/4)
                return 3;
        case 0:                // not almost full/empty, not half full (>1/4 to <2/4)
        default:               // can't happen, but avoid compiler warn
                return 2;
                break;
        }
        return -1;              // can't happen, but avoid compiler warn
}

/*-----------------------Utility------------------------------*/
// This routine sends the ABORT command to A/D chips.
// The ABORT command amounts to a soft reset--they stay configured.

static void A2DStopRead(struct A2DBoard *brd, int channel)
{
        int ntry;
        const int NTRY=10;
        unsigned short status;
        for (ntry = 0; ntry < NTRY; ntry++) {
                if (!(ntry % 4)) {
                        // Point to the A2D command register
                        outb(A2DIO_A2DSTAT, brd->cmd_addr);

                        // Send specified A/D the abort (soft reset) command
                        outw(AD7725_ABORT, CHAN_ADDR16(brd, channel));
                }

                outb(A2DIO_A2DSTAT + A2DIO_LBSD3, brd->cmd_addr);
                status = inw(CHAN_ADDR16(brd, channel));
                if (!(status & 0x7ffe)) break;
                KLOG_INFO("%s: A2DStopRead, channel=%d, status=%#x, ntry=%d\n",
                                        brd->deviceName,channel,(int)status,ntry);
        }
        return;
}

/*-----------------------Utility------------------------------*/
// Send ABORT command to all A/D chips

static void A2DStopReadAll(struct A2DBoard *brd)
{
        int i;
        for (i = 0; i < NUM_USABLE_NCAR_A2D_CHANNELS; i++)
                        A2DStopRead(brd, i);
}

/*-----------------------Utility------------------------------*/
// This routine sets all A/D chips to auto mode

static void A2DAuto(struct A2DBoard *brd)
{
        // Point to the FIFO Control word
        outb(A2DIO_FIFO, brd->cmd_addr);

        // Set Auto run bit and send to FIFO control byte
        brd->FIFOCtl |= A2DAUTO;
        outb(brd->FIFOCtl, brd->base_addr);
        return;
}

/*-----------------------Utility------------------------------*/
// This routine sets all A/D chips to non-auto mode

static void A2DNotAuto(struct A2DBoard *brd)
{
        // Point to the FIFO Control word
        outb(A2DIO_FIFO, brd->cmd_addr);

        // Turn off the auto bit and send to FIFO control byte
        brd->FIFOCtl &= ~A2DAUTO;
        outb(brd->FIFOCtl, brd->base_addr);
        return;
}

/*-----------------------Utility------------------------------*/
// Start the selected A/D in acquisition mode

static int A2DStart(struct A2DBoard *brd, int channel)
{
        int ntry;
        const int NTRY=20;
        unsigned short status;
        unsigned short instr = AD7725_READDATA; 
        unsigned short expected = AD7725StatusInstrBits(instr);

        if (channel < 0 || channel >= NUM_USABLE_NCAR_A2D_CHANNELS)
                return -EINVAL;

        for (ntry = 0; ntry < NTRY; ntry++) {
                if (!(ntry % 1)) {
                        // Point at the A/D command channel
                        outb(A2DIO_A2DSTAT, brd->cmd_addr);
                        // Start the selected A/D
                        outw(instr, CHAN_ADDR16(brd, channel));
                }

                // check it got there
                outb(A2DIO_A2DSTAT + A2DIO_LBSD3, brd->cmd_addr);
                status = inw(CHAN_ADDR16(brd, channel));
                if ((status & A2DSTAT_INSTR_MASK) == expected) break;
                KLOG_INFO
                   ("%s: Instruction 0x%04x on channel %d failed: expected=0x%04hx, actual=0x%04hx, status=0x%04x, ntry=%d\n",
                    brd->deviceName, instr, channel,expected,(status & A2DSTAT_INSTR_MASK),status,ntry);
                // udelay(10);
        }
        if (ntry == NTRY) return -EIO;
        return 0;
}

static int A2DStartAll(struct A2DBoard *brd)
{
        int i;
        int ret = 0;
        for (i = 0; i < NUM_USABLE_NCAR_A2D_CHANNELS; i++)
                if ((ret = A2DStart(brd, i)) != 0) return ret;
        return ret;
}

/*-----------------------Utility------------------------------*/
// Configure A/D channel with coefficient array 'filter'
static int A2DConfig(struct A2DBoard *brd, int channel)
{
        int coef;
        unsigned short status;
        unsigned short instr;
        unsigned short expected;
        int t1;
        int nCoefs =
            sizeof (brd->ocfilter) / sizeof (brd->ocfilter[0]);
        int ntry;
        const int NTRY=10;

        KLOG_DEBUG("%s: configuring channel %d\n", brd->deviceName,
                    channel);
        if (channel < 0 || channel >= NUM_USABLE_NCAR_A2D_CHANNELS)
                return -EINVAL;

        instr = AD7725_WRCONFIG;
        expected = AD7725StatusInstrBits(instr);

        for (ntry = 0; ntry < NTRY; ntry++) {
                if (!(ntry % 2)) {
                        // Set up to write a command to a channel
                        outb(A2DIO_A2DSTAT, brd->cmd_addr);
                        // Set configuration write mode for our channel
                        outw(instr, CHAN_ADDR16(brd, channel));
                }
                status = AD7725Status(brd, channel);
                if ((status & A2DSTAT_INSTR_MASK) == expected) break;
                KLOG_WARNING
                   ("%s: WRCONFIG instruction on channel %d failed: expected=0x%04hx, actual=0x%04hx, status=0x%04x, ntry=%d\n",
                    brd->deviceName,channel,expected,(status & A2DSTAT_INSTR_MASK),status,ntry);
        }
        if (ntry == NTRY) {
                KLOG_ERR
                   ("%s: Instruction 0x%04x on channel %d failed: expected=0x%04hx, actual=0x%04hx, status=0x%04x, ntry=%d\n",
                    brd->deviceName, instr, channel,expected,(status & A2DSTAT_INSTR_MASK),status,ntry);
                return -ETIMEDOUT;
        }

        KLOG_DEBUG("%s: downloading filter coefficients, nCoefs=%d\n", brd->deviceName,nCoefs);
	t1 = GET_MSEC_CLOCK;

        for (coef = 0; coef < nCoefs; coef++) {
                // Wait for interrupt bit to set before sending coefficient
                if (waitForChannelInterrupt(brd, channel,coef) != 0) {
                        KLOG_ERR
                            ("%s: timeout before sending coeficient %d, channel %d\n",
                             brd->deviceName,coef,channel);
                        return -ETIMEDOUT;
                }
                // Read status word from target a/d and check for errors
                outb(A2DIO_A2DSTAT + A2DIO_LBSD3, brd->cmd_addr);
                status = inw(CHAN_ADDR16(brd, channel));
                if (!(status & A2DDATAREQ)) {
                        KLOG_ERR("%s: Data Request bit not set before download of coef %d on channel %d, status=%#04hx\n",
                                brd->deviceName,coef,channel,status);
                        return -EIO;
                }
                if ((status & A2DCRCERR)) {
                        KLOG_ERR("%s: CRC Error bit set prior to download of coef %d on channel %d, status=%#04hx\n",
                                brd->deviceName,coef,channel,status);
                        return -EIO;
                }
                if ((status & A2DIDERR)) {
                        KLOG_ERR("%s: ID Error bit set prior to download of coef %d on channel %d, status=%#04hx\n",
                                brd->deviceName,coef,channel,status);
                        return -EIO;
                }

                // write out coefficient
                outb(A2DIO_D2A0, brd->cmd_addr);
                outw(brd->ocfilter[coef], CHAN_ADDR16(brd, channel));
        }
        /* AD7725 asserts interrupt after the last coef is written */
        if (waitForChannelInterrupt(brd, channel,coef) != 0) {
                KLOG_ERR("%s: timeout after last coefficient (%d) sent on channel %d\n",
                     brd->deviceName,coef,channel);
                return -ETIMEDOUT;
        }
        // Read status word from target a/d and check for errors
        outb(A2DIO_A2DSTAT + A2DIO_LBSD3, brd->cmd_addr);
        status = inw(CHAN_ADDR16(brd, channel));
        KLOG_DEBUG("after sending coef %d, stat=%#04x\n",coef,stat);

        if (status & A2DIDERR) {
                KLOG_ERR("%s: ID Error bit set after last coefficient (%d) on channel %d\n",
                         brd->deviceName, coef, channel);
                return -EIO;
        }

        if (status & A2DCRCERR) {
                KLOG_ERR("%s: CRC Error bit set after last coefficient (%d) on channel %d\n",
                         brd->deviceName,coef, channel);
                return -EIO;
        }

        // We should have CFGEND status now (channel configured and ready)
        if ((status & A2DCONFIGEND) == 0) {
                KLOG_ERR("%s: CFGEND bit not set in status after configuring channel %d\n",
                     brd->deviceName, channel);
                return -EIO;
        }
        brd->cur_status.goodval[channel] = status;
        KLOG_DEBUG("%s: channel %d, %d filter coefficients downloaded, time=%d msec\n",
                        brd->deviceName,channel,nCoefs,GET_MSEC_CLOCK-t1);
        return 0;
}

/*-----------------------Utility------------------------------*/
// Configure all A/D's with same filter

static int A2DConfigAll(struct A2DBoard *brd)
{
        int ret = 0;
        int i;
        for (i = 0; i < NUM_USABLE_NCAR_A2D_CHANNELS; i++) {
                if ((ret = A2DConfig(brd, i)) < 0) return ret;
        }
        return 0;
}

// the status bits are in the upper byte contain the serial number
static int getSerialNumber(struct A2DBoard *brd)
{
        unsigned short stat = A2DBoardStatus(brd);
        // fetch serial number
        return (stat >> 6);     // S/N is upper 10 bits
}

static void ppsCallback1(void *ptr)
{
        struct A2DBoard *brd = (struct A2DBoard *) ptr;
        unsigned short stat;
	stat = A2DBoardStatus(brd);
        if ((stat & INV1PPS) == 0) {
		KLOG_DEBUG("%s: found PPS, GET_MSEC_CLOCK=%d\n",
			brd->deviceName,GET_MSEC_CLOCK);
		brd->havePPS = 1;
		wake_up_interruptible(&brd->ppsWaitQ);
        }
}

/* Utility function to wait for INV1PPS to be zero.
 * Return: negative errno:
 *  -ETIMEDOUT= no PPS found after 3 seconds
 *  -EINTR: interrupted by a signal
 * or 0=OK.
 * This registers an irig callback, to check for the PPS, and
 * then unregisters it.
 */
static int waitFor1PPS (struct A2DBoard *brd,irig_callback_func* ppsfunc)
{
        int ret = 0;
	long lret;
        long waitTimeout;
	brd->havePPS = 0;

        if ((ret = mutex_lock_interruptible(&brd->mutex)))
                return ret;
	brd->ppsCallback =
	    register_irig_callback(ppsfunc,0, IRIG_100_HZ,brd,&ret);
        if (!brd->ppsCallback) {
                KLOG_ERR("%s: error: register_irig_callback failed\n",brd->deviceName);
		mutex_unlock(&brd->mutex);
                return ret;
        }
	mutex_unlock(&brd->mutex);

#if defined(CONFIG_MACH_ARCOM_MERCURY) || defined(CONFIG_MACH_ARCOM_VULCAN)
	waitTimeout = 10 * HZ;
#else
	waitTimeout = 2 * HZ;
#endif

        /* this wait does a general memory barrier before checking the event,
         * so the above STORE of brd->havePPS will be visible.
         */
        lret = wait_event_interruptible_timeout(brd->ppsWaitQ,brd->havePPS,waitTimeout);
        if (lret == 0) {
                KLOG_ERR("%s: error: PPS not found\n",brd->deviceName);
		ret = -ETIMEDOUT;
        }
	else if (lret < 0) ret = lret;

	// we don't use mutex_lock_interruptible here. It shouldn't
	// need interrupting - and we want to unregister the callback
	// in any case.
        mutex_lock(&brd->mutex);
	if (brd->ppsCallback) unregister_irig_callback(brd->ppsCallback);
	brd->ppsCallback = 0;
	mutex_unlock(&brd->mutex);
        return ret;
}

static int A2DSetGainAndOffset(struct A2DBoard *brd)
{
        int i;
        int ret = 0;
        int repeat;

        brd->FIFOCtl = 0;       // Clear FIFO Control Word

        brd->OffCal = 0x0;

        // HACK! the CPLD logic needs to be fixed!  
        for (repeat = 0; repeat < 3; repeat++) {
                for (i = 0; i < NUM_USABLE_NCAR_A2D_CHANNELS; i++) {
                        /*
                         * Set gain for all channels
                         */
                        if ((ret = A2DSetGain(brd, i)) != 0)
                                return ret;
                }
                outb(A2DIO_D2A1, brd->cmd_addr);
                msleep(10);
        }
        // END HACK!

        SetOffset(brd);

        KLOG_DEBUG("%s: success!\n",brd->deviceName);
        return 0;
}

static int addSampleConfig(struct A2DBoard *brd,
    struct nidas_a2d_sample_config* cfg)
{
        int ret = 0;
        struct a2d_filter_info* filters;
        struct a2d_filter_info* finfo;
        int nfilters;
        struct short_filter_methods methods;
        int i;

        if (brd->busy) {
                KLOG_ERR("%s: A2D's running. Can't configure\n",brd->deviceName);
                return -EBUSY;
        }

        // grow the filter info array with one more element
        nfilters = brd->nfilters + 1;
        filters = kmalloc(nfilters * sizeof (struct a2d_filter_info),
                               GFP_KERNEL);
        if (!filters) return -ENOMEM;

        // copy previous filter infos, and free the old space
        memcpy(filters,brd->filters,
            brd->nfilters * sizeof(struct a2d_filter_info));
        kfree(brd->filters);

        finfo = filters + brd->nfilters;
        brd->filters = filters;
        brd->nfilters = nfilters;

        memset(finfo, 0, sizeof(struct a2d_filter_info));

        if (!(finfo->channels =
                kmalloc(cfg->nvars * sizeof(int),GFP_KERNEL)))
                return -ENOMEM;

        memcpy(finfo->channels,cfg->channels,cfg->nvars * sizeof(int));
        finfo->nchans = cfg->nvars;

        KLOG_DEBUG("%s: sindex=%d,nfilters=%d\n",
                   brd->deviceName, cfg->sindex, brd->nfilters);

        if (cfg->sindex < 0 || cfg->sindex >= brd->nfilters)
                return -EINVAL;

        KLOG_DEBUG("%s: scanRate=%d,cfg->rate=%d\n",
                   brd->deviceName, brd->scanRate, cfg->rate);

        if (brd->scanRate % cfg->rate) {
                KLOG_ERR
                    ("%s: A2D scanRate=%d is not a multiple of the rate=%d for sample %d\n",
                     brd->deviceName, brd->scanRate, cfg->rate,
                     cfg->sindex);
                return -EINVAL;
        }

        finfo->decimate = brd->scanRate / cfg->rate;
        finfo->filterType = cfg->filterType;
        finfo->index = cfg->sindex;

        KLOG_DEBUG("%s: decimate=%d,filterType=%d,index=%d\n",
                   brd->deviceName, finfo->decimate, finfo->filterType,
                   finfo->index);

        methods = get_short_filter_methods(cfg->filterType);
        if (!methods.init) {
                KLOG_ERR("%s: filter type %d unsupported\n",
                         brd->deviceName, cfg->filterType);
                return -EINVAL;
        }
        finfo->finit = methods.init;
        finfo->fconfig = methods.config;
        finfo->filter = methods.filter;
        finfo->fcleanup = methods.cleanup;

        /* Create the filter object */
        finfo->filterObj = finfo->finit();
        if (!finfo->filterObj)
                return -ENOMEM;

        /* Configure the filter */
        ret = finfo->fconfig(finfo->filterObj, finfo->index,
                               finfo->nchans,
                               finfo->channels,
                               finfo->decimate,
                               cfg->filterData,
                               cfg->nFilterData);

        /* keep track of the total number of output samples/sec */
        brd->totalOutputRate += cfg->rate;

        for (i = 0; i < cfg->nvars; i++) {
                int ichan = cfg->channels[i];
                if (ichan < 0 || ichan >= NUM_NCAR_A2D_CHANNELS)
                        return -EINVAL;
                brd->gain[ichan] = cfg->gain[i];
                brd->offset[ichan] = !cfg->bipolar[i];
        }
        KLOG_DEBUG("%s: ret=%d\n", brd->deviceName, ret);

        return ret;
}
        
static void freeFilters(struct A2DBoard *brd)
{
        int i;
        for (i = 0; i < brd->nfilters; i++) {
            struct a2d_filter_info* finfo = brd->filters + i;
            /* cleanup filter */
            if (finfo->filterObj && finfo->fcleanup)
                finfo->fcleanup(finfo->filterObj);
            finfo->filterObj = 0;
            finfo->fcleanup = 0;
            kfree(finfo->channels);
        }
        kfree(brd->filters);
        brd->filters = 0;
        brd->nfilters = 0;


}

/*
 * Perform initial board setup.
 */
static int configBoard(struct A2DBoard *brd, struct nidas_a2d_config* cfg)
{
        int ret = 0;

        brd->scanRate = cfg->scanRate;

        /*
         * Scan deltaT, time in milliseconds between A2D scans of
         * the channels.
         */
        brd->scanDeltatMsec =   // compute in microseconds first to avoid trunc
            (USECS_PER_SEC / brd->scanRate) / USECS_PER_MSEC;

        brd->latencyJiffies = (cfg->latencyUsecs * HZ) / USECS_PER_SEC;
        if (brd->latencyJiffies == 0)
                brd->latencyJiffies = HZ / 10;
        return ret;
}

/*-----------------------Utility------------------------------*/
// 000 +01 +05 -10 +10
// --------------------
//  X   X   X   X   X   // 1 T    -10...+10
//  X   X   X   -   X   // 2 F      0...+10
//  X   X   X   -   -   // 2 T    -05...+05
//  X   X   X   -   -   // 4 F      0...+05
//
#define X -1
int GainOffsetToEnum[5][2] = {
//    0  1   <- offset = !bipolar
    { X, X}, // gain 0
    { X, 0}, // gain 1      1T
    { 1, 2}, // gain 2   2F 2T
    { X, X}, // gain 3
    { 3, X}, // gain 4   4F
};

int withinRange(int volt, int gain, int offset)
{
        int GO = GainOffsetToEnum[gain][!offset];

        if ( !CalVoltIsValid(volt) )
                return 0;
        if (GO < 0)
                return 0;
        if ( (volt == -10) && (GO > 0) )
                return 0;
        if ( (volt == 10) && (GO > 1) )
                return 0;
        return 1;
}
/**
 * Invoke filters.
 */
static void do_filters(struct A2DBoard *brd, dsm_sample_time_t tt,
                       const short *dp)
{
        int i;

// #define DO_FILTER_DEBUG
#if defined(DEBUG) & defined(DO_FILTER_DEBUG)
        static size_t nfilt = 0;
        static int maxAvail = 0;
        static int minAvail = 99999;

        i = CIRC_SPACE(brd->a2d_samples.head, brd->a2d_samples.tail,
                       brd->a2d_samples.size);
        if (i < minAvail)
                minAvail = i;
        if (i > maxAvail)
                maxAvail = i;
        if (!(nfilt++ % 1000)) {
                KLOG_DEBUG("%s: minAvail=%d,maxAvail=%d\n", minAvail,
                           brd->deviceName,maxAvail);
                maxAvail = 0;
                minAvail = 99999;
                nfilt = 1;
        }
#endif

        for (i = 0; i < brd->nfilters; i++) {
                short_sample_t *osamp = (short_sample_t *)
                    GET_HEAD(brd->a2d_samples, brd->a2d_samples.size);

                if (!osamp) {
                        /*
			 * no output sample available. Still execute filter so its state is up-to-date.
			 */
                        struct a2d_sample toss;
			/*
			 * Make sure sizeof tossed sample is big enough for the filter.
			 */
			BUG_ON(sizeof(struct a2d_sample) <
				SIZEOF_DSM_SAMPLE_HEADER + (NUM_NCAR_A2D_CHANNELS + 1) * sizeof(short));
                        if (!(brd->skippedSamples++ % 5000))
                                KLOG_WARNING("%s: skippedSamples=%d\n",
                                             brd->deviceName,
                                             brd->skippedSamples);
                        brd->filters[i].filter(brd->filters[i].filterObj,
                                               tt, dp, 1,
                                               (short_sample_t *) & toss);
                } else if (brd->filters[i].
                           filter(brd->filters[i].filterObj, tt, dp,
                                  1, osamp)) {

#ifdef __BIG_ENDIAN
                        // convert to little endian
                        int j;
                        osamp->id = cpu_to_le16(osamp->id);
                        for (j = 0; j < osamp->length / sizeof (short) - 1; j++)
                                osamp->data[j] = cpu_to_le16(osamp->data[j]);
#elif defined __LITTLE_ENDIAN
#else
#error "UNSUPPORTED ENDIAN-NESS"
#endif
                        INCREMENT_HEAD(brd->a2d_samples,
                                       brd->a2d_samples.size);
                }
        }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void a2d_bottom_half(struct work_struct *work)
#else
static void a2d_bottom_half(void *work)
#endif
{
        struct A2DBoard *brd =
            container_of(work, struct A2DBoard, sampleWorker);
        struct dsm_sample *insamp;

        while ((insamp = GET_TAIL(brd->fifo_samples,brd->fifo_samples.size))) {

                int nval = insamp->length / sizeof (short);

                short *dp = (short *) insamp->data;
                short *ep;
                int ndt;
                dsm_sample_time_t tt0;

                ep = dp + nval;

                BUG_ON((nval % NUM_NCAR_A2D_CHANNELS) != 0);

                /*
                 * How much to adjust the time tags backwards.
                 * Example:
                 *   poll rate = 20Hz (how often we download the A2D fifo)
                 *   scanRate = 500Hz (rate that A2D is scanning all 8 channels)
                 * Each FIFO poll then downloads 500/20 = 25 A2D scans.
                 * In the sample of 25 scans * 8 channels = 200 short integers
                 * from the A2D FIFO which was assigned a time tag 00:00.050, 
                 * we assume the individual 25 scans have timetags of:
                 * 00:00.000, 00:00.002, 00:00.004, 00:00.010, ... , 00:00.048
                 *
                 * ndt = # of deltaT to back up for first timetag
                 */
                 
                /*
                 * Test March 4, 2012, with PPS signal into channel 0 of an A2D.
                 * PPS from a Symmetricom S250 in the lab.
                 * IRIG was sync'd to the Symmetricom.  A2D sampled at 500 Hz.
                 *
                 * If an additional 3 delta-Ts are subtracted from the time tags,
                 * then the beginning of the PPS rise happens is time-tagged 
                 * on the exact second, in the data_dump output:
                 *
                 * 2012 03 05 00:02:14.9880   0.002       4      0    147 
                 * 2012 03 05 00:02:14.9900   0.002       4      0    149 
                 * 2012 03 05 00:02:14.9920   0.002       4      0    148 
                 * 2012 03 05 00:02:14.9940   0.002       4      0    147 
                 * 2012 03 05 00:02:14.9960   0.002       4      0    148 
                 * 2012 03 05 00:02:14.9980   0.002       4      0    147 
                 * 2012 03 05 00:02:15.0000   0.002       4      0    -13 
                 * 2012 03 05 00:02:15.0020   0.002       4      0    279 
                 * 2012 03 05 00:02:15.0040   0.002       4      0   -124 
                 * 2012 03 05 00:02:15.0060   0.002       4      0    360 
                 * 2012 03 05 00:02:15.0080   0.002       4      0   -229 
                 * 2012 03 05 00:02:15.0100   0.002       4      0    479 
                 * 2012 03 05 00:02:15.0120   0.002       4      0   -406 
                 * 2012 03 05 00:02:15.0140   0.002       4      0   1236 
                 * 2012 03 05 00:02:15.0160   0.002       4      0  14285 
                 * 2012 03 05 00:02:15.0180   0.002       4      0  16645 
                 * 2012 03 05 00:02:15.0200   0.002       4      0  15507 
                 * 2012 03 05 00:02:15.0220   0.002       4      0  16357 
                 */
                ndt = nval / NUM_NCAR_A2D_CHANNELS + 3;
                tt0 = insamp->timetag - ndt * brd->scanDeltatMsec;

                for (; dp < ep;) {
                        do_filters(brd, tt0, dp);
                        dp += NUM_NCAR_A2D_CHANNELS;
                        tt0 += brd->scanDeltatMsec;
                }
                INCREMENT_TAIL(brd->fifo_samples, brd->fifo_samples.size);
                // We wake up the read queue here so that the filters
                // don't have to know about it.  How often the
                // queue is woken depends on the requested latency.
                // 
                // Since the sample queue may fill up before
                // latencyJiffies have elapsed, we also wake the
                // read queue if the output sample queue is half full.
                if (brd->a2d_samples.head != brd->a2d_samples.tail) {
                        if (((long) jiffies - (long) brd->lastWakeup) >
                            brd->latencyJiffies ||
                            CIRC_SPACE(brd->a2d_samples.head,
                                       brd->a2d_samples.tail,
                                       brd->a2d_samples.size) <
                            brd->a2d_samples.size / 2) {
                                wake_up_interruptible(&brd->rwaitq_a2d);
                                brd->lastWakeup = jiffies;
                        }
                }
        }
}

/*
 * Read and discard a scan of data from A2D FIFO.
 */
static void discardA2DFifo(struct A2DBoard *brd)
{
        int i;
        for (i = 0; i < brd->nFifoValues; i++) {
                inw(brd->base_addr16);
        }
}

/*
 * Get a pointer to an empty fifo sample and
 * fill its data portion from the A2D fifo.  Queue
 * the sampleWorker thread to process the sample.
 */
static void readA2DFifo(struct A2DBoard *brd)
{
        struct dsm_sample *samp;
        int i;
        short* dp;
        /*
         * Read a sample from the hardware FIFO on the card.
         * Note that a FIFO sample will contain multiple samples
         * for each A2D channel:
         *      nsamp = brd->scanRate / brd->pollRate
         * The A2D scanRate is typically 500. For a brd->pollRate of
         * 25, the FIFO sample will contain 500 / 25 = 20 samples from
         * each of the 8 channels, giving 160 16-bit integers
         * with the channel number varying most rapidly.
         * These FIFO samples are then passed to the workqueue
         * bottom half, where they are broken out into individual,
         * timetagged samples for the requested channels, using the
         * requested simple boxcar or pickoff filters.
         */
        outb(A2DIO_FIFO, brd->cmd_addr);        // Set up to read data

        if (brd->discardNextScan) {
                discardA2DFifo(brd);
		brd->discardNextScan = 0;
		return;
        }
		
	samp = GET_HEAD(brd->fifo_samples, brd->fifo_samples.size);
        if (!samp) {            // no output sample available
                discardA2DFifo(brd);
                brd->skippedSamples +=
                    brd->nFifoValues / NUM_NCAR_A2D_CHANNELS;
		if (!(brd->skippedSamples % 100))
			KLOG_WARNING("%s: skippedSamples=%d\n", brd->deviceName,
				     brd->skippedSamples);
                return;
        }
	/*
	 * We are purposefully behind by 1 or more polling periods
         * in reading the FIFO so that we can be sure that
         * we're not reading from an empty fifo.  Therefore adjust
         * the sample timetags backwards by that number of polling periods.
         */
        samp->timetag = GET_MSEC_CLOCK - brd->delaysBeforeFirstPoll * brd->pollDeltatMsec;
        /*
         * Read the FIFO. FIFO values are little-endian.
         * inw converts data to host endian, whereas insw does not.
         * We want to convert to host endian in order to do
         * filtering here in the driver.
         * insw on the Vulcan just does a while loop in C, calling
         * inw and flipping the bytes. Therefore it is more efficient to do
         * our own loop with inw since we have to negate each value anyway.
         * Before sending the a2d values up to user space, they are
         * converted to little-endian, which is the convention for A2D data.
         */
        dp = (short *) samp->data;
        for (i = 0; i < brd->nFifoValues; i++) {
                *dp++ = -inw(brd->base_addr16);   // note: value is negated
        }

        samp->length = brd->nFifoValues * sizeof (short);
        /* increment head, this sample is ready for consumption */
        INCREMENT_HEAD(brd->fifo_samples, brd->fifo_samples.size);
        queue_work(work_queue, &brd->sampleWorker);
}

/*
 * Function scheduled to be called from IRIG driver at the polling frequency.
 * This is called in software interrupt context.
 * Create a sample from the A2D FIFO, and pass it on to other
 * routines to break it up and filter the samples.
 */
static void ReadSampleCallback(void *ptr)
{
        struct A2DBoard *brd = (struct A2DBoard *) ptr;
        int preFlevel;

        if (brd->numPollDelaysLeft > 0) {
                brd->numPollDelaysLeft--;
                return;
        }

        /*
         * If board is not healty, we're out-a-here.
         */
        if (brd->errorState != 0) return;

        /*
         * How full is the card's FIFO?
         */
        preFlevel = getA2DFIFOLevel(brd);
        brd->cur_status.preFifoLevel[preFlevel]++;

        /*
         * See discussion about delaysBeforeFirstPoll concerning
         * how full the FIFO should be before a poll.
         * If the fifo level is 4 (3/4 to 4/4 full) (or greater)
         * it could overflow before the next poll.
         */
#ifdef POLL_WHEN_QUARTER_FULL
        if (preFlevel < 2 || preFlevel > 3) {
                brd->resets++;
#ifdef USE_RESET_WORKER
		KLOG_ERR("%s: restarting acquistion due to bad FIFO level %d, expected 2 (1/4 to 3/4 full)\n",
                                    brd->deviceName,preFlevel);
                brd->errorState = WAITING_FOR_RESET;
                queue_work(work_queue, &brd->resetWorker);
#else
		KLOG_ERR("%s: bad FIFO level %d, expected 2 (1/4 to 3/4 full)\n",
                                    brd->deviceName,preFlevel);
                brd->errorState = -EIO;
#endif
                return;
        }
#else
        /*
         * If overflows due to missed IRIG interrupts are a recurring issue, we could
         * allow a preFlevel of 4 at this point, indicating that the FIFO
         * is over 3/4 but not completely full. However there is a danger that it
         * could overflow before readA2DFifo() gets around to reading.
         * It is probably a better idea to fix the missed interrupts, or
         * try to detect them with something like watchdog tasklet.
         */
        if (preFlevel < 1 || preFlevel > 3) {
                brd->resets++;
#ifdef USE_RESET_WORKER
		KLOG_ERR("%s: restarting acquistion due to bad FIFO level %d, expected 1 (less than 1/4 full)\n",
                                    brd->deviceName,preFlevel);
                brd->errorState = WAITING_FOR_RESET;
                queue_work(work_queue, &brd->resetWorker);
#else
		KLOG_ERR("%s: bad FIFO level %d, expected 1 (less than 1/4 full)\n",
                                    brd->deviceName,preFlevel);
                brd->errorState = -EIO;
#endif
                return;
        }
#endif

        /*
         * Read the fifo.
         */
        readA2DFifo(brd);

        brd->readCtr++;

        /*
         * Update stats every 10 seconds
         */
        if (!(brd->readCtr % (brd->pollRate * 10))) {
                /*
                 * copy current status to prev_status for access by ioctl
                 * NCAR_A2D_GET_STATUS
                 */
                brd->cur_status.skippedSamples = brd->skippedSamples;
                brd->cur_status.resets = brd->resets;
                brd->cur_status.ser_num = brd->ser_num;
                memcpy(&brd->prev_status, &brd->cur_status,
                       sizeof (struct ncar_a2d_status));
                memset(&brd->cur_status, 0, sizeof (struct ncar_a2d_status));
        }

}

// Callback function to read an I2C temperature sample

static void TemperatureCallback(void *ptr)
{
        struct A2DBoard *brd = (struct A2DBoard *) ptr;
        short_sample_t *osamp = (short_sample_t *)
                    GET_HEAD(brd->a2d_samples, brd->a2d_samples.size);
        if (!osamp) {            // no output sample available
                if (!(brd->skippedSamples++ % 1000))
			KLOG_WARNING("%s: skippedSamples=%d\n",
                             brd->deviceName, brd->skippedSamples);
                return;
        }
        osamp->timetag = GET_MSEC_CLOCK;
        osamp->length = 2 * sizeof (short);
        osamp->id = cpu_to_le16(NCAR_A2D_TEMPERATURE_INDEX);
        brd->currentTemp = A2DTemp(brd);
        osamp->data[0]   = cpu_to_le16(brd->currentTemp);
        INCREMENT_HEAD(brd->a2d_samples, brd->a2d_samples.size);
        wake_up_interruptible(&brd->rwaitq_a2d);
}

/**
 * Stop data collection on the selected board.
 */
static int stopBoard(struct A2DBoard *brd)
{
        int ret = 0;
        int i;

        // interrupt the 1PPS or acquisition
        brd->interrupted = 1;

        // Turn off the callback routines
        if (brd->a2dCallback)
                unregister_irig_callback(brd->a2dCallback);
        brd->a2dCallback = 0;
        if (brd->tempCallback)
                unregister_irig_callback(brd->tempCallback);
        brd->tempCallback = 0;

        // wait until callbacks are definitely finished
        flush_irig_callbacks();

        // wake any waiting reads
        wake_up_interruptible(&brd->rwaitq_a2d);

        AD7725StatusAll(brd);   // Read status and clear IRQ's

        A2DNotAuto(brd);        // Shut off auto mode (if enabled)

        // Abort all the A/D's
        A2DStopReadAll(brd);

        // must flush the workqueue before freeing the filters
        flush_workqueue(work_queue);

        freeFilters(brd);
        free_dsm_circ_buf(&brd->fifo_samples);
        free_dsm_circ_buf(&brd->a2d_samples);

        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
            brd->gain[i] = 0;

        brd->totalOutputRate = 0;

        brd->busy = 0;          // Reset the busy flag

        return ret;
}

/* 
 * Reset the A2D.
 */
static int resetBoard(struct A2DBoard *brd)
{
        int ret;
#ifdef USE_RESET_WORKER
        brd->errorState = WAITING_FOR_RESET;
#endif

        if (brd->a2dCallback)
                unregister_irig_callback(brd->a2dCallback);
        brd->a2dCallback = 0;

        if (brd->tempCallback)
                unregister_irig_callback(brd->tempCallback);
        brd->tempCallback = 0;
        // wait until callbacks are definitely finished
	flush_irig_callbacks();

        EMPTY_CIRC_BUF(brd->fifo_samples);
        EMPTY_CIRC_BUF(brd->a2d_samples);

        // Sync with 1PPS
        if ((ret = waitFor1PPS(brd,ppsCallback1)) != 0)
                return ret;

        A2DStopReadAll(brd);    // Send Abort command to all A/Ds

        if ((ret = A2DStartAll(brd)) != 0) return ret;  // Start all the A/Ds

        A2DSetSYNC(brd);        // Stop A/D clocks
        A2DAuto(brd);           // Switch to automatic mode
	A2DClearFIFO(brd);      // Clear the board's FIFO...

        KLOG_DEBUG("%s: Setting 1PPS Enable line\n",brd->deviceName);
        msleep(20);
        A2DEnable1PPS(brd);     // Enable sync with 1PPS

        if ((ret = waitFor1PPS(brd,ppsCallback1)) != 0)
                return ret;

        brd->discardNextScan = 1;       // whether to discard the initial scan
        brd->interrupted = 0;
        brd->readCtr = 0;
        brd->skippedSamples = 0;
        brd->numPollDelaysLeft = brd->delaysBeforeFirstPoll;

        // start the IRIG callback routine at the polling rate
        brd->a2dCallback =
            register_irig_callback(ReadSampleCallback,0, brd->irigRate, brd,&ret);
        if (!brd->a2dCallback) {
                KLOG_ERR("%s: error: register_irig_callback failed\n",brd->deviceName);
                return ret;
        }

        if (brd->tempRate != IRIG_NUM_RATES) {
            brd->tempCallback =
                register_irig_callback(TemperatureCallback,0, brd->tempRate,
                                       brd,&ret);
            if (!brd->tempCallback) {
                    KLOG_ERR("%s: error: register_irig_callback failed\n",brd->deviceName);
                    return ret;
            }
        }

        brd->busy = 1;          // Set the busy flag
        KLOG_DEBUG("%s: IRIG callbacks registered @ %d\n", brd->deviceName,GET_MSEC_CLOCK);

        KLOG_INFO("%s: reset succeeded\n", brd->deviceName);
        brd->errorState = ret;
        return ret;
}

#ifdef USE_RESET_WORKER
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void resetBoardWorkFunc(struct work_struct *work)
#else
static void resetBoardWorkFunc(void *work)
#endif
{
        struct A2DBoard *brd =
            container_of(work, struct A2DBoard, resetWorker);
        int ret = resetBoard(brd);
        if (ret && brd->resets > NUM_RESET_ATTEMPTS) {
                brd->errorState = ret;
                brd->resets = 0;
        }
}
#endif

/**
 * Start data collection on the selected board.  This is only called
 * via user space initiation (i.e., through the NCAR_A2D_RUN ioctl).
 */
static int startBoard(struct A2DBoard *brd)
{
        int ret;
        int nFifoValues;
        int haveMaster = 0;
        int wordsPerSec, pollRate;
        int nsamps;

        /* Create sample circular buffers to hold this much
         * data to get over momentary lapses. Then the bottom-half
         * worker can get this far behind the polling,
         * and likewise, user reads can get this far behind the
         * bottom-half worker without losing data.
         */
        int maxBufferSecs = 2;

        int i;

        /*
         * Set the master now if we were given an explicit one
         */
        if (Master[BOARD_INDEX(brd)] >= 0) {
                if ((ret =
                     A2DSetMaster(brd, Master[BOARD_INDEX(brd)])) < 0)
                        return ret;
                haveMaster = 1;
        }

        for (i = 0; !haveMaster && i < NUM_USABLE_NCAR_A2D_CHANNELS; i++) {
		if (brd->gain[i] > 0) {
			if ((ret = A2DSetMaster(brd, i)) < 0) return ret;
			haveMaster = 1;
                }
        }
        if (!haveMaster) return -EIO;

        // Configure DAC gain codes
        if ((ret = A2DSetGainAndOffset(brd)) != 0)
                return ret;

        // Make sure SYNC is cleared so clocks are running
        KLOG_DEBUG("%s: clearing SYNC\n",brd->deviceName);
        A2DClearSYNC(brd);

#define SHORT_START
#ifdef SHORT_START
        // Start then reset the A/D's
        // Start conversions
        KLOG_DEBUG("%s: starting A/D's\n",brd->deviceName);
        if ((ret = A2DStartAll(brd)) != 0) return ret;  // Start all the A/Ds

        // If starting from a cold boot, one needs to
        // let the A2Ds run for a bit before downloading
        // the filter data.
        msleep(20);

        // Then do a soft reset
        KLOG_DEBUG("%s: soft resetting A/D's\n",brd->deviceName);
        A2DStopReadAll(brd);
#endif
        A2DClearFIFO(brd);

        // Configure the A/D's
        KLOG_DEBUG("%s: sending filter config data to A/Ds\n",brd->deviceName);
        if ((ret = A2DConfigAll(brd)) != 0)
                return ret;
        // Reset the A/D's
        KLOG_DEBUG("%s: resetting A/Ds\n",brd->deviceName);
        A2DStopReadAll(brd);

        KLOG_DEBUG("%s: A/Ds ready for synchronous start\n",brd->deviceName);

        /*
         * Zero out status
         */
        memset(&brd->cur_status, 0, sizeof (struct ncar_a2d_status));
        memset(&brd->prev_status, 0, sizeof (struct ncar_a2d_status));

        memset(&brd->a2d_read_state, 0, sizeof (struct sample_read_state));
        EMPTY_CIRC_BUF(brd->a2d_samples);
        brd->lastWakeup = jiffies;

        wordsPerSec = brd->scanRate * NUM_NCAR_A2D_CHANNELS;

#ifdef FIXED_POLL_RATE
        pollRate = FIXED_POLL_RATE;
#else
        /*
         * Set the pollRate to read the maximum amount, but less
         * than 1/4 the FIFO size.
         * wordsPerSec is 4000 for a scan rate of 500/sec, so the
         * minimum possible poll rate is 15.6 Hz which will get
         * rounded up to 20 here.
         */
        pollRate = wordsPerSec / (HWFIFODEPTH / 4);
        if (pollRate < 10) pollRate = 10;
        else if (pollRate < 20) pollRate = 20;
        else if (pollRate < 25) pollRate = 25;
        else if (pollRate < 50) pollRate = 50;
        else pollRate = 100;
#endif

        brd->irigRate = irigClockRateToEnum(pollRate);
        BUG_ON(brd->irigRate == IRIG_NUM_RATES);

        /*
         * How many data values do we read per poll?
         */
        nFifoValues =
            brd->scanRate / pollRate * NUM_NCAR_A2D_CHANNELS;

#ifdef POLL_WHEN_QUARTER_FULL
        /* Purposefully get behind on polling so that the
         * FIFO is then at least 1/4 full when it is polled. Then
         * the FIFO can't be emptied on a poll, since the amount
         * read in a poll is less than 1/4 of the FIFO.
         */
        brd->delaysBeforeFirstPoll = HWFIFODEPTH / 4 / nFifoValues;
#else
        /*
         * If POLL_WHEN_QUARTER_FULL is not defined, then delaysBeforeFirstPoll
         * is fixed at one, in which case the FIFO will be less than 1/4 full
         * when polled, but one can be pretty certain that a poll won't drain it,
         * except in the weird case that one or more IRIG interrupts come too soon
         * due to the IRIG card getting badly out-of-sync. I don't know if this
         * ever happens.
         */
        brd->delaysBeforeFirstPoll = 1;
#endif

        KLOG_INFO("%s: pollRate=%d, nFifoValues=%d, first poll delay=%d\n",
                   brd->deviceName, pollRate,nFifoValues,brd->delaysBeforeFirstPoll);

        /*
         * If nFifoValues increases or polling rate changes, re-allocate samples
         */
        nsamps = pollRate * maxBufferSecs;
        /* next higher power of 2. fls()=find-last-set bit, numbered from 1 */
        nsamps = 1 << fls(nsamps);
        if (nsamps < 4) nsamps = 4;

        if (nFifoValues > brd->nFifoValues || nsamps != brd->fifo_samples.size) {
                ret = realloc_dsm_circ_buf(&brd->fifo_samples,
                                           nFifoValues * sizeof (short),nsamps);
                if (ret)
                        return ret;
        }
        brd->pollRate = pollRate;
        brd->nFifoValues = nFifoValues;

        /* number of output samples in maxBuffSecs */
        nsamps = maxBufferSecs * brd->totalOutputRate;
        /* next higher power of 2. fls()=find-last-set bit, numbered from 1 */
        nsamps = 1 << fls(nsamps);
        if (nsamps < 4) nsamps = 4;

        if (nsamps != brd->a2d_samples.size) {
                /*
                 * Data portion of a filtered sample contains a short integer id
                 * in addition to the data, hence we allocate one extra short for the id
                 */
                ret = realloc_dsm_circ_buf(&brd->a2d_samples,
                           (NUM_NCAR_A2D_CHANNELS + 1) * sizeof (short),nsamps);
                if (ret)
                        return ret;
        }

        /*
         * Scan deltaT, time in milliseconds between A2D scans of
         * the channels.
         */
        brd->scanDeltatMsec =   // compute in microseconds first to avoid trunc
            (USECS_PER_SEC / brd->scanRate) / USECS_PER_MSEC;

        /*
         * Poll deltaT, time in milliseconds between when we poll the A2D fifo.
         */
        brd->pollDeltatMsec = brd->scanDeltatMsec * brd->scanRate / brd->pollRate;

        KLOG_DEBUG("%s: nFifoValues=%d,scanDeltatMsec=%d,pollDeltatMsec=%d\n",
                   brd->deviceName,brd->nFifoValues, brd->scanDeltatMsec,brd->pollDeltatMsec);
        KLOG_DEBUG("%s: totalOutputRate=%d Hz, fifo circbuf size=%d, output circbuf size=%d\n",
            brd->deviceName,brd->totalOutputRate,brd->fifo_samples.size,brd->a2d_samples.size);

        brd->interrupted = 0;

        /*
         * Finally reset, which will start collection.
         */
        ret = resetBoard(brd);
        return ret;
}

/**
 * User-space open of the A/D device.
 */
static int ncar_a2d_open(struct inode *inode, struct file *filp)
{
        struct A2DBoard *brd = BoardInfo + iminor(inode);
        int i;

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        brd->i2c = I2CSCL | I2CSDA;
        brd->tempRate = IRIG_NUM_RATES;

        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
                brd->gain[i] = 0;

        KLOG_INFO("Opening %s\n", brd->deviceName);
        filp->private_data = brd;
        return 0;
}

/**
 * User-space close of the A/D device.
 */
static int ncar_a2d_release(struct inode *inode, struct file *filp)
{
        struct A2DBoard *brd = (struct A2DBoard *) filp->private_data;
        int ret = 0;

        KLOG_INFO("Releasing %s\n", brd->deviceName);

        /*
         * Turn off the callback routine
         */
        stopBoard(brd);

        return ret;
}

/**
 * Support for select/poll on the user device
 */
static unsigned int ncar_a2d_poll(struct file *filp, poll_table * wait)
{
        struct A2DBoard *brd = (struct A2DBoard *) filp->private_data;
        unsigned int mask = 0;
        poll_wait(filp, &brd->rwaitq_a2d, wait);

        if (brd->interrupted)
                mask |= POLLHUP;

#ifdef USE_RESET_WORKER
        if (brd->errorState && brd->errorState != WAITING_FOR_RESET)
                mask |= POLLERR;
#else
        if (brd->errorState) mask |= POLLERR;
#endif

        if (sample_remains(&brd->a2d_read_state) ||
                GET_TAIL(brd->a2d_samples,brd->a2d_samples.size))
                mask |= POLLIN | POLLRDNORM;    /* readable */
        return mask;
}

/**
 * User-space read on the A/D device.
 */
static ssize_t
ncar_a2d_read(struct file *filp, char __user * buf, size_t count,
              loff_t * pos)
{
        struct A2DBoard *brd = (struct A2DBoard *) filp->private_data;

        if (brd->interrupted) {
                KLOG_DEBUG("returning EOF after board interrupt\n");
                return 0;
        }
#ifdef USE_RESET_WORKER
        if (brd->errorState && brd->errorState != WAITING_FOR_RESET)
		return brd->errorState;
                
#else
        if (brd->errorState) return brd->errorState;
#endif

        return nidas_circbuf_read(filp, buf, count,
                                  &brd->a2d_samples, &brd->a2d_read_state,
                                  &brd->rwaitq_a2d);
}

/*
 * Function that is called on receipt of an ioctl request.
 * Return: negative Linux errno, or 0=OK
 */
static long
ncar_a2d_ioctl(struct file *filp, unsigned int cmd,unsigned long arg)
{
        struct A2DBoard *brd = (struct A2DBoard *) filp->private_data;
        void __user *userptr = (void __user *) arg;
        int len = _IOC_SIZE(cmd);
        int ret = -EINVAL;
        int rate, i;
        struct ncar_a2d_setup setup;
        unsigned short CalChans = 0;

        switch (cmd) {
        case NIDAS_A2D_GET_NCHAN:
                {
                        int nchan = NUM_USABLE_NCAR_A2D_CHANNELS;
                        if (copy_to_user(userptr, &nchan, sizeof(nchan)))
                                return -EFAULT;
                        ret = sizeof(int);
                }
                break;

        case NCAR_A2D_GET_STATUS:   /* user get of status */
                if (len != sizeof (struct ncar_a2d_status)) {
                        KLOG_ERR
                            ("NCAR_A2D_GET_STATUS len %d != sizeof(struct ncar_a2d_status)\n",
                             len);
                        break;
                }
                if (copy_to_user(userptr, &brd->prev_status, len))
                        return -EFAULT;
                ret = len;
                break;

        case NIDAS_A2D_SET_CONFIG:
                if (len != sizeof (struct nidas_a2d_config)) {
                        KLOG_ERR
                            ("NIDAS_A2D_SET_CONFIG len %d != sizeof(struct nidas_a2d_config)\n",
                             len);
                        break;  // invalid length
                }
                if (brd->busy) {
                        KLOG_WARNING
                            ("%s: A/D card is running. Can't configure.\n",
                             brd->deviceName);
                        ret = -EBUSY;
                        break;
                }
                {
                        struct nidas_a2d_config cfg;
                        if (copy_from_user(&cfg, userptr, len) != 0) {
                                ret = -EFAULT;
                                break;
                        }
                        ret = configBoard(brd,&cfg);
                }
                break;

        case NIDAS_A2D_CONFIG_SAMPLE:
                if (brd->busy) {
                        KLOG_WARNING
                            ("%s: A/D card is running. Can't configure.\n",
                             brd->deviceName);
                        ret = -EBUSY;
                        break;
                }
                {
                        /*
                         * copy structure without the contents
                         * of cfg.filterData, which has variable length
                         * depending on the filter. Then allocate another
                         * struct with an additional cfg.nFilterData bytes
                         * and copy into it.
                         */
                        struct nidas_a2d_sample_config cfg;
                        struct nidas_a2d_sample_config* cfgp;
                        if (copy_from_user(&cfg, userptr, len) != 0) {
                            ret = -EFAULT;
                            break;
                        }
                        // allocate large enough structure for filter data
                        len = sizeof(struct nidas_a2d_sample_config) + 
                            cfg.nFilterData;
                        cfgp = kmalloc(len,GFP_KERNEL);
                        if (!cfgp) {
                                ret = -ENOMEM;
                                break;
                        }
                        if (copy_from_user(cfgp, userptr, len) != 0) {
                                kfree(cfgp);
                                ret = -EFAULT;
                                break;
                        }
                        ret = addSampleConfig(brd,cfgp);
                        kfree(cfgp);
                }
                if (ret != 0)
                        KLOG_ERR
                            ("%s: setup failed with state %d\n",
                             brd->deviceName, ret);
                else
                        KLOG_DEBUG("%s: setup succeeded\n",
                                   brd->deviceName);
                break;

        case NCAR_A2D_SET_OCFILTER:
                if (len != sizeof (struct ncar_a2d_ocfilter_config)) {
                        KLOG_ERR
                            ("%s: NCAR_A2D_SET_OCFILTER len %d != sizeof(struct ncar_a2d_ocfilter_config)\n",
                             brd->deviceName,len);
                        break;  // invalid length
                }

                if (brd->busy) {
                        KLOG_WARNING
                            ("%s: A/D is running. Can't configure.\n",
                             brd->deviceName);
                        ret = -EBUSY;
                        break;
                }
                {
                        // use kmalloc/kfree here to use heap instead of stack.
                        // gcc 4.4 gives warning when stack is > 1024 bytes
                        struct ncar_a2d_ocfilter_config* cfg =
                            kmalloc(sizeof(struct ncar_a2d_ocfilter_config),GFP_KERNEL);
                        if (!cfg) {
                            ret = -ENOMEM;
                            break;
                        }
                        if (copy_from_user(cfg, userptr, len) != 0) {
                                kfree(cfg);
                                ret = -EFAULT;
                                break;
                        }
                        memcpy(brd->ocfilter,cfg->filter,sizeof(cfg->filter));
                        kfree(cfg);
                        ret = 0;
                }
                break;

        case NCAR_A2D_GET_SETUP:
                if (len != sizeof (struct ncar_a2d_setup))
                        break;

                if (!brd->busy) {
                        ret = -EAGAIN;
                        break;
                }
                memcpy(setup.gain,   brd->gain,       NUM_NCAR_A2D_CHANNELS * sizeof(int));
                memcpy(setup.offset, brd->offset,     NUM_NCAR_A2D_CHANNELS * sizeof(int));
                memcpy(setup.calset, brd->cal.calset, NUM_NCAR_A2D_CHANNELS * sizeof(int));
                setup.vcal =         brd->cal.vcal;

                ret = copy_to_user(userptr, &setup, sizeof(setup));
                if (ret < 0)
                        break;

                ret = sizeof (setup);
                break;

        case NCAR_A2D_SET_CAL:
                if (!brd->busy) {
                        ret = -EAGAIN;
                        break;
                }
                if (len != sizeof (struct ncar_a2d_cal_config))
                        break;  // invalid length

                if (copy_from_user(&(brd->cal),userptr, len) != 0) {
                        ret = -EFAULT;
                        break;
                }
                // switch OFF vcal generator
                UnSetVcal(brd);

                // change channels states
                for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                        CalChans >>= 1;
                        if (brd->cal.calset[i] != 0)
                                CalChans += 0x80;

                        // enable or disable channels
                        brd->cal.calset[i] *= brd->cal.state;

                        // disable channels that can't measure at the new voltage
                        brd->cal.calset[i] *=
                          withinRange(brd->cal.vcal, brd->gain[i], brd->offset[i]);
                }
                SetCal(brd);

                // leave vcal generatot OFF when all channels are disabled
                ret = 0;
                if ( (brd->cal.state == 0) && (CalChans == 0xFF) ) {
                        KLOG_INFO("%s: All channels and vcal generator are OFF.\n", brd->deviceName);
                        break;
                }
                // All channels setup, enable vcal generator.
                SetVcal(brd);
                break;

        case NCAR_A2D_RUN:
                ret = startBoard(brd);
                break;

        case NCAR_A2D_STOP:
                ret = stopBoard(brd);
                break;

        case NCAR_A2D_GET_TEMP:
                if (len != sizeof (short))
                        break;
                ret = copy_to_user(userptr, &brd->currentTemp, len);
                if (ret < 0)
                        break;
                ret = sizeof (short);
                break;

        case NCAR_A2D_SET_TEMPRATE:
                /*
                 * Set temperature query rate (using enum irigClockRates)
                 */
                if (len != sizeof (int)) {
                        KLOG_WARNING
                            ("%s: A2DTEMP_SET_RATE, bad len\n",brd->deviceName);
                        ret = -EINVAL;
                        break;
                }

                if (copy_from_user(&rate, userptr, len) != 0) {
                        ret = -EFAULT;
                        break;
                }

                if (rate > IRIG_10_HZ) {
                        ret = -EINVAL;
                        KLOG_WARNING
                            ("%s: Illegal rate for A/D temp probe (> 10 Hz)\n",brd->deviceName);
                        break;
                }
                brd->tempRate = rate;
                ret = 0;
                break;

        default:
                KLOG_ERR("%s: Bad A2D ioctl 0x%x\n", brd->deviceName,cmd);
                break;
        }
        if (ret == -EAGAIN)
                KLOG_ERR ("%s: A2D board is not running yet.\n", brd->deviceName);

        return ret;
}

/*-----------------------Module------------------------------*/
// Stops the A/D and releases reserved memory
static void __exit ncar_a2d_cleanup(void)
{
        int ib;
        int i;


        if (MAJOR(ncar_a2d_device)) {
                cdev_del(&ncar_a2d_cdev);
                unregister_chrdev_region(ncar_a2d_device, NumBoards);
        }

        if (BoardInfo) {
                for (ib = 0; ib < NumBoards; ib++) {
                        struct A2DBoard *brd = BoardInfo + ib;

                        if (brd->busy) {
                                stopBoard(brd);
                                AD7725StatusAll(brd);   // Read status and clear IRQ's
                        }

			// we don't use mutex_lock_interruptible here. It shouldn't
			// need interrupting - and we want to unregister the callback
			// in any case.
			mutex_lock(&brd->mutex);
			if (brd->ppsCallback) unregister_irig_callback(brd->ppsCallback);
			brd->ppsCallback = 0;
			flush_irig_callbacks();
			mutex_unlock(&brd->mutex);

                        free_dsm_circ_buf(&brd->a2d_samples);
                        free_dsm_circ_buf(&brd->fifo_samples);
                        if (brd->filters) {
                                for (i = 0; i < brd->nfilters; i++) {
                                        if (brd->filters[i].channels)
                                                kfree(brd->filters[i].
                                                      channels);
                                        brd->filters[i].channels = 0;
                                }
                                kfree(brd->filters);
                                brd->filters = 0;
                        }

                        if (brd->base_addr)
                                release_region(brd->base_addr, A2DIOWIDTH);
                        brd->base_addr = 0;

                }
        }

        kfree(BoardInfo);
        BoardInfo = 0;

        if (work_queue)
                destroy_workqueue(work_queue);

        KLOG_NOTICE("NCAR A/D cleanup complete\n");

        return;
}

/*-----------------------Module------------------------------*/

static int __init ncar_a2d_init(void)
{
        int error = -EINVAL;
        int ib, i;
        int chn;
        int nconfirmed,ntry;
        unsigned short status;

#ifndef SVNREVISION
#define SVNREVISION "unknown"
#endif
        KLOG_NOTICE("version: %s\n", SVNREVISION);
        KLOG_NOTICE("compiled on %s at %s\n", __DATE__, __TIME__);

        BoardInfo = 0;

        work_queue = create_singlethread_workqueue("ncar_a2d");

        /* count non-zero ioport addresses, gives us the number of boards */
        for (ib = 0; ib < MAX_A2D_BOARDS; ib++) {
                if (IoPort[ib] == 0)
                        break;
        }

        NumBoards = ib;
        if (NumBoards == 0) {
                KLOG_ERR("No boards configured, all IoPort[]==0\n");
                goto err;
        }
        KLOG_DEBUG("configuring %d board(s)...\n", NumBoards);

        error = -ENOMEM;
        BoardInfo =
            kmalloc(NumBoards * sizeof (struct A2DBoard), GFP_KERNEL);
        if (!BoardInfo)
                goto err;
	// initialize structure to zero, then initialize things
	// that are non-zero
	memset(BoardInfo, 0, NumBoards * sizeof (struct A2DBoard));

        /* initialize each A2DBoard structure */
        for (ib = 0; ib < NumBoards; ib++) {
                struct A2DBoard *brd = BoardInfo + ib;
                unsigned long addr;

                // for informational messages only at this point
                sprintf(brd->deviceName, "/dev/%s%d", DEVNAME_A2D, ib);

                /*
                 * Base address and command address
                 */
                addr = IoPort[ib] + SYSTEM_ISA_IOPORT_BASE;

                // Request the necessary I/O region
                if (!request_region(addr, A2DIOWIDTH, "NCAR A/D")) {
                        KLOG_ERR("ioport %#x already in use at virtual address %#lx\n",
                                        IoPort[ib],addr);
                        goto err;
                }
                brd->base_addr16 = brd->base_addr = addr;

#if defined(CONFIG_MACH_ARCOM_MERCURY) || defined(CONFIG_MACH_ARCOM_VULCAN)
                /* On Vulcans, do 16 bit I/O with address 0x1XXX */
                brd->base_addr16 = addr + VULCAN_IO_WINDOW_1_START;
#endif
                brd->cmd_addr = addr + A2DCMDADDR;

                mutex_init(&brd->mutex);

                // A2DNotAuto(brd);        // Shut off auto mode (if enabled)

                nconfirmed = 0;

                for (chn=0; chn<NUM_USABLE_NCAR_A2D_CHANNELS; chn++) {
                        unsigned short instr = AD7725_ABORT; 
                        // unsigned short instr = AD7725_READDATA; 
                        unsigned short expected = AD7725StatusInstrBits(instr);
// #define INIT_LOOP
#ifdef INIT_LOOP
                        for (i = 0; i < 10; i++) {
#endif
                                for (ntry = 0; ntry < 20; ntry++) {
                                        if (!(ntry % 2)) {
                                                outb(A2DIO_A2DSTAT, brd->cmd_addr);
                                                outw(instr, CHAN_ADDR16(brd, chn));
                                        }
                                        outb(A2DIO_A2DSTAT + A2DIO_LBSD3, brd->cmd_addr);
                                        status = inw(CHAN_ADDR16(brd, chn));


#ifdef INIT_LOOP
                                        KLOG_INFO("%s: chan %d response after cmd=%#04hx is %#04x, expected=%#04hx, status=%#04x, ntry=%d\n",
                                                brd->deviceName,chn,instr,(status&A2DSTAT_INSTR_MASK),expected,status,ntry);
                                        msleep(1000);
#else
                                        /*
                                         * No card at that address.
                                         */
                                        if (status == 0xffff) break;
                                        if ((status & A2DSTAT_INSTR_MASK) == expected) break;
                                        KLOG_NOTICE("%s: chan %d response after cmd=%#04hx is %#04x, expected=%#04hx, status=%#04x, ntry=%d\n",
                                                brd->deviceName,chn,instr,(status&A2DSTAT_INSTR_MASK),expected,status,ntry);
#endif
                                }
#ifdef INIT_LOOP
                        }
#endif
                        if ((status & A2DSTAT_INSTR_MASK) == expected) nconfirmed++;
                        else if (status != 0xffff) KLOG_INFO("channel %d not confirmed\n\n",chn);
                }
                if (nconfirmed < NUM_USABLE_NCAR_A2D_CHANNELS)
                {
                        /* Just log this as a notice.  Errors will be logged if the
                         * user actually tries to open the device.  */
                        KLOG_NOTICE("%s: NCAR A/D card not detected at ioport=%#x\n",
                                             brd->deviceName,IoPort[ib]);

                        if (ib == 0) goto err;
                        release_region(brd->base_addr, A2DIOWIDTH);
                        brd->base_addr16 = brd->base_addr = 0;
                        NumBoards = ib;
                        break;
                }

                brd->ser_num = getSerialNumber(brd);

                KLOG_INFO("%s: NCAR A/D board confirmed at 0x%03x, system address 0x%08lx, serial number=%hd\n",
                        brd->deviceName,IoPort[ib],brd->base_addr,brd->ser_num);

                brd->FIFOCtl = 0;

                brd->tempRate = IRIG_NUM_RATES;

                /*
                 * Initialize the read wait queue
                 */
                init_waitqueue_head(&brd->rwaitq_a2d);

		/* and the pps wait queue */
		init_waitqueue_head(&brd->ppsWaitQ);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
                INIT_WORK(&brd->sampleWorker, a2d_bottom_half);
#else
                INIT_WORK(&brd->sampleWorker, a2d_bottom_half,
                          &brd->sampleWorker);
#endif

#ifdef USE_RESET_WORKER
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
                INIT_WORK(&brd->resetWorker, resetBoardWorkFunc);
#else
                INIT_WORK(&brd->resetWorker, resetBoardWorkFunc,
                          &brd->resetWorker);
#endif
#endif

                /*
                 * Unset before each cal voltage change to avoid shorting
                 */
                brd->cal.vcal = -99;
                UnSetVcal(brd);

                /*
                 * We don't know how many values we will read from the FIFO yet,
                 * so the fifo sample circular buffer is allocated then
                 * when nFifoValues is known.
                 */
                brd->nFifoValues = 0;
                EMPTY_CIRC_BUF(brd->fifo_samples);
                EMPTY_CIRC_BUF(brd->a2d_samples);
        }

        /*
         * Initialize and add the user-visible devices for the A/D functions
         */
        if ((error = alloc_chrdev_region(&ncar_a2d_device, 0, NumBoards,
                                         DEVNAME_A2D)) < 0) {
                KLOG_ERR
                    ("Error %d allocating device major number for '%s'\n",
                     -error, DEVNAME_A2D);
                goto err;
        }

        cdev_init(&ncar_a2d_cdev, &ncar_a2d_fops);
        ncar_a2d_cdev.owner = THIS_MODULE;
        if ((error = cdev_add(&ncar_a2d_cdev, ncar_a2d_device, NumBoards)) < 0) {
                KLOG_ERR("cdev_add() for NCAR A/D failed!\n");
                goto err;
        }

        KLOG_DEBUG("A2D ncar_a2d_init complete.\n");

        return error;

      err:

        if (MAJOR(ncar_a2d_device) > 0) {
            cdev_del(&ncar_a2d_cdev);
            unregister_chrdev_region(ncar_a2d_device, NumBoards);
        }

        if (BoardInfo) {
                for (ib = 0; ib < NumBoards; ib++) {
                        struct A2DBoard *brd = BoardInfo + ib;

                        if (brd->filters) {
                                for (i = 0; i < brd->nfilters; i++) {
                                        if (brd->filters[i].channels)
                                                kfree(brd->filters[i].
                                                      channels);
                                        brd->filters[i].channels = 0;
                                }
                                kfree(brd->filters);
                                brd->filters = 0;
                        }
                        if (brd->base_addr)
                                release_region(brd->base_addr, A2DIOWIDTH);
                        brd->base_addr16 = brd->base_addr = 0;
                }
                kfree(BoardInfo);
                BoardInfo = 0;
        }

        if (work_queue)
                destroy_workqueue(work_queue);

        return error;
}

module_init(ncar_a2d_init);
module_exit(ncar_a2d_cleanup);
