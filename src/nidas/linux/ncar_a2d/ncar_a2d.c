/*
  ncar_a2d.c

  Linux driver for NCAR/EOL/RAF A/D card, adapted from the RTLinux
  a2d_driver code.

  $LastChangedRevision: 3678 $
  $LastChangedDate: 2007-02-16 14:12:21 -0700 (Fri, 16 Feb 2007) $
  $LastChangedBy: wasinger $
  $HeadURL: http://svn.atd.ucar.edu/svn/nids/trunk/src/nidas/rtlinux/ncar_a2d.c $

  Copyright 2007 UCAR, NCAR, All Rights Reserved
*/

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <asm/io.h>

#include <nidas/linux/irigclock.h>
#include <nidas/linux/isa_bus.h>
#include <nidas/linux/klog.h>
#include <nidas/linux/ncar_a2d.h>

MODULE_AUTHOR("Chris Burghart <burghart@ucar.edu>");
MODULE_DESCRIPTION("NCAR A/D driver");

// Clock/data line bits for i2c interface
#define I2CSCL 0x2
#define I2CSDA 0x1

#define DO_A2D_STATRD
//#define TEMPDEBUG
//#define DO_FTRUNCATE  // Truncate the size of the data FIFO
//#define A2D_ACQ_IN_SEPARATE_THREAD

MODULE_PARM(IoPort, "1-" __MODULE_STRING(MAX_A2D_BOARDS) "i");
MODULE_PARM_DESC(IoPort, "ISA port address of each board, e.g.: 0x3A0");

MODULE_PARM(Invert, "1-" __MODULE_STRING(MAX_A2D_BOARDS) "i");
MODULE_PARM_DESC(Invert, "Whether to invert counts, default=1(true)");

MODULE_PARM(Master, "1-" __MODULE_STRING(MAX_A2D_BOARDS) "i");
MODULE_PARM_DESC(Master, "Sets master A/D for each board, default=7");

/* I/O port addresses of installed boards, 0=no board installed */
static int IoPort[MAX_A2D_BOARDS] = { 0x3A0, 0, 0, 0 };

/* Which A2D chip is the master.*/
static int Master[MAX_A2D_BOARDS] = { 7, 7, 7, 7 };

/*
 * Whether to invert counts. This should be 1(true) for new cards.
 * Early versions of the A2D cards do not need it.
 * This is settable as a module parameter.  We could do
 * it by checking the serial number in firmware, but
 * don't have faith that these serial numbers will be
 * set correctly in the firmware on the cards.
 */
static int Invert[MAX_A2D_BOARDS] = { 1, 1, 1, 1 };

/* number of A2D boards in system (number of non-zero ioport values) */
static int NumBoards = 0;

static struct A2DBoard* BoardInfo = 0;

/* 
 * number of devices on a board. This is the number of
 * /dev/dsma2d* devices, from the user's point of view, that one
 * board represents.  Device 0 is the 8 A2D
 * channels, device 1 is the temperature sensor.
 */
#define NDEVICES 2

/*
 * init and cleanup prototypes
 */
int init_module(void);
void cleanup_module(void);

/****************  End of IOCTL Section ******************/

static struct timespec Usec1 =   { 0, 1000 };
static struct timespec Usec10 =  { 0, 10000 };
static struct timespec Usec20 =  { 0, 20000 };
static struct timespec Usec100 = { 0, 100000 };
static struct timespec Msec2 =  { 0, 2000000 };
static struct timespec Msec10 =  { 0, 10000000 };
static struct timespec Msec20 =  { 0, 20000000 };
static struct timespec Msec30 =  { 0, 30000000 };

static int startA2DResetThread(struct A2DBoard* brd);

/*-----------------------Utility------------------------------*/
// I2C serial bus control utilities

static inline void 
i2c_clock_hi(struct A2DBoard* brd)
{
    brd->i2c |= I2CSCL;  // Set clock bit hi
    outb(A2DIO_STAT, brd->chan_addr);  // Clock high
    outb(brd->i2c, brd->addr);
    udelay(1);
    return;
}

static inline void 
i2c_clock_lo(struct A2DBoard* brd)
{
    brd->i2c &= ~I2CSCL;  // Set clock bit low
    outb(A2DIO_STAT, brd->chan_addr);  // Clock low
    outb(brd->i2c, brd->addr);
    udelay(1);
    return;
}

static inline void 
i2c_data_hi(struct A2DBoard* brd)
{
    brd->i2c |= I2CSDA;  // Set data bit hi
    outb(A2DIO_STAT, brd->chan_addr);  // Data high
    outb(brd->i2c, brd->addr);
    udelay(1);
    return;
}

static inline void 
i2c_data_lo(struct A2DBoard* brd)
{
    brd->i2c &= ~I2CSDA;  // Set data bit lo
    outb(A2DIO_STAT, brd->chan_addr);  // Data high
    outb(brd->i2c, brd->addr);
    udelay(1);
    return;
}

/*-----------------------Utility------------------------------*/
// Read on-board LM92 temperature sensor via i2c serial bus
// The signed short returned is weighted .0625 deg C per bit

static short 
A2DTemp(struct A2DBoard* brd)
{
    // This takes 68 i2c operations to perform.
    // Using a delay of 10 usecs, this should take
    // approximately 680 usecs.

    unsigned char b1;
    unsigned char b2;
    unsigned char t1;
    short x;
    unsigned char i, address = 0x48;  // Address of temperature register


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
    for (i = 0; i < 8; i++)
    {
	// set data line
	if (b1 & 0x80) i2c_data_hi(brd);
	else i2c_data_lo(brd);

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
    for (i = 0; i < 8; i++)
    {
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
    for (i = 0; i < 8; i++)
    {
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

    x = (short)(b1<<8 | b2)>>3;

#ifdef TEMPDEBUG
    KLOG_DEBUG("b1=0x%02X, b2=0x%02X, b1b2>>3 0x%04X, degC = %d.%1d\n",
	       b1, b2, x, x/16, (10*(x%16))/16);
#endif
    return x;
}

//XX static void 
//XX sendTemp(struct A2DBoard* brd)
//XX {
//XX     I2C_TEMP_SAMPLE samp;
//XX     samp.timestamp = GET_MSEC_CLOCK;
//XX     samp.size = sizeof(short);
//XX     samp.data = brd->i2cTempData = A2DTemp(brd);
//XX #ifdef DEBUG
//XX     KLOG_DEBUG("Brd temp %d.%1d degC\n", samp.data/16, (10*(samp.data%16))/16);
//XX #endif
//XX 
//XX     if (brd->i2cTempfd >= 0) {
//XX 	// Write to up-fifo
//XX 	if (sys_write(brd->i2cTempfd, &samp,
//XX 		      SIZEOF_DSM_SAMPLE_HEADER + samp.size) < 0) {
//XX 	    KLOG_ERR("error: write %s: %s, shutting down this fifo\n",
//XX 		     brd->i2cTempFifoName, sys_strerror(sys_errno));
//XX 	    sys_close(brd->i2cTempfd);
//XX 	    brd->i2cTempfd = -1;
//XX 	}
//XX     }
//XX }

/*-----------------------End I2C Utils -----------------------*/


/*-----------------------Utility------------------------------*/
// Read status of A2D chip specified by A2DSel 0-7

static US 
A2DStatus(struct A2DBoard* brd, int A2DSel)
{
    // Point at the A/D status channel
    outb(A2DSTATRD, brd->chan_addr);
    return (inw(brd->addr + A2DSel*2));
}

static void 
A2DStatusAll(struct A2DBoard* brd)
{
    int i;
    for(i = 0; i < MAXA2DS; i++)
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

static int 
A2DSetGain(struct A2DBoard* brd, int a2dSel)
{
    A2D_SET *a2d;
    int a2dGain;
    int a2dGainMul;
    int a2dGainDiv;
    US gainCode;

    // If no A/D selected return error -1
    if(a2dSel < 0 || a2dSel >= MAXA2DS) return -EINVAL;

    a2d = &brd->config;
    a2dGain = a2d->gain[a2dSel];
    a2dGainMul = a2d->gainMul[a2dSel];
    a2dGainDiv = a2d->gainDiv[a2dSel];
    gainCode = 0;

    // unused channel gains are set to zero in the configuration
    if (a2dGain != 0)
	gainCode = (US)(a2dGainMul*a2dGain/a2dGainDiv);

    //   KLOG_DEBUG("a2dSel = %d   a2dGain = %d   "
    //                "a2dGainMul = %d   a2dGainDiv = %d   gainCode = %d\n",
    //                a2dSel, a2dGain, a2dGainMul, a2dGainDiv, gainCode);

    // The new 12-bit DAC has lower input resistance (7K ohms as opposed
    // to 10K ohms for the 8-bit DAC). The gain is the ratio of the amplifier
    // feedback resistor to the input resistance. Therefore, for the same
    // gain codes, the new board will yield higher gains by a factor of
    // approximately 1.43. I believe you will want to divide the old gain
    // codes (0-255) by 1.43. The new gain code will be between
    // 0 - 4095. So, after you divide the old gain code by 1.43 you
    // will want to multiply by 16. This is the same as multiplying
    // by 16/1.43 = 11.2.
    // gainCode = (gainCode << 4) & 0xfff0;
    //                               input 5Hz sine and/or saw
    //           GC                  input:   at A2D:         raw counts:    on AEROS:      

    gainCode = 0x0800 + a2dSel;  //  +/- 1v   1.84 to 2.14v      -83  1359   -0.02 to 0.42v
    gainCode = 0x1000 + a2dSel;  //  +/- 1v   1.75 to 2.24v    -1595  2893   -0.48 to 0.88v
    gainCode = 0x1950 + a2dSel;  //  +/- 1v   1.84 to 2.24v    -1585  2887   -0.48 to 0.88v
    gainCode = 0x2000 + a2dSel;  //  +/- 1v   1.84 to 2.14v      -81  1360   -0.02 to 0.42v
    gainCode = 0x1950 + a2dSel;  //  +/- 1v   1.74 to 2.24v    -1637  2916   -0.50 to 0.90v
    gainCode = 0x0800 + a2dSel;  //  +/- 1v   1.74 to 2.24v    -1637  2919   -0.50 to 0.90v
    gainCode = 0x2000 + a2dSel;  //  +/- 1v   1.74 to 2.26v    -2241  3510   -0.68 to 1.08v
    gainCode = 0x1000 + a2dSel;  //  +/- 1v   1.64 to 2.30v    -2241  3513   -0.68 to 1.08v at taken w/ square wave
    gainCode = 0x1950 + a2dSel;  //  +/- 1v   1.74 to 2.28v    -1636  2914   -0.50 to 0.90v at taken w/ square wave
    // power cyle
    gainCode = 0x1950 + a2dSel;  //  +/- 1v   0.44 to 3.48v   -21892 23643   -6.75 to 7.25v at taken w/ square wave
    // reboot
    gainCode = 0x4100 + a2dSel;  //  +/- 5v  30129        -27380
    gainCode = 0x4200 + a2dSel;  //  +/- 5v  30147        -27390
    gainCode = 0x4400 + a2dSel;  //  +/- 5v  31505        -28434
    gainCode = 0x4800 + a2dSel;  //  +/- 5v  31536        -28451
    gainCode = 0x4840 + a2dSel;  //  +/- 5v  32729        -32768
    gainCode = 0x4800 + a2dSel;  //  +/- 5v  32765        -32768
    // reboot
    gainCode = 0x4400 + a2dSel;  //  +/- 5v  32765        -32768
    // reboot
    gainCode = 0x2410 + a2dSel;  //  +/-10v  31516        -28439

    // KLOG_DEBUG("gainCode: 0x%04x\n", gainCode);

    // if(a2dSel != 0) gainCode = 0x4790 + a2dSel;
    gainCode = 0x1900 + a2dSel;  //  +/-10v  31516        -28439

    if (a2d->offset[a2dSel]) {
	if (a2d->gain[a2dSel] == 10)
	    gainCode = 0x1000 + a2dSel;  //   0 to +20  ???
	else if (a2d->gain[a2dSel] == 20)
	    gainCode = 0x4000 + a2dSel;  //   0 to +10
	else if (a2d->gain[a2dSel] == 40)
	    gainCode = 0x8000 + a2dSel;  //   0 to +5
	else
	    gainCode = 0x0000 + a2dSel;
    } else {
	if (a2d->gain[a2dSel] == 10)
	    gainCode = 0x1900 + a2dSel;  // -10 to +10
	else
	    gainCode = 0x0000 + a2dSel;
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
    // 1.  Write (or set) D2A0. This is accomplished by writing to the A/D
    // with the lower four address bits (SA0-SA3) set to all "ones" and the
    // data bus to 0x03.
    //   KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_D2A0, brd->chan_addr);
    outb(A2DIO_D2A0, brd->chan_addr);
    mdelay(10);
    // 2. Then write to the A/D card with lower address bits set to "zeros"
    // and data bus set to the gain value for the specific channel with the
    // upper data three bits equal to the channel address. The lower 12
    // bits are the gain code and data bit 12 is equal zero. So for channel
    // 0 write: (xxxxxxxxxxxx0000) where the x's are the gain code.
    KLOG_DEBUG("chn: %d   offset: %d   gain: %2d   outb( 0x%x, 0x%x)\n", 
	       a2dSel, a2d->offset[a2dSel], a2d->gain[a2dSel], gainCode, 
	       brd->addr);
    // KLOG_DEBUG("outb( 0x%x, 0x%x);\n", gainCode, brd->addr);
    outw(gainCode, brd->addr);
    mdelay(10);
    return 0;
}

/*-----------------------Utility------------------------------*/
// A2DSetMaster routes the interrupt signal from the target A/D chip
// to the ISA bus interrupt line.

static int 
A2DSetMaster(struct A2DBoard* brd, int a2dSel)
{
    if(a2dSel < 0 || a2dSel >= MAXA2DS) {
	KLOG_ERR("A2DSetMaster, bad chip number: %d\n", a2dSel);
	return -EINVAL;
    }

    KLOG_DEBUG("A2DSetMaster, Master=%d\n", a2dSel);

    // Point at the FIFO status channel
    outb(A2DIO_FIFOSTAT, brd->chan_addr);

    // Write the master to register
    outb((char)a2dSel, brd->addr);
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
static int 
A2DSetVcal(struct A2DBoard* brd)
{
    // Check that V is within limits
    int ret = -EINVAL;
    int i, valid[] = {0x01, 0x02, 0x04, 0x08, 0x10};
    for (i=0; i<5; i++)
	if (brd->cal.vcalx8 == valid[i]) ret = 0;
    if (ret) return ret;

    // Point to the calibration DAC channel
    outb(A2DIO_D2A2, brd->chan_addr);
    KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_D2A2, brd->chan_addr);

    // Write cal voltage code
    outw(brd->cal.vcalx8, brd->addr);
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
static void 
A2DSetCal(struct A2DBoard* brd)
{
    US OffChans = 0;
    US CalChans = 0;
    int i;

    // Change the calset array of bools into a byte
    for(i = 0; i < MAXA2DS; i++)
    {
	OffChans >>= 1;
	CalChans >>= 1;
	if(brd->config.offset[i] != 0) OffChans += 0x80;
	if(brd->cal.calset[i] != 0)    CalChans += 0x80;
    }
    // Point at the system control input channel
    outb(A2DIO_SYSCTL, brd->chan_addr);
    KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_SYSCTL, brd->chan_addr);

    // Set the appropriate bits in OffCal
    brd->OffCal = (OffChans<<8) & 0xFF00;
    brd->OffCal |= CalChans;
    brd->OffCal = ~(brd->OffCal) & 0xFFFF; // invert bits

    // Send OffCal word to system control word
    outw(brd->OffCal, brd->addr);
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
static void 
A2DSetOffset(struct A2DBoard* brd)
{
    US OffChans = 0;
    int i;

    // Change the offset array of bools into a byte
    for(i = 0; i < MAXA2DS; i++)
    {
	OffChans >>= 1;
	if(brd->config.offset[i] != 0) OffChans += 0x80;
    }
    // Point at the system control input channel
    outb(A2DIO_SYSCTL, brd->chan_addr);
    KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_SYSCTL, brd->chan_addr);

    // Set the appropriate bits in OffCal
    brd->OffCal = (OffChans<<8) & 0xFF00;
    brd->OffCal = ~(brd->OffCal) & 0xFFFF; // invert bits

    // Send OffCal word to system control word
    outw(brd->OffCal, brd->addr);
    KLOG_DEBUG("JDW brd->OffCal:  0x%04x\n", brd->OffCal);
}

/*-----------------------Utility------------------------------*/
// Set A2D SYNC flip/flop.  This stops the A/D's until cleared
// under program control or by a positive 1PPS transition
// 1PPS enable must be asserted in order to sync on 1PPS

static void 
A2DSetSYNC(struct A2DBoard* brd)
{
    outb(A2DIO_FIFO, brd->chan_addr);

    brd->FIFOCtl |= A2DSYNC;  // Ensure that SYNC bit in FIFOCtl is set.

    // Cycle the sync clock while keeping SYNC bit high
    outb(brd->FIFOCtl,              brd->addr);
    outb(brd->FIFOCtl | A2DSYNCCK,  brd->addr);
    outb(brd->FIFOCtl,              brd->addr);
    return;
}

/*-----------------------Utility------------------------------*/
// Clear the SYNC flag

static void 
A2DClearSYNC(struct A2DBoard* brd)
{
    outb(A2DIO_FIFO, brd->chan_addr);

    brd->FIFOCtl &= ~A2DSYNC;  // Ensure that SYNC bit in FIFOCtl is cleared.

    // Cycle the sync clock while keeping sync lowthe SYNC data line
    outb(brd->FIFOCtl,              brd->addr);
    outb(brd->FIFOCtl | A2DSYNCCK,  brd->addr);
    outb(brd->FIFOCtl,              brd->addr);
    return;
}

/*-----------------------Utility------------------------------*/
// Enable 1PPS sync

static void 
A2D1PPSEnable(struct A2DBoard* brd)
{
    // Point at the FIFO control byte
    outb(A2DIO_FIFO, brd->chan_addr);

    // Set the 1PPS enable bit
    outb(brd->FIFOCtl | A2D1PPSEBL, brd->addr);

    return;
}

/*-----------------------Utility------------------------------*/
// Disable 1PPS sync

static void 
A2D1PPSDisable(struct A2DBoard* brd)
{
    // Point to FIFO control byte
    outb(A2DIO_FIFO, brd->chan_addr);

    // Clear the 1PPS enable bit
    outb(brd->FIFOCtl & ~A2D1PPSEBL, brd->addr);

    return;
}

/*-----------------------Utility------------------------------*/
// Clear (reset) the data FIFO

static void 
A2DClearFIFO(struct A2DBoard* brd)
{
    // Point to FIFO control byte
    outb(A2DIO_FIFO, brd->chan_addr);

    brd->FIFOCtl &= ~FIFOCLR;  // Ensure that FIFOCLR bit is not set in FIFOCtl

    outb(brd->FIFOCtl,           brd->addr);
    outb(brd->FIFOCtl | FIFOCLR, brd->addr);  // Cycle FCW bit 0 to clear FIFO
    outb(brd->FIFOCtl,           brd->addr);

    return;
}

/*-----------------------Utility------------------------------*/
// A2DFIFOEmpty checks the FIFO empty status bit and returns
// 1 if empty, 0 if not empty

static inline int 
A2DFIFOEmpty(struct A2DBoard* brd)
{
    // Point at the FIFO status channel
    outb(A2DIO_FIFOSTAT, brd->chan_addr);
    return (inw(brd->addr) & FIFONOTEMPTY) == 0;
}

// Read FIFO till empty, discarding data
// return number of bad status values found
static int 
A2DEmptyFIFO( struct A2DBoard* brd)
{
    int nbad = 0;
    int i;
    while(!A2DFIFOEmpty(brd)) {
	// Point to FIFO read subchannel
	outb(A2DIO_FIFO, brd->chan_addr);
	for (i = 0; i < MAXA2DS; i++) {
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
static inline int 
getA2DFIFOLevel(struct A2DBoard* brd)
{
    unsigned short stat;
    outb(A2DIO_FIFOSTAT, brd->chan_addr);
    stat = inw(brd->addr);

    // If FIFONOTFULL is 0, fifo IS full
    if((stat & FIFONOTFULL) == 0) return 5;

    // If FIFONOTEMPTY is 0, fifo IS empty
    else if((stat & FIFONOTEMPTY) == 0) return 0;

    // Figure out which 1/4 of the 1024 FIFO words we're filled to

    // bit 0, 0x1, half full
    // bit 1, 0x2, either almost full (>=3/4) or almost empty (<=1/4).
    switch(stat&0x03) // Switch on stat's 2 LSB's
    {
      case 3:   // almost full/empty, half full (>=3/4 to <4/4)
	return 4;
      case 2:   // almost full/empty, not half full (empty to <=1/4)
	return 1;
      case 1:   // not almost full/empty, half full (>=2/4 to <3/4)
	return 3;
      case 0:   // not almost full/empty, not half full (>1/4 to <2/4)
      default:  // can't happen, but avoid compiler warn
	return 2;
	break;
    }
    return 1;    // can't happen, but avoid compiler warn
}

/*-----------------------Utility------------------------------*/
// This routine sends the ABORT command to all A/D's.
// The ABORT command amounts to a soft reset--they
//  stay configured.

static void 
A2DReset(struct A2DBoard* brd, int a2dSel)
{
    // Point to the A2D command register
    outb(A2DCMNDWR, brd->chan_addr);

    // Send specified A/D the abort (soft reset) command
    outw(A2DABORT, brd->addr + a2dSel*2);
    return;
}

static void 
A2DResetAll(struct A2DBoard* brd)
{
    int i;
    for(i = 0; i < MAXA2DS; i++) A2DReset(brd, i);
}

/*-----------------------Utility------------------------------*/
// This routine sets the A/D's to auto mode

static void 
A2DAuto(struct A2DBoard* brd)
{
    // Point to the FIFO Control word
    outb(A2DIO_FIFO, brd->chan_addr);

    // Set Auto run bit and send to FIFO control byte
    brd->FIFOCtl |=  A2DAUTO;
    outb(brd->FIFOCtl, brd->addr);
    return;
}

/*-----------------------Utility------------------------------*/
// This routine sets the A/D's to non-auto mode

static void 
A2DNotAuto(struct A2DBoard* brd)
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

static void 
A2DStart(struct A2DBoard* brd, int a2dSel)
{
    // Point at the A/D command channel
    outb(A2DCMNDWR, brd->chan_addr);

    // Start the selected A/D
    outw(A2DREADDATA, brd->addr + a2dSel*2);
    return;
}

static void 
A2DStartAll(struct A2DBoard* brd)
{
    int i;
    for(i = 0; i < MAXA2DS; i++)
	A2DStart(brd, i);
}

/*-----------------------Utility------------------------------*/
// Configure A/D A2dSel with coefficient array 'filter'
static int 
A2DConfig(struct A2DBoard* brd, int a2dSel)
{
    int j, ctr = 0;
    US stat;
    UC intmask=1, intbits[8] = {1, 2, 4, 8, 16, 32, 64, 128};
    int tomsgctr = 0;
    int crcmsgctr = 0;

    if(a2dSel < 0 || a2dSel >= MAXA2DS) return -EINVAL;

    // Point to the A/D write configuration channel
    outb(A2DCMNDWR, brd->chan_addr);

    // Set the interrupt mask
    intmask = intbits[a2dSel];

    // Set configuration write mode
    outw(A2DWRCONFIG, brd->addr + a2dSel*2);

    for(j = 0; j < CONFBLLEN*CONFBLOCKS + 1; j++)
    {
	// Set channel pointer to Config write and
	//   write out configuration word
	outb(A2DCONFWR, brd->chan_addr);
	outw(brd->config.filter[j], brd->addr + a2dSel*2);
	mdelay(30);

	// Set channel pointer to sysctl to read int lines
	// Wait for interrupt bit to set

	outb(A2DIO_SYSCTL, brd->chan_addr);
	while((inb(brd->addr) & intmask) == 0)
	{
	    mdelay(30);
	    if(ctr++ > 10000)
	    {
		tomsgctr++;
		//          KLOG_WARNING("INTERRUPT TIMEOUT! chip = %1d\n", a2dSel);
		//          return -ETIMEDOUT;
		break;
	    }
	}
	// Read status word from target a/d to clear interrupt
	outb(A2DSTATRD, brd->chan_addr);
	stat = inw(brd->addr + a2dSel*2);

	// Check status bits for errors
	if(stat & A2DCRCERR)
	{
	    crcmsgctr++;
	    brd->cur_status.badval[a2dSel] = stat;  // Error status word
	    //       KLOG_WARNING("CRC ERROR! chip = %1d, stat = 0x%04X\n", a2dSel, stat);
	    //       return -EIO;
	}
    }
    if (crcmsgctr > 0)
	KLOG_ERR("%3d CRC Errors chip = %1d\n",
		 crcmsgctr, a2dSel);
    else
	KLOG_DEBUG("%3d CRC Errors chip = %1d\n",
		   crcmsgctr, a2dSel);

    if (tomsgctr > 0)
	KLOG_ERR("%3d Interrupt Timeout Errors chip = %1d\n",
		 tomsgctr, a2dSel);
    else
	KLOG_DEBUG("%3d Interrupt Timeout Errors chip = %1d\n",
		   tomsgctr, a2dSel);

    brd->cur_status.goodval[a2dSel] = stat;  // Final status word after load
    mdelay(2);
    return 0;
}

/*-----------------------Utility------------------------------*/
// Configure all A/D's with same filter

static int 
A2DConfigAll(struct A2DBoard* brd)
{
    int ret;
    int i;
    for(i = 0; i < MAXA2DS; i++)
	if ((ret = A2DConfig(brd, i)) < 0) return ret;
    return 0;
}

// the status bits are in the upper byte contain the serial number
static int 
getSerialNumber(struct A2DBoard* brd)
{
    unsigned short stat;
    // fetch serial number
    outb(A2DIO_FIFOSTAT, brd->chan_addr);
    stat = inw(brd->addr);
    KLOG_DEBUG("brd->chan_addr: %x  stat: %x\n", brd->chan_addr, stat);
    return (stat & 0xFFC0)>>6;  // S/N is upper 10 bits
}


/*-----------------------Utility------------------------------*/
// Utility function to wait for INV1PPS to be zero.
// Return: negative errno, or 0=OK.

static int 
waitFor1PPS(struct A2DBoard* brd)
{
    unsigned short stat;
    int timeit = 0;
    // Point at the FIFO status channel
    outb(A2DIO_FIFOSTAT, brd->chan_addr);
    while(timeit++ < 240000)
    {
	if (brd->interrupted) return -EINTR;
	// Read status, check INV1PPS bit
	stat = inw(brd->addr);
	if((stat & INV1PPS) == 0) return 0;
	udelay(50);  // Wait 50 usecs and try again
    }
    KLOG_ERR("1PPS not detected--no sync to GPS\n");
    return -ETIMEDOUT;
}

static int A2DSetup(struct A2DBoard* brd)
{
    A2D_SET *a2d = &brd->config;
    int i;
    int ret;
    int repeat;

#ifdef DO_A2D_STATRD
    brd->FIFOCtl = A2DSTATEBL;   // Clear most of FIFO Control Word
#else
    brd->FIFOCtl = 0;            // Clear FIFO Control Word
#endif

    brd->OffCal = 0x0;

    for(repeat = 0; repeat < 3; repeat++) { // HACK! the CPLD logic needs to be fixed!
	for(i = 0; i < MAXA2DS; i++)
	{
	    if ((ret = A2DSetGain(brd, i)) < 0) return ret;
	    // Find maximum rate
	    if(a2d->Hz[i] > brd->MaxHz) brd->MaxHz = a2d->Hz[i];  
//	    KLOG_DEBUG("brd->MaxHz = %d   a2d->Hz[%d] = %d\n", 
//		       brd->MaxHz, i, a2d->Hz[i]);
	    brd->requested[i] = (a2d->Hz[i] > 0);
	}
	outb(A2DIO_D2A1, brd->chan_addr);
	mdelay(10);
	outb(A2DIO_D2A2, brd->chan_addr);
	mdelay(10);
	outb(A2DIO_D2A1, brd->chan_addr);
	mdelay(10);
    } // END HACK!
    brd->cur_status.ser_num = getSerialNumber(brd);
    // KLOG_DEBUG("A2D serial number = %d\n", brd->cur_status.ser_num);

    if ((ret = A2DSetMaster(brd, brd->master)) < 0) return ret;

    A2DSetOffset(brd);

    KLOG_DEBUG("success!\n");
    return 0;
}


/*--------------------- Thread function ----------------------*/
// A2DThread loads the A2Ds with filter data from A2D structure.
//
// NOTE: This must be called from within a real-time thread
//                      Otherwise the critical delays will not work properly

static void* 
A2DSetupThread(void *thread_arg)
{
    struct A2DBoard* brd = (struct A2DBoard*) thread_arg;
    int ret = 0;

    // while (1) { // DEBUG continuous DAC setting...
    // Configure DAC gain codes
    if ((ret = A2DSetup(brd)) < 0) return (void*)-ret;
    // KLOG_DEBUG("DAC settings - loop forever... ret = %d\n", ret);
    // mdelay(1000);
    // }
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
    mdelay(50);  // Let them run a few milliseconds (50)

    // Then do a soft reset
    KLOG_DEBUG("Soft resetting A/D's\n");
    A2DResetAll(brd);

    // Configure the A/D's
    // while (1) { // DEBUG continuous filter setting...
    KLOG_DEBUG("Sending filter config data to A/Ds\n");
    if ((ret = A2DConfigAll(brd)) < 0) return (void*)-ret;
    // KLOG_DEBUG("filter settings - loop forever... ret = %d\n", ret);
    // mdelay(1000);
    // }
    // Reset the A/D's
    KLOG_DEBUG("Resetting A/Ds\n");
    A2DResetAll(brd);

    mdelay(1);  // Give A/D's a chance to load
    KLOG_DEBUG("A/Ds ready for synchronous start\n");

    return (void*)ret;
}

#if 0
#define nlast 3
static unsigned short last_stat[nlast][8];
static unsigned short last_cnts[nlast][8];
#endif

/*-----------------------Utility------------------------------*/
// return:  negative: negative errno of write to RTL FIFO
//          positive: number of bad status values in A2D data

static inline int 
getA2DSample(struct A2DBoard* brd)
{
    int flevel;
    A2DSAMPLE samp;
    register SS* dataptr;
    int nbad;
    int iread;
    
    if (!brd->enableReads) return 0;

    // make sure only one getA2DSample function is running
    if (brd->readActive) return 0;
    brd->readActive = 1;

    flevel = getA2DFIFOLevel(brd);

    if (brd->discardNextScan) {
	if (flevel > 0) A2DEmptyFIFO(brd);
	brd->discardNextScan = 0;
	brd->readActive = 0;
	return 0;
    }

    samp.timestamp = GET_MSEC_CLOCK;
    // adjust time tag to time of first sub-sample
    if (samp.timestamp < brd->ttMsecAdj) samp.timestamp += MSECS_PER_DAY;
    samp.timestamp -= brd->ttMsecAdj;

    brd->cur_status.preFifoLevel[flevel]++;
    if (flevel != brd->expectedFifoLevel) {
	if (!(brd->nbadFifoLevel++ % 1000)) {
	    KLOG_ERR("clock=%ld, pre-read fifo level=%d is not expected "
		     "value=%d (%d times)\n", GET_MSEC_CLOCK, flevel, 
		     brd->expectedFifoLevel, brd->nbadFifoLevel);
	    if (flevel == 5)
		KLOG_ERR("Is the external clock plugged into the A/D?\n");
	    if (brd->nbadFifoLevel > 1) {
		brd->readActive = 0;
		startA2DResetThread(brd);
		return 0;
	    }
	}
	if (flevel == 0) {
	    brd->readActive = 0;
	    return 0;
	}
    }

    // dataptr points to beginning of data section of A2DSAMPLE
    dataptr = samp.data;

    // sys_irqstate_t irqstate;     // JDW
    // sys_no_interrupts(irqstate); // JDW

    outb(A2DIO_FIFO, brd->chan_addr);

    nbad = 0;
    for (iread = 0; iread < brd->nreads; iread++) {
	int ichan = iread % MAXA2DS;
	signed short counts;
#ifdef DO_A2D_STATRD
	unsigned short stat = inw(brd->addr);

	// Inverted bits for later cards
	if (brd->invertCounts) counts = -inw(brd->addr);
	else counts = inw(brd->addr);
#if 0
	// DEBUG keep a history of the last few reads
	int ilast;
	for (ilast=0; ilast<nlast-1; ilast++) {
	    last_stat[ilast][ichan] = last_stat[ilast+1][ichan];
	    last_cnts[ilast][ichan] = last_cnts[ilast+1][ichan];
	}
	last_stat[ilast][ichan] = (unsigned short)stat;
	last_cnts[ilast][ichan] = (unsigned short)counts;
#endif
	// check for acceptable looking status value
	if ((stat & A2DSTATMASK) != A2DEXPSTATUS) {
#if 0
	    KLOG_DEBUG("--------- SPIKE! --------- %d of %d\n", iread, brd->nreads);
	    for (ilast=0; ilast<nlast; ilast++)
		KLOG_DEBUG("%4x %4x  %4x %4x  %4x %4x  %4x %4x  %4x %4x  %4x %4x  %4x %4x  %4x %4x\n",
			   last_stat[ilast][0], last_cnts[ilast][0],
			   last_stat[ilast][1], last_cnts[ilast][1],
			   last_stat[ilast][2], last_cnts[ilast][2],
			   last_stat[ilast][3], last_cnts[ilast][3],
			   last_stat[ilast][4], last_cnts[ilast][4],
			   last_stat[ilast][5], last_cnts[ilast][5],
			   last_stat[ilast][6], last_cnts[ilast][6],
			   last_stat[ilast][7], last_cnts[ilast][7]);
#endif
	    KLOG_DEBUG("--------- SPIKE! --------- read: %2d  chn: %d  stat: %x  data: %x\n", iread, ichan, stat, counts);
	    nbad++;
	    brd->cur_status.nbad[ichan]++;
	    brd->cur_status.badval[ichan] = stat;
	    //       counts = -32768;  // set to missing value
	}
	else brd->cur_status.goodval[ichan] = stat;
#else
	// Inverted bits for later cards
	if (brd->invertCounts) counts = -inw(brd->addr);
	else counts = inw(brd->addr);

#endif
	//    if (counts > 0x1000) KLOG_DEBUG("Greater than 0x1000\n");
	if (brd->requested[ichan]) *dataptr++ = counts;
    }
    flevel = getA2DFIFOLevel(brd);
    brd->cur_status.postFifoLevel[flevel]++;

    // sys_restore_interrupts(irqstate);  // JDW

    if (flevel > 0) {
	if (!(brd->fifoNotEmpty++ % 1000))
	    KLOG_WARNING("post-read fifo level=%d (not empty): %d times.\n",
			 flevel, brd->fifoNotEmpty);

	if (flevel > brd->expectedFifoLevel || brd->fifoNotEmpty > 1) {
	    brd->readActive = 0;
	    startA2DResetThread(brd);
	    return nbad;
	}
    }
    if (nbad > 0) brd->nbadScans++;

    // DSMSensor::printStatus queries these values every 10 seconds
    if (!(++brd->readCtr % (INTRP_RATE * 10))) {

	// debug print every minute, or if there are bad scans
	if (!(brd->readCtr % (INTRP_RATE * 60)) || brd->nbadScans) {
	    KLOG_DEBUG("GET_MSEC_CLOCK=%d, nbadScans=%d\n",
		       GET_MSEC_CLOCK, brd->nbadScans);
	    KLOG_DEBUG("nbadFifoLevel=%d, #fifoNotEmpty=%d, #skipped=%d, #resets=%d\n",
		       brd->nbadFifoLevel, brd->fifoNotEmpty, brd->skippedSamples, brd->resets);
	    KLOG_DEBUG("pre-scan  fifo=%d,%d,%d,%d,%d,%d (0,<=1/4,<2/4,<3/4,<4/4,full)\n",
		       brd->cur_status.preFifoLevel[0],
		       brd->cur_status.preFifoLevel[1],
		       brd->cur_status.preFifoLevel[2],
		       brd->cur_status.preFifoLevel[3],
		       brd->cur_status.preFifoLevel[4],
		       brd->cur_status.preFifoLevel[5]);
	    KLOG_DEBUG("post-scan fifo=%d,%d,%d,%d,%d,%d\n",
		       brd->cur_status.postFifoLevel[0],
		       brd->cur_status.postFifoLevel[1],
		       brd->cur_status.postFifoLevel[2],
		       brd->cur_status.postFifoLevel[3],
		       brd->cur_status.postFifoLevel[4],
		       brd->cur_status.postFifoLevel[5]);
	    KLOG_DEBUG("last good status= %04x %04x %04x %04x %04x %04x %04x %04x\n",
		       brd->cur_status.goodval[0],
		       brd->cur_status.goodval[1],
		       brd->cur_status.goodval[2],
		       brd->cur_status.goodval[3],
		       brd->cur_status.goodval[4],
		       brd->cur_status.goodval[5],
		       brd->cur_status.goodval[6],
		       brd->cur_status.goodval[7]);

	    if (brd->nbadScans > 0) {
		KLOG_DEBUG("last bad status=  %04x %04x %04x %04x %04x %04x %04x %04x\n",
			   brd->cur_status.badval[0],
			   brd->cur_status.badval[1],
			   brd->cur_status.badval[2],
			   brd->cur_status.badval[3],
			   brd->cur_status.badval[4],
			   brd->cur_status.badval[5],
			   brd->cur_status.badval[6],
			   brd->cur_status.badval[7]);
		KLOG_DEBUG("num  bad status=  %4d %4d %4d %4d %4d %4d %4d %4d\n",
			   brd->cur_status.nbad[0],
			   brd->cur_status.nbad[1],
			   brd->cur_status.nbad[2],
			   brd->cur_status.nbad[3],
			   brd->cur_status.nbad[4],
			   brd->cur_status.nbad[5],
			   brd->cur_status.nbad[6],
			   brd->cur_status.nbad[7]);

		if (brd->nbadScans > 10) {
		    brd->readActive = 0;
		    startA2DResetThread(brd);
		    return 0;
		}
	    }
	    brd->readCtr = 0;
	} // debug printout

	brd->nbadScans = 0;

	// copy current status to prev_status for access by ioctl A2D_GET_STATUS
	brd->cur_status.nbadFifoLevel = brd->nbadFifoLevel;
	brd->cur_status.fifoNotEmpty = brd->fifoNotEmpty;
	brd->cur_status.skippedSamples = brd->skippedSamples;
	brd->cur_status.resets = brd->resets;
	memcpy(&brd->prev_status, &brd->cur_status, sizeof(A2D_STATUS));
	memset(&brd->cur_status, 0, sizeof(A2D_STATUS));
    }

    samp.size = (char*)dataptr - (char*)samp.data;

    if (brd->a2dfd >= 0 && samp.size > 0) {

	size_t slen = SIZEOF_DSM_SAMPLE_HEADER + samp.size;

	// check if buffer full, or latency time has elapsed.
	if (brd->head + slen > sizeof(brd->buffer) ||
	    !(++brd->sampleCnt % brd->latencyCnt)) {

	    // Write to up-fifo
	    ssize_t wlen;
//XX	    if ((wlen = sys_write(brd->a2dfd, brd->buffer+brd->tail, 
//XX				  brd->head - brd->tail)) < 0) {
//XX		int ierr = sys_errno;
//XX		KLOG_ERR("error: write of %d bytes to %s: %s. Closing\n",
//XX			 brd->head-brd->tail, brd->a2dFifoName, 
//XX			 sys_strerror(sys_errno));
//XX		sys_close(brd->a2dfd);
//XX		brd->a2dfd = -1;
//XX		brd->readActive = 0;
//XX		return -convert_sys_errno(ierr);
//XX	    }
	    if (wlen != brd->head-brd->tail)
		KLOG_WARNING("warning: short write: request=%d, actual=%d\n",
			     brd->head-brd->tail, wlen);
	    brd->tail += wlen;
	    if (brd->tail == brd->head) brd->head = brd->tail = 0;
	    brd->sampleCnt = 0;
	}
	if (brd->head + slen <= sizeof(brd->buffer)) {
	    memcpy(brd->buffer+brd->head, &samp, slen);
	    brd->head += slen;
	}
	else if (!(brd->skippedSamples++ % 1000))
	    KLOG_WARNING("warning: %d samples lost due to backlog in %s\n",
			 brd->skippedSamples, brd->a2dFifoName);
    }
#ifdef A2D_ACQ_IN_SEPARATE_THREAD
    if (brd->doTemp) {
//XX 	sendTemp(brd);
	brd->doTemp = 0;
    }
#endif

#ifdef TIME_CHECK
    if (GET_MSEC_CLOCK != samp.timestamp)
	KLOG_WARNING("excessive time in data-acq loop: start=%d,end=%d\n",
		     samp.timestamp, GET_MSEC_CLOCK);
#endif

    brd->readActive = 0;
    return nbad;
}


// function is scheduled to be called from IRIG driver at 100Hz
static void 
a2dIrigCallback(void *ptr)
{
    struct A2DBoard* brd = (struct A2DBoard*) ptr;
#ifdef A2D_ACQ_IN_SEPARATE_THREAD
    // If the data acquisition is done in another thread
    // simply post the semaphore so the A2DGetDataThread
    // can do its thing
    sys_sem_post(&((struct A2DBoard*)brd)->acq_sem);
#else
    // otherwise, get and send the sample
    // udelay(10);
    getA2DSample((struct A2DBoard*)brd);
#endif
}

static void i2cTempIrigCallback(void *ptr);

//XX static int 
//XX openI2CTemp(struct A2DBoard* brd, int rate)
//XX {
//XX     // limit rate to something reasonable
//XX     if (rate > IRIG_10_HZ) {
//XX 	KLOG_ERR("Illegal rate for I2C temperature probe. Exceeds 10Hz\n");
//XX 	return -EINVAL;
//XX     }
//XX     brd->i2c = 0x3;
//XX 
//XX     if (brd->i2cTempfd >= 0) sys_close(brd->i2cTempfd);
//XX     if((brd->i2cTempfd = sys_open(brd->i2cTempFifoName,
//XX 				  SYS_O_NONBLOCK | SYS_O_WRONLY)) < 0)
//XX     {
//XX 	KLOG_ERR("error: opening %s: %s\n",
//XX 		 brd->i2cTempFifoName, sys_strerror(sys_errno));
//XX 	return -convert_sys_errno(sys_errno);
//XX     }
//XX 
//XX #ifdef DO_FTRUNCATE
//XX     int fifosize = sizeof(I2C_TEMP_SAMPLE)*10;
//XX     if (fifosize < 512) fifosize = 512;
//XX     if (sys_ftruncate(brd->i2cTempfd, fifosize) < 0) {
//XX 	KLOG_ERR("error: ftruncate %s: size=%d: %s\n",
//XX 		 brd->i2cTempFifoName, sizeof(I2C_TEMP_SAMPLE),
//XX 		 sys_strerror(sys_errno));
//XX 	return -convert_sys_errno(sys_errno);
//XX     }
//XX #endif
//XX     brd->i2cTempRate = rate;
//XX     register_irig_callback(&i2cTempIrigCallback, rate, brd);
//XX 
//XX     return 0;
//XX }

//XX static int 
//XX closeI2CTemp(struct A2DBoard* brd)
//XX {
//XX     unregister_irig_callback(&i2cTempIrigCallback, brd->i2cTempRate, brd);
//XX 
//XX     brd->doTemp = 0;
//XX     int fd = brd->i2cTempfd;
//XX     brd->i2cTempfd = -1;
//XX     if (fd >= 0) sys_close(fd);
//XX     return 0;
//XX }

// Callback function to send I2C temperature data to user space.

static void 
i2cTempIrigCallback(void *ptr)
{
    struct A2DBoard* brd = (struct A2DBoard*)ptr;
#ifdef A2D_ACQ_IN_SEPARATE_THREAD
    brd->doTemp = 1;
#else
//XX     sendTemp(brd);
#endif
}


#ifdef A2D_ACQ_IN_SEPARATE_THREAD
/*------------------ Main A2D thread -------------------------*/
// data acquisition loop
// 1 waits on a semaphore from the 100Hz callback
// 2 reads the A2D fifo
// 3 repeat

static void* 
A2DGetDataThread(void *thread_arg)
{
    struct A2DBoard* brd = (struct A2DBoard*) thread_arg;

    int res;

    // The A2Ds should be done writing to the FIFO in
    // 2 * 8 * 800 nsec = 12.8 usec

    KLOG_DEBUG("Starting data-acq loop, GET_MSEC_CLOCK=%d\n",
	       GET_MSEC_CLOCK);

    // Here's the acquisition loop
    for (;;) {
	sys_sem_wait(&brd->acq_sem);
	if (brd->interrupted) break;
	if ((res = getA2DSample(brd)) < 0) return (void*)(-res);
    }
    KLOG_DEBUG("Exiting A2DGetDataThread\n");
    return 0;
}
#endif

// Reset the A2D, but do not close the fifos.
// This does an unregister_irig_callback, so it can't be
// called from the irig callback function itself.
// Use startA2DResetThread in that case.

static int 
resetA2D(struct A2DBoard* brd)
{
    int res;
    
    KLOG_DEBUG("doing unregister_irig_callback\n");
    unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);
    KLOG_DEBUG("unregister_irig_callback done\n");

    // interrupt the 1PPS or acquisition thread
    brd->interrupted = 1;
#ifdef A2D_ACQ_IN_SEPARATE_THREAD
    if (brd->acq_thread) {
	sys_sem_post(&brd->acq_sem);
	sys_pthread_join(brd->acq_thread, &thread_status);
	brd->acq_thread = 0;
	if (thread_status != (void*)0) ret = -(int)thread_status;
    }
#endif

    brd->interrupted = 0;

    // Start a RT thread to allow syncing with 1PPS
    KLOG_DEBUG("doing waitFor1PPS, GET_MSEC_CLOCK=%d\n", GET_MSEC_CLOCK);
    res = waitFor1PPS(brd);
    if (res) return res;
    KLOG_DEBUG("Found initial PPS, GET_MSEC_CLOCK=%d\n", GET_MSEC_CLOCK);


    A2DResetAll(brd);    // Send Abort command to all A/Ds
    A2DStatusAll(brd);   // Read status from all A/Ds

    A2DStartAll(brd);    // Start all the A/Ds
    A2DStatusAll(brd);   // Read status again from all A/Ds

    A2DSetSYNC(brd);     // Stop A/D clocks
    A2DAuto(brd);                // Switch to automatic mode

    KLOG_DEBUG("Setting 1PPS Enable line\n");

    mdelay(20);
    A2D1PPSEnable(brd);// Enable sync with 1PPS

    KLOG_DEBUG("doing waitFor1PPS, GET_MSEC_CLOCK=%d\n", GET_MSEC_CLOCK);
    res = waitFor1PPS(brd);
    if (res) 
	return res;

    KLOG_DEBUG("Found second PPS, GET_MSEC_CLOCK=%d\n", GET_MSEC_CLOCK);
    A2DClearFIFO(brd);  // Reset FIFO

#ifdef A2D_ACQ_IN_SEPARATE_THREAD
    // Zero the semaphore
    sys_sem_init(&brd->acq_sem, 0, 0);
    // Start data acquisition thread
    sys_pthread_attr_t attr;
    sys_pthread_attr_init(&attr);
    sys_pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
    sys_pthread_attr_setstackaddr(&attr, brd->acq_thread_stack);
    if (sys_pthread_create(&brd->acq_thread, &attr, A2DGetDataThread, brd)) {
	KLOG_ERR("Error starting acq thread: %s\n",
		 sys_strerror(sys_errno));
	return -convert_sys_errno(sys_errno);
    }
    sys_pthread_attr_destroy(&attr);
#endif

    brd->discardNextScan = 1;  // whether to discard the initial scan
    brd->readActive = 0;
    brd->enableReads = 1;
    brd->interrupted = 0;
    brd->nbadScans = 0;
    brd->readCtr = 0;
    brd->nbadFifoLevel = 0;
    brd->fifoNotEmpty = 0;
    brd->skippedSamples = 0;
    brd->resets++;

    // start the IRIG callback routine at 100 Hz
    register_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);

    return 0;
}

# ifdef NOTDEF
static void* 
A2DResetThreadFunc(void *thread_arg)
{
    struct A2DBoard* brd = (struct A2DBoard*) thread_arg;
    int ret = resetA2D(brd);
    return (void*)-ret;
}

static int 
startA2DResetThread(struct A2DBoard* brd)
{
    KLOG_WARNING("GET_MSEC_CLOCK=%d, Resetting A2D\n", GET_MSEC_CLOCK);
    brd->enableReads = 0;
    // Shut down any existing reset thread
    if (brd->reset_thread) {
	sys_pthread_cancel(brd->reset_thread);
	sys_pthread_join(brd->reset_thread, NULL);
	brd->reset_thread = 0;
    }

    KLOG_DEBUG("Starting reset thread\n");

    sys_pthread_attr_t attr;
    sys_pthread_attr_init(&attr);
    sys_pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
    sys_pthread_attr_setstackaddr(&attr, brd->reset_thread_stack);
    if (sys_pthread_create(&brd->reset_thread, &attr, A2DResetThreadFunc, brd)) {
	KLOG_ERR("Error starting A2DResetThreadFunc: %s\n",
		 sys_strerror(sys_errno));
	return -convert_sys_errno(sys_errno);
    }
    sys_pthread_attr_destroy(&attr);
    return 0;
}

static int 
openA2D(struct A2DBoard* brd)
{
    brd->busy = 1;  // Set the busy flag
    brd->doTemp = 0;
    brd->acq_thread = 0;
    brd->latencyCnt = brd->config.latencyUsecs /
	(USECS_PER_SEC / INTRP_RATE);
    if (brd->latencyCnt == 0) brd->latencyCnt = 1;
#ifdef DEBUG
    KLOG_DEBUG("latencyUsecs=%d, latencyCnt=%d\n",
	       brd->config.latencyUsecs, brd->latencyCnt);
#endif

    brd->sampleCnt = 0;
    // buffer indices
    brd->head = 0;
    brd->tail = 0;

    brd->nreads = brd->MaxHz*MAXA2DS/INTRP_RATE;

    // expected fifo level just before we read
    brd->expectedFifoLevel = (brd->nreads * 4) / HWFIFODEPTH + 1;
    // level of 1 means <=1/4
    if (brd->nreads == HWFIFODEPTH/4) brd->expectedFifoLevel = 1;

    /*
     * How much to adjust the time tags backwards.
     * Example:
     *   interrupt rate = 100Hz (how often we download the A2D fifo)
     *   max sample rate = 500Hz
     * When we download the A2D fifos at time=00:00.010, we are getting
     *   the 5 samples that (we assume) were sampled at times:
     *   00:00.002, 00:00.004, 00:00.006, 00:00.008 and 00:00.010
     * So the block of data containing the 5 samples will have
     * a time tag of 00:00.002. Code that breaks the block
     * into samples will use the block timetag for the initial sub-sample
     * and then add 1/MaxHz to get successive time tags.
     *
     * Note that the lowest, on-board re-sample rate of this
     * A2D is 500Hz.  Actually it is something like 340Hz,
     * but 500Hz is a rate we can sub-divide into desired rates
     * of 100Hz, 50Hz, etc. Support for lower sampling rates
     * will involve FIR filtering, perhaps in this module.
     */
    brd->ttMsecAdj =     // compute in microseconds first to avoid trunc
	(USECS_PER_SEC / INTRP_RATE - USECS_PER_SEC / brd->MaxHz) /
	USECS_PER_MSEC;

    KLOG_DEBUG("nreads=%d, expectedFifoLevel=%d, ttMsecAdj=%d\n",
	       brd->nreads, brd->expectedFifoLevel, brd->ttMsecAdj);

    memset(&brd->cur_status , 0, sizeof(A2D_STATUS));
    memset(&brd->prev_status, 0, sizeof(A2D_STATUS));

    if (brd->a2dfd >= 0) sys_close(brd->a2dfd);
    if((brd->a2dfd = sys_open(brd->a2dFifoName,
			      SYS_O_NONBLOCK | SYS_O_WRONLY)) < 0)
    {
	KLOG_ERR("error: opening %s: %s\n",
		 brd->a2dFifoName, sys_strerror(sys_errno));
	return -convert_sys_errno(sys_errno);
    }
#ifdef DO_FTRUNCATE
    if (sys_ftruncate(brd->a2dfd, sizeof(brd->buffer)*4) < 0) {
	KLOG_ERR("error: ftruncate %s: size=%d: %s\n",
		 brd->a2dFifoName, sizeof(brd->buffer),
		 sys_strerror(sys_errno));
	return -convert_sys_errno(sys_errno);
    }
#endif

    int res = startA2DResetThread(brd);
    void* thread_status;
    sys_pthread_join(brd->reset_thread, &thread_status);
    brd->reset_thread = 0;
    if (thread_status != (void*)0) res = -(int)thread_status;

    brd->resets = 0;

    return res;
}

/**
 * @param joinAcqThread 1 means do a pthread_join of the acquisition thread.
 *              0 means don't do pthread_join (to avoid a deadlock)
 * @return negative UNIX errno
 */
static int 
closeA2D(struct A2DBoard* brd)
{
    int ret = 0;
#ifdef A2D_ACQ_IN_SEPARATE_THREAD
    void* thread_status;
#endif

    brd->doTemp = 0;
    // interrupt the 1PPS or acquisition thread
    brd->interrupted = 1;

    // Shut down the setup thread
    if (brd->setup_thread) {
	sys_pthread_cancel(brd->setup_thread);
	sys_pthread_join(brd->setup_thread, NULL);
	brd->setup_thread = 0;
    }

    // Shut down the reset thread
    if (brd->reset_thread) {
	sys_pthread_cancel(brd->reset_thread);
	sys_pthread_join(brd->reset_thread, NULL);
	brd->reset_thread = 0;
    }
#ifdef A2D_ACQ_IN_SEPARATE_THREAD
    if (brd->acq_thread) {
	sys_sem_post(&brd->acq_sem);
	sys_pthread_join(brd->acq_thread, &thread_status);
	brd->acq_thread = 0;
	if (thread_status != (void*)0) ret = -(int)thread_status;
    }
#endif

    // Turn off the callback routine
    unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);

    A2DStatusAll(brd);   // Read status and clear IRQ's

    A2DNotAuto(brd);     // Shut off auto mode (if enabled)

    // Abort all the A/D's
    A2DResetAll(brd);

    if (brd->a2dfd >= 0) {
	int fdtmp = brd->a2dfd;
	brd->a2dfd = -1;
	sys_close(fdtmp);
    }
    brd->busy = 0;       // Reset the busy flag

    return ret;
}

/*
 * Function that is called on receipt of an ioctl request over the
 * ioctl FIFO.
 * Return: negative Linux errno (not RTLinux errnos), or 0=OK
 */
static int 
ioctlCallback(int cmd, int board, int port, void *buf, sys_size_t len)
{
    // return LINUX errnos here, not SYS_XXX errnos.
    int ret = -EINVAL;
    void* thread_status;

    // paranoid check if initialized (probably not necessary)
    if (!BoardInfo) return ret;

    struct A2DBoard* brd = BoardInfo + board;

#ifdef DEBUG
    KLOG_DEBUG("ioctlCallback cmd=%x board=%d port=%d len=%d\n",
	       cmd, board, port, len);
#endif

    switch (cmd)
    {
      case GET_NUM_PORTS:               /* user get */
	if (len != sizeof(int)) break;
	KLOG_DEBUG("GET_NUM_PORTS\n");
	*(int *) buf = NDEVICES;
	ret = sizeof(int);
	break;

      case A2D_GET_STATUS:              /* user get of status */
	if (port != 0) break;  // port 0 is the A2D, port 1 is I2C temp
	if (len != sizeof(A2D_STATUS)) break;
	memcpy(buf, &brd->prev_status, len);
	ret = len;
	break;

      case A2D_SET_CONFIG:              /* user set */
	if (port != 0) break;  // port 0 is the A2D, port 1 is I2C temp
	if (len != sizeof(A2D_SET)) break;     // invalid length
	if(brd->busy) {
	    KLOG_ERR("A2D's running. Can't reset\n");
	    ret = -EBUSY;
	    break;
	}
	KLOG_DEBUG("A2D_SET_CONFIG\n");
	memcpy(&brd->config, (A2D_SET*)buf, sizeof(A2D_SET));

	KLOG_DEBUG("Starting setup thread\n");
	if (sys_pthread_create(&brd->setup_thread, NULL, A2DSetupThread, brd)) {
	    KLOG_ERR("Error starting A2DSetupThread: %s\n",
		     sys_strerror(sys_errno));
	    return -convert_sys_errno(sys_errno);
	}
	sys_pthread_join(brd->setup_thread, &thread_status);
	KLOG_DEBUG("Setup thread finished\n");
	brd->setup_thread = 0;

	if (thread_status != (void*)0) ret = -(int)thread_status;
	else ret = 0;          // OK
	KLOG_DEBUG("A2D_SET_CONFIG done, ret=%d\n", ret);
	break;

      case A2D_CAL_IOCTL:               /* user set */
	KLOG_DEBUG("A2D_CAL_IOCTL\n");
	if (port != 0) break;  // port 0 is the A2D, port 1 is I2C temp
	if (len != sizeof(A2D_CAL)) break;     // invalid length
	memcpy(&brd->cal, (A2D_CAL*)buf, sizeof(A2D_CAL));
	A2DSetVcal(brd);
	A2DSetCal(brd);
	ret = 0;
	break;

      case A2D_RUN_IOCTL:
	if (port != 0) break;  // port 0 is the A2D, port 1 is I2C temp
	// clean up acquisition thread if it was left around
#ifdef A2D_ACQ_IN_SEPARATE_THREAD
	if (brd->acq_thread) {
	    brd->interrupted = 1;
	    sys_pthread_cancel(brd->acq_thread);
	    sys_pthread_join(brd->acq_thread, &thread_status);
	    brd->acq_thread = 0;
	}
#endif
	/*
	  KLOG_DEBUG("DEBUG set a channel to a calibration voltage\n");
	  brd->cal.vcalx8 = 0x2;  // REMOVE ME (TEST CAL)
	  brd->cal.calset[0] = 1; // REMOVE ME (TEST CAL)
	  brd->cal.calset[1] = 0; // REMOVE ME (TEST CAL)
	  brd->cal.calset[2] = 0; // REMOVE ME (TEST CAL)
	  brd->cal.calset[3] = 0; // REMOVE ME (TEST CAL)
	  brd->cal.calset[4] = 0; // REMOVE ME (TEST CAL)
	  brd->cal.calset[5] = 0; // REMOVE ME (TEST CAL)
	  brd->cal.calset[6] = 0; // REMOVE ME (TEST CAL)
	  brd->cal.calset[7] = 0; // REMOVE ME (TEST CAL)
	  A2DSetVcal(brd);        // REMOVE ME (TEST CAL)
	  A2DSetCal(brd);         // REMOVE ME (TEST CAL)
	*/
	KLOG_DEBUG("A2D_RUN_IOCTL\n");
	ret = openA2D(brd);
	KLOG_DEBUG("A2D_RUN_IOCTL finished\n");
	break;

      case A2D_STOP_IOCTL:
	if (port != 0) break;  // port 0 is the A2D, port 1 is I2C temp
	KLOG_DEBUG("A2D_STOP_IOCTL\n");
	ret = closeA2D(brd);
	KLOG_DEBUG("closeA2D, ret=%d\n", ret);
	break;
      case A2D_OPEN_I2CT:
	if (port != 1) break;  // port 0 is the A2D, port 1 is I2C temp
	KLOG_DEBUG("A2D_OPEN_I2CT\n");
	if (port != 1) break;  // port 0 is the A2D, port 1 is I2C temp
	if (len != sizeof(int)) break; // invalid length
	int rate = *(int*)buf;
	ret = openI2CTemp(brd, rate);
	break;
      case A2D_CLOSE_I2CT:
	if (port != 1) break;  // port 0 is the A2D, port 1 is I2C temp
	KLOG_DEBUG("A2D_CLOSE_I2CT\n");
	if (port != 1) break;  // port 0 is the A2D, port 1 is I2C temp
	ret = closeI2CTemp(brd);
	break;
      case A2D_GET_I2CT:
	if (port != 1) break;  // port 0 is the A2D, port 1 is I2C temp
	if (len != sizeof(short)) break;
	*(short *) buf = brd->i2cTempData;
	ret = sizeof(short);
	break;

      default:
	break;
    }
    return ret;
}

/*-----------------------Module------------------------------*/
// Stops the A/D and releases reserved memory
void 
cleanup_module(void)
{
    int ib;
    if (!BoardInfo) return;

    for (ib = 0; ib < NumBoards; ib++) {
	struct A2DBoard* brd = BoardInfo + ib;

	// remove the callback routines
	// (does nothing if it isn't registered)
	unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);
	unregister_irig_callback(&i2cTempIrigCallback, brd->i2cTempRate, brd);

	A2DStatusAll(brd);        // Read status and clear IRQ's

	// Shut down the setup thread
	if (brd->setup_thread) {
	    sys_pthread_cancel(brd->setup_thread);
	    sys_pthread_join(brd->setup_thread, NULL);
	    brd->setup_thread = 0;
	}

	// Shut down the setup thread
	if (brd->reset_thread) {
	    sys_pthread_cancel(brd->reset_thread);
	    sys_pthread_join(brd->reset_thread, NULL);
	    brd->reset_thread = 0;
	}

	// Shut down the acquisition thread
#ifdef A2D_ACQ_IN_SEPARATE_THREAD
	if (brd->acq_thread) {
	    sys_pthread_cancel(brd->acq_thread);
	    sys_pthread_join(brd->acq_thread, NULL);
	    brd->acq_thread = 0;
	}
	sys_sem_destroy(&brd->acq_sem);
	if (brd->acq_thread_stack) kfree(brd->acq_thread_stack);
#endif

	// close and remove A2D fifo
	if (brd->a2dfd >= 0) sys_close(brd->a2dfd);
	if (brd->a2dFifoName) {
	    sys_unlink(brd->a2dFifoName);
	    kfree(brd->a2dFifoName);
	}

	// close and remove temperature fifo
	if (brd->i2cTempfd >= 0) sys_close(brd->i2cTempfd);
	if (brd->i2cTempFifoName) {
	    sys_unlink(brd->i2cTempFifoName);
	    kfree(brd->i2cTempFifoName);
	}

	if (brd->reset_thread_stack) kfree(brd->reset_thread_stack);

	if (brd->ioctlhandle) closeIoctlFIFO(brd->ioctlhandle);
	brd->ioctlhandle = 0;

	if (brd->addr)
	    release_region(brd->addr, A2DIOWIDTH);
	brd->addr = 0;
    }

    kfree(BoardInfo);
    BoardInfo = 0;

    KLOG_DEBUG("Analog cleanup complete\n");

    return;
}

/*-----------------------Module------------------------------*/

int 
init_module()
{
#if 0
    int ilast, ichan;
    for (ilast=0; ilast<nlast; ilast++)
	for (ichan=0; ichan<8; ichan++) {
	    last_stat[ilast][ichan] = 0;
	    last_cnts[ilast][ichan] = 0;
	}
#endif
    int error = -EINVAL;
    int ib;

    BoardInfo = 0;

    // DSM_VERSION_STRING is found in dsm_version.h
    KLOG_NOTICE("version: %s\n", DSM_VERSION_STRING);

    /* count non-zero ioport addresses, gives us the number of boards */
    for (ib = 0; ib < MAX_A2D_BOARDS; ib++)
	if (IoPort[ib] == 0) break;
    NumBoards = ib;
    if (NumBoards == 0) {
	KLOG_ERR("No boards configured, all IoPort[]==0\n");
	goto err;
    }
    KLOG_DEBUG("configuring %d board(s)...\n", NumBoards);

    error = -ENOMEM;
    BoardInfo = kmalloc( NumBoards * sizeof(struct A2DBoard) );
    if (!BoardInfo) goto err;

    KLOG_DEBUG("sizeof(struct A2DBoard): 0x%x\n", sizeof(struct A2DBoard));

    /* initialize each A2DBoard structure */
    for (ib = 0; ib < NumBoards; ib++) {
	struct A2DBoard* brd = BoardInfo + ib;

	KLOG_DEBUG("initializing board[%d] at 0x%x\n", ib, brd);
	// initialize structure to zero, then initialize things
	// that are non-zero
	memset(brd, 0, sizeof(struct A2DBoard));

#ifdef A2D_ACQ_IN_SEPARATE_THREAD
	sys_sem_init(&brd->acq_sem, 0, 0);
#endif
	brd->a2dfd = -1;
	brd->i2cTempfd = -1;
	// default latency, 1/10 second.
	brd->config.latencyUsecs = USECS_PER_SEC / 10;
#ifdef DO_A2D_STATRD
	brd->FIFOCtl = A2DSTATEBL;
#else
	brd->FIFOCtl = 0;
#endif
	brd->i2c = 0x3;

	brd->invertCounts = Invert[ib];
	brd->master = Master[ib];
    }

    /* initialize necessary members in each A2DBoard structure */
    for (ib = 0; ib < NumBoards; ib++) {
	struct A2DBoard* brd = BoardInfo + ib;

	error = -EBUSY;
	unsigned int addr =  IoPort[ib] + SYSTEM_ISA_IOPORT_BASE;
	// Get the mapped board address
	if (check_region(addr, A2DIOWIDTH)) {
	    KLOG_ERR("ioports at 0x%x already in use\n", addr);
	    goto err;
	}

	request_region(addr, A2DIOWIDTH, "NCAR_A2D");
	brd->addr = addr;
	brd->chan_addr = addr + A2DIOLOAD;

	/* Open up my ioctl FIFOs, register my ioctlCallback function */
	error = -EIO;
	brd->ioctlhandle =
	    openIoctlFIFO(devprefix, ib, ioctlCallback,
			  nioctlcmds, ioctlcmds);

	if (!brd->ioctlhandle) goto err;

	// Open the A2D fifo to user space
	error = -ENOMEM;
	brd->a2dFifoName = makeDevName(devprefix, "_in_", ib*NDEVICES);
	if (!brd->a2dFifoName) goto err;

	// remove broken device file before making a new one
	if ((sys_unlink(brd->a2dFifoName) < 0 && sys_errno != SYS_ENOENT)
	    || sys_mkfifo(brd->a2dFifoName, 0666) < 0) {
	    KLOG_ERR("error: unlink/mkfifo %s: %s\n",
		     brd->a2dFifoName, sys_strerror(sys_errno));
	    error = -convert_sys_errno(sys_errno);
	    goto err;
	}

	// Open the fifo for I2C temperature data to user space
	error = -ENOMEM;
	brd->i2cTempFifoName = makeDevName(devprefix, "_in_", ib*NDEVICES+1);
	if (!brd->i2cTempFifoName) goto err;

	// remove broken device file before making a new one
	if ((sys_unlink(brd->i2cTempFifoName) < 0 && sys_errno != SYS_ENOENT)
	    || sys_mkfifo(brd->i2cTempFifoName, 0666) < 0) {
	    KLOG_ERR("error: unlink/mkfifo %s: %s\n",
		     brd->i2cTempFifoName, sys_strerror(sys_errno));
	    error = -convert_sys_errno(sys_errno);
	    goto err;
	}

	/* allocate thread stacks at init module time */
	if (!(brd->reset_thread_stack = kmalloc(THREAD_STACK_SIZE)))
	    goto err;
#ifdef A2D_ACQ_IN_SEPARATE_THREAD
	if (!(brd->acq_thread_stack = kmalloc(THREAD_STACK_SIZE)))
	    goto err;
#endif
    }

    KLOG_DEBUG("A2D init_module complete.\n");

    return 0;
  err:

    if (BoardInfo) {
	for (ib = 0; ib < NumBoards; ib++) {
	    struct A2DBoard* brd = BoardInfo + ib;
	    if (brd->reset_thread_stack) kfree(brd->reset_thread_stack);
#ifdef A2D_ACQ_IN_SEPARATE_THREAD
	    if (brd->acq_thread_stack) kfree(brd->acq_thread_stack);
#endif

#ifdef A2D_ACQ_IN_SEPARATE_THREAD
	    sys_sem_destroy(&brd->acq_sem);
#endif
	    if (brd->a2dFifoName) {
		sys_unlink(brd->a2dFifoName);
		kfree(brd->a2dFifoName);
	    }

	    if (brd->i2cTempFifoName) {
		sys_unlink(brd->i2cTempFifoName);
		kfree(brd->i2cTempFifoName);
	    }

	    if (brd->ioctlhandle) closeIoctlFIFO(brd->ioctlhandle);
	    brd->ioctlhandle = 0;

	    if (brd->addr)
		release_region(brd->addr, A2DIOWIDTH);
	    brd->addr = 0;
	}

    }

    kfree(BoardInfo);
    BoardInfo = 0;
    return error;
}
# endif // ifdef NOTDEF
