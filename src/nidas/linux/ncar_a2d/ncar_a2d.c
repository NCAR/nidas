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

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <nidas/linux/util.h>
#include <nidas/linux/irigclock.h>
#include <nidas/linux/isa_bus.h>

#define DEBUG
#include <nidas/linux/klog.h>

#include "ncar_a2d_priv.h"

MODULE_AUTHOR("Chris Burghart <burghart@ucar.edu>");
MODULE_DESCRIPTION("NCAR A/D driver");
MODULE_LICENSE("GPL");

#define DO_A2D_STATRD

/* Number of reset attempts to try before giving up
 * and returning an error to the user via the poll
 * and read method.
 */
#define NUM_RESET_ATTEMPTS  5
//#define TEMPDEBUG

/* Selecting ioport address with SW22 on ncar A2D:
 * C=closed means set in direction of arrow on SW22, bit value=0
 * o=open, bit value=1
 * addr     8 7 6 5 4 3 2 1
 * 0x3a0    C C o o o C o C     viper, board 0
 * 0xba0    o C o o o C o C     vulcan, board 0
 */

/* I/O port addresses of installed boards, 0=no board installed */
static int IoPort[MAX_A2D_BOARDS] = { 0x3A0, 0, 0, 0 };

/* 
 * Which A2D chip is the master? (-1 causes the first requested channel to
 * be the master)
 */
static int Master[MAX_A2D_BOARDS] = { -1, -1, -1, -1 };

/*
 * Whether to invert counts.  This should be true for newer cards, but
 * early versions of the A2D cards did not invert signals.  This is
 * settable as a module parameter.  We could do it by checking the serial
 * number in firmware, but don't have faith that these serial numbers will
 * be set correctly in the firmware on the cards.
 */
static int Invert[MAX_A2D_BOARDS] = { 1, 1, 1, 1 };

static int nIoPort,nInvert,nMaster;
#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(IoPort, int, &nIoPort, S_IRUGO);
module_param_array(Invert, bool, &nInvert, S_IRUGO);
module_param_array(Master, int, &nMaster, S_IRUGO);
#else
module_param_array(IoPort, int, nIoPort, S_IRUGO);
module_param_array(Invert, bool, nInvert, S_IRUGO);
module_param_array(Master, int, nMaster, S_IRUGO);
#endif

MODULE_PARM_DESC(IoPort, "ISA port address of each board, e.g.: 0x3A0");
MODULE_PARM_DESC(Invert, "Whether to invert counts, default=1(true)");
MODULE_PARM_DESC(Master,
                 "Master A/D for the board, default=first requested channel");

/* number of A2D boards in system (number of non-zero ioport values) */
static int NumBoards = 0;

static struct A2DBoard *BoardInfo = 0;
#define BOARD_INDEX(boardptr) (((boardptr) - BoardInfo) / \
				sizeof(struct A2DBoard))

static struct workqueue_struct *work_queue = 0;

/* 
 * Address for a specific channel on a board: board base address + 2 * channel
 */
static inline int CHAN_ADDR(struct A2DBoard *brd, int channel)
{
        return (brd->base_addr + 2 * channel);
}

/*
 * prototypes
 */
int init_module(void);
void cleanup_module(void);
static int stopBoard(struct A2DBoard *brd);
static ssize_t ncar_a2d_read(struct file *filp, char __user * buf,
                             size_t count, loff_t * f_pos);
static int ncar_a2d_open(struct inode *inode, struct file *filp);
static int ncar_a2d_release(struct inode *inode, struct file *filp);
static unsigned int ncar_a2d_poll(struct file *filp, poll_table * wait);
static int ncar_a2d_ioctl(struct inode *inode, struct file *filp,
                          unsigned int cmd, unsigned long arg);
/*
 * Tag to mark waiting for reset attempt.  Any value not likely to 
 * show up as a system error is fine here...
 */
static const int WAITING_FOR_RESET = 77777;

/*
 * Operations for the devices
 */
static struct file_operations ncar_a2d_fops = {
        .owner = THIS_MODULE,
        .read = ncar_a2d_read,
        .open = ncar_a2d_open,
        .ioctl = ncar_a2d_ioctl,
        .release = ncar_a2d_release,
        .poll = ncar_a2d_poll,
};

/**
 * Info for A/D user devices
 */
#define DEVNAME_A2D "ncar_a2d"
static dev_t A2DDevStart = MKDEV(0, 0);
static struct cdev A2DCdev;

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
static inline void i2c_getAck(struct A2DBoard *brd)
{
        unsigned char ack = 0;
        i2c_clock_hi(brd);
//    ack = inb(brd->base_addr) & 0x1;
        i2c_clock_lo(brd);
        if (ack != 0)
                KLOG_NOTICE("Oops on I2C ACK from board %d!\n",
                            BOARD_INDEX(brd));
}

/*
 * Acknowledge with a zero bit.
 */
static inline void i2c_putAck(struct A2DBoard *brd)
{
        i2c_data_lo(brd);
        i2c_clock_hi(brd);
        i2c_clock_lo(brd);  // 
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

        i2c_putAck(brd);        // 3 I2C operations
        // total: 2 * 8 + 3 = 19 I2C ops

        return byte;
}

static inline void i2c_put_byte(struct A2DBoard *brd, unsigned char byte)
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

        i2c_getAck(brd);        // 2 I2C operations
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
        outb(A2DIO_FIFO, brd->cmd_addr);
        brd->FIFOCtl |= FIFOWREBL;
        outb(brd->FIFOCtl, brd->base_addr);
        outb(A2DIO_D2A2, brd->cmd_addr);
        /*
         * Send I2C start sequence
         */
        i2c_start_sequence(brd);        // 4 operations

        /*
         * I2C address goes in bits 7-1, and we set bit 0 to indicate we want
         * to read.
         */
        b0 = (address << 1) | 1;
        i2c_put_byte(brd, b0);  // 26 operations

        /*
         * Get the two data bytes
         */
        b0 = i2c_get_byte(brd); // 19 operations
        b1 = i2c_get_byte(brd); // 19 operations

        /*
         * Send I2C stop sequence
         */
        i2c_stop_sequence(brd); // 4 operations

        /*
         * Disable access to the i2c chip
         */
        outb(A2DIO_FIFO, brd->cmd_addr);
        brd->FIFOCtl &= ~FIFOWREBL;
        outb(brd->FIFOCtl, brd->base_addr);

        return (short) ((b0 << 8 | b1) >> 3);
}

/*-----------------------End I2C Utils -----------------------*/


/*-----------------------Utility------------------------------*/


/*
 * Wait up to maxmsecs milliseconds for the interrupt bit for the 
 * selected channel to be set.
 */
inline static int
waitForChannelInterrupt(struct A2DBoard *brd, int channel, int maxmsecs)
{
        int cnt;
        unsigned char interrupts;
        unsigned char mask = (1 << channel);
        int mwait = 1;

        schedule();
        udelay(10);
        schedule();

        outb(A2DIO_RDINTR, brd->cmd_addr);
        for (cnt = 0; cnt <= (maxmsecs / mwait); cnt++) {
                interrupts = inb(brd->base_addr);
                if ((interrupts & mask) != 0) {
                        if (cnt > 1)
                                KLOG_DEBUG
                                    ("interrupt bit set for channel %d, cnt=%d\n",
                                     channel, cnt);
                        return 0;
                }
                // udelay(10);
                msleep(mwait);
        }
        return -ETIMEDOUT;
}


// Read status of AD7725 A/D chip specified by channel 0-7
static unsigned short AD7725Status(struct A2DBoard *brd, int channel)
{
        outb(A2DIO_RDCHANSTAT, brd->cmd_addr);
        return (inw(CHAN_ADDR(brd, channel)));
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

/*
 * Return true iff the last instruction as shown in the A/D chip's status
 * matches the given A/D chip instruction.
 */
inline int
confirm7725Instruction(unsigned short status, unsigned short instr)
{
        /* Mask for all the instruction bits in the status word */
        const unsigned short instr_mask = A2DINSTREG15 | A2DINSTREG13 |
            A2DINSTREG12 | A2DINSTREG11 | A2DINSTREG06 | A2DINSTREG05 |
            A2DINSTREG04 | A2DINSTREG01 | A2DINSTREG00;
        unsigned short status_instr;
        unsigned short expected = 0;

        if (((instr >> 15) & 1) != 0)
                expected |= A2DINSTREG15;
        if (((instr >> 13) & 1) != 0)
                expected |= A2DINSTREG13;
        if (((instr >> 12) & 1) != 0)
                expected |= A2DINSTREG12;
        if (((instr >> 11) & 1) != 0)
                expected |= A2DINSTREG11;
        if (((instr >> 6) & 1) != 0)
                expected |= A2DINSTREG06;
        if (((instr >> 5) & 1) != 0)
                expected |= A2DINSTREG05;
        if (((instr >> 4) & 1) != 0)
                expected |= A2DINSTREG04;
        if (((instr >> 1) & 1) != 0)
                expected |= A2DINSTREG01;
        if (((instr >> 0) & 1) != 0)
                expected |= A2DINSTREG00;

        // instruction bits from status
        status_instr = status & instr_mask;

        if (status_instr != expected) {
                KLOG_NOTICE("status 0x%04x "
                           "(instr bits: actual 0x%04x, expected 0x%04x).\n",
                           status, status_instr, expected);
        }

        return (status_instr == expected);
}


static inline int
A2DConfirmInstruction(struct A2DBoard *brd, int channel,
                      unsigned short instr)
{
        unsigned short status = AD7725Status(brd, channel);
        int ok = confirm7725Instruction(status, instr);

        if (!ok)
                KLOG_NOTICE
                    ("Instruction 0x%04x on channel %d not confirmed\n",
                     instr, channel);

        return ok;
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
                        return -EINVAL;
                }
        } else {
                switch(brd->gain[channel]) {
                case 1:
                        gainCode = 0x2200 + channel;    // -10 to +10
                        break;
                case 2:
                        gainCode = 0x4400 + channel;
                        break;
                case 4:
                        gainCode = 0x8800 + channel;
                        break;
                default:
                        return -EINVAL;
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
            ("chn: %d   offset: %d   gain: %2d   outb( 0x%x, 0x%x)\n",
             channel, brd->offset[channel], brd->gain[channel], gainCode,
             brd->base_addr);
        // KLOG_DEBUG("outb( 0x%x, 0x%x);\n", gainCode, brd->base_addr);
        outw(gainCode, brd->base_addr);
        msleep(10);
        return 0;
}

/*-----------------------Utility------------------------------*/
// A2DSetMaster routes the interrupt signal from the target A/D chip
// to the ISA bus interrupt line.

static int A2DSetMaster(struct A2DBoard *brd, int channel)
{
        if (channel < 0 || channel >= NUM_USABLE_NCAR_A2D_CHANNELS) {
                KLOG_ERR("bad master chip number: %d\n", channel);
                return -EINVAL;
        }

        KLOG_DEBUG("A2DSetMaster, Master=%d\n", channel);
        outb(A2DIO_WRMASTER, brd->cmd_addr);
        outb((char) channel, brd->base_addr);
        return 0;
}

/*-----------------------Utility------------------------------*/
// Set a calibration voltage for all channels:
//
//  bit  volts
//  0x01 gnd
//  0x02 +1
//  0x04 +5
//  0x08 -10
//  0x10 +10
//
static int A2DSetVcal(struct A2DBoard *brd)
{
        // Check that V is within limits
        int ret = -EINVAL;
        int i, valid[] = { 0x01, 0x02, 0x04, 0x08, 0x10 };
        for (i = 0; i < 5; i++) {
                if (brd->cal.vcalx8 == valid[i])
                        ret = 0;
        }
        if (ret)
                return ret;

        // Point to the calibration DAC channel
        outb(A2DIO_D2A2, brd->cmd_addr);
        KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_D2A2, brd->cmd_addr);

        // Write cal voltage code
        outw(brd->cal.vcalx8, brd->base_addr);
        KLOG_DEBUG("brd->cal.vcalx8=0x%x\n", brd->cal.vcalx8);
        return 0;
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
static void A2DSetCal(struct A2DBoard *brd)
{
        unsigned short OffChans = 0;
        unsigned short CalChans = 0;
        int i;

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
        outb(A2DIO_WRCALOFF, brd->cmd_addr);
        KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_WRCALOFF, brd->cmd_addr);

        // Set the appropriate bits in OffCal
        brd->OffCal = (OffChans << 8) & 0xFF00;
        brd->OffCal |= CalChans;
        brd->OffCal = ~(brd->OffCal) & 0xFFFF;  // invert bits

        // Send OffCal word to system control word
        outw(brd->OffCal, brd->base_addr);
        KLOG_DEBUG("brd->OffCal:  0x%04x\n", brd->OffCal);
}

/*-----------------------Utility------------------------------*/
// Switch channels specified by Chans bits to offset mode.
// Offset bits are in upper byte
//
// 0xXX00
//   \|
//    8 bit selection of which channel is offset (unipolar = high)
//
static void A2DSetOffset(struct A2DBoard *brd)
{
        unsigned short OffChans = 0;
        int i;

        // Change the offset array of bools into a byte
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                OffChans >>= 1;
                if (brd->offset[i] != 0)
                        OffChans += 0x80;
        }
        // Point at the system control input channel
        outb(A2DIO_WRCALOFF, brd->cmd_addr);
        KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_WRCALOFF, brd->cmd_addr);

        // Set the appropriate bits in OffCal
        brd->OffCal = (OffChans << 8) & 0xFF00;
        brd->OffCal = ~(brd->OffCal) & 0xFFFF;  // invert bits

        // Send OffCal word to system control word
        outw(brd->OffCal, brd->base_addr);
        KLOG_DEBUG("brd->OffCal:  0x%04x\n", brd->OffCal);
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
        outb(A2DIO_RDBOARDSTAT, brd->cmd_addr);
        return inw(brd->base_addr);
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
        // Point to the A2D command register
        outb(A2DIO_WRCMD, brd->cmd_addr);

        // Send specified A/D the abort (soft reset) command
        outw(AD7725_ABORT, CHAN_ADDR(brd, channel));
        return;
}

/*-----------------------Utility------------------------------*/
// Send ABORT command to all A/D chips

static void A2DStopReadAll(struct A2DBoard *brd)
{
        int i;
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
                if (brd->gain[i] > 0)
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
        if (channel < 0 || channel >= NUM_USABLE_NCAR_A2D_CHANNELS) {

                return -EINVAL;
        }
        // Point at the A/D command channel
        outb(A2DIO_WRCMD, brd->cmd_addr);

        // Start the selected A/D
        outw(AD7725_READDATA, CHAN_ADDR(brd, channel));
        return 0;
}

static int A2DStartAll(struct A2DBoard *brd)
{
        int i;
        int ret = 0;
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
                if (brd->gain[i] > 0 && (ret = A2DStart(brd, i) != 0))
                    return ret;
        return ret;
}

/*-----------------------Utility------------------------------*/
// Configure A/D channel with coefficient array 'filter'
static int A2DConfig(struct A2DBoard *brd, int channel)
{
        int coef;
        unsigned short stat;
        int nCoefs =
            sizeof (brd->ocfilter) / sizeof (brd->ocfilter[0]);

        KLOG_INFO("Configuring board %d/channel %d\n", BOARD_INDEX(brd),
                    channel);
        if (channel < 0 || channel >= NUM_USABLE_NCAR_A2D_CHANNELS)
                return -EINVAL;

        // Set up to write a command to a channel
        outb(A2DIO_WRCMD, brd->cmd_addr);

        // Set configuration write mode for our channel
        outw(AD7725_WRCONFIG, CHAN_ADDR(brd, channel));

        // Verify that the command got there...
        if (!A2DConfirmInstruction(brd, channel, AD7725_WRCONFIG)) {
                stat = AD7725Status(brd, channel);
                KLOG_ERR
                    ("Failed confirmation for A/D instruction 0x%04x on "
                     "channel %d, status=0x%04x\n", AD7725_WRCONFIG,
                     channel, stat);
                return -EIO;
        }
        // Wait for interrupt bit to set
        if (waitForChannelInterrupt(brd, channel, 10) != 0) {
                KLOG_ERR
                    ("Timeout waiting before sending coefficients on channel %d\n",
                     channel);
                return -ETIMEDOUT;
        }
        KLOG_DEBUG("downloading filter coefficients, nCoefs=%d\n", nCoefs);

        for (coef = 0; coef < nCoefs; coef++) {
                // Set up for config write and write out coefficient
                outb(A2DIO_WRCOEF, brd->cmd_addr);
                outw(brd->ocfilter[coef], CHAN_ADDR(brd, channel));

                if (waitForChannelInterrupt(brd, channel, 10) != 0) {
                        KLOG_ERR
                            ("Timeout waiting after coefficient %d on channel %d\n",
                             coef, channel);
                        return -ETIMEDOUT;
                }
                outb(A2DIO_RDINTR, brd->cmd_addr);

                // Read status word from target a/d and check for errors
                stat = AD7725Status(brd, channel);

                if (stat & A2DIDERR) {
                        KLOG_ERR("Bad ID value on coefficient %d, channel %d\n", coef,channel);
                        return -EIO;
                }

                if (stat & A2DCRCERR) {
                        KLOG_ERR("BAD CRC @ coefficient %d, channel %d", coef,channel);
                        return -EIO;
                }
        }

        // We should have CFGEND status now (channel configured and ready)
        stat = AD7725Status(brd, channel);
        if ((stat & A2DCONFIGEND) == 0) {
                KLOG_ERR
                    ("CFGEND bit not set in status after configuring channel %d\n",
                     channel);
                return -EIO;
        }

        outb(A2DIO_RDCHANSTAT, brd->cmd_addr);
        brd->cur_status.goodval[channel] = inw(CHAN_ADDR(brd, channel));
        return 0;
}

/*-----------------------Utility------------------------------*/
// Configure all A/D's with same filter

static int A2DConfigAll(struct A2DBoard *brd)
{
        int ret = 0;
        int i;
        for (i = 0; i < NUM_USABLE_NCAR_A2D_CHANNELS; i++) {
                if ((ret = A2DConfig(brd, i)) < 0)
                        return ret;
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


/*-----------------------Utility------------------------------*/
// Utility function to wait for INV1PPS to be zero.
// Return: negative errno, or 0=OK.

static int waitFor1PPS_test(struct A2DBoard *brd)
{
        unsigned short stat;
        int msecs;
        int uwait = 10;
        int i, j;
        int utry;

        for (i = 0; i < 50; i++) {

                msecs =
                    MSECS_PER_SEC - (GET_MSEC_CLOCK % MSECS_PER_SEC) - 20;
                KLOG_DEBUG("GET_MSEC_CLOCK=%ld, sleeping %d msecs\n",
                          GET_MSEC_CLOCK, msecs);
                msleep(msecs);

                utry = (i + 1) * 40 * USECS_PER_MSEC / uwait;

                for (j = 0; j < utry; j++) {
                        if (brd->interrupted)
                                return -EINTR;
                        // Read status, check INV1PPS bit
                        stat = A2DBoardStatus(brd);
                        if ((stat & INV1PPS) == 0) {
                                KLOG_DEBUG("Found 1PPS after %d usecs, GET_MSEC_CLOCK=%ld\n",
                                           j * uwait,GET_MSEC_CLOCK);
                                return 0;
                        }

                        udelay(uwait);
                        schedule();
                }
        }
        KLOG_ERR("1PPS not detected--no sync to GPS\n");
        return -ETIMEDOUT;
}
static int waitFor1PPS(struct A2DBoard *brd)
{
        unsigned short stat;
        int uwait = 5;
        int i;

        for (i = 0; i < 2 * USECS_PER_SEC / uwait; i++) {
                if (brd->interrupted)
                        return -EINTR;
                // Read status, check INV1PPS bit
                stat = A2DBoardStatus(brd);
                if ((stat & INV1PPS) == 0) {
                        KLOG_DEBUG("Found 1PPS after %d usecs, GET_MSEC_CLOCK=%ld\n",
                               i * uwait,GET_MSEC_CLOCK);
                        return 0;
                }

                schedule();
                udelay(uwait);
                schedule();
        }
        KLOG_ERR("1PPS not detected--no sync to GPS\n");
        return -ETIMEDOUT;
}

static int A2DSetGainAndOffset(struct A2DBoard *brd)
{
        int i;
        int ret = 0;
        int repeat;

#ifdef DO_A2D_STATRD
        brd->FIFOCtl = A2DSTATEBL;      // Clear most of FIFO Control Word
#else
        brd->FIFOCtl = 0;       // Clear FIFO Control Word
#endif

        brd->OffCal = 0x0;

        // HACK! the CPLD logic needs to be fixed!  
        for (repeat = 0; repeat < 3; repeat++) {
                for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                        /*
                         * Set gain for requested channels
                         */
                        if (brd->gain[i] > 0
                            && (ret = A2DSetGain(brd, i)) != 0)
                                return ret;
                }
                outb(A2DIO_D2A1, brd->cmd_addr);
                msleep(10);
                outb(A2DIO_D2A2, brd->cmd_addr);
                msleep(10);
                outb(A2DIO_D2A1, brd->cmd_addr);
                msleep(10);
        }
        // END HACK!


        brd->cur_status.ser_num = getSerialNumber(brd);
        KLOG_INFO("serial number=%d\n",brd->cur_status.ser_num);

        A2DSetOffset(brd);

        KLOG_DEBUG("success!\n");
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
                KLOG_ERR("A2D's running. Can't configure\n");
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
                       A2D_SAMPLE_QUEUE_SIZE);
        if (i < minAvail)
                minAvail = i;
        if (i > maxAvail)
                maxAvail = i;
        if (!(nfilt++ % 1000)) {
                KLOG_DEBUG("minAvail=%d,maxAvail=%d\n", minAvail,
                           maxAvail);
                maxAvail = 0;
                minAvail = 99999;
                nfilt = 1;
        }
#endif

        for (i = 0; i < brd->nfilters; i++) {
                short_sample_t *osamp = (short_sample_t *)
                    GET_HEAD(brd->a2d_samples, A2D_SAMPLE_QUEUE_SIZE);
                if (!osamp) {
                        // no output sample available
                        // still execute filter so its state is up-to-date.
                        struct a2d_sample toss;
                        if (!(brd->skippedSamples++ % 1000))
                                KLOG_WARNING("%s: skippedSamples=%d\n",
                                             brd->deviceName,
                                             brd->skippedSamples);
                        brd->filters[i].filter(brd->filters[i].filterObj,
                                               tt, dp, brd->skipFactor,
                                               (short_sample_t *) & toss);
                } else if (brd->filters[i].
                           filter(brd->filters[i].filterObj, tt, dp,
                                  brd->skipFactor, osamp)) {

#ifdef __BIG_ENDIAN
                        // convert to little endian
                        int j;
                        for (j = 0; j < osamp->length / sizeof (short); j++)
                                osamp->data[j] = cpu_to_le16(osamp->data[j]);
#elif defined __LITTLE_ENDIAN
#else
#error "UNSUPPORTED ENDIAN-NESS"
#endif
                        INCREMENT_HEAD(brd->a2d_samples,
                                       A2D_SAMPLE_QUEUE_SIZE);
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

        while (brd->fifo_samples.head != brd->fifo_samples.tail) {
                struct dsm_sample *insamp =
                    brd->fifo_samples.buf[brd->fifo_samples.tail];

                int nval =
                    insamp->length / sizeof (short) / brd->skipFactor;

                short *dp = (short *) insamp->data;
                short *ep;
                int ndt;
                dsm_sample_time_t tt0;

#ifdef DO_A2D_STATRD
                dp++;           // skip over first status word
#endif

                ep = dp + nval * brd->skipFactor;

                BUG_ON((nval % NUM_NCAR_A2D_CHANNELS) != 0);

                /*
                 * How much to adjust the time tags backwards.
                 * Example:
                 *   poll rate = 100Hz (how often we download the A2D fifo)
                 *   scanRate = 500Hz
                 * When we download the A2D fifos at time=00:00.010, we are
                 *  getting the 5 samples that (we assume) were sampled at
                 *  times: 00:00.002, 00:00.004, 00:00.006,
                 *  00:00.008 and 00:00.010
                 * ndt = # of deltaT to back up for first timetag
                 */
                ndt = (nval - 1) / NUM_NCAR_A2D_CHANNELS;
                tt0 = insamp->timetag - ndt * brd->scanDeltatMsec;

                for (; dp < ep;) {
                        do_filters(brd, tt0, dp);
                        dp += NUM_NCAR_A2D_CHANNELS * brd->skipFactor;
                        tt0 += brd->scanDeltatMsec;
                }
                INCREMENT_TAIL(brd->fifo_samples, FIFO_SAMPLE_QUEUE_SIZE);
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
                                       A2D_SAMPLE_QUEUE_SIZE) <
                            A2D_SAMPLE_QUEUE_SIZE / 2) {
                                wake_up_interruptible(&brd->rwaitq_a2d);
                                brd->lastWakeup = jiffies;
                        }
                }
        }
}

/*
 * Get a pointer to an empty fifo sample and
 * fill its data portion from the A2D fifo.  Queue
 * the sampleWorker thread to process the sample.
 */
static void readA2DFifo(struct A2DBoard *brd)
{
//#define DETECT_SPIKE 30000 // undefine to disable
#ifdef DETECT_SPIKE
        static int last[8];
        int chan, diff;
#endif

        struct dsm_sample *samp;
        int i;
        short* dp;
        /*
         * Read the data for the DSM sample from the card.  Note that a DSM
         * sample will contain brd->sampsPerCallback individual samples for
         * each requested channel.
         */
        outb(A2DIO_FIFO, brd->cmd_addr);        // Set up to read data

        samp = GET_HEAD(brd->fifo_samples, FIFO_SAMPLE_QUEUE_SIZE);
        if (!samp) {            // no output sample available
                brd->skippedSamples +=
                    brd->nFifoValues / NUM_NCAR_A2D_CHANNELS /
                    brd->skipFactor;
                KLOG_WARNING("%s: skippedSamples=%d\n", brd->deviceName,
                             brd->skippedSamples);
                insw(brd->base_addr, brd->discardBuffer, brd->nFifoValues);
                return;
        }
        samp->timetag = GET_MSEC_CLOCK;
        /*
         * Read the fifo.  Note that inw on the Vulcan munges things to 
         * local CPU (i.e. big-endian) order, which is OK here,
         * since we will be doing filtering on the data.
         * Before sending it up the user space it is converted
         * to little-endian, which is the convention for A2D data.
         */
        // KLOG_DEBUG("reading fifo, nvalues=%d\n",brd->nFifoValues);

        /* Note that inw converts data to host endian,
         * whereas insw does not */
        insw(brd->base_addr, (short *) samp->data, brd->nFifoValues);
        if (brd->invertCounts) {
                dp = (short *) samp->data;
                for (i = 0; i < brd->nFifoValues/brd->skipFactor; i++) {
#ifdef DO_A2D_STATRD
                        dp++;           // skip over status word
#endif
#ifdef __BIG_ENDIAN
                        *dp = -le16_to_cpu(*dp);
#else
                        *dp = -*dp;
#endif
#ifdef DETECT_SPIKE
                        chan = i % (brd->nFifoValues / NUM_NCAR_A2D_CHANNELS / brd->skipFactor);
                        diff = last[chan] - *dp;
                        if (diff < 0) diff *= -1;
                        if (diff > DETECT_SPIKE) KLOG_DEBUG("** A2D SPIKE of %d on channel %d at %ld **\n",
                                                    diff, chan, GET_MSEC_CLOCK);
                        last[chan] = *dp;
#endif
                        dp++;
                }
        }
#ifdef __BIG_ENDIAN
        else {
                dp = (short *) samp->data;
                for (i = 0; i < brd->nFifoValues/brd->skipFactor; i++) {
#ifdef DO_A2D_STATRD
                        dp++;           // skip over status word
#endif
                        *dp = -le16_to_cpu(*dp);
#ifdef DETECT_SPIKE
                        chan = i % (brd->nFifoValues / NUM_NCAR_A2D_CHANNELS / brd->skipFactor);
                        diff = last[chan] - *dp;
                        if (diff < 0) diff *= -1;
                        if (diff > DETECT_SPIKE) KLOG_DEBUG("** A2D SPIKE of %d on channel %d at %ld **\n",
                                                    diff, chan, GET_MSEC_CLOCK);
                        last[chan] = *dp;
#endif
                        dp++;
                }
        }
#endif

        samp->length = brd->nFifoValues * sizeof (short);
        /* increment head, this sample is ready for consumption */
        INCREMENT_HEAD(brd->fifo_samples, FIFO_SAMPLE_QUEUE_SIZE);

#ifdef CHECK_FOR_DELAYED_WORK
        if (queue_work(work_queue, &brd->sampleWorker) &&
            !(brd->delayedWork++ % 1000))
                KLOG_INFO("%s: delayedWork=%zu\n",
                          brd->deviceName, brd->delayedWork);
#else
        queue_work(work_queue, &brd->sampleWorker);
#endif
}


/*
 * Function scheduled to be called from IRIG driver at 100Hz frequency.
 * Create a sample from the A2D FIFO, and pass it on to other
 * routines to break it up and filter the samples.
 */
static void ReadSampleCallback(void *ptr)
{
        struct A2DBoard *brd = (struct A2DBoard *) ptr;
        static int entrycount = 0;
        int preFlevel, postFlevel;

        entrycount++;

        /*
         * If board is not healty, we're out-a-here.
         */
        if (brd->errorState != 0)
                return;

        /*
         * Clear the card's FIFO if we were asked to discard a scan
         */
        if (brd->discardNextScan) {
                A2DClearFIFO(brd);
                brd->discardNextScan = 0;
                return;
        }

        /*
         * How full is the card's FIFO?
         */
        preFlevel = getA2DFIFOLevel(brd);
        brd->cur_status.preFifoLevel[preFlevel]++;

        /*
         * If FIFO is empty, just return
         */
        if (preFlevel == 0) {
                if (!(brd->cur_status.preFifoLevel[0] % 100))
                        KLOG_WARNING("empty FIFO %d\n",
                                    brd->cur_status.preFifoLevel[0]);
                return;
        }

        /*
         * If FIFO is full, there's a problem
         */
        if (preFlevel == 5) {
                KLOG_ERR("%d Restarting card with full FIFO @ %ld\n",
                         entrycount, GET_MSEC_CLOCK);
                brd->errorState = WAITING_FOR_RESET;
#ifdef CHECK_FOR_DELAYED_WORK
                if (queue_work(work_queue, &brd->resetWorker) &&
                    !(brd->delayedWork++ % 1000))
                        KLOG_INFO("%s: delayedWork=%zu\n",
                                  brd->deviceName, brd->delayedWork);
#else
                queue_work(work_queue, &brd->resetWorker);
#endif
                return;
        }

        /*
         * Read the fifo.
         */
        readA2DFifo(brd);

        brd->readCtr++;

        /*
         * Keep track of the card's FIFO level when we're done reading
         */
        postFlevel = getA2DFIFOLevel(brd);
        brd->cur_status.postFifoLevel[postFlevel]++;

        /*
         * If we have a bunch of consecutive reads that don't end with the
         * card's FIFO empty, then a delay probably made us miss some samples 
         * after we last cleared the FIFO, and hence we're time-tagging the data
         * incorrectly.  Clear the FIFO again and we should get all synced
         * up.  Note that we accept a few consecutive non-empty endings because
         * this function gets called back at an *average* rate of 100Hz, but
         * sometimes with gaps and then bursts of calls.
         */
        if (postFlevel > 0)
                brd->consecutiveNonEmpty++;
        else
                brd->consecutiveNonEmpty = 0;

        if (brd->consecutiveNonEmpty == 100) {
                KLOG_WARNING
                    ("Clearing card FIFO after %d consecutive non-empty ends\n",
                     brd->consecutiveNonEmpty);
                brd->consecutiveNonEmpty = 0;
                A2DClearFIFO(brd);
        }


        /*
         * Update stats every 10 seconds
         */
        if (!(brd->readCtr % (A2D_POLL_RATE * 10))) {
                brd->nbadScans = 0;

                /*
                 * copy current status to prev_status for access by ioctl
                 * A2D_GET_STATUS
                 */
                brd->cur_status.nbadFifoLevel = brd->nbadFifoLevel;
                brd->cur_status.fifoNotEmpty = brd->fifoNotEmpty;
                brd->cur_status.skippedSamples = brd->skippedSamples;
                brd->cur_status.resets = brd->resets;
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
                    GET_HEAD(brd->a2d_samples, A2D_SAMPLE_QUEUE_SIZE);
        if (!osamp) {            // no output sample available
                brd->skippedSamples++;
                KLOG_WARNING("%s: skippedSamples=%d\n",
                             brd->deviceName, brd->skippedSamples);
                return;
        }
        osamp->timetag = GET_MSEC_CLOCK;
        osamp->length = 2 * sizeof (short);
        osamp->data[0] = cpu_to_le16(NCAR_A2D_TEMPERATURE_INDEX);
        brd->currentTemp = A2DTemp(brd);
        osamp->data[1]   = cpu_to_le16(brd->currentTemp);
        INCREMENT_HEAD(brd->a2d_samples, A2D_SAMPLE_QUEUE_SIZE);
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

        // wake any waiting reads
        wake_up_interruptible(&brd->rwaitq_a2d);

        AD7725StatusAll(brd);   // Read status and clear IRQ's

        A2DNotAuto(brd);        // Shut off auto mode (if enabled)

        // Abort all the A/D's
        A2DStopReadAll(brd);

        freeFilters(brd);

        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
            brd->gain[i] = 0;

        brd->busy = 0;          // Reset the busy flag

        return ret;
}

/* 
 * Reset the A2D.
 */
static int resetBoard(struct A2DBoard *brd)
{
        int ret;
        brd->errorState = WAITING_FOR_RESET;
        brd->resets++;

        if (brd->a2dCallback)
                unregister_irig_callback(brd->a2dCallback);
        brd->a2dCallback = 0;

        if (brd->tempCallback)
                unregister_irig_callback(brd->tempCallback);
        brd->tempCallback = 0;

        // Sync with 1PPS
        KLOG_DEBUG("doing waitFor1PPS, GET_MSEC_CLOCK=%ld\n",
                   GET_MSEC_CLOCK);
        if ((ret = waitFor1PPS(brd)) != 0)
                return ret;

        KLOG_DEBUG("Found initial PPS, GET_MSEC_CLOCK=%ld\n",
                   GET_MSEC_CLOCK);

        A2DStopReadAll(brd);    // Send Abort command to all A/Ds
        AD7725StatusAll(brd);   // Read status from all A/Ds

        if ((ret = A2DStartAll(brd)) != 0) return ret;  // Start all the A/Ds
        AD7725StatusAll(brd);   // Read status again from all A/Ds

        A2DSetSYNC(brd);        // Stop A/D clocks
        A2DAuto(brd);           // Switch to automatic mode

        KLOG_DEBUG("Setting 1PPS Enable line\n");

        msleep(20);
        A2DEnable1PPS(brd);     // Enable sync with 1PPS

        KLOG_DEBUG("doing waitFor1PPS, GET_MSEC_CLOCK=%ld\n",
                   GET_MSEC_CLOCK);
        if ((ret = waitFor1PPS(brd)) != 0)
                return ret;

        KLOG_DEBUG("Found second PPS, GET_MSEC_CLOCK=%ld\n",
                   GET_MSEC_CLOCK);
        A2DClearFIFO(brd);      // Clear the board's FIFO...

        brd->discardNextScan = 1;       // whether to discard the initial scan
        brd->interrupted = 0;
        brd->nbadScans = 0;
        brd->readCtr = 0;
        brd->nbadFifoLevel = 0;
        brd->fifoNotEmpty = 0;
        brd->skippedSamples = 0;
        brd->fifo_samples.head = brd->fifo_samples.tail = 0;
        brd->consecutiveNonEmpty = 0;

        // start the IRIG callback routine at 100 Hz
        //
        brd->a2dCallback =
            register_irig_callback(ReadSampleCallback, IRIG_100_HZ, brd,&ret);
        if (!brd->a2dCallback) {
                KLOG_ERR("Error: register_irig_callback failed\n");
                return ret;
        }

        if (brd->tempRate != IRIG_NUM_RATES) {
            brd->tempCallback =
                register_irig_callback(TemperatureCallback, brd->tempRate,
                                       brd,&ret);
            if (!brd->tempCallback) {
                    KLOG_ERR("Error: register_irig_callback failed\n");
                    return ret;
            }
        }

        brd->busy = 1;          // Set the busy flag
        KLOG_DEBUG("IRIG callbacks registered @ %ld\n", GET_MSEC_CLOCK);

        KLOG_INFO("reset succeeded for board %d\n", BOARD_INDEX(brd));
        brd->errorState = ret;
        return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void resetBoardWorkFunc(struct work_struct *work)
#else
static void resetBoardWorkFunc(void *work)
#endif
{
        struct A2DBoard *brd =
            container_of(work, struct A2DBoard, resetWorker);
        int ret = resetBoard(brd);
        if (ret && brd->resets > NUM_RESET_ATTEMPTS)
                brd->errorState = ret;
}

/**
 * Start data collection on the selected board.  This is only called
 * via user space initiation (i.e., through the NCAR_A2D_RUN ioctl).
 */
static int startBoard(struct A2DBoard *brd)
{
        int ret;
        int nFifoValues;
        int haveMaster = 0;
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
                if ((ret = A2DSetMaster(brd, i)) < 0) return ret;
                haveMaster = 1;
        }
        if (!haveMaster) return -EIO;

        // Configure DAC gain codes
        if ((ret = A2DSetGainAndOffset(brd)) != 0)
                return ret;

        // Make sure SYNC is cleared so clocks are running
        KLOG_DEBUG("Clearing SYNC\n");
        A2DClearSYNC(brd);

        // Start then reset the A/D's
        // Start conversions
        KLOG_DEBUG("Starting A/D's\n");
        A2DStartAll(brd);

        // If starting from a cold boot, one needs to
        // let the A2Ds run for a bit before downloading
        // the filter data.
        msleep(20);

        // Then do a soft reset
        KLOG_DEBUG("Soft resetting A/D's\n");
        A2DStopReadAll(brd);

        // Configure the A/D's
        KLOG_DEBUG("Sending filter config data to A/Ds\n");
        if ((ret = A2DConfigAll(brd)) != 0)
                return ret;

        // Reset the A/D's
        KLOG_DEBUG("Resetting A/Ds\n");
        A2DStopReadAll(brd);

        msleep(1);              // Give A/D's a chance to load
        KLOG_DEBUG("A/Ds ready for synchronous start\n");

        /*
         * Zero out status
         */
        memset(&brd->cur_status, 0, sizeof (struct ncar_a2d_status));
        memset(&brd->prev_status, 0, sizeof (struct ncar_a2d_status));

        brd->a2d_samples.head = brd->a2d_samples.tail = 0;
        memset(&brd->a2d_read_state, 0, sizeof (struct sample_read_state));
        brd->lastWakeup = jiffies;

        /*
         * How many data values do we read per poll?
         */
        nFifoValues =
            brd->scanRate / A2D_POLL_RATE * NUM_NCAR_A2D_CHANNELS;
        brd->skipFactor = 1;

#ifdef DO_A2D_STATRD
        nFifoValues *= 2;       // twice as many reads if status is being sent
        brd->skipFactor = 2;
#endif

        KLOG_DEBUG("starting board %d, nFifoValues=%d\n",
                   BOARD_INDEX(brd), brd->nFifoValues);

        /*
         * If nFifoValues increases, re-allocate samples
         */
        if (nFifoValues > brd->nFifoValues) {
                ret = realloc_dsm_circ_buf(&brd->fifo_samples,
                                           nFifoValues * sizeof (short),
                                           FIFO_SAMPLE_QUEUE_SIZE);
                if (ret)
                        return ret;

                kfree(brd->discardBuffer);
                brd->discardBuffer = (short *)
                    kmalloc(nFifoValues * sizeof (short), GFP_KERNEL);
                if (!brd->discardBuffer)
                        return -ENOMEM;
        }
        brd->nFifoValues = nFifoValues;
        brd->fifo_samples.head = brd->fifo_samples.tail = 0;

        /*
         * Scan deltaT, time in milliseconds between A2D scans of
         * the channels.
         */
        brd->scanDeltatMsec =   // compute in microseconds first to avoid trunc
            (USECS_PER_SEC / brd->scanRate) / USECS_PER_MSEC;

        KLOG_DEBUG("nFifoValues=%d,scanDeltatMsec=%d\n",
                   brd->nFifoValues, brd->scanDeltatMsec);

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

        /* before changing head & tails here one should
         * make sure the sample producer and consumer threads
         * are not running.
         */
        brd->fifo_samples.head = brd->fifo_samples.tail = 0;
        brd->a2d_samples.head = brd->a2d_samples.tail = 0;
        memset(&brd->a2d_read_state, 0, sizeof (struct sample_read_state));
        brd->resets = 0;
        brd->errorState = 0;

        brd->i2c = I2CSCL | I2CSDA;
        brd->tempRate = IRIG_NUM_RATES;

        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
                brd->gain[i] = 0;

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

        KLOG_INFO("Releasing board %d\n", BOARD_INDEX(brd));

        /*
         * Turn off the callback routine
         */
        stopBoard(brd);

        freeFilters(brd);

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

        if (brd->errorState && brd->errorState != WAITING_FOR_RESET)
                mask |= POLLERR;

        if (sample_remains(&brd->a2d_read_state) ||
            brd->a2d_samples.head != brd->a2d_samples.tail)
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
        if (brd->errorState && brd->errorState != WAITING_FOR_RESET)
                return brd->errorState;

        return nidas_circbuf_read(filp, buf, count,
                                  &brd->a2d_samples, &brd->a2d_read_state,
                                  &brd->rwaitq_a2d);
}

/*
 * Function that is called on receipt of an ioctl request.
 * Return: negative Linux errno, or 0=OK
 */
static int
ncar_a2d_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
               unsigned long arg)
{
        struct A2DBoard *brd = (struct A2DBoard *) filp->private_data;
        void __user *userptr = (void __user *) arg;
        int len = _IOC_SIZE(cmd);
        int ret = -EINVAL;
        int rate;

        // KLOG_DEBUG("IOCTL for board %d: cmd=%x, len=%d\n",
          //          BOARD_INDEX(brd), cmd, len);

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
                            ("A/D card %d is running. Can't configure.\n",
                             BOARD_INDEX(brd));
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
                            ("A/D card %d is running. Can't configure.\n",
                             BOARD_INDEX(brd));
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
                            ("setup for board %d failed with state %d\n",
                             BOARD_INDEX(brd), ret);
                else
                        KLOG_DEBUG("setup for board %d succeeded\n",
                                   BOARD_INDEX(brd));
                break;
        case NCAR_A2D_SET_OCFILTER:
                if (len != sizeof (struct ncar_a2d_ocfilter_config)) {
                        KLOG_ERR
                            ("NCAR_A2D_SET_OCFILTER len %d != sizeof(struct ncar_a2d_ocfilter_config)\n",
                             len);
                        break;  // invalid length
                }

                if (brd->busy) {
                        KLOG_WARNING
                            ("A/D card %d is running. Can't configure.\n",
                             BOARD_INDEX(brd));
                        ret = -EBUSY;
                        break;
                }
                {
                        struct ncar_a2d_ocfilter_config cfg;
                        if (copy_from_user(&cfg, userptr, len) != 0) {
                                ret = -EFAULT;
                                break;
                        }
                        memcpy(brd->ocfilter,cfg.filter,sizeof(cfg.filter));
                        ret = 0;
                }
                break;
        case NCAR_A2D_SET_CAL:
                if (len != sizeof (struct ncar_a2d_cal_config))
                        break;  // invalid length
                if (copy_from_user(&brd->cal,userptr, len) != 0) {
                        ret = -EFAULT;
                        break;
                }
                ret = 0; break; //DISABLED!  Spowart stole the A2DIO_D2A2 line for A2DTemp!
                A2DSetVcal(brd);
                A2DSetCal(brd);
                ret = 0;
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
                            ("A2DTEMP_SET_RATE, bad len\n");
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
                            ("Illegal rate for A/D temp probe (> 10 Hz)\n");
                        break;
                }
                brd->tempRate = rate;
                ret = 0;
                break;
        default:
                KLOG_ERR("Bad A2D ioctl 0x%x\n", cmd);
                break;
        }
        return ret;
}

/*-----------------------Module------------------------------*/
// Stops the A/D and releases reserved memory
void cleanup_module(void)
{
        int ib;
        int i;


        if (MAJOR(A2DDevStart)) {
                cdev_del(&A2DCdev);
                unregister_chrdev_region(A2DDevStart, NumBoards);
        }

        if (BoardInfo) {
                for (ib = 0; ib < NumBoards; ib++) {
                        struct A2DBoard *brd = BoardInfo + ib;

                        free_dsm_circ_buf(&brd->a2d_samples);
                        free_dsm_circ_buf(&brd->fifo_samples);
                        kfree(brd->discardBuffer);
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
                        if (brd->busy) {
                                stopBoard(brd);
                                AD7725StatusAll(brd);   // Read status and clear IRQ's
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

int init_module()
{
        int error = -EINVAL;
        int ib, i;

#ifdef DETECT_SPIKE
        KLOG_DEBUG("** SPIKE detection enabled.  Sensitivity set to: %d **\n", DETECT_SPIKE); 
#endif
        BoardInfo = 0;

        work_queue = create_singlethread_workqueue("ncar_a2d");

        /* count non-zero ioport addresses, gives us the number of boards */
        for (ib = 0; ib < MAX_A2D_BOARDS; ib++) {
                if (IoPort[ib] == 0)
                        break;

#if defined(CONFIG_MACH_ARCOM_MERCURY) || defined(CONFIG_MACH_ARCOM_VULCAN)
                /* 
                 * Try to warn about 8-bit-only ioport values on the Vulcan.
                 * Since we can't query the settings for I/O window 1 for the
                 * PCI1520 CardBus bridge (which connects us to the PC/104 bus),
                 * this warning is only a guess.
                 */
                if (IoPort[ib] < 0x800) {
                        KLOG_WARNING
                            ("IoPort 0x%x for board %d is less than 0x800, "
                             "and this is probably an 8-bit-only port;\n",
                             IoPort[ib], ib);
                        KLOG_WARNING
                            ("(the port must lie in I/O window 1, as defined in "
                             "<linux>/arch/arm/mach-ixp4xx/mercury-pc104.c)\n");
                }
#endif
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

        /* initialize each A2DBoard structure */
        for (ib = 0; ib < NumBoards; ib++) {
                struct A2DBoard *brd = BoardInfo + ib;
                unsigned long addr;

                // initialize structure to zero, then initialize things
                // that are non-zero
                memset(brd, 0, sizeof (struct A2DBoard));

                // for informational messages only at this point
                sprintf(brd->deviceName, "/dev/%s%d", DEVNAME_A2D, ib);

                /*
                 * Base address and command address
                 */
                addr = IoPort[ib] + SYSTEM_ISA_IOPORT_BASE;

                // Request the necessary I/O region
                if (!request_region(addr, A2DIOWIDTH, "NCAR A/D")) {
                        KLOG_ERR("ioport at 0x%lx already in use\n", addr);
                        goto err;
                }
                brd->base_addr = addr;
                brd->cmd_addr = addr + A2DCMDADDR;

                /*
                 * See if we get an expected response at this port.  
                 * Method:
                 *   o start channel 0
                 *   o wait 20 ms for channel "warmup" (seems to be necessary...)
                 *   o stop channel 0
                 *   o send AD7725_WRCONFIG to channel 0
                 *   o get channel 0 status
                 *   o verify that channel 0 saw the AD7725_WRCONFIG command
                 */
                outb(A2DIO_WRCMD, brd->cmd_addr);
                outw(AD7725_READDATA, brd->base_addr);  // start channel 0
                msleep(20);     // wait a bit...
                outb(A2DIO_WRCMD, brd->cmd_addr);
                outw(AD7725_ABORT, brd->base_addr);     // stop channel 0
                outb(A2DIO_WRCMD, brd->cmd_addr);
                outw(AD7725_WRCONFIG, brd->base_addr);  // send WRCONFIG to channel 0
                // Make sure channel 0 status confirms receipt of AD7725_WRCONFIG cmd
                if (!A2DConfirmInstruction(brd, 0, AD7725_WRCONFIG)) {
                        KLOG_ERR("Bad response on IoPort 0x%04x.  "
                                 "Is there really an NCAR A/D card there?\n",
                                 IoPort[ib]);
                        error = -ENODEV;
                        goto err;
                } else
                        KLOG_INFO("NCAR A/D board confirmed at 0x%03x\n",
                                    brd->base_addr);
                /*
                 * Do we tell the board to interleave status with data?
                 */

#ifdef DO_A2D_STATRD
                brd->FIFOCtl = A2DSTATEBL;
#else
                brd->FIFOCtl = 0;
#endif

                brd->tempRate = IRIG_NUM_RATES;

                brd->invertCounts = Invert[ib];

                /*
                 * Initialize the read wait queue
                 */
                init_waitqueue_head(&brd->rwaitq_a2d);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
                INIT_WORK(&brd->sampleWorker, a2d_bottom_half);
                INIT_WORK(&brd->resetWorker, resetBoardWorkFunc);
#else
                INIT_WORK(&brd->sampleWorker, a2d_bottom_half,
                          &brd->sampleWorker);
                INIT_WORK(&brd->resetWorker, resetBoardWorkFunc,
                          &brd->resetWorker);
#endif

                /*
                 * Other initialization
                 */
                /*
                 * We don't know how many values we will read from the FIFO yet,
                 * so the fifo sample circular buffer is allocated then
                 * when nFifoValues is known.
                 */
                brd->nFifoValues = 0;
                brd->fifo_samples.buf = 0;
                brd->fifo_samples.head = brd->fifo_samples.tail = 0;

                error = alloc_dsm_circ_buf(&brd->a2d_samples,
                                           (NUM_NCAR_A2D_CHANNELS + 1) *
                                           sizeof (short),
                                           A2D_SAMPLE_QUEUE_SIZE);
                if (error)
                        return error;

                brd->discardBuffer = 0;
        }

        /*
         * Initialize and add the user-visible devices for the A/D functions
         */
        if ((error = alloc_chrdev_region(&A2DDevStart, 0, NumBoards,
                                         DEVNAME_A2D)) < 0) {
                KLOG_ERR
                    ("Error %d allocating device major number for '%s'\n",
                     -error, DEVNAME_A2D);
                goto err;
        } else
                KLOG_DEBUG("Got major device number %d for '%s'\n",
                            MAJOR(A2DDevStart), DEVNAME_A2D);

        cdev_init(&A2DCdev, &ncar_a2d_fops);
        if ((error = cdev_add(&A2DCdev, A2DDevStart, NumBoards)) < 0) {
                KLOG_ERR("cdev_add() for NCAR A/D failed!\n");
                goto err;
        }

        KLOG_DEBUG("A2D init_module complete.\n");

        return 0;

      err:

        unregister_chrdev_region(A2DDevStart, NumBoards);
        cdev_del(&A2DCdev);

        if (BoardInfo) {
                for (ib = 0; ib < NumBoards; ib++) {
                        struct A2DBoard *brd = BoardInfo + ib;

                        free_dsm_circ_buf(&brd->fifo_samples);
                        free_dsm_circ_buf(&brd->a2d_samples);
                        kfree(brd->discardBuffer);
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
                kfree(BoardInfo);
                BoardInfo = 0;
        }

        if (work_queue)
                destroy_workqueue(work_queue);

        return error;
}
