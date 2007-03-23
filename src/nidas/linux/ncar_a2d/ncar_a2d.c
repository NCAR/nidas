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
#include <linux/fs.h>	/* has to be before <linux/cdev.h>! GRRR! */
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <asm/io.h>
#include <asm/uaccess.h>

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
 * prototypes
 */
int init_module(void);
void cleanup_module(void);
static ssize_t ncar_a2d_read(struct file *filp, char __user *buf,
			     size_t count,loff_t *f_pos);
static int ncar_a2d_open(struct inode *inode, struct file *filp);
static int ncar_a2d_release(struct inode *inode, struct file *filp);
static int ncar_a2d_ioctl(struct inode *inode, struct file *filp, 
			  unsigned int cmd, unsigned long arg);
//static ssize_t i2c_temp_read(struct file *filp, char __user *buf,
//			       size_t count,loff_t *f_pos);
//static int i2c_temp_open(struct inode *inode, struct file *filp);
//static int i2c_temp_release(struct inode *inode, struct file *filp);
static int i2c_temp_ioctl(struct inode *inode, struct file *filp, 
			    unsigned int cmd, unsigned long arg);


static struct file_operations ncar_a2d_fops = {
    .owner   = THIS_MODULE,
    .read    = ncar_a2d_read,
    .open    = ncar_a2d_open,
    .ioctl   = ncar_a2d_ioctl,
    .release = ncar_a2d_release,
};

static struct file_operations i2c_temp_fops = {
    .owner   = THIS_MODULE,
//    .read    = i2c_temp_read,
//    .open    = i2c_temp_open,
    .ioctl   = i2c_temp_ioctl,
//    .release = i2c_temp_release,
};

/**
 * Info for A/D user devices
 */
#define DEVNAME_A2D "ncar_a2d"
static dev_t A2DDevStart;
static struct cdev A2DCdev;

/**
 * Info for I2C board temperature devices
 */
#define DEVNAME_A2D_TEMP "ncar_a2d_boardtemp"
static dev_t I2CTempDevStart;
static struct cdev I2CTempCdev;

/**
 * Utility function to tell how much space is available on a kfifo.
 * Returns (fifo->size - kfifo_len(fifo))
 */
static inline unsigned int 
kfifo_avail(struct kfifo *fifo)
{
    unsigned long flags;
    unsigned int ret;
    
    spin_lock_irqsave(fifo->lock, flags);
    ret = fifo->size - __kfifo_len(fifo);
    spin_unlock_irqrestore(fifo->lock, flags);
    return ret;
}


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

static unsigned short 
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
    unsigned short gainCode;

    // If no A/D selected return error -1
    if(a2dSel < 0 || a2dSel >= MAXA2DS) return -EINVAL;

    a2d = &brd->config;
    a2dGain = a2d->gain[a2dSel];
    a2dGainMul = a2d->gainMul[a2dSel];
    a2dGainDiv = a2d->gainDiv[a2dSel];
    gainCode = 0;

    // unused channel gains are set to zero in the configuration
    if (a2dGain != 0)
	gainCode = (unsigned short)(a2dGainMul*a2dGain/a2dGainDiv);

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
    unsigned short OffChans = 0;
    unsigned short CalChans = 0;
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
    unsigned short OffChans = 0;
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
A2DEnable1PPS(struct A2DBoard* brd)
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
A2DDisable1PPS(struct A2DBoard* brd)
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
    unsigned short stat;
    unsigned char intmask=1, intbits[8] = {1, 2, 4, 8, 16, 32, 64, 128};
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

static int 
A2DSetup(struct A2DBoard* brd)
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


int
setupBoard(struct A2DBoard* brd)
{
    int ret = 0;

    // while (1) { // DEBUG continuous DAC setting...
    // Configure DAC gain codes
    if ((ret = A2DSetup(brd)) < 0) return -ret;
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
    if ((ret = A2DConfigAll(brd)) < 0) return -ret;
    // KLOG_DEBUG("filter settings - loop forever... ret = %d\n", ret);
    // mdelay(1000);
    // }
    // Reset the A/D's
    KLOG_DEBUG("Resetting A/Ds\n");
    A2DResetAll(brd);

    mdelay(1);  // Give A/D's a chance to load
    KLOG_DEBUG("A/Ds ready for synchronous start\n");

    return ret;
}


static void
taskSetupBoard(unsigned long taskletArg)
{
    struct A2DBoard* brd = (struct A2DBoard*)taskletArg;
    int status = setupBoard(brd);
    if (status != 0)
	KLOG_WARNING("setup error %d for board @ 0x%x\n", status, brd->addr);
}
    

static void
taskGetA2DSample(unsigned long taskletArg)
{
    struct A2DBoard* brd = (struct A2DBoard*)taskletArg;
    int flevel;
    A2DSAMPLE samp;
    register short* dataptr;
    int nbad;
    int iread;
    
    if (!brd->buffer || !brd->enableReads)
	return;

    flevel = getA2DFIFOLevel(brd);

    if (brd->discardNextScan) {
	if (flevel > 0) A2DEmptyFIFO(brd);
	brd->discardNextScan = 0;
	return;
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
		tasklet_schedule(&brd->resetTasklet);
		return;
	    }
	}
	if (flevel == 0)
	    return;
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
	if (brd->invertCounts) 
	    counts = -inw(brd->addr);
	else 
	    counts = inw(brd->addr);

	// check for acceptable looking status value
	if ((stat & A2DSTATMASK) != A2DEXPSTATUS) {
	    KLOG_DEBUG("--------- SPIKE! --------- read: %2d  chn: %d  "
		       "stat: %x  data: %x\n", iread, ichan, stat, counts);
	    nbad++;
	    brd->cur_status.nbad[ichan]++;
	    brd->cur_status.badval[ichan] = stat;
	}
	else 
	    brd->cur_status.goodval[ichan] = stat;
#else
	// Inverted bits for later cards
	if (brd->invertCounts) 
	    counts = -inw(brd->addr);
	else
	    counts = inw(brd->addr);

#endif
	//    if (counts > 0x1000) KLOG_DEBUG("Greater than 0x1000\n");
	if (brd->requested[ichan])
	    *dataptr++ = counts;
    }
    flevel = getA2DFIFOLevel(brd);
    brd->cur_status.postFifoLevel[flevel]++;

    // sys_restore_interrupts(irqstate);  // JDW

    if (flevel > 0) {
	if (!(brd->fifoNotEmpty++ % 1000))
	    KLOG_WARNING("post-read fifo level=%d (not empty): %d times.\n",
			 flevel, brd->fifoNotEmpty);

	if (flevel > brd->expectedFifoLevel || brd->fifoNotEmpty > 1) {
	    tasklet_schedule(&brd->resetTasklet);
	    if (nbad > 0)
		KLOG_ERR("nbad is %d\n", nbad);
	    return;
	}
    }
    if (nbad > 0)
	brd->nbadScans++;

    // DSMSensor::printStatus queries these values every 10 seconds
    if (!(++brd->readCtr % (INTRP_RATE * 10))) {
	// debug print every minute, or if there are bad scans
	if (!(brd->readCtr % (INTRP_RATE * 60)) || brd->nbadScans) {
	    KLOG_DEBUG("GET_MSEC_CLOCK=%d, nbadScans=%d\n",
		       GET_MSEC_CLOCK, brd->nbadScans);
	    KLOG_DEBUG("nbadFifoLevel=%d, #fifoNotEmpty=%d, #skipped=%d, "
		       "#resets=%d\n", brd->nbadFifoLevel, brd->fifoNotEmpty, 
		       brd->skippedSamples, brd->resets);
	    KLOG_DEBUG("pre-scan  fifo=%d,%d,%d,%d,%d,%d "
		       "(0,<=1/4,<2/4,<3/4,<4/4,full)\n",
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
	    KLOG_DEBUG("last good status= %04x %04x %04x %04x %04x %04x "
		       "%04x %04x\n",
		       brd->cur_status.goodval[0],
		       brd->cur_status.goodval[1],
		       brd->cur_status.goodval[2],
		       brd->cur_status.goodval[3],
		       brd->cur_status.goodval[4],
		       brd->cur_status.goodval[5],
		       brd->cur_status.goodval[6],
		       brd->cur_status.goodval[7]);

	    if (brd->nbadScans > 0) {
		KLOG_DEBUG("last bad status=  %04x %04x %04x %04x %04x "
			   "%04x %04x %04x\n",
			   brd->cur_status.badval[0],
			   brd->cur_status.badval[1],
			   brd->cur_status.badval[2],
			   brd->cur_status.badval[3],
			   brd->cur_status.badval[4],
			   brd->cur_status.badval[5],
			   brd->cur_status.badval[6],
			   brd->cur_status.badval[7]);
		KLOG_DEBUG("num  bad status=  %4d %4d %4d %4d %4d %4d "
			   "%4d %4d\n",
			   brd->cur_status.nbad[0],
			   brd->cur_status.nbad[1],
			   brd->cur_status.nbad[2],
			   brd->cur_status.nbad[3],
			   brd->cur_status.nbad[4],
			   brd->cur_status.nbad[5],
			   brd->cur_status.nbad[6],
			   brd->cur_status.nbad[7]);

		if (brd->nbadScans > 10) {
		    tasklet_schedule(&brd->resetTasklet);
		    return;
		}
	    }
	    brd->readCtr = 0;
	} // debug printout

	brd->nbadScans = 0;

	// copy current status to prev_status for access by ioctl
	// A2D_GET_STATUS
	brd->cur_status.nbadFifoLevel = brd->nbadFifoLevel;
	brd->cur_status.fifoNotEmpty = brd->fifoNotEmpty;
	brd->cur_status.skippedSamples = brd->skippedSamples;
	brd->cur_status.resets = brd->resets;
	memcpy(&brd->prev_status, &brd->cur_status, sizeof(A2D_STATUS));
	memset(&brd->cur_status, 0, sizeof(A2D_STATUS));
    }

    samp.size = (char*)dataptr - (char*)samp.data;

    if (samp.size > 0) {
	unsigned long flags;
	unsigned long avail;
	size_t slen = SIZEOF_DSM_SAMPLE_HEADER + samp.size;
	/*
	 * Put the sample into the user device buffer only if we have
	 * enough space for it.  Otherwise, drop the sample.
	 */
	spin_lock_irqsave(brd->buffer->lock, flags);
	avail = brd->buffer->size - __kfifo_len(brd->buffer);

	if (avail >= slen) {
	    __kfifo_put(brd->buffer, (unsigned char*)&samp, slen);
	    /*
	     * Wake up waiting read
	     */
	    wake_up_interruptible(&(brd->rwaitq));
	}
	else
	    brd->skippedSamples++;

	spin_unlock_irqrestore(brd->buffer->lock, flags);

	if (brd->skippedSamples && !(brd->skippedSamples % 1000))
	    KLOG_WARNING("%d samples lost\n", brd->skippedSamples);
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

    if (nbad > 0)
	KLOG_ERR("leaving taskGetA2DSample, nbad is %d\n", nbad);
    return;
}


// function is scheduled to be called from IRIG driver at 100Hz
static void 
a2dIrigCallback(void *ptr)
{
    struct A2DBoard* brd = (struct A2DBoard*) ptr;
    tasklet_schedule(&brd->getSampleTasklet);
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
    brd->doTemp = 1;
}


// Reset the A2D, but do not close the fifos.
// This does an unregister_irig_callback, so it can't be
// called from the irig callback function itself.
// Schedule the board's resetTasklet in that case.

static int 
resetBoard(struct A2DBoard* brd)
{
    int res;
    
    KLOG_DEBUG("doing unregister_irig_callback\n");
    unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);
    KLOG_DEBUG("unregister_irig_callback done\n");

    // Sync with 1PPS
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
    A2DEnable1PPS(brd);// Enable sync with 1PPS

    KLOG_DEBUG("doing waitFor1PPS, GET_MSEC_CLOCK=%d\n", GET_MSEC_CLOCK);
    res = waitFor1PPS(brd);
    if (res) 
	return res;

    KLOG_DEBUG("Found second PPS, GET_MSEC_CLOCK=%d\n", GET_MSEC_CLOCK);
    A2DClearFIFO(brd);  // Reset FIFO

    brd->discardNextScan = 1;  // whether to discard the initial scan
    brd->enableReads = 1;
    brd->interrupted = 0;
    brd->nbadScans = 0;
    brd->readCtr = 0;
    brd->nbadFifoLevel = 0;
    brd->fifoNotEmpty = 0;
    brd->skippedSamples = 0;
    brd->resets++;

    return 0;
}

/*
 * Task to perform a board reset.
 */
static void
taskResetBoard(unsigned long taskletArg)
{
    struct A2DBoard* brd = (struct A2DBoard*)taskletArg;
    int status;

    brd->enableReads = 0;
    if ((status = resetBoard(brd)) != 0)
	KLOG_WARNING("A2DResetTask() failed for board @ 0x%x with status %d\n",
		     brd->addr, status);
}


/* static int  */
/* openA2D(struct A2DBoard* brd) */
/* { */
/*     int result; */
    
/*     brd->busy = 1;  // Set the busy flag */
/*     brd->doTemp = 0; */
/*     brd->latencyCnt = brd->config.latencyUsecs / */
/* 	(USECS_PER_SEC / INTRP_RATE); */
/*     if (brd->latencyCnt == 0) brd->latencyCnt = 1; */
/* #ifdef DEBUG */
/*     KLOG_DEBUG("latencyUsecs=%d, latencyCnt=%d\n", */
/* 	       brd->config.latencyUsecs, brd->latencyCnt); */
/* #endif */

/*     brd->sampleCnt = 0; */
/*     /\* */
/*      * No buffer until a user opens the device */
/*      *\/ */
/*     brd->buffer = NULL; */

/*     brd->nreads = brd->MaxHz * MAXA2DS / INTRP_RATE; */

/*     // expected fifo level just before we read */
/*     brd->expectedFifoLevel = (brd->nreads * 4) / HWFIFODEPTH + 1; */
/*     // level of 1 means <=1/4 */
/*     if (brd->nreads == HWFIFODEPTH/4) brd->expectedFifoLevel = 1; */

/*     /\* */
/*      * How much to adjust the time tags backwards. */
/*      * Example: */
/*      *   interrupt rate = 100Hz (how often we download the A2D fifo) */
/*      *   max sample rate = 500Hz */
/*      * When we download the A2D fifos at time=00:00.010, we are getting */
/*      *   the 5 samples that (we assume) were sampled at times: */
/*      *   00:00.002, 00:00.004, 00:00.006, 00:00.008 and 00:00.010 */
/*      * So the block of data containing the 5 samples will have */
/*      * a time tag of 00:00.002. Code that breaks the block */
/*      * into samples will use the block timetag for the initial sub-sample */
/*      * and then add 1/MaxHz to get successive time tags. */
/*      * */
/*      * Note that the lowest, on-board re-sample rate of this */
/*      * A2D is 500Hz.  Actually it is something like 340Hz, */
/*      * but 500Hz is a rate we can sub-divide into desired rates */
/*      * of 100Hz, 50Hz, etc. Support for lower sampling rates */
/*      * will involve FIR filtering, perhaps in this module. */
/*      *\/ */
/*     brd->ttMsecAdj =     // compute in microseconds first to avoid trunc */
/* 	(USECS_PER_SEC / INTRP_RATE - USECS_PER_SEC / brd->MaxHz) / */
/* 	USECS_PER_MSEC; */

/*     KLOG_DEBUG("nreads=%d, expectedFifoLevel=%d, ttMsecAdj=%d\n", */
/* 	       brd->nreads, brd->expectedFifoLevel, brd->ttMsecAdj); */

/*     memset(&brd->cur_status , 0, sizeof(A2D_STATUS)); */
/*     memset(&brd->prev_status, 0, sizeof(A2D_STATUS)); */

/*     brd->enableReads = 0; */
/*     result = resetBoard(brd); */
/*     brd->resets = 0; */

/*     return result; */
/* } */

//XX /**
//XX  * @return negative UNIX errno
//XX  */
//XX static int 
//XX closeA2D(struct A2DBoard* brd)
//XX {
//XX     int ret = 0;
//XX 
//XX     brd->doTemp = 0;
//XX     // interrupt the 1PPS or acquisition
//XX     brd->interrupted = 1;
//XX 
//XX     // Turn off the callback routine
//XX     unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);
//XX 
//XX     A2DStatusAll(brd);   // Read status and clear IRQ's
//XX 
//XX     A2DNotAuto(brd);     // Shut off auto mode (if enabled)
//XX 
//XX     // Abort all the A/D's
//XX     A2DResetAll(brd);
//XX 
//XX     brd->busy = 0;       // Reset the busy flag
//XX 
//XX     return ret;
//XX }

/**
 * User-space open of the A/D device.
 */
static int 
ncar_a2d_open(struct inode *inode, struct file *filp)
{
    struct A2DBoard *brd = BoardInfo + iminor(inode);
    int errval;

    init_waitqueue_head(&brd->rwaitq);
    spin_lock_init(&brd->bufLock);
    brd->buffer = kfifo_alloc(A2D_BUFFER_SIZE, GFP_KERNEL, &brd->bufLock);
    if (IS_ERR(brd->buffer)) {
	errval = (int)brd->buffer;
	KLOG_ERR("Error %d from kfifo_alloc\n", errval);
	return errval;
    }

    // start the IRIG callback routine at 100 Hz
    errval = register_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);
    if (errval < 0) 
    {
	KLOG_ERR("Error %d registering callback\n", errval);
	return errval;
    }

    filp->private_data = brd;
    return 0;
}


/**
 * User-space close of the A/D device.
 */
static int 
ncar_a2d_release(struct inode *inode, struct file *filp)
{
    struct A2DBoard *brd = (struct A2DBoard*)filp->private_data;

    int ret = 0;

    brd->doTemp = 0;
    /*
     * interrupt the 1PPS or acquisition
     */
    brd->interrupted = 1;

    /*
     * Turn off the callback routine
     */
    unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);

    A2DStatusAll(brd);   // Read status and clear IRQ's

    A2DNotAuto(brd);     // Shut off auto mode (if enabled)

    // Abort all the A/D's
    A2DResetAll(brd);


    brd->busy = 0;       // Reset the busy flag
    /*
     * Get rid of the FIFO to hold data destined for user space
     */
    kfifo_free(brd->buffer);
    brd->buffer = 0;

    return ret;
}

/**
 * User-space read on the A/D device.
 */
static ssize_t 
ncar_a2d_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    struct A2DBoard *brd = (struct A2DBoard*)filp->private_data;
    ssize_t nwritten;
    int percent_full;
    int fifo_len;
    int wlen;
    unsigned char c;
    unsigned int csize = sizeof(c);

    if ((kfifo_len(brd->buffer) == 0) && (filp->f_flags & O_NONBLOCK))
	return -EAGAIN;


    while (1) {
	if (wait_event_interruptible(brd->rwaitq, 1))
	    return -ERESTARTSYS;

	/*
	 * How full is the buffer?
	 */
	fifo_len = kfifo_len(brd->buffer);
	percent_full = (100 * fifo_len) / A2D_BUFFER_SIZE;

	/* 
	 * Don't send anything until we have at least latencyCnt samples (or
	 * the buffer is nearly full)
	 */
	if ((brd->sampleCnt < brd->latencyCnt) && (percent_full < 90))
	    continue;

	if (brd->sampleCnt < brd->latencyCnt)
	    KLOG_INFO("Read proceeding with %d%% full buffer\n", percent_full);

	if (count != fifo_len)
	    KLOG_INFO("Read not getting all data in buffer (%d < %d)\n",
		      count, fifo_len);
	
	/*
	 * Bytes to write
	 */
	wlen = (count > fifo_len) ? fifo_len : count;

	/*
	 * Copy to the user's buffer
	 */
	nwritten = 0;
	while ((nwritten < wlen) && 
	       (kfifo_get(brd->buffer, &c, csize) == csize)) {
	    if (put_user(c, buf++) != 0)
		return -EFAULT;
	    nwritten++;
	}

	/*
	 * Zero the sample count when they empty the buffer.
	 * BUG: Of course the sample count becomes inaccurate if the user
	 * does not read enough to empty the buffer....
	 */
	if (nwritten == fifo_len)
	    brd->sampleCnt = 0;

	return nwritten;
    }
}

/*
 * Function that is called on receipt of an ioctl request.
 * Return: negative Linux errno, or 0=OK
 */
static int 
ncar_a2d_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, 
	       unsigned long arg)
{
    struct A2DBoard *brd = (struct A2DBoard*)filp->private_data;
    void __user* userptr = (void __user*)arg;
//XX    int rate;
    int len = _IOC_SIZE(cmd);
    int ret = -EINVAL;

#ifdef DEBUG
    KLOG_DEBUG("ncar_a2d_ioctl cmd=%x board addr=0x%x len=%d\n",
	       cmd, brd->addr, len);
#endif

    switch (cmd)
    {
      case A2D_GET_STATUS:              /* user get of status */
	if (len != sizeof(A2D_STATUS)) 
	    break;
	memcpy(userptr, &brd->prev_status, len);
	ret = len;
	break;

      case A2D_SET_CONFIG:              /* user set */
	if (len != sizeof(A2D_SET)) 
	    break;     // invalid length
	if(brd->busy) {
	    KLOG_ERR("A2D's running. Can't reset\n");
	    ret = -EBUSY;
	    break;
	}
	KLOG_DEBUG("A2D_SET_CONFIG\n");
	memcpy(&brd->config, (A2D_SET*)userptr, sizeof(A2D_SET));

	KLOG_DEBUG("Starting setup\n");
	ret = setupBoard(brd);

	KLOG_DEBUG("A2D_SET_CONFIG done, ret=%d\n", ret);
	break;

      case A2D_CAL_IOCTL:               /* user set */
	KLOG_DEBUG("A2D_CAL_IOCTL\n");
	if (len != sizeof(A2D_CAL)) 
	    break;     // invalid length
	if (copy_from_user(&brd->cal, (A2D_CAL*)userptr, len) == 0)
	{
	    ret = -EFAULT;
	    break;
	}
	A2DSetVcal(brd);
	A2DSetCal(brd);
	ret = 0;
	break;

//XX      case A2D_RUN_IOCTL:
//XX	/*
//XX	KLOG_DEBUG("DEBUG set a channel to a calibration voltage\n");
//XX		   brd->cal.vcalx8 = 0x2;  // REMOVE ME (TEST CAL)
//XX		   brd->cal.calset[0] = 1; // REMOVE ME (TEST CAL)
//XX		   brd->cal.calset[1] = 0; // REMOVE ME (TEST CAL)
//XX		   brd->cal.calset[2] = 0; // REMOVE ME (TEST CAL)
//XX		   brd->cal.calset[3] = 0; // REMOVE ME (TEST CAL)
//XX		   brd->cal.calset[4] = 0; // REMOVE ME (TEST CAL)
//XX		   brd->cal.calset[5] = 0; // REMOVE ME (TEST CAL)
//XX		   brd->cal.calset[6] = 0; // REMOVE ME (TEST CAL)
//XX		   brd->cal.calset[7] = 0; // REMOVE ME (TEST CAL)
//XX		   A2DSetVcal(brd);        // REMOVE ME (TEST CAL)
//XX		   A2DSetCal(brd);         // REMOVE ME (TEST CAL)
//XX	*/
//XX	KLOG_DEBUG("A2D_RUN_IOCTL\n");
//XX	ret = openA2D(brd);
//XX	KLOG_DEBUG("A2D_RUN_IOCTL finished\n");
//XX	break;

//XX      case A2D_STOP_IOCTL:
//XX	KLOG_DEBUG("A2D_STOP_IOCTL\n");
//XX	ret = closeA2D(brd);
//XX	KLOG_DEBUG("closeA2D, ret=%d\n", ret);
//XX	break;

//XX      case A2D_OPEN_I2CT:
//XX	KLOG_DEBUG("A2D_OPEN_I2CT\n");
//XX	if (len != sizeof(int)) 
//XX	    break; // invalid length
//XX	rate = *(int*)userptr;
//XX	ret = openI2CTemp(brd, rate);
//XX	break;

//XX      case A2D_CLOSE_I2CT:
//XX	KLOG_DEBUG("A2D_CLOSE_I2CT\n");
//XX	ret = closeI2CTemp(brd);
//XX	break;

      case A2D_GET_I2CT:
	if (len != sizeof(short)) 
	    break;
	*(short *)userptr = brd->i2cTempData;
	ret = sizeof(short);
	break;

      default:
	break;
    }
    return ret;
}

static int 
i2c_temp_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, 
	       unsigned long arg)
{
    struct A2DBoard *brd = (struct A2DBoard*)filp->private_data;
    void __user* userptr = (void __user*)arg;
//XX    int rate;
    int len = _IOC_SIZE(cmd);
    int ret = -EINVAL;

#ifdef DEBUG
    KLOG_DEBUG("i2c_temp_ioctl cmd=%x board addr=0x%x len=%d\n",
	       cmd, brd->addr, len);
#endif

    switch (cmd)
    {
      case A2D_GET_I2CT:
	if (len != sizeof(short)) 
	    break;
	*(short *)userptr = brd->i2cTempData;
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

    cdev_del(&I2CTempCdev);

    cdev_del(&A2DCdev);

    for (ib = 0; ib < NumBoards; ib++) {
	struct A2DBoard* brd = BoardInfo + ib;

	// remove the callback routines
	// (does nothing if it isn't registered)
	unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ, brd);
	unregister_irig_callback(&i2cTempIrigCallback, brd->i2cTempRate, brd);

	A2DStatusAll(brd);        // Read status and clear IRQ's

//XX	Close user-space devices

	if (brd->addr)
	    release_region(brd->addr, A2DIOWIDTH);
	brd->addr = 0;
    }

    kfree(BoardInfo);
    BoardInfo = 0;

    KLOG_DEBUG("NCAR A/D cleanup complete\n");

    return;
}

/*-----------------------Module------------------------------*/

int 
init_module()
{
    int error = -EINVAL;
    int ib;

    BoardInfo = 0;

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
    BoardInfo = kmalloc(NumBoards * sizeof(struct A2DBoard), GFP_KERNEL);
    if (!BoardInfo)
	goto err;

    KLOG_DEBUG("sizeof(struct A2DBoard): 0x%x\n", sizeof(struct A2DBoard));

    /* initialize each A2DBoard structure */
    for (ib = 0; ib < NumBoards; ib++) {
	struct A2DBoard* brd = BoardInfo + ib;

	KLOG_DEBUG("initializing board[%d] at 0x%x\n", ib, brd);
	// initialize structure to zero, then initialize things
	// that are non-zero
	memset(brd, 0, sizeof(struct A2DBoard));

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
	unsigned int addr;
	
	tasklet_init(&brd->setupTasklet, taskSetupBoard, (unsigned long)brd);
	tasklet_init(&brd->getSampleTasklet, taskGetA2DSample, 
		     (unsigned long)brd);
	tasklet_init(&brd->resetTasklet, taskResetBoard, (unsigned long)brd);

	error = -EBUSY;
	addr =  IoPort[ib] + SYSTEM_ISA_IOPORT_BASE;
	// Get the mapped board address
	if (! request_region(addr, A2DIOWIDTH, "NCAR A/D")) {
	    KLOG_ERR("ioport at 0x%x already in use\n", addr);
	    goto err;
	}

	brd->addr = addr;
	brd->chan_addr = addr + A2DIOLOAD;
    }

    KLOG_DEBUG("A2D init_module complete.\n");

    /*
     * Initialize and add the user-visible devices for the A/D functions
     */
    if ((error = alloc_chrdev_region(&A2DDevStart, 0, NumBoards, 
				     DEVNAME_A2D)) < 0)
    {
	KLOG_ERR("Error %d allocating device major number for '%s'\n",
		 -error, DEVNAME_A2D);
        goto err;
    }
    else
	KLOG_NOTICE("Got major device number %d for '%s'\n",
		    MAJOR(A2DDevStart), DEVNAME_A2D);
    
    cdev_init(&A2DCdev, &ncar_a2d_fops);
    if ((error = cdev_add(&A2DCdev, A2DDevStart, NumBoards)) < 0)
    {
	KLOG_ERR("cdev_add() for NCAR A/D failed!\n");
	goto err;
    }

    /*
     * Initialize and add the user-visible devices for I2C board temperature
     */
    if ((error = alloc_chrdev_region(&I2CTempDevStart, 0, NumBoards, 
				     DEVNAME_A2D_TEMP)) < 0)
    {
	KLOG_ERR("Error %d allocating device major number for '%s'\n",
		 -error, DEVNAME_A2D_TEMP);
        goto err;
    }
    else
	KLOG_NOTICE("Got major device number %d for '%s'\n",
		    MAJOR(I2CTempDevStart), DEVNAME_A2D_TEMP);
    
    cdev_init(&I2CTempCdev, &i2c_temp_fops);
    if ((error = cdev_add(&I2CTempCdev, I2CTempDevStart, NumBoards)) < 0)
    {
	KLOG_ERR("cdev_add() for NCAR A/D temperature sensors failed!\n");
	goto err;
    }

    return 0;

  err:
    
    if (BoardInfo) {
	for (ib = 0; ib < NumBoards; ib++) {
	    struct A2DBoard* brd = BoardInfo + ib;

//XX	    CLOSE USER-SPACE DEVICES

	    if (brd->addr)
		release_region(brd->addr, A2DIOWIDTH);
	    brd->addr = 0;
	}

    }

    kfree(BoardInfo);
    BoardInfo = 0;
    return error;
}
