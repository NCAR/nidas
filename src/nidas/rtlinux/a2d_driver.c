/*
    a2d_driver.c

  Driver for NCAR/EOL/RAF A/D card.

  $LastChangedRevision$
      $LastChangedDate$
        $LastChangedBy$
              $HeadURL$

  Copyright 2005 UCAR, NCAR, All Rights Reserved

  Original author: Grant Gray
  Revisions:
*/

// Clock/data line bits for i2c interface
#define I2CSCL 0x2
#define I2CSDA 0x1

#define DO_A2D_STATRD
#define CHECK_A2D_STATRD
//#define TEMPDEBUG
//#define DO_FTRUNCATE  // Truncate the size of the data FIFO

/* RTLinux module includes...  */

#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_stdio.h>
#include <rtl_posixio.h>
#include <rtl_pthread.h>
#include <rtl_unistd.h>
#include <linux/ioport.h>

#include <nidas/rtlinux/dsmlog.h>
#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/rtlinux/irigclock.h>
#include <nidas/rtlinux/a2d_driver.h>
#include <nidas/rtlinux/dsm_version.h>
#include <nidas/linux/klog.h>

/* ioport addresses of installed boards, 0=no board installed */
static int ioport[MAX_A2D_BOARDS] = { 0x3A0, 0, 0, 0 };

/* Which A2D chip is the master.*/
static int master[MAX_A2D_BOARDS] = { 7, 7, 7, 7 };


/*
 * Whether to invert counts. This should be 1(true) for new cards.
 * Early versions of the A2D cards do not need it.
 * This is settable as a module parameter.  We could do
 * it by checking the serial number in firmware, but
 * don't have faith that these serial numbers will be
 * set correctly in the firmware on the cards.
 */
static int invert[MAX_A2D_BOARDS] = { 1, 1, 1, 1 };

/* number of A2D boards in system (number of non-zero ioport values) */
static int numboards = 0;

MODULE_AUTHOR("Grant Gray <gray@ucar.edu>");
MODULE_DESCRIPTION("HIAPER A/D driver for RTLinux");
RTLINUX_MODULE(DSMA2D);

MODULE_PARM(ioport, "1-" __MODULE_STRING(MAX_A2D_BOARDS) "i");
MODULE_PARM_DESC(ioport, "ISA port address of each board, e.g.: 0x3A0");

MODULE_PARM(invert, "1-" __MODULE_STRING(MAX_A2D_BOARDS) "i");
MODULE_PARM_DESC(invert, "Whether to invert counts, default=1(true)");

MODULE_PARM(master, "1-" __MODULE_STRING(MAX_A2D_BOARDS) "i");
MODULE_PARM_DESC(master, "Sets master A/D for each board, default=7");

static struct A2DBoard *boardInfo = 0;

static const char *devprefix = "dsma2d";

/* number of devices on a board. This is the number of
 * /dev/dsma2d* devices, from the user's point of view, that one
 * board represents. 
 */
#define NDEVICES 1

/*
 * Stack for 1PPS and reset threads
 */
#define THREAD_STACK_SIZE 1024


int init_module(void);
void cleanup_module(void);

/****************  IOCTL Section *************************/

static struct ioctlCmd ioctlcmds[] = {
        {GET_NUM_PORTS, _IOC_SIZE(GET_NUM_PORTS)},
        {A2D_GET_STATUS, _IOC_SIZE(A2D_GET_STATUS)},
        {A2D_SET_CONFIG, sizeof (A2D_SET)},
        {A2D_SET_CAL, sizeof (A2D_CAL)},
        {A2D_RUN, _IOC_SIZE(A2D_RUN)},
        {A2D_STOP, _IOC_SIZE(A2D_STOP)},
        {A2DTEMP_GET_TEMP, _IOC_SIZE(A2DTEMP_GET_TEMP)},
        {A2DTEMP_SET_RATE, _IOC_SIZE(A2DTEMP_SET_RATE)},
        {NIDAS_A2D_ADD_FILTER,sizeof(struct nidas_a2d_filter_config)}
};

static int nioctlcmds = sizeof (ioctlcmds) / sizeof (struct ioctlCmd);

/****************  End of IOCTL Section ******************/

static struct rtl_timespec usec1 = { 0, 1000 };
static struct rtl_timespec usec10 = { 0, 10000 };
static struct rtl_timespec usec20 = { 0, 20000 };
static struct rtl_timespec usec100 = { 0, 100000 };
static struct rtl_timespec msec20 = { 0, 20000000 };

static int startA2DResetThread(struct A2DBoard *brd);

/*-----------------------Utility------------------------------*/
// I2C serial bus control utilities

static inline void i2c_clock_hi(struct A2DBoard *brd)
{
        brd->i2c |= I2CSCL;     // Set clock bit hi
        outb(A2DIO_STAT, brd->chan_addr);       // Clock high
        outb(brd->i2c, brd->addr);
        rtl_nanosleep(&usec1, 0);
        return;
}

static inline void i2c_clock_lo(struct A2DBoard *brd)
{
        brd->i2c &= ~I2CSCL;    // Set clock bit low
        outb(A2DIO_STAT, brd->chan_addr);       // Clock low
        outb(brd->i2c, brd->addr);
        rtl_nanosleep(&usec1, 0);
        return;
}

static inline void i2c_data_hi(struct A2DBoard *brd)
{
        brd->i2c |= I2CSDA;     // Set data bit hi
        outb(A2DIO_STAT, brd->chan_addr);       // Data high
        outb(brd->i2c, brd->addr);
        rtl_nanosleep(&usec1, 0);
        return;
}

static inline void i2c_data_lo(struct A2DBoard *brd)
{
        brd->i2c &= ~I2CSDA;    // Set data bit lo
        outb(A2DIO_STAT, brd->chan_addr);       // Data high
        outb(brd->i2c, brd->addr);
        rtl_nanosleep(&usec1, 0);
        return;
}

/*-----------------------Utility------------------------------*/
// Read on-board LM92 temperature sensor via i2c serial bus
// The signed short returned is weighted .0625 deg C per bit

static short A2DTemp(struct A2DBoard *brd)
{
        // This takes 68 i2c operations to perform.
        // Using a delay of 10 usecs, this should take
        // approximately 680 usecs.

        unsigned char b1;
        unsigned char b2;
        unsigned char t1;
        short x;
        unsigned char i, address = 0x48;        // Address of temperature register


        // shift the address over one, and set the READ indicator
        b1 = (address << 1) | 1;

        // a start state is indicated by data going from hi to lo,
        // when clock is high.
        i2c_data_hi(brd);
        i2c_clock_hi(brd);
        i2c_data_lo(brd);
        i2c_clock_lo(brd);
        // i2c_data_hi(brd);  // wasn't in Charlie's code

        // Shift out the address/read byte
        for (i = 0; i < 8; i++) {
                // set data line
                if (b1 & 0x80)
                        i2c_data_hi(brd);
                else
                        i2c_data_lo(brd);

                b1 = b1 << 1;
                // raise clock
                i2c_clock_hi(brd);
                // lower clock
                i2c_clock_lo(brd);
        }

        // clock the slave's acknowledge bit
        i2c_clock_hi(brd);
        i2c_clock_lo(brd);

        // shift in the first data byte
        b1 = 0;
        for (i = 0; i < 8; i++) {
                // raise clock
                i2c_clock_hi(brd);
                // get data
                t1 = 0x1 & inb(brd->addr);
                b1 = (b1 << 1) | t1;
                // lower clock
                i2c_clock_lo(brd);
        }

        // Send the acknowledge bit
        i2c_data_lo(brd);
        i2c_clock_hi(brd);
        i2c_clock_lo(brd);
        // i2c_data_hi(brd);  // wasn't in Charlie's code

        // shift in the second data byte
        b2 = 0;
        for (i = 0; i < 8; i++) {
                i2c_clock_hi(brd);
                t1 = 0x1 & inb(brd->addr);
                b2 = (b2 << 1) | t1;
                i2c_clock_lo(brd);
        }

        // a stop state is signalled by data going from
        // lo to hi, when clock is high.
        i2c_data_lo(brd);
        i2c_clock_hi(brd);
        i2c_data_hi(brd);

        x = (short) (b1 << 8 | b2) >> 3;

#ifdef TEMPDEBUG
        DSMLOG_DEBUG
            ("b1=0x%02X, b2=0x%02X, b1b2>>3 0x%04X, degC = %d.%1d\n", b1,
             b2, x, x / 16, (10 * (x % 16)) / 16);
#endif

        return x;
}

/*-----------------------End I2C Utils -----------------------*/


/*-----------------------Utility------------------------------*/
// Read status of A2D chip specified by A2DSel 0-7

static unsigned short A2DStatus(struct A2DBoard *brd, int A2DSel)
{
        // Point at the A/D status channel
        outb(A2DSTATRD, brd->chan_addr);
        return (inw(brd->addr + A2DSel * 2));
}

static void A2DStatusAll(struct A2DBoard *brd)
{
        int i;
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
                brd->cur_status.goodval[i] = A2DStatus(brd, i);

        return;
}

/*-----------------------Utility------------------------------*/
// A2DSetGain sets an A/D Channel gain selected by A2DSel.
// TODO: update old DAC calculation in comment below...
// Allowable gain values are 1 <= A2DGain <= 25.5
//   in 255 gain steps.
// The gain is calculated from gaincode (0-255) as:
//   gain = 5*5.12*gaincode/256 = .1*gaincode.
// The gain can go down to .7 before the system has saturation problems
// Check this 

static int A2DSetGain(struct A2DBoard *brd, int A2DSel)
{
        // If no A/D selected return error -1
        if (A2DSel < 0 || A2DSel >= NUM_NCAR_A2D_CHANNELS)
                return -EINVAL;

        A2D_SET *a2d = &brd->config;
        int A2DGain = a2d->gain[A2DSel];
        int A2DGainMul = a2d->gainMul[A2DSel];
        int A2DGainDiv = a2d->gainDiv[A2DSel];
        unsigned short GainCode = 0;

        // unused channel gains are set to zero in the configuration
        if (A2DGain != 0)
                GainCode = A2DGainMul * A2DGain / A2DGainDiv;

//   DSMLOG_DEBUG("A2DSel = %d   A2DGain = %d   "
//                "A2DGainMul = %d   A2DGainDiv = %d   GainCode = %d\n",
//                A2DSel, A2DGain, A2DGainMul, A2DGainDiv, GainCode);

        // The new 12-bit DAC has lower input resistance (7K ohms as opposed
        // to 10K ohms for the 8-bit DAC). The gain is the ratio of the amplifier
        // feedback resistor to the input resistance. Therefore, for the same
        // gain codes, the new board will yield higher gains by a factor of
        // approximately 1.43. I believe you will want to divide the old gain
        // codes (0-255) by 1.43. The new gain code will be between
        // 0 - 4095. So, after you divide the old gain code by 1.43 you
        // will want to multiply by 16. This is the same as multiplying
        // by 16/1.43 = 11.2.
// GainCode = (GainCode << 4) & 0xfff0;
        //                               input 5Hz sine and/or saw
        //           GC                  input:   at A2D:         raw counts:    on AEROS:      

        GainCode = 0x0800 + A2DSel;     //  +/- 1v   1.84 to 2.14v      -83  1359   -0.02 to 0.42v
        GainCode = 0x1000 + A2DSel;     //  +/- 1v   1.75 to 2.24v    -1595  2893   -0.48 to 0.88v
        GainCode = 0x1950 + A2DSel;     //  +/- 1v   1.84 to 2.24v    -1585  2887   -0.48 to 0.88v
        GainCode = 0x2000 + A2DSel;     //  +/- 1v   1.84 to 2.14v      -81  1360   -0.02 to 0.42v
        GainCode = 0x1950 + A2DSel;     //  +/- 1v   1.74 to 2.24v    -1637  2916   -0.50 to 0.90v
        GainCode = 0x0800 + A2DSel;     //  +/- 1v   1.74 to 2.24v    -1637  2919   -0.50 to 0.90v
        GainCode = 0x2000 + A2DSel;     //  +/- 1v   1.74 to 2.26v    -2241  3510   -0.68 to 1.08v
        GainCode = 0x1000 + A2DSel;     //  +/- 1v   1.64 to 2.30v    -2241  3513   -0.68 to 1.08v at taken w/ square wave
        GainCode = 0x1950 + A2DSel;     //  +/- 1v   1.74 to 2.28v    -1636  2914   -0.50 to 0.90v at taken w/ square wave
        // power cyle
        GainCode = 0x1950 + A2DSel;     //  +/- 1v   0.44 to 3.48v   -21892 23643   -6.75 to 7.25v at taken w/ square wave
        // reboot
        GainCode = 0x4100 + A2DSel;     //  +/- 5v  30129        -27380
        GainCode = 0x4200 + A2DSel;     //  +/- 5v  30147        -27390
        GainCode = 0x4400 + A2DSel;     //  +/- 5v  31505        -28434
        GainCode = 0x4800 + A2DSel;     //  +/- 5v  31536        -28451
        GainCode = 0x4840 + A2DSel;     //  +/- 5v  32729        -32768
        GainCode = 0x4800 + A2DSel;     //  +/- 5v  32765        -32768
        // reboot
        GainCode = 0x4400 + A2DSel;     //  +/- 5v  32765        -32768
        // reboot
        GainCode = 0x2410 + A2DSel;     //  +/-10v  31516        -28439

// DSMLOG_DEBUG("GainCode: 0x%04x\n", GainCode);

        // if(A2DSel != 0) GainCode = 0x4790 + A2DSel;
        GainCode = 0x1900 + A2DSel;     //  +/-10v  31516        -28439

        if (a2d->offset[A2DSel]) {
                if (a2d->gain[A2DSel] == 10)
                        GainCode = 0x1100 + A2DSel;     //   0 to +20  ???
                else if (a2d->gain[A2DSel] == 20)
                        GainCode = 0x4400 + A2DSel;     //   0 to +10
                else if (a2d->gain[A2DSel] == 40)
                        GainCode = 0x8800 + A2DSel;     //   0 to +5
                else
                        GainCode = 0x0000 + A2DSel;
        } else {
                if (a2d->gain[A2DSel] == 10)
                        GainCode = 0x2200 + A2DSel;     // -10 to +10  // was 0x1900
                else if (a2d->gain[A2DSel] == 20)
                        GainCode = 0x4400 + A2DSel;     //  -5 to  +5
                else if (a2d->gain[A2DSel] == 40)
                        GainCode = 0x8800 + A2DSel;     //  -2.5 to  +2.5
                else
                        GainCode = 0x0000 + A2DSel;
        }
/*
              0x241  0x479
02:15:57.042    639 -29518
02:15:58.042    639 -29518
chan 0 railed!
chan 0 railed!
chan 0 railed!
chan 0 railed!
02:15:59.042  21614  11551
02:16:00.042  32696  32494
02:16:01.042  32697  32496
02:16:02.042  32698  32498
02:16:03.042  32698  32498
02:16:04.042  32698  32498
02:16:05.042  32699  32499
02:16:06.042  32699  32500
02:16:07.042  32699  32501
02:16:08.042  32700  32501
02:16:09.042  32700  32501
02:16:10.042  32700  32501
02:16:11.042  32700  32501
02:16:12.042  32700  32502
02:16:13.042  32700  32502
02:16:14.042  32700  32502
02:16:15.042  32700  32502
02:16:16.042  32701  32503
02:16:17.042  32701  32503
02:16:18.042  32701  32503
02:16:19.042  32701  32503
02:16:20.042  32701  32504
02:16:21.042  32701  32504
*/
        // 1.  Write (or set) D2A0. This is accomplished by writing to the A/D with the lower
        // four address bits (SA0-SA3) set to all "ones" and the data bus to 0x03.
//   DSMLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_D2A0, brd->chan_addr);
        outb(A2DIO_D2A0, brd->chan_addr);
        rtl_usleep(10000);
        // 2. Then write to the A/D card with lower address bits set to "zeros" and data
        // bus set to the gain value for the specific channel with the upper data three bits
        // equal to the channel address. The lower 12 bits are the gain code and data bit 12
        // is equal zero. So for channel 0 write: (xxxxxxxxxxxx0000) where the x's are the
        // gain code.
        DSMLOG_DEBUG
            ("chn: %d   offset: %d   gain: %2d   outb( 0x%x, 0x%x)\n",
             A2DSel, a2d->offset[A2DSel], a2d->gain[A2DSel], GainCode,
             brd->addr);
// DSMLOG_DEBUG("outb( 0x%x, 0x%x);\n", GainCode, brd->addr);
        outw(GainCode, brd->addr);
        rtl_usleep(10000);
        return 0;
}

/*-----------------------Utility------------------------------*/
// A2DSetMaster routes the interrupt signal from the target A/D chip
// to the ISA bus interrupt line.

static int A2DSetMaster(struct A2DBoard *brd, int A2DSel)
{
        if (A2DSel < 0 || A2DSel >= NUM_NCAR_A2D_CHANNELS) {
                DSMLOG_ERR("A2DSetMaster, bad chip number: %d\n", A2DSel);
                return -EINVAL;
        }

        DSMLOG_DEBUG("A2DSetMaster, master=%d\n", A2DSel);

        // Point at the FIFO status channel
        outb(A2DIO_FIFOSTAT, brd->chan_addr);

        // Write the master to register
        outb((char) A2DSel, brd->addr);
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
        for (i = 0; i < 5; i++)
                if (brd->cal.vcalx8 == valid[i])
                        ret = 0;
        if (ret)
                return ret;

        // Point to the calibration DAC channel
        outb(A2DIO_D2A2, brd->chan_addr);
        DSMLOG_DEBUG("outb( 0x%02x, 0x%x);\n", A2DIO_D2A2, brd->chan_addr);

        // Write cal voltage code
        outw(brd->cal.vcalx8, brd->addr);
        DSMLOG_DEBUG("outw( 0x%04x, 0x%x);\n", brd->cal.vcalx8, brd->addr);
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
                if (brd->config.offset[i] != 0)
                        OffChans += 0x80;
                if (brd->cal.calset[i] != 0)
                        CalChans += 0x80;
        }
        // Point at the system control input channel
        outb(A2DIO_SYSCTL, brd->chan_addr);
        DSMLOG_DEBUG("outb( 0x%02x, 0x%x);\n", A2DIO_SYSCTL,
                     brd->chan_addr);

        // Set the appropriate bits in OffCal
        brd->OffCal = (OffChans << 8) & 0xFF00;
        brd->OffCal |= CalChans;
        brd->OffCal = ~(brd->OffCal) & 0xFFFF;  // invert bits

        // Send OffCal word to system control word
        outw(brd->OffCal, brd->addr);
        DSMLOG_DEBUG("outw( 0x%04x, 0x%x);\n", brd->OffCal, brd->addr);
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
                if (brd->config.offset[i] != 0)
                        OffChans += 0x80;
        }
        // Point at the system control input channel
        outb(A2DIO_SYSCTL, brd->chan_addr);
        DSMLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_SYSCTL, brd->chan_addr);

        // Set the appropriate bits in OffCal
        brd->OffCal = (OffChans << 8) & 0xFF00;
        brd->OffCal = ~(brd->OffCal) & 0xFFFF;  // invert bits

        // Send OffCal word to system control word
        outw(brd->OffCal, brd->addr);
        DSMLOG_DEBUG("JDW brd->OffCal:  0x%04x\n", brd->OffCal);
}

/*-----------------------Utility------------------------------*/
// Set A2D SYNC flip/flop.  This stops the A/D's until cleared
// under program control or by a positive 1PPS transition
// 1PPS enable must be asserted in order to sync on 1PPS

static void A2DSetSYNC(struct A2DBoard *brd)
{
        outb(A2DIO_FIFO, brd->chan_addr);

        brd->FIFOCtl |= A2DSYNC;        // Ensure that SYNC bit in FIFOCtl is set.

        // Cycle the sync clock while keeping SYNC bit high
        outb(brd->FIFOCtl, brd->addr);
        outb(brd->FIFOCtl | A2DSYNCCK, brd->addr);
        outb(brd->FIFOCtl, brd->addr);
        return;
}

/*-----------------------Utility------------------------------*/
// Clear the SYNC flag

static void A2DClearSYNC(struct A2DBoard *brd)
{
        outb(A2DIO_FIFO, brd->chan_addr);

        brd->FIFOCtl &= ~A2DSYNC;       // Ensure that SYNC bit in FIFOCtl is cleared.

        // Cycle the sync clock while keeping sync lowthe SYNC data line
        outb(brd->FIFOCtl, brd->addr);
        outb(brd->FIFOCtl | A2DSYNCCK, brd->addr);
        outb(brd->FIFOCtl, brd->addr);
        return;
}

/*-----------------------Utility------------------------------*/
// Enable 1PPS sync

static void A2D1PPSEnable(struct A2DBoard *brd)
{
        // Point at the FIFO control byte
        outb(A2DIO_FIFO, brd->chan_addr);

        // Set the 1PPS enable bit
        outb(brd->FIFOCtl | A2D1PPSEBL, brd->addr);

        return;
}

/*-----------------------Utility------------------------------*/
// Disable 1PPS sync

static void A2D1PPSDisable(struct A2DBoard *brd)
{
        // Point to FIFO control byte
        outb(A2DIO_FIFO, brd->chan_addr);

        // Clear the 1PPS enable bit
        outb(brd->FIFOCtl & ~A2D1PPSEBL, brd->addr);

        return;
}

/*-----------------------Utility------------------------------*/
// Clear (reset) the data FIFO

static void A2DClearFIFO(struct A2DBoard *brd)
{
        // Point to FIFO control byte
        outb(A2DIO_FIFO, brd->chan_addr);

        brd->FIFOCtl &= ~FIFOCLR;       // Ensure that FIFOCLR bit is not set in FIFOCtl

        outb(brd->FIFOCtl, brd->addr);
        outb(brd->FIFOCtl | FIFOCLR, brd->addr);        // Cycle FCW bit 0 to clear FIFO
        outb(brd->FIFOCtl, brd->addr);

        return;
}

/*-----------------------Utility------------------------------*/
// A2DFIFOEmpty checks the FIFO empty status bit and returns
// 1 if empty, 0 if not empty

static inline int A2DFIFOEmpty(struct A2DBoard *brd)
{
        // Point at the FIFO status channel
        outb(A2DIO_FIFOSTAT, brd->chan_addr);
        unsigned short stat = inw(brd->addr);

        return (stat & FIFONOTEMPTY) == 0;
}

// Read FIFO till empty, discarding data
// return number of bad status values found
static int A2DEmptyFIFO(struct A2DBoard *brd)
{
        int nbad = 0;
        int i;
        while (!A2DFIFOEmpty(brd)) {
                // Point to FIFO read subchannel
                outb(A2DIO_FIFO, brd->chan_addr);
                for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {

#ifdef DO_A2D_STATRD
                        unsigned short stat = inw(brd->addr);
                        inw(brd->addr);
                        // check for acceptable looking status value
                        if ((stat & A2DSTATMASK) != A2DEXPSTATUS)
                                nbad++;
#else
                        inw(brd->addr);
#endif
                }
        }
        return nbad;

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
        unsigned short stat;
        outb(A2DIO_FIFOSTAT, brd->chan_addr);
        stat = inw(brd->addr);

        // If FIFONOTFULL is 0, fifo IS full
        if ((stat & FIFONOTFULL) == 0)
                return 5;

        // If FIFONOTEMPTY is 0, fifo IS empty
        else if ((stat & FIFONOTEMPTY) == 0)
                return 0;

        // Figure out which 1/4 of the 1024 FIFO words we're filled to

        // bit 0, 0x1, half full
        // bit 1, 0x2, either almost full (>=3/4) or almost empty (<=1/4).
        switch (stat & 0x03)    // Switch on stat's 2 LSB's
        {
        case 3:                // allmost full/empty, half full (>=3/4 to <4/4)
                return 4;
        case 2:                // allmost full/empty, not half full (empty to <=1/4)
                return 1;
        case 1:                // not allmost full/empty, half full (>=2/4 to <3/4)
                return 3;
        case 0:                // not allmost full/empty, not half full (>1/4 to <2/4)
        default:               // can't happen, but avoid compiler warn
                return 2;
                break;
        }
        return 1;               // can't happen, but avoid compiler warn
}

/*-----------------------Utility------------------------------*/
// This routine sends the ABORT command to all A/D's.
// The ABORT command amounts to a soft reset--they
//  stay configured.

static void A2DReset(struct A2DBoard *brd, int A2DSel)
{
        // Point to the A2D command register
        outb(A2DCMNDWR, brd->chan_addr);

        // Send specified A/D the abort (soft reset) command
        outw(A2DABORT, brd->addr + A2DSel * 2);
        return;
}

static void A2DResetAll(struct A2DBoard *brd)
{
        int i;
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
                A2DReset(brd, i);
}

/*-----------------------Utility------------------------------*/
// This routine sets the A/D's to auto mode

static void A2DAuto(struct A2DBoard *brd)
{
        // Point to the FIFO Control word
        outb(A2DIO_FIFO, brd->chan_addr);

        // Set Auto run bit and send to FIFO control byte
        brd->FIFOCtl |= A2DAUTO;
        outb(brd->FIFOCtl, brd->addr);
        return;
}

/*-----------------------Utility------------------------------*/
// This routine sets the A/D's to non-auto mode

static void A2DNotAuto(struct A2DBoard *brd)
{
        // Point to the FIFO Control word
        outb(A2DIO_FIFO, brd->chan_addr);

        // Turn off the auto bit and send to FIFO control byte
        brd->FIFOCtl &= ~A2DAUTO;
        outb(brd->FIFOCtl, brd->addr);
        return;
}

/*-----------------------Utility------------------------------*/
// Start the selected A/D in acquisition mode

static void A2DStart(struct A2DBoard *brd, int A2DSel)
{
        // Point at the A/D command channel
        outb(A2DCMNDWR, brd->chan_addr);

        // Start the selected A/D
        outw(A2DREADDATA, brd->addr + A2DSel * 2);
        return;
}

static void A2DStartAll(struct A2DBoard *brd)
{
        int i;
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
                A2DStart(brd, i);
}

/*-----------------------Utility------------------------------*/
// Configure A/D A2dSel with coefficient array 'filter'
static int A2DConfig(struct A2DBoard *brd, int A2DSel)
{
        int j, ctr = 0;
        unsigned short stat;
        unsigned char intmask = 1, intbits[8] =
            { 1, 2, 4, 8, 16, 32, 64, 128 };
        int tomsgctr = 0;
        int crcmsgctr = 0;

        if (A2DSel < 0 || A2DSel >= NUM_NCAR_A2D_CHANNELS)
                return -EINVAL;

        // Point to the A/D write configuration channel
        outb(A2DCMNDWR, brd->chan_addr);

        // Set the interrupt mask
        intmask = intbits[A2DSel];

        // Set configuration write mode
        outw(A2DWRCONFIG, brd->addr + A2DSel * 2);

        for (j = 0; j < CONFBLLEN * CONFBLOCKS + 1; j++) {
                // Set channel pointer to Config write and
                //   write out configuration word
                outb(A2DCONFWR, brd->chan_addr);
                outw(brd->config.filter[j], brd->addr + A2DSel * 2);
                rtl_usleep(30);

                // Set channel pointer to sysctl to read int lines
                // Wait for interrupt bit to set

                outb(A2DIO_SYSCTL, brd->chan_addr);
                while ((inb(brd->addr) & intmask) == 0) {
                        rtl_usleep(30);
                        if (ctr++ > 10000) {
                                tomsgctr++;
//          DSMLOG_WARNING("INTERRUPT TIMEOUT! chip = %1d\n", A2DSel);
//          return -ETIMEDOUT;
                                break;
                        }
                }
                // Read status word from target a/d to clear interrupt
                outb(A2DSTATRD, brd->chan_addr);
                stat = inw(brd->addr + A2DSel * 2);

                // Check status bits for errors
                if (stat & A2DCRCERR) {
                        crcmsgctr++;
                        brd->cur_status.badval[A2DSel] = stat;  // Error status word
//       DSMLOG_WARNING("CRC ERROR! chip = %1d, stat = 0x%04X\n", A2DSel, stat);
//       return -EIO;
                }
        }
        if (crcmsgctr > 0)
                DSMLOG_ERR("%3d CRC Errors chip = %1d\n",
                           crcmsgctr, A2DSel);
        else
                DSMLOG_DEBUG("%3d CRC Errors chip = %1d\n",
                             crcmsgctr, A2DSel);

        if (tomsgctr > 0)
                DSMLOG_ERR("%3d Interrupt Timeout Errors chip = %1d\n",
                           tomsgctr, A2DSel);
        else
                DSMLOG_DEBUG("%3d Interrupt Timeout Errors chip = %1d\n",
                             tomsgctr, A2DSel);

        brd->cur_status.goodval[A2DSel] = stat; // Final status word following load
        rtl_usleep(2000);
        return 0;
}

/*-----------------------Utility------------------------------*/
// Configure all A/D's with same filter

static int A2DConfigAll(struct A2DBoard *brd)
{
        int ret;
        int i;
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
                if ((ret = A2DConfig(brd, i)) < 0)
                        return ret;
        return 0;
}

// the status bits are in the upper byte contain the serial number
static int getSerialNumber(struct A2DBoard *brd)
{
        unsigned short stat;
        // fetch serial number
        outb(A2DIO_FIFOSTAT, brd->chan_addr);
        stat = inw(brd->addr);
        // DSMLOG_DEBUG("brd->chan_addr: %x  stat: %x\n", brd->chan_addr, stat);
        return (stat & 0xFFC0) >> 6;    // S/N is upper 10 bits
}


/*-----------------------Utility------------------------------*/
// Utility function to wait for INV1PPS to be zero.
// Since it uses rtl_usleep, it must be called from a
// real-time thread.
// Return: negative Linux (not RTLinux) errno, or 0=OK.

static int waitFor1PPS(struct A2DBoard *brd)
{
        unsigned short stat;
        int timeit = 0;
        int waitUsec = 50;
        int totalWaitSec = 5;
        // Point at the FIFO status channel
        outb(A2DIO_FIFOSTAT, brd->chan_addr);

        while (timeit++ < (USECS_PER_SEC * totalWaitSec) / waitUsec) {
                // Read status, check INV1PPS bit
                stat = inw(brd->addr);
                if ((stat & INV1PPS) == 0)
                        return 0;
                rtl_usleep(waitUsec);   // Wait and try again
        }
        DSMLOG_ERR("1PPS not detected--no sync to GPS\n");
        return -ETIMEDOUT;
}

static int setupA2DFilters(struct A2DBoard *brd)
{

        int i,j;
        int uniqueSampleIndx[NUM_NCAR_A2D_CHANNELS];
        A2D_SET *cfg = &brd->config;

        if (brd->filters) {
                for (j = 0; j < brd->nfilters; j++) {
                        if (brd->filters[j].channels)
                                rtl_gpos_free(brd->filters[j].channels);
                        brd->filters[j].channels = 0;
                }
                rtl_gpos_free(brd->filters);
                brd->filters = 0;
        }

        brd->nfilters = 0;
        /* count number of unique samples requested.
         * A filter will be created for each sample */
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                int index = cfg->sampleIndex[i];
                // zero gain means the channel is to be skipped
                brd->requested[i] = index >= 0;
                if (index < 0) continue;

                for (j = 0; j < brd->nfilters; j++)
                        if (index == uniqueSampleIndx[j])
                                break;
                if (j == brd->nfilters)
                        uniqueSampleIndx[brd->nfilters++] = index;
        }

        if (brd->nfilters == 0) {
                KLOG_ERR("%s: no channels requested, all indices<0\n",
                         brd->a2dFifoName);
                return -EINVAL;
        }
        if (!(brd->filters = rtl_gpos_malloc(brd->nfilters *
                                       sizeof (struct a2d_filter_info))))
                                        return -ENOMEM;
        memset(brd->filters, 0,
               brd->nfilters * sizeof (struct a2d_filter_info));

        /* Count number of channels for each sample */
        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                int index = cfg->sampleIndex[i];
                if (index < 0) continue;
                if (index >= brd->nfilters) {
                        KLOG_ERR("%s: invalid sample index=%d, max=%d\n",
                                 brd->a2dFifoName,index,brd->nfilters-1);
                        return -EINVAL;
                }
                brd->filters[index].nchans++;
        }

        /* Allocate array of indicies into scanned channel sequence */
        for (i = 0; i < brd->nfilters; i++) {
                if (!(brd->filters[i].channels =
                    rtl_gpos_malloc(brd->filters[i].nchans * sizeof (int))))
                        return -ENOMEM;
                brd->filters[i].nchans = 0;
        }

        for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                int index = cfg->sampleIndex[i];
                if (index < 0) continue;
                brd->filters[index].channels[brd->filters[index].nchans++] = i;
        }
        return 0;
}

static int A2DSetup(struct A2DBoard *brd)
{
        A2D_SET *cfg = &brd->config;
        int i,j;
        int ret;

        brd->nfilters = 0;

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

#ifdef DO_A2D_STATRD
        brd->FIFOCtl = A2DSTATEBL;      // Clear most of FIFO Control Word
#else
        brd->FIFOCtl = 0;       // Clear FIFO Control Word
#endif

        brd->OffCal = 0x0;

        for (j = 0; j < 3; j++) {       // HACK! the CPLD logic needs to be fixed!
                for (i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
                        if (cfg->gain[i] != 0
                            && (ret = A2DSetGain(brd, i)) < 0)
                                return ret;
                outb(A2DIO_D2A1, brd->chan_addr);
                rtl_usleep(10000);
                outb(A2DIO_D2A2, brd->chan_addr);
                rtl_usleep(10000);
                outb(A2DIO_D2A1, brd->chan_addr);
                rtl_usleep(10000);
        }                       // END HACK!
        brd->cur_status.ser_num = getSerialNumber(brd);
// DSMLOG_DEBUG("A2D serial number = %d\n", brd->cur_status.ser_num);

        if ((ret = A2DSetMaster(brd, brd->master)) < 0)
                return ret;

        A2DSetOffset(brd);

        DSMLOG_DEBUG("success!\n");
        return 0;
}

/*--------------------- Thread function ----------------------*/
// A2DThread loads the A2Ds with filter data from A2D structure.
//
// NOTE: This must be called from within a real-time thread
//                      Otherwise the critical delays will not work properly

static void *A2DSetupThread(void *thread_arg)
{
        struct A2DBoard *brd = (struct A2DBoard *) thread_arg;
        int ret = 0;

        // Configure DAC gain codes
        if ((ret = A2DSetup(brd)) < 0)
                return (void *) -ret;

        // Make sure SYNC is cleared so clocks are running
        DSMLOG_DEBUG("Clearing SYNC\n");
        A2DClearSYNC(brd);

        // Start then reset the A/D's
        // Start conversions
        DSMLOG_DEBUG("Starting A/D's\n");
        A2DStartAll(brd);

        // If starting from a cold boot, one needs to
        // let the A2Ds run for a bit before downloading
        // the filter data.
        rtl_usleep(50000);      // Let them run a few milliseconds (50)

        // Then do a soft reset
        DSMLOG_DEBUG("Soft resetting A/D's\n");
        A2DResetAll(brd);

        // Configure the A/D's
        DSMLOG_DEBUG("Sending filter config data to A/Ds\n");
        if ((ret = A2DConfigAll(brd)) < 0)
                return (void *) -ret;
        // Reset the A/D's
        DSMLOG_DEBUG("Resetting A/Ds\n");
        A2DResetAll(brd);

        rtl_usleep(1000);       // Give A/D's a chance to load
        DSMLOG_DEBUG("A/Ds ready for synchronous start\n");

        return (void *) ret;
}

/*
 * Configure a filter.  A2D should not be running.
 */
static int configA2DFilter(struct A2DBoard *brd,
                           struct nidas_a2d_filter_config *cfg)
{
        struct a2d_filter_info *fcfg;
        struct short_filter_methods methods;
        int result;

        if (brd->busy) {
                KLOG_ERR("A2D's running. Can't configure\n");
                return -EBUSY;
        }

        KLOG_DEBUG("%s: index=%d,nfilters=%d\n",
                   brd->a2dFifoName, cfg->index, brd->nfilters);

        if (cfg->index < 0 || cfg->index >= brd->nfilters)
                return -EINVAL;

        fcfg = &brd->filters[cfg->index];

        KLOG_DEBUG("%s: scanRate=%d,cfg->rate=%d\n",
                   brd->a2dFifoName, brd->scanRate, cfg->rate);

        if (brd->scanRate % cfg->rate) {
                KLOG_ERR
                    ("%s: A2D scanRate=%d is not a multiple of the rate=%d for sample %d\n",
                     brd->a2dFifoName, brd->scanRate, cfg->rate,
                     cfg->index);
                return -EINVAL;
        }

        /* cleanup a previous filter if one exists */
        if (fcfg->filterObj && fcfg->fcleanup)
                fcfg->fcleanup(fcfg->filterObj);
        fcfg->filterObj = 0;
        fcfg->fcleanup = 0;

        fcfg->decimate = brd->scanRate / cfg->rate;
        fcfg->filterType = cfg->filterType;
        fcfg->index = cfg->index;

        KLOG_DEBUG("%s: decimate=%d,filterType=%d,index=%d\n",
                   brd->a2dFifoName, fcfg->decimate, fcfg->filterType,
                   fcfg->index);

        methods = get_short_filter_methods(cfg->filterType);
        if (!methods.init) {
                KLOG_ERR("%s: filter type %d unsupported\n",
                         brd->a2dFifoName, cfg->filterType);
                return -EINVAL;
        }
        fcfg->finit = methods.init;
        fcfg->fconfig = methods.config;
        fcfg->filter = methods.filter;
        fcfg->fcleanup = methods.cleanup;

        /* Create the filter object */
        fcfg->filterObj = fcfg->finit();
        if (!fcfg->filterObj)
                return -ENOMEM;

        /* Configure the filter */
        switch (fcfg->filterType) {
        case NIDAS_FILTER_BOXCAR:
                {
                        struct boxcar_filter_config bcfg;
                        KLOG_DEBUG("%s: BOXCAR\n", brd->a2dFifoName);
                        bcfg.npts = cfg->boxcarNpts;
                        result = fcfg->fconfig(fcfg->filterObj, fcfg->index,
                                               fcfg->nchans,
                                               fcfg->channels,
                                               fcfg->decimate, &bcfg);
                }
                break;
        case NIDAS_FILTER_PICKOFF:
                KLOG_DEBUG("%s: PICKOFF\n", brd->a2dFifoName);
                result = fcfg->fconfig(fcfg->filterObj, fcfg->index,
                                       fcfg->nchans, fcfg->channels,
                                       fcfg->decimate, 0);
                break;
        default:
                result = -EINVAL;
                break;
        }

        KLOG_DEBUG("%s: result=%d\n", brd->a2dFifoName, result);
        return result;
}

/*-----------------------Utility------------------------------*/

static inline void getA2DSample(struct A2DBoard *brd)
{
        int nbad = 0;
        struct short_sample *samp;
        int i;

        if (!brd->enableReads)
                return;

        int flevel = getA2DFIFOLevel(brd);

        if (brd->discardNextScan) {
                if (flevel > 0)
                        A2DEmptyFIFO(brd);
                brd->discardNextScan = 0;
                return;
        }

/*
// DEBUG TEST - strobe across each channel and each VCAL voltage periodically
   static int strobe_vcal_lastime = 0;
   static int strobe_vcal_channel = 0;
   static int strobe_vcal_voltage = 0x01;
   char *strobe_vcal_voltage_str;
   int i;
   if (strobe_vcal_lastime == 0)
      strobe_vcal_lastime = samp.timetag + 10000;
   if (samp.timetag > strobe_vcal_lastime + 10000) {
      strobe_vcal_lastime = samp.timetag;
      switch (strobe_vcal_voltage) {
      case 0x01: strobe_vcal_voltage_str = " gnd"; break;
      case 0x02: strobe_vcal_voltage_str = " +1v"; break;
      case 0x04: strobe_vcal_voltage_str = " +5v"; break;
      case 0x08: strobe_vcal_voltage_str = "-10v"; break;
      case 0x10: strobe_vcal_voltage_str = "+10v"; break;
      default: break;
      }
      DSMLOG_DEBUG("%d: set channel %d to calibration voltage to %s\n",
                   strobe_vcal_lastime, strobe_vcal_channel, strobe_vcal_voltage_str);

      for (i=0; i<8; i++)
         if (strobe_vcal_channel == i)
            brd->cal.calset[i] = 1;
         else
            brd->cal.calset[i] = 0;
      brd->cal.vcalx8 = strobe_vcal_voltage;
#if STROBE_VOLTS_THEN_CHANNELS
      strobe_vcal_voltage<<=1;
      if (strobe_vcal_voltage == 0x20) {
         strobe_vcal_voltage = 0x01;
         strobe_vcal_channel++;
         if (strobe_vcal_channel == 8)
            strobe_vcal_channel = 0;
      }
#else
      strobe_vcal_channel++;
      if (strobe_vcal_channel == 8) {
         strobe_vcal_channel = 0;
         strobe_vcal_voltage<<=1;
         if (strobe_vcal_voltage == 0x20)
            strobe_vcal_voltage = 0x01;
      }
#endif
      A2DSetVcal(brd);
      A2DSetCal(brd);
   }
// END DEBUG TEST - strobe across each channel and each VCAL voltage periodically
*/
        brd->cur_status.preFifoLevel[flevel]++;
        if (flevel != brd->expectedFifoLevel) {
                if (!(brd->nbadFifoLevel++ % 1000)) {
                        DSMLOG_ERR
                            ("clock=%d, pre-read fifo level=%d is not expected value=%d (%d times)\n",
                             GET_MSEC_CLOCK, flevel,
                             brd->expectedFifoLevel, brd->nbadFifoLevel);
                        if (flevel == 5)
                                DSMLOG_ERR
                                    ("Is the external clock cable plugged into the A2D board?\n",
                                     flevel);
                        if (brd->nbadFifoLevel > 1) {
                                startA2DResetThread(brd);
                                return;
                        }
                }
                if (flevel == 0)
                        return;
        }

        outb(A2DIO_FIFO, brd->chan_addr);

        samp = (struct short_sample*) GET_HEAD(brd->fifo_samples, FIFO_SAMPLE_QUEUE_SIZE);
        if (!samp) {            // no output sample available
                brd->skippedSamples +=
                    brd->nFifoValues / NUM_NCAR_A2D_CHANNELS /
                    brd->skipFactor;
                KLOG_WARNING("%s: skippedSamples=%d\n", brd->a2dFifoName,
                             brd->skippedSamples);
                insw(brd->addr, brd->discardBuffer, brd->nFifoValues);
                return;
        }
        samp->timetag = GET_MSEC_CLOCK;
        /*
         * Read the fifo.
         */
        insw(brd->addr, samp->data, brd->nFifoValues);
        samp->length = brd->nFifoValues * sizeof (short);

#ifdef DO_A2D_STATRD
#ifdef CHECK_A2D_STATRD
        nbad = 0;
        for (i = 0; i < brd->nFifoValues; i += 2) {
                int ichan = (i / 2) % NUM_NCAR_A2D_CHANNELS;

                if ((samp->data[i] & A2DSTATMASK) != A2DEXPSTATUS) {
                        nbad++;
                        brd->cur_status.nbad[ichan]++;
                        brd->cur_status.badval[ichan] = samp->data[i];
                        //       samp->data[i+1] = -32768;  // set to missing value
                } else
                        brd->cur_status.goodval[ichan] = samp->data[i];
        }
#endif

        // Invert bits for later cards
        if (brd->invertCounts) {
                for (i = 1; i < brd->nFifoValues; i += 2)
                        samp->data[i] = -samp->data[i];
        }
#else
        // Invert bits for later cards
        if (brd->invertCounts) {
                for (i = 0; i < brd->nFifoValues; i++)
                        samp->data[i] = -samp->data[i];
        }
#endif

        /* increment head, this sample is ready for consumption */
        INCREMENT_HEAD(brd->fifo_samples, FIFO_SAMPLE_QUEUE_SIZE);
        rtl_sem_post(&brd->bh_sem);

        flevel = getA2DFIFOLevel(brd);
        brd->cur_status.postFifoLevel[flevel]++;

        if (flevel > 0) {
                if (!(brd->fifoNotEmpty++ % 1000))
                        DSMLOG_WARNING
                            ("post-read fifo level=%d (not empty): %d times.\n",
                             flevel, brd->fifoNotEmpty);

                if (flevel > brd->expectedFifoLevel
                    || brd->fifoNotEmpty > 1) {
                        startA2DResetThread(brd);
                        return;
                }
        }
#if defined(DO_A2D_STATRD) && defined(CHECK_A2D_STATRD)
        if (nbad > 0)
                brd->nbadScans++;
#endif

        // DSMSensor::printStatus queries these values every 10 seconds
        if (!(++brd->readCtr % (A2D_POLL_RATE * 10))) {

                // debug print every minute, or if there are bad scans
                if (!(brd->readCtr % (A2D_POLL_RATE * 60))
                    || brd->nbadScans) {
#if defined(DO_A2D_STATRD) && defined(CHECK_A2D_STATRD)
                        DSMLOG_DEBUG("GET_MSEC_CLOCK=%d, nbadScans=%d\n",
                                     GET_MSEC_CLOCK, brd->nbadScans);
#endif
                        DSMLOG_DEBUG
                            ("nbadFifoLevel=%d, #fifoNotEmpty=%d, #skipped=%d, #resets=%d\n",
                             brd->nbadFifoLevel, brd->fifoNotEmpty,
                             brd->skippedSamples, brd->resets);
                        DSMLOG_DEBUG
                            ("pre-scan  fifo=%d,%d,%d,%d,%d,%d (0,<=1/4,<2/4,<3/4,<4/4,full)\n",
                             brd->cur_status.preFifoLevel[0],
                             brd->cur_status.preFifoLevel[1],
                             brd->cur_status.preFifoLevel[2],
                             brd->cur_status.preFifoLevel[3],
                             brd->cur_status.preFifoLevel[4],
                             brd->cur_status.preFifoLevel[5]);
                        DSMLOG_DEBUG("post-scan fifo=%d,%d,%d,%d,%d,%d\n",
                                     brd->cur_status.postFifoLevel[0],
                                     brd->cur_status.postFifoLevel[1],
                                     brd->cur_status.postFifoLevel[2],
                                     brd->cur_status.postFifoLevel[3],
                                     brd->cur_status.postFifoLevel[4],
                                     brd->cur_status.postFifoLevel[5]);
#if defined(DO_A2D_STATRD) && defined(CHECK_A2D_STATRD)
                        DSMLOG_DEBUG
                            ("last good status= %04x %04x %04x %04x %04x %04x %04x %04x\n",
                             brd->cur_status.goodval[0],
                             brd->cur_status.goodval[1],
                             brd->cur_status.goodval[2],
                             brd->cur_status.goodval[3],
                             brd->cur_status.goodval[4],
                             brd->cur_status.goodval[5],
                             brd->cur_status.goodval[6],
                             brd->cur_status.goodval[7]);

                        if (brd->nbadScans > 0) {
                                DSMLOG_DEBUG
                                    ("last bad status=  %04x %04x %04x %04x %04x %04x %04x %04x\n",
                                     brd->cur_status.badval[0],
                                     brd->cur_status.badval[1],
                                     brd->cur_status.badval[2],
                                     brd->cur_status.badval[3],
                                     brd->cur_status.badval[4],
                                     brd->cur_status.badval[5],
                                     brd->cur_status.badval[6],
                                     brd->cur_status.badval[7]);
                                DSMLOG_DEBUG
                                    ("num  bad status=  %4d %4d %4d %4d %4d %4d %4d %4d\n",
                                     brd->cur_status.nbad[0],
                                     brd->cur_status.nbad[1],
                                     brd->cur_status.nbad[2],
                                     brd->cur_status.nbad[3],
                                     brd->cur_status.nbad[4],
                                     brd->cur_status.nbad[5],
                                     brd->cur_status.nbad[6],
                                     brd->cur_status.nbad[7]);

                                if (brd->nbadScans > 10) {
                                        startA2DResetThread(brd);
                                        return;
                                }
                        }
#endif
                        brd->readCtr = 0;
                }               // debug printout
                brd->nbadScans = 0;
                // copy current status to prev_status for access by ioctl A2D_GET_STATUS
                brd->cur_status.nbadFifoLevel = brd->nbadFifoLevel;
                brd->cur_status.fifoNotEmpty = brd->fifoNotEmpty;
                brd->cur_status.skippedSamples = brd->skippedSamples;
                brd->cur_status.resets = brd->resets;
                memcpy(&brd->prev_status, &brd->cur_status,
                       sizeof (A2D_STATUS));
                memset(&brd->cur_status, 0, sizeof (A2D_STATUS));

        }

#ifdef TIME_CHECK
        if (GET_MSEC_CLOCK != samp.timetag)
                DSMLOG_WARNING
                    ("excessive time in data-acq loop: start=%d,end=%d\n",
                     samp.timetag, GET_MSEC_CLOCK);
#endif

}

// function is scheduled to be called from IRIG driver at 100Hz
static void a2dIrigCallback(void *ptr)
{
        struct A2DBoard *brd = (struct A2DBoard *) ptr;
        getA2DSample((struct A2DBoard *) brd);
}

// Callback function to read I2C temperature samples
static void i2cTempIrigCallback(void *ptr)
{
        struct A2DBoard *brd = (struct A2DBoard *) ptr;

        short_sample_t *osamp = (short_sample_t *)
                GET_HEAD(brd->temp_samples, TEMP_SAMPLE_QUEUE_SIZE);
        if (!osamp) {
                    if (!(brd->skippedSamples++ % 1000))
                            KLOG_WARNING("%s: skippedSamples=%d\n",
                                         brd->a2dFifoName,
                                         brd->skippedSamples);
        }
        else {
                osamp->timetag = GET_MSEC_CLOCK;
                osamp->length = 2 * sizeof (short);
                osamp->data[0] = cpu_to_le16(NCAR_A2D_TEMPERATURE_INDEX);
                osamp->data[1] = brd->i2cTempData = cpu_to_le16(A2DTemp(brd));
                INCREMENT_HEAD(brd->temp_samples,
                                       TEMP_SAMPLE_QUEUE_SIZE);
#ifdef DEBUG
                DSMLOG_DEBUG("Brd temp %d.%1d degC\n", brd->i2cTempData / 16,
                             (10 * (brd->i2cTempData % 16)) / 16);
#endif
        }
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
                        if (!(brd->skippedSamples++ % 1000))
                                KLOG_WARNING("%s: skippedSamples=%d\n",
                                             brd->a2dFifoName,
                                             brd->skippedSamples);
                        brd->filters[i].filter(brd->filters[i].filterObj,
                                               tt, dp, brd->skipFactor,
                                               brd->discardSample);
                } else if (brd->filters[i].
                           filter(brd->filters[i].filterObj, tt, dp,
                                  brd->skipFactor, osamp)) {

#ifdef __BIG_ENDIAN
                        // convert to little endian
                        int j;
                        // sample type field (don't invert)
                        osamp->data[0] = cpu_to_le16(osamp->data[0]);
                        for (j = 1; j < osamp->length / sizeof (short);
                             j++)
                                osamp->data[j] =
                                    cpu_to_le16(osamp->data[j]);
#endif

                        INCREMENT_HEAD(brd->a2d_samples,
                                       A2D_SAMPLE_QUEUE_SIZE);
                        KLOG_DEBUG("do_filters: samples head=%d,tail=%d\n",
                                   brd->a2d_samples.head,
                                   brd->a2d_samples.tail);
                }
        }
}

static int writeSampleToUser(struct A2DBoard *brd,struct short_sample *samp)
{
        size_t slen =
            SIZEOF_DSM_SAMPLE_HEADER +
            samp->length;
        // check if buffer full, or latency time has elapsed.
        if (brd->ohead > brd->otail &&
                (brd->ohead + slen > A2D_OUTPUT_BUFFER_SIZE ||
                 ((long) jiffies - (long) brd->lastWrite) >
                    brd->latencyJiffies)) {
                // Write on rtl fifo to user land.
                ssize_t wlen;
                if ((wlen = rtl_write(brd->a2dfd, brd->obuffer + brd->otail,
                                      brd->ohead - brd->otail)) < 0) {
                        int ierr = rtl_errno;
                        DSMLOG_ERR
                            ("error: write of %d bytes to %s: %s. Closing\n",
                             brd->ohead - brd->otail,
                             brd->a2dFifoName,
                             rtl_strerror(rtl_errno));
                        rtl_close(brd->a2dfd);
                        brd->a2dfd = -1;
                        return ierr;
                }
                if (wlen != brd->ohead - brd->otail)
                        DSMLOG_WARNING
                            ("warning: short write: request=%d, actual=%d\n",
                             brd->ohead -
                             brd->otail, wlen);
                brd->otail += wlen;
                if (brd->otail == brd->ohead) brd->ohead = brd->otail = 0;
                brd->lastWrite = jiffies;

        }
        if (brd->ohead + slen <=
            A2D_OUTPUT_BUFFER_SIZE) {
                memcpy(brd->obuffer + brd->ohead,
                       samp, slen);
                brd->ohead += slen;
        } else if (!(brd->skippedSamples++ % 1000))
                DSMLOG_WARNING
                    ("warning: %d samples lost due to backlog in %s\n",
                     brd->skippedSamples,
                     brd->a2dFifoName);
        return 0;
}

/**
 * Thread function which applies filters to the fifo data.
 */
static void *a2d_bh_thread(void *thread_arg)
{
        struct A2DBoard *brd = (struct A2DBoard *) thread_arg;

        DSMLOG_DEBUG("Starting bottom-half thread, GET_MSEC_CLOCK=%d\n",
                     GET_MSEC_CLOCK);

        for (;;) {
                rtl_sem_wait(&brd->bh_sem);
                if (brd->interrupt_bh)
                        break;
                while (brd->fifo_samples.head != brd->fifo_samples.tail) {
                        struct short_sample *insamp =
                            (struct short_sample*) brd->fifo_samples.buf[brd->fifo_samples.tail];

                        int nval =
                            insamp->length / sizeof (short) /
                            brd->skipFactor;

                        short *dp = (short *) insamp->data;
                        short *ep;
                        int ndt;
                        dsm_sample_time_t tt0;

#ifdef DO_STAT_RD
                        dp++;   // skip over first status word
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

                        KLOG_DEBUG("bottom half, nval=%d,ndt=%d, dt=%ld\n",
                                   nval, ndt, ndt * brd->scanDeltatMsec);

                        for (; dp < ep;) {
                                do_filters(brd, tt0, dp);
                                dp += NUM_NCAR_A2D_CHANNELS * brd->skipFactor;
                                tt0 += brd->scanDeltatMsec;
                        }
                        INCREMENT_TAIL(brd->fifo_samples,
                                       FIFO_SAMPLE_QUEUE_SIZE);
                        // write the filtered samples to the output rtl fifo
                        while (brd->a2d_samples.head !=
                               brd->a2d_samples.tail) {
                                struct short_sample *samp = (short_sample_t *)
                                    brd->a2d_samples.buf[brd->a2d_samples.
                                                         tail];
                                int ret = writeSampleToUser(brd,samp);
                                if (ret) return (void *)convert_rtl_errno(ret);
                                INCREMENT_TAIL(brd->a2d_samples,
                                       A2D_SAMPLE_QUEUE_SIZE);
                        }       // loop over available filtered samples
                }               // loop over available fifo samples

                // write temperature samples to the output rtl fifo
                while (brd->temp_samples.head !=
                       brd->temp_samples.tail) {
                        struct short_sample *samp = (struct short_sample*)
                            brd->temp_samples.buf[brd->temp_samples.
                                                 tail];
                        int ret = writeSampleToUser(brd,samp);
                        if (ret) return (void *)convert_rtl_errno(ret);
                        INCREMENT_TAIL(brd->temp_samples,
                               TEMP_SAMPLE_QUEUE_SIZE);
                }       // loop over available temperature samples

        }                       // loop waiting for semaphore
        DSMLOG_DEBUG("Exiting a2d_bh_thread\n");
        return 0;
}

// Reset the A2D.
// This does an unregister_irig_callback, so it can't be
// called from the irig callback function itself.
// Use startA2DResetThread in that case.

static int resetA2D(struct A2DBoard *brd)
{
        int saveTempRate = brd->tempRate;
        DSMLOG_DEBUG("doing unregister_irig_callback\n");
        unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);
        if (brd->tempRate != IRIG_NUM_RATES)
                unregister_irig_callback(i2cTempIrigCallback,
                                         brd->tempRate, brd);
        brd->tempRate = IRIG_NUM_RATES;
        DSMLOG_DEBUG("unregister_irig_callback done\n");

        DSMLOG_DEBUG("doing waitFor1PPS, GET_MSEC_CLOCK=%d\n",
                     GET_MSEC_CLOCK);
        int res = waitFor1PPS(brd);
        if (res)
                return res;
        DSMLOG_DEBUG("Found initial PPS, GET_MSEC_CLOCK=%d\n",
                     GET_MSEC_CLOCK);


        A2DResetAll(brd);       // Send Abort command to all A/Ds
        A2DStatusAll(brd);      // Read status from all A/Ds

        A2DStartAll(brd);       // Start all the A/Ds
        A2DStatusAll(brd);      // Read status again from all A/Ds

        A2DSetSYNC(brd);        // Stop A/D clocks
        A2DAuto(brd);           // Switch to automatic mode

        DSMLOG_DEBUG("Setting 1PPS Enable line\n");

        rtl_nanosleep(&msec20, 0);
        A2D1PPSEnable(brd);     // Enable sync with 1PPS


        DSMLOG_DEBUG("doing waitFor1PPS, GET_MSEC_CLOCK=%d\n",
                     GET_MSEC_CLOCK);
        res = waitFor1PPS(brd);
        if (res)
                return res;
        DSMLOG_DEBUG("Found second PPS, GET_MSEC_CLOCK=%d\n",
                     GET_MSEC_CLOCK);
        A2DClearFIFO(brd);      // Reset FIFO

        brd->discardNextScan = 1;       // whether to discard the initial scan
        brd->enableReads = 1;
        brd->nbadScans = 0;
        brd->readCtr = 0;
        brd->nbadFifoLevel = 0;
        brd->fifoNotEmpty = 0;
        brd->skippedSamples = 0;
        brd->resets++;

        // start the IRIG callback routine at 100 Hz
        brd->fifo_samples.head = brd->fifo_samples.tail = 0;
        register_irig_callback(a2dIrigCallback, IRIG_100_HZ, brd);

        brd->tempRate = saveTempRate;
        if (brd->tempRate != IRIG_NUM_RATES) {
            res =
                register_irig_callback(i2cTempIrigCallback, brd->tempRate, brd);
            if (res != 0) return res;
        }

        return 0;
}

static void *A2DResetThreadFunc(void *thread_arg)
{
        struct A2DBoard *brd = (struct A2DBoard *) thread_arg;
        int ret = resetA2D(brd);
        return (void *) -ret;
}

static int startA2DResetThread(struct A2DBoard *brd)
{
        DSMLOG_WARNING("GET_MSEC_CLOCK=%d, Resetting A2D\n",
                       GET_MSEC_CLOCK);
        brd->enableReads = 0;
        // Shut down any existing reset thread
        if (brd->reset_thread) {
                rtl_pthread_cancel(brd->reset_thread);
                rtl_pthread_join(brd->reset_thread, NULL);
                brd->reset_thread = 0;
        }

        DSMLOG_DEBUG("Starting reset thread\n");

        rtl_pthread_attr_t attr;
        rtl_pthread_attr_init(&attr);
        rtl_pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
        rtl_pthread_attr_setstackaddr(&attr, brd->reset_thread_stack);
        if (rtl_pthread_create
            (&brd->reset_thread, &attr, A2DResetThreadFunc, brd)) {
                DSMLOG_ERR("Error starting A2DResetThreadFunc: %s\n",
                           rtl_strerror(rtl_errno));
                return -convert_rtl_errno(rtl_errno);
        }
        rtl_pthread_attr_destroy(&attr);
        return 0;
}

static int openA2D(struct A2DBoard *brd)
{
        int nFifoValues;
        int ret = 0;

        brd->busy = 1;          // Set the busy flag

        brd->fifo_samples.head = brd->fifo_samples.tail = 0;
        brd->a2d_samples.head = brd->a2d_samples.tail = 0;
        brd->temp_samples.head = brd->temp_samples.tail = 0;
        brd->ohead = brd->otail = 0;

        brd->lastWrite = jiffies;

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

        /*
         * If nFifoValues increases, re-allocate samples
         */
        if (nFifoValues > brd->nFifoValues) {
                ret = realloc_dsm_circ_buf(&brd->fifo_samples,
                                           nFifoValues * sizeof (short),
                                           FIFO_SAMPLE_QUEUE_SIZE);
                if (ret)
                        return ret;

                rtl_gpos_free(brd->discardBuffer);
                brd->discardBuffer = (short *)
                    rtl_gpos_malloc(nFifoValues * sizeof (short));
                if (!brd->discardBuffer)
                        return -ENOMEM;
        }

        brd->nFifoValues = nFifoValues;

        // expected fifo level just before we read
        brd->expectedFifoLevel = (brd->nFifoValues * 4) / HWFIFODEPTH + 1;
        // level of 1 means <=1/4
        if (brd->nFifoValues == HWFIFODEPTH / 4)
                brd->expectedFifoLevel = 1;

        DSMLOG_INFO
            ("nFifoValues=%d,expectedFifoLevel=%d,scanDeltatMsec=%d\n",
             brd->nFifoValues, brd->expectedFifoLevel,
             brd->scanDeltatMsec);

        memset(&brd->cur_status, 0, sizeof (A2D_STATUS));
        memset(&brd->prev_status, 0, sizeof (A2D_STATUS));

        if (brd->a2dfd >= 0)
                rtl_close(brd->a2dfd);
        if ((brd->a2dfd = rtl_open(brd->a2dFifoName,
                                   RTL_O_NONBLOCK | RTL_O_WRONLY)) < 0) {
                DSMLOG_ERR("error: opening %s: %s\n",
                           brd->a2dFifoName, rtl_strerror(rtl_errno));
                return -convert_rtl_errno(rtl_errno);
        }

#ifdef DO_FTRUNCATE
        if (rtl_ftruncate(brd->a2dfd, sizeof (brd->buffer) * 4) < 0) {
                DSMLOG_ERR("error: ftruncate %s: size=%d: %s\n",
                           brd->a2dFifoName, sizeof (brd->buffer),
                           rtl_strerror(rtl_errno));
                return -convert_rtl_errno(rtl_errno);
        }
#endif
        // Zero the semaphore
        rtl_sem_init(&brd->bh_sem, 0, 0);

        brd->interrupt_bh = 0;
        int res = startA2DResetThread(brd);
        void *thread_status;
        rtl_pthread_join(brd->reset_thread, &thread_status);
        brd->reset_thread = 0;
        if (thread_status != (void *) 0)
                res = -(int) thread_status;

        // Start bottom-half filter thread

        rtl_pthread_attr_t attr;
        rtl_pthread_attr_init(&attr);
        rtl_pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
        rtl_pthread_attr_setstackaddr(&attr, brd->bh_thread_stack);
        if (rtl_pthread_create(&brd->bh_thread, &attr, a2d_bh_thread, brd)) {
                DSMLOG_ERR("Error starting bottom-half thread: %s\n",
                           rtl_strerror(rtl_errno));
                return -convert_rtl_errno(rtl_errno);
        }
        rtl_pthread_attr_destroy(&attr);

        brd->resets = 0;

        return res;
}

/**
 * @param joinAcqThread 1 means do a pthread_join of the acquisition thread.
 *              0 means don't do pthread_join (to avoid a deadlock)
 * @return negative UNIX errno
 */
static int closeA2D(struct A2DBoard *brd)
{
        int ret = 0;
        void *thread_status;

        // Shut down the setup thread
        if (brd->setup_thread) {
                rtl_pthread_cancel(brd->setup_thread);
                rtl_pthread_join(brd->setup_thread, NULL);
                brd->setup_thread = 0;
        }
        // Shut down the reset thread
        if (brd->reset_thread) {
                rtl_pthread_cancel(brd->reset_thread);
                rtl_pthread_join(brd->reset_thread, NULL);
                brd->reset_thread = 0;
        }
        // Turn off the callback routine
        unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);

        if (brd->tempRate != IRIG_NUM_RATES)
                unregister_irig_callback(i2cTempIrigCallback,
                                         brd->tempRate, brd);
        brd->tempRate = IRIG_NUM_RATES;

        // interrupt and join the bottom-half filter thread
        brd->interrupt_bh = 1;
        if (brd->bh_thread) {
                rtl_sem_post(&brd->bh_sem);
                rtl_pthread_join(brd->bh_thread, &thread_status);
                brd->bh_thread = 0;
                if (thread_status != (void *) 0)
                        ret = -(int) thread_status;
        }

        A2DStatusAll(brd);      // Read status and clear IRQ's

        A2DNotAuto(brd);        // Shut off auto mode (if enabled)

        // Abort all the A/D's
        A2DResetAll(brd);

        if (brd->a2dfd >= 0) {
                int fdtmp = brd->a2dfd;
                brd->a2dfd = -1;
                rtl_close(fdtmp);
        }
        brd->busy = 0;          // Reset the busy flag

        return ret;
}

/*
 * Function that is called on receipt of an ioctl request over the
 * ioctl FIFO.
 * Return: negative Linux errno (not RTLinux errnos), or 0=OK
 */
static int ioctlCallback(int cmd, int board, int port,
                         void *buf, rtl_size_t len)
{
        // return LINUX errnos here, not RTL_XXX errnos.
        int ret = -EINVAL;
        void *thread_status;

        // paranoid check if initialized (probably not necessary)
        if (!boardInfo)
                return ret;

        struct A2DBoard *brd = boardInfo + board;
        int rate;

#ifdef DEBUG
        DSMLOG_INFO("ioctlCallback cmd=%x board=%d port=%d len=%d\n",
                     cmd, board, port, len);
#endif

        switch (cmd) {
        case GET_NUM_PORTS:    /* user get */
                if (len != sizeof (int))
                        break;
                DSMLOG_DEBUG("GET_NUM_PORTS\n");
                *(int *) buf = NDEVICES;
                ret = sizeof (int);
                break;

        case A2D_GET_STATUS:   /* user get of status */
                if (port != 0) break;
                if (len != sizeof (A2D_STATUS))
                        break;
                memcpy(buf, &brd->prev_status, len);
                ret = len;
                break;

        case A2D_SET_CONFIG:   /* user set */
                if (port != 0) break;
                if (len != sizeof (A2D_SET))
                        break;  // invalid length
                if (brd->busy) {
                        DSMLOG_ERR("A2D's running. Can't reset\n");
                        ret = -EBUSY;
                        break;
                }
                DSMLOG_DEBUG("A2D_SET_CONFIG\n");
                memcpy(&brd->config, (A2D_SET *) buf, sizeof (A2D_SET));

                DSMLOG_DEBUG("Starting setup thread\n");
                if (rtl_pthread_create
                    (&brd->setup_thread, NULL, A2DSetupThread, brd)) {
                        DSMLOG_ERR("Error starting A2DSetupThread: %s\n",
                                   rtl_strerror(rtl_errno));
                        return -convert_rtl_errno(rtl_errno);
                }
                rtl_pthread_join(brd->setup_thread, &thread_status);
                DSMLOG_DEBUG("Setup thread finished\n");
                brd->setup_thread = 0;

                if (thread_status != (void *) 0)
                        ret = -(int) thread_status;
                else
                        ret = 0;        // OK
                DSMLOG_DEBUG("A2D_SET_CONFIG done, ret=%d\n", ret);


                ret = setupA2DFilters(brd);
                break;

        case A2D_SET_CAL:      /* user set */
                DSMLOG_DEBUG("A2D_CAL_IOCTL\n");
                if (port != 0) break;
                if (len != sizeof (A2D_CAL))
                        break;  // invalid length
                memcpy(&brd->cal, (A2D_CAL *) buf, sizeof (A2D_CAL));
                A2DSetVcal(brd);
                A2DSetCal(brd);
                ret = 0;
                break;
        case NIDAS_A2D_ADD_FILTER:     /* user set */
                {
                        struct nidas_a2d_filter_config cfg;
                        if (len != sizeof (struct nidas_a2d_filter_config))
                                break;
                        memcpy(&cfg, buf,
                               sizeof (struct nidas_a2d_filter_config));
                        ret = configA2DFilter(brd, &cfg);
                }
                break;
        case A2D_RUN:
                if (port != 0) break;
/*
//  bit  volts
//  0x01 gnd
//  0x02 +1
//  0x04 +5
//  0x08 -10
//  0x10 +10
         DSMLOG_DEBUG("DEBUG set a channel to a calibration voltage\n");
         brd->cal.vcalx8 = 0x04; // REMOVE ME (TEST CAL)
         brd->cal.calset[0] = 0; // REMOVE ME (TEST CAL)
         brd->cal.calset[1] = 1; // REMOVE ME (TEST CAL)
         brd->cal.calset[2] = 0; // REMOVE ME (TEST CAL)
         brd->cal.calset[3] = 0; // REMOVE ME (TEST CAL)
         brd->cal.calset[4] = 0; // REMOVE ME (TEST CAL)
         brd->cal.calset[5] = 0; // REMOVE ME (TEST CAL)
         brd->cal.calset[6] = 0; // REMOVE ME (TEST CAL)
         brd->cal.calset[7] = 0; // REMOVE ME (TEST CAL)
         A2DSetVcal(brd);        // REMOVE ME (TEST CAL)
         A2DSetCal(brd);         // REMOVE ME (TEST CAL)
*/
                DSMLOG_DEBUG("A2D_RUN_IOCTL\n");
                ret = openA2D(brd);
                DSMLOG_DEBUG("A2D_RUN_IOCTL finished\n");
                break;

        case A2D_STOP:
                if (port != 0) break;
                DSMLOG_DEBUG("A2D_STOP_IOCTL\n");
                ret = closeA2D(brd);
                DSMLOG_DEBUG("closeA2D, ret=%d\n", ret);
                break;
        case A2DTEMP_SET_RATE:
                if (port != 0) break;
                /*
                 * Set temperature query rate (using enum irigClockRates)
                 */
                if (len != sizeof (int)) {
                        KLOG_WARNING
                            ("A2DTEMP_SET_RATE, bad len\n");
                        ret = -EINVAL;
                        break;
                }

                memcpy(&rate, buf, len);
                if (rate > IRIG_10_HZ) {
                        ret = -EINVAL;
                        KLOG_WARNING
                            ("Illegal rate for A/D temp probe (> 10 Hz)\n");
                        break;
                }
                brd->tempRate = rate;
                ret = 0;
                break;
        case A2DTEMP_GET_TEMP:
                if (port != 0) break; 
                if (len != sizeof (short))
                        break;
                *(short *) buf = brd->i2cTempData;
                ret = sizeof (short);
                break;

        default:
                break;
        }
        return ret;
}

/*-----------------------Module------------------------------*/
// Stops the A/D and releases reserved memory
void cleanup_module(void)
{
        int ib, i;
        if (!boardInfo)
                return;

        for (ib = 0; ib < numboards; ib++) {
                struct A2DBoard *brd = boardInfo + ib;

                // remove the callback routines
                // (does nothing if it isn't registered)
                unregister_irig_callback(a2dIrigCallback, IRIG_100_HZ,
                                         brd);
                if (brd->tempRate != IRIG_NUM_RATES)
                    unregister_irig_callback(i2cTempIrigCallback,
                                         brd->tempRate, brd);

                A2DStatusAll(brd);      // Read status and clear IRQ's

                // Shut down the setup thread
                if (brd->setup_thread) {
                        rtl_pthread_cancel(brd->setup_thread);
                        rtl_pthread_join(brd->setup_thread, NULL);
                        brd->setup_thread = 0;
                }
                // Shut down the setup thread
                if (brd->reset_thread) {
                        rtl_pthread_cancel(brd->reset_thread);
                        rtl_pthread_join(brd->reset_thread, NULL);
                        brd->reset_thread = 0;
                }
                // Shut down the bottom-half thread
                if (brd->bh_thread) {
                        rtl_pthread_cancel(brd->bh_thread);
                        rtl_pthread_join(brd->bh_thread, NULL);
                        brd->bh_thread = 0;
                }
                rtl_sem_destroy(&brd->bh_sem);
                if (brd->bh_thread_stack)
                        rtl_gpos_free(brd->bh_thread_stack);

                free_dsm_circ_buf(&brd->fifo_samples);
                free_dsm_circ_buf(&brd->a2d_samples);
                free_dsm_circ_buf(&brd->temp_samples);
                rtl_gpos_free(brd->discardSample);
                rtl_gpos_free(brd->discardBuffer);
                if (brd->filters) {
                        for (i = 0; i < brd->nfilters; i++) {
                                if (brd->filters[i].channels)
                                        rtl_gpos_free(brd->filters[i].
                                                      channels);
                                brd->filters[i].channels = 0;
                        }
                        rtl_gpos_free(brd->filters);
                        brd->filters = 0;
                }
                // close and remove A2D fifo
                if (brd->a2dfd >= 0)
                        rtl_close(brd->a2dfd);
                if (brd->a2dFifoName) {
                        rtl_unlink(brd->a2dFifoName);
                        rtl_gpos_free(brd->a2dFifoName);
                }

                if (brd->reset_thread_stack)
                        rtl_gpos_free(brd->reset_thread_stack);

                if (brd->ioctlhandle)
                        closeIoctlFIFO(brd->ioctlhandle);
                brd->ioctlhandle = 0;

                if (brd->addr)
                        release_region(brd->addr, A2DIOWIDTH);
                brd->addr = 0;
        }

        rtl_gpos_free(boardInfo);
        boardInfo = 0;

        DSMLOG_DEBUG("Analog cleanup complete\n");

        return;
}

/*-----------------------Module------------------------------*/

int init_module()
{
        int error = -EINVAL;
        int ib, i;

        boardInfo = 0;

        // DSM_VERSION_STRING is found in dsm_version.h
        DSMLOG_NOTICE("version: %s\n", DSM_VERSION_STRING);

        /* count non-zero ioport addresses, gives us the number of boards */
        for (ib = 0; ib < MAX_A2D_BOARDS; ib++)
                if (ioport[ib] == 0)
                        break;
        numboards = ib;
        if (numboards == 0) {
                DSMLOG_ERR("No boards configured, all ioport[]==0\n");
                goto err;
        }
        DSMLOG_DEBUG("configuring %d board(s)...\n", numboards);

        error = -ENOMEM;
        boardInfo = rtl_gpos_malloc(numboards * sizeof (struct A2DBoard));
        if (!boardInfo)
                goto err;

        DSMLOG_DEBUG("sizeof(struct A2DBoard): 0x%x\n",
                     sizeof (struct A2DBoard));

        /* initialize each A2DBoard structure */
        for (ib = 0; ib < numboards; ib++) {
                struct A2DBoard *brd = boardInfo + ib;

                DSMLOG_DEBUG("initializing board[%d] at 0x%x\n", ib, brd);
                // initialize structure to zero, then initialize things
                // that are non-zero
                memset(brd, 0, sizeof (struct A2DBoard));

                rtl_sem_init(&brd->bh_sem, 0, 0);
                brd->a2dfd = -1;
                // default latency, 1/10 second.
                brd->config.latencyUsecs = USECS_PER_SEC / 10;

#ifdef DO_A2D_STATRD
                brd->FIFOCtl = A2DSTATEBL;
#else
                brd->FIFOCtl = 0;
#endif

                brd->i2c = 0x3;

                brd->invertCounts = invert[ib];
                brd->master = master[ib];
        }

        /* initialize necessary members in each A2DBoard structure */
        for (ib = 0; ib < numboards; ib++) {
                struct A2DBoard *brd = boardInfo + ib;

                error = -EBUSY;
                unsigned int addr = ioport[ib] + SYSTEM_ISA_IOPORT_BASE;
                // Get the mapped board address
                if (check_region(addr, A2DIOWIDTH)) {
                        DSMLOG_ERR("ioports at 0x%x already in use\n",
                                   addr);
                        goto err;
                }

                request_region(addr, A2DIOWIDTH, "A2D_DRIVER");
                brd->addr = addr;
                brd->chan_addr = addr + A2DIOLOAD;

                /* Open up my ioctl FIFOs, register my ioctlCallback function */
                error = -EIO;
                brd->ioctlhandle =
                    openIoctlFIFO(devprefix, ib, ioctlCallback,
                                  nioctlcmds, ioctlcmds);

                if (!brd->ioctlhandle)
                        goto err;

                // Open the A2D fifo to user space
                error = -ENOMEM;
                brd->a2dFifoName =
                    makeDevName(devprefix, "_in_", ib * NDEVICES);
                if (!brd->a2dFifoName)
                        goto err;

                // remove broken device file before making a new one
                if ((rtl_unlink(brd->a2dFifoName) < 0
                     && rtl_errno != RTL_ENOENT)
                    || rtl_mkfifo(brd->a2dFifoName, 0666) < 0) {
                        DSMLOG_ERR("error: unlink/mkfifo %s: %s\n",
                                   brd->a2dFifoName,
                                   rtl_strerror(rtl_errno));
                        error = -convert_rtl_errno(rtl_errno);
                        goto err;
                }

                /* allocate thread stacks at init module time */
                if (!
                    (brd->reset_thread_stack =
                     rtl_gpos_malloc(THREAD_STACK_SIZE)))
                        goto err;
                if (!
                    (brd->bh_thread_stack =
                     rtl_gpos_malloc(THREAD_STACK_SIZE)))
                        goto err;

                /*
                 * Samples from the FIFO. We don't know how many values
                 * we will read from the FIFO yet, so set the sample size
                 * to 0 and realloc when nFifoValues is known.
                 */
                brd->nFifoValues = 0;

                error = alloc_dsm_circ_buf(&brd->a2d_samples,
                                           (NUM_NCAR_A2D_CHANNELS + 1) *
                                           sizeof (short),
                                           A2D_SAMPLE_QUEUE_SIZE);
                if (error)
                        return error;

                error = alloc_dsm_circ_buf(&brd->temp_samples,
                                           2 * sizeof (short),
                                           TEMP_SAMPLE_QUEUE_SIZE);
                if (error)
                        return error;

                error = -ENOMEM;
                brd->discardSample = rtl_gpos_malloc(
                        SIZEOF_DSM_SAMPLE_HEADER +
                       (NUM_NCAR_A2D_CHANNELS + 1) * sizeof (short));
                if (!brd->discardSample) goto err;

                brd->discardBuffer = 0;

        }

        DSMLOG_DEBUG("A2D init_module complete.\n");

        return 0;
      err:

        if (boardInfo) {
                for (ib = 0; ib < numboards; ib++) {
                        struct A2DBoard *brd = boardInfo + ib;

                        free_dsm_circ_buf(&brd->fifo_samples);
                        free_dsm_circ_buf(&brd->a2d_samples);
                        free_dsm_circ_buf(&brd->temp_samples);
                        rtl_gpos_free(brd->discardSample);
                        rtl_gpos_free(brd->discardBuffer);

                        if (brd->filters) {
                                for (i = 0; i < brd->nfilters; i++) {
                                        if (brd->filters[i].channels)
                                                rtl_gpos_free(brd->
                                                              filters[i].
                                                              channels);
                                        brd->filters[i].channels = 0;
                                }
                                rtl_gpos_free(brd->filters);
                                brd->filters = 0;
                        }

                        if (brd->reset_thread_stack)
                                rtl_gpos_free(brd->reset_thread_stack);
                        if (brd->bh_thread_stack)
                                rtl_gpos_free(brd->bh_thread_stack);

                        rtl_sem_destroy(&brd->bh_sem);
                        if (brd->a2dFifoName) {
                                rtl_unlink(brd->a2dFifoName);
                                rtl_gpos_free(brd->a2dFifoName);
                        }

                        if (brd->ioctlhandle)
                                closeIoctlFIFO(brd->ioctlhandle);
                        brd->ioctlhandle = 0;

                        if (brd->addr)
                                release_region(brd->addr, A2DIOWIDTH);
                        brd->addr = 0;
                }

        }

        rtl_gpos_free(boardInfo);
        boardInfo = 0;
        return error;
}
