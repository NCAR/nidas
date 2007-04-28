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

#include <linux/autoconf.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>	/* has to be before <linux/cdev.h>! GRRR! */
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>
#include <linux/version.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <nidas/linux/irigclock.h>
#include <nidas/linux/isa_bus.h>
#include <nidas/linux/klog.h>
#include <nidas/linux/ncar_a2d.h>

//#define DEBUG

MODULE_AUTHOR("Chris Burghart <burghart@ucar.edu>");
MODULE_DESCRIPTION("NCAR A/D driver");

// Clock/data line bits for i2c interface
static const unsigned long I2CSCL = 0x2;
static const unsigned long I2CSDA = 0x1;

#define DO_A2D_STATRD
#define IGNORE_DATA_STATUS
//#define TEMPDEBUG

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

module_param_array(IoPort, int, NULL, S_IRUGO);
module_param_array(Invert, bool, NULL, S_IRUGO);
module_param_array(Master, int, NULL, S_IRUGO);

MODULE_PARM_DESC(IoPort, "ISA port address of each board, e.g.: 0x3A0");
MODULE_PARM_DESC(Invert, "Whether to invert counts, default=1(true)");
MODULE_PARM_DESC(Master, 
		 "Master A/D for the board, default=first requested channel");

/* number of A2D boards in system (number of non-zero ioport values) */
static int NumBoards = 0;

static struct A2DBoard* BoardInfo = 0;
#define BOARD_INDEX(boardptr) (((boardptr) - BoardInfo) / \
				sizeof(struct A2DBoard))

/* 
 * Address for a specific channel on a board: board base address + 2 * channel
 */
static inline int
CHAN_ADDR(struct A2DBoard* brd, int channel)
{
    return(brd->base_addr + 2 * channel);
}

/*
 * prototypes
 */
int init_module(void);
void cleanup_module(void);
static int stopBoard(struct A2DBoard* brd);
static void getDSMSampleData(struct A2DBoard* brd, A2DSAMPLE* dsmSample);
static ssize_t ncar_a2d_read(struct file *filp, char __user *buf,
			     size_t count,loff_t *f_pos);
static int ncar_a2d_open(struct inode *inode, struct file *filp);
static int ncar_a2d_release(struct inode *inode, struct file *filp);
static unsigned int ncar_a2d_poll(struct file *filp, poll_table *wait);
static int ncar_a2d_ioctl(struct inode *inode, struct file *filp, 
			  unsigned int cmd, unsigned long arg);
//static ssize_t i2c_temp_read(struct file *filp, char __user *buf,
//			       size_t count,loff_t *f_pos);
//static int i2c_temp_open(struct inode *inode, struct file *filp);
//static int i2c_temp_release(struct inode *inode, struct file *filp);
static int ncar_a2dtemp_ioctl(struct inode *inode, struct file *filp, 
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
    .owner   = THIS_MODULE,
    .read    = ncar_a2d_read,
    .open    = ncar_a2d_open,
    .ioctl   = ncar_a2d_ioctl,
    .release = ncar_a2d_release,
    .poll    = ncar_a2d_poll,
};

static struct file_operations i2c_temp_fops = {
    .owner   = THIS_MODULE,
//    .read    = ncar_a2dtemp_read,
//    .open    = ncar_a2dtemp_open,
    .ioctl   = ncar_a2dtemp_ioctl,
//    .release = ncar_a2dtemp_release,
//    .poll    = ncar_a2dtemp_poll,
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

/*-----------------------Utility------------------------------*/
// I2C serial bus control utilities

static inline void 
i2c_clock_hi(struct A2DBoard* brd)
{
    brd->i2c |= I2CSCL;  // Set clock bit hi
    outb(A2DIO_WRCMD, brd->cmd_addr);  // Clock high
    outb(brd->i2c, brd->base_addr);
    udelay(1);
    return;
}

static inline void 
i2c_clock_lo(struct A2DBoard* brd)
{
    brd->i2c &= ~I2CSCL;  // Set clock bit low
    outb(A2DIO_WRCMD, brd->cmd_addr);  // Clock low
    outb(brd->i2c, brd->base_addr);
    udelay(1);
    return;
}

static inline void 
i2c_data_hi(struct A2DBoard* brd)
{
    brd->i2c |= I2CSDA;  // Set data bit hi
    outb(A2DIO_WRCMD, brd->cmd_addr);  // Data high
    outb(brd->i2c, brd->base_addr);
    udelay(1);
    return;
}

static inline void 
i2c_data_lo(struct A2DBoard* brd)
{
    brd->i2c &= ~I2CSDA;  // Set data bit lo
    outb(A2DIO_WRCMD, brd->cmd_addr);  // Data high
    outb(brd->i2c, brd->base_addr);
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
	t1 = 0x1 & inb(brd->base_addr);
	b1 = (b1 << 1) | t1;
	// lower clock
	i2c_clock_lo(brd);
    }

    // Send the acknowledge bit
    i2c_data_lo(brd);
    i2c_clock_hi(brd);
    i2c_clock_lo(brd);

    // shift in the second data byte
    b2 = 0;
    for (i = 0; i < 8; i++)
    {
	i2c_clock_hi(brd);
	t1 = 0x1 & inb(brd->base_addr);
	b2 = (b2 << 1) | t1;
	i2c_clock_lo(brd);
    }

    // a stop state is signalled by data going from
    // lo to hi, when clock is high.
    i2c_data_lo(brd);
    i2c_clock_hi(brd);
    i2c_data_hi(brd);

    x = (short)(b1<<8 | b2)>>3;

    return x;
}

/*-----------------------End I2C Utils -----------------------*/


/*-----------------------Utility------------------------------*/


/*
 * Wait up to maxusecs useconds for the interrupt bit for the 
 * selected channel to be set.
 */
inline static int 
waitForChannelInterrupt(struct A2DBoard* brd, int channel, int maxusecs)
{
   int cnt;
   int interval = 10; /* usec delay between checks */
   unsigned char interrupts;
   unsigned char mask = (1 << channel);

   outb(A2DIO_RDINTR, brd->cmd_addr);
   for (cnt = 0; cnt <= (maxusecs / interval); cnt++) {
      interrupts = inb(brd->base_addr);
      if ((interrupts & mask) != 0)
         return 0;
      udelay(interval);
   }
   return -ETIMEDOUT;
}


// Read status of AD7725 A/D chip specified by channel 0-7
static unsigned short
AD7725Status(struct A2DBoard* brd, int channel)
{
    outb(A2DIO_RDCHANSTAT, brd->cmd_addr);
    return (inw(CHAN_ADDR(brd, channel)));
}

static void 
AD7725StatusAll(struct A2DBoard* brd)
{
    int i;
    for (i = 0; i < MAXA2DS; i++)
	if (brd->requested[i])
	    brd->cur_status.goodval[i] = AD7725Status(brd, i);
    
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

    if (status_instr != expected)
    {
	KLOG_DEBUG("status 0x%04x "
		   "(instr bits: actual 0x%04x, expected 0x%04x).\n", 
		   status, status_instr, expected);
    }

    return (status_instr == expected);
}

    
static inline int
A2DConfirmInstruction(struct A2DBoard* brd, int channel, unsigned short instr)
{
    unsigned short status = AD7725Status(brd, channel);
    int ok = confirm7725Instruction(status, instr);

    if (! ok)
	KLOG_DEBUG("Instruction 0x%04x on channel %d not confirmed\n", 
		   instr, channel);

    return ok;
}

static inline int
DSMSampleSize(struct A2DBoard* brd)
{
    return(SIZEOF_DSM_SAMPLE_HEADER + 
	   (2 * brd->sampsPerCallback * brd->nRequestedChannels));
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

static int 
A2DSetGain(struct A2DBoard* brd, int channel)
{
    A2D_SET *a2d;
    int a2dGain;
    int a2dGainMul;
    int a2dGainDiv;
    unsigned short gainCode;

    // If no A/D selected return error -1
    if (channel < 0 || channel >= MAXA2DS)
	return -EINVAL;

    a2d = &brd->config;
    a2dGain = a2d->gain[channel];
    a2dGainMul = a2d->gainMul[channel];
    a2dGainDiv = a2d->gainDiv[channel];
    gainCode = 0;

    // unused channel gains are set to zero in the configuration
    if (a2dGain != 0)
	gainCode = (unsigned short)(a2dGainMul * a2dGain / a2dGainDiv);

    // The new 12-bit DAC has lower input resistance (7K ohms as opposed
    // to 10K ohms for the 8-bit DAC). The gain is the ratio of the amplifier
    // feedback resistor to the input resistance. Therefore, for the same
    // gain codes, the new board will yield higher gains by a factor of
    // approximately 1.43. I believe you will want to divide the old gain
    // codes (0-255) by 1.43. The new gain code will be between
    // 0 - 4095. So, after you divide the old gain code by 1.43 you
    // will want to multiply by 16. This is the same as multiplying
    // by 16/1.43 = 11.2.
    if (a2d->offset[channel]) {
	if (a2d->gain[channel] == 10)
	    gainCode = 0x1000 + channel;  //   0 to +20  ???
	else if (a2d->gain[channel] == 20)
	    gainCode = 0x4000 + channel;  //   0 to +10
	else if (a2d->gain[channel] == 40)
	    gainCode = 0x8000 + channel;  //   0 to +5
	else
	    gainCode = 0x0000 + channel;
    } else {
	if (a2d->gain[channel] == 10)
	    gainCode = 0x1900 + channel;  // -10 to +10
	else
	    gainCode = 0x0000 + channel;
    }
    // 1.  Write (or set) D2A0. This is accomplished by writing to the A/D
    // with the lower four address bits (SA0-SA3) set to all "ones" and the
    // data bus to 0x03.
    //   KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_D2A0, brd->cmd_addr);
    outb(A2DIO_D2A0, brd->cmd_addr);
    mdelay(10);
    // 2. Then write to the A/D card with lower address bits set to "zeros"
    // and data bus set to the gain value for the specific channel with the
    // upper data three bits equal to the channel address. The lower 12
    // bits are the gain code and data bit 12 is equal zero. So for channel
    // 0 write: (xxxxxxxxxxxx0000) where the x's are the gain code.
    KLOG_DEBUG("chn: %d   offset: %d   gain: %2d   outb( 0x%x, 0x%x)\n", 
	       channel, a2d->offset[channel], a2d->gain[channel], gainCode, 
	       brd->base_addr);
    // KLOG_DEBUG("outb( 0x%x, 0x%x);\n", gainCode, brd->base_addr);
    outw(gainCode, brd->base_addr);
    mdelay(10);
    return 0;
}

/*-----------------------Utility------------------------------*/
// A2DSetMaster routes the interrupt signal from the target A/D chip
// to the ISA bus interrupt line.

static int 
A2DSetMaster(struct A2DBoard* brd, int channel)
{
    if (channel < 0 || channel >= MAXA2DS) {
	KLOG_ERR("bad master chip number: %d\n", channel);
	return -EINVAL;
    }

    if (NO_CHANNEL_7 && channel == 7)
    {
	KLOG_ERR("Channel 7 is not available on 'Vulcan-ized' cards!\n");
	return -EINVAL;
    }

    KLOG_DEBUG("A2DSetMaster, Master=%d\n", channel);
    outb(A2DIO_WRMASTER, brd->cmd_addr);
    outb((char)channel, brd->base_addr);
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
    {
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
static void 
A2DSetCal(struct A2DBoard* brd)
{
    unsigned short OffChans = 0;
    unsigned short CalChans = 0;
    int i;

    // Change the calset array of bools into a byte
    for (i = 0; i < MAXA2DS; i++)
    {
	OffChans >>= 1;
	CalChans >>= 1;
	if (brd->config.offset[i] != 0) OffChans += 0x80;
	if (brd->cal.calset[i] != 0)    CalChans += 0x80;
    }
    // Point at the system control input channel
    outb(A2DIO_WRCALOFF, brd->cmd_addr);
    KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_WRCALOFF, brd->cmd_addr);

    // Set the appropriate bits in OffCal
    brd->OffCal = (OffChans<<8) & 0xFF00;
    brd->OffCal |= CalChans;
    brd->OffCal = ~(brd->OffCal) & 0xFFFF; // invert bits

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
static void 
A2DSetOffset(struct A2DBoard* brd)
{
    unsigned short OffChans = 0;
    int i;

    // Change the offset array of bools into a byte
    for (i = 0; i < MAXA2DS; i++)
    {
	OffChans >>= 1;
	if (brd->config.offset[i] != 0) OffChans += 0x80;
    }
    // Point at the system control input channel
    outb(A2DIO_WRCALOFF, brd->cmd_addr);
    KLOG_DEBUG("outb( 0x%x, 0x%x);\n", A2DIO_WRCALOFF, brd->cmd_addr);

    // Set the appropriate bits in OffCal
    brd->OffCal = (OffChans<<8) & 0xFF00;
    brd->OffCal = ~(brd->OffCal) & 0xFFFF; // invert bits

    // Send OffCal word to system control word
    outw(brd->OffCal, brd->base_addr);
    KLOG_DEBUG("JDW brd->OffCal:  0x%04x\n", brd->OffCal);
}

/*-----------------------Utility------------------------------*/
// Set A2D SYNC flip/flop.  This stops the A/D's until cleared
// under program control or by a positive 1PPS transition
// 1PPS enable must be asserted in order to sync on 1PPS

static void 
A2DSetSYNC(struct A2DBoard* brd)
{
    outb(A2DIO_FIFO, brd->cmd_addr);

    brd->FIFOCtl |= A2DSYNC;  // Ensure that SYNC bit in FIFOCtl is set.

    // Cycle the sync clock while keeping SYNC bit high
    outb(brd->FIFOCtl,              brd->base_addr);
    outb(brd->FIFOCtl | A2DSYNCCK,  brd->base_addr);
    outb(brd->FIFOCtl,              brd->base_addr);
    return;
}

/*-----------------------Utility------------------------------*/
// Clear the SYNC flag

static void 
A2DClearSYNC(struct A2DBoard* brd)
{
    outb(A2DIO_FIFO, brd->cmd_addr);

    brd->FIFOCtl &= ~A2DSYNC;  // Ensure that SYNC bit in FIFOCtl is cleared.

    // Cycle the sync clock while keeping sync lowthe SYNC data line
    outb(brd->FIFOCtl,              brd->base_addr);
    outb(brd->FIFOCtl | A2DSYNCCK,  brd->base_addr);
    outb(brd->FIFOCtl,              brd->base_addr);
    return;
}

/*-----------------------Utility------------------------------*/
// Enable 1PPS sync

static void 
A2DEnable1PPS(struct A2DBoard* brd)
{
    // Point at the FIFO control byte
    outb(A2DIO_FIFO, brd->cmd_addr);

    // Set the 1PPS enable bit
    outb(brd->FIFOCtl | A2D1PPSEBL, brd->base_addr);

    return;
}

/*-----------------------Utility------------------------------*/
// Clear (reset) the data FIFO

static void 
A2DClearFIFO(struct A2DBoard* brd)
{
    // Point to FIFO control byte
    outb(A2DIO_FIFO, brd->cmd_addr);

    brd->FIFOCtl &= ~FIFOCLR;  // Ensure that FIFOCLR bit is not set in FIFOCtl

    outb(brd->FIFOCtl,           brd->base_addr);
    outb(brd->FIFOCtl | FIFOCLR, brd->base_addr);  // cycle bit 0 to clear FIFO
    outb(brd->FIFOCtl,           brd->base_addr);

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
static inline unsigned short
A2DBoardStatus(struct A2DBoard* brd)
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
static inline int 
getA2DFIFOLevel(struct A2DBoard* brd)
{
    unsigned short stat = A2DBoardStatus(brd);

    // If FIFONOTFULL is 0, fifo IS full
    if ((stat & FIFONOTFULL) == 0) return 5;

    // If FIFONOTEMPTY is 0, fifo IS empty
    else if ((stat & FIFONOTEMPTY) == 0) return 0;

    // Figure out which 1/4 of the 1024 FIFO words we're filled to

    // bit 0, 0x1, set when FIFO >= half full
    // bit 1, 0x2, either almost full (>=3/4) or almost empty (<=1/4).
    switch (stat&0x03) // Switch on stat's 2 LSB's
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
    return -1;    // can't happen, but avoid compiler warn
}

/*-----------------------Utility------------------------------*/
// This routine sends the ABORT command to A/D chips.
// The ABORT command amounts to a soft reset--they stay configured.

static void 
A2DStopRead(struct A2DBoard* brd, int channel)
{
    // Point to the A2D command register
    outb(A2DIO_WRCMD, brd->cmd_addr);

    // Send specified A/D the abort (soft reset) command
    outw(AD7725_ABORT, CHAN_ADDR(brd, channel));
    return;
}

/*-----------------------Utility------------------------------*/
// Send ABORT command to all A/D chips

static void 
A2DStopReadAll(struct A2DBoard* brd)
{
    int i;
    for (i = 0; i < MAXA2DS; i++)
	if (brd->requested[i])
	    A2DStopRead(brd, i);
}

/*-----------------------Utility------------------------------*/
// This routine sets all A/D chips to auto mode

static void 
A2DAuto(struct A2DBoard* brd)
{
    // Point to the FIFO Control word
    outb(A2DIO_FIFO, brd->cmd_addr);

    // Set Auto run bit and send to FIFO control byte
    brd->FIFOCtl |=  A2DAUTO;
    outb(brd->FIFOCtl, brd->base_addr);
    return;
}

/*-----------------------Utility------------------------------*/
// This routine sets all A/D chips to non-auto mode

static void 
A2DNotAuto(struct A2DBoard* brd)
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

static void 
A2DStart(struct A2DBoard* brd, int channel)
{
    // Point at the A/D command channel
    outb(A2DIO_WRCMD, brd->cmd_addr);

    // Start the selected A/D
    outw(AD7725_READDATA, CHAN_ADDR(brd, channel));
    return;
}

static void 
A2DStartAll(struct A2DBoard* brd)
{
    int i;
    for (i = 0; i < MAXA2DS; i++)
	if (brd->requested[i])
	    A2DStart(brd, i);
}

/*-----------------------Utility------------------------------*/
// Configure A/D channel with coefficient array 'filter'
static int 
A2DConfig(struct A2DBoard* brd, int channel)
{
    int coef;
    unsigned short stat;
    int nCoefs = sizeof(brd->config.filter) / sizeof(brd->config.filter[0]);

    KLOG_NOTICE("Configuring board %d/channel %d\n", BOARD_INDEX(brd), 
		channel);
    if (channel < 0 || channel >= MAXA2DS)
	return -EINVAL;

    // Set up to write a command to a channel
    outb(A2DIO_WRCMD, brd->cmd_addr);

    // Set configuration write mode for our channel
    outw(AD7725_WRCONFIG, CHAN_ADDR(brd, channel));

    // Verify that the command got there...
    if (! A2DConfirmInstruction(brd, channel, AD7725_WRCONFIG))
    {
	stat = AD7725Status(brd, channel);
	KLOG_ERR("Failed confirmation for A/D instruction 0x%04x on "
		 "channel %d, status=0x%04x\n", AD7725_WRCONFIG, channel, 
		 stat);
	return -EIO;
    }

    // Wait for interrupt bit to set
    if (waitForChannelInterrupt(brd, channel, 250) != 0)
    {
	KLOG_ERR("Timeout waiting before sending coefficients on channel %d\n",
		 channel);
	return -ETIMEDOUT;
    }

    for (coef = 0; coef < nCoefs; coef++)
    {
	// Set up for config write and write out coefficient
	outb(A2DIO_WRCOEF, brd->cmd_addr);
	outw(brd->config.filter[coef], CHAN_ADDR(brd, channel));

	if (waitForChannelInterrupt(brd, channel, 250) != 0)
	{
	    KLOG_ERR("Timeout waiting after coefficient %d on channel %d\n",
		     coef, channel);
	    return -ETIMEDOUT;
	}
	outb(A2DIO_RDINTR, brd->cmd_addr);

	// Read status word from target a/d and check for errors
	stat = AD7725Status(brd, channel);

	if (stat & A2DIDERR)
	{
	    KLOG_ERR("Bad ID value on coefficient %d\n", coef);
	    return -EIO;
	}

	if (stat & A2DCRCERR)
	{
	    KLOG_ERR("BAD CRC @ coefficient %d", coef);
	    return -EIO;
	}
    }

    // We should have CFGEND status now (channel configured and ready)
    stat = AD7725Status(brd, channel);
    if ((stat & A2DCONFIGEND) == 0)
    {
	KLOG_ERR("CFGEND bit not set in status after configuring channel %d\n",
		 channel);
	return -EIO;
    }

    outb(A2DIO_RDCHANSTAT, brd->cmd_addr);
    brd->cur_status.goodval[channel] = inw(CHAN_ADDR(brd, channel));
    return 0;
}

/*-----------------------Utility------------------------------*/
// Configure all A/D's with same filter

static int 
A2DConfigAll(struct A2DBoard* brd)
{
    int ret = 0;
    int i;
    for (i = 0; i < MAXA2DS; i++)
    {
	if (brd->requested[i] && (ret = A2DConfig(brd, i)) < 0)
	    return ret;
    }
    return 0;
}

// the status bits are in the upper byte contain the serial number
static int 
getSerialNumber(struct A2DBoard* brd)
{
    unsigned short stat = A2DBoardStatus(brd);
    // fetch serial number
    KLOG_DEBUG("brd->cmd_addr: 0x%04x, stat: 0x%04x\n", brd->cmd_addr, stat);
    return (stat >> 6);  // S/N is upper 10 bits
}


/*-----------------------Utility------------------------------*/
// Utility function to wait for INV1PPS to be zero.
// Return: negative errno, or 0=OK.

static int 
waitFor1PPS(struct A2DBoard* brd)
{
    unsigned short stat;
    unsigned int uwait = 50;
    int timeit = 0;

    while (timeit++ < (2000000 / uwait))  // wait up to two seconds
    {
	if (brd->interrupted) return -EINTR;
	// Read status, check INV1PPS bit
	stat = A2DBoardStatus(brd);
	if ((stat & INV1PPS) == 0) {
	    KLOG_DEBUG("Found 1PPS after %d usecs\n", timeit * uwait);
	    return 0;
	}
	
	udelay(uwait);  // Wait uwait usecs and try again
    }
    KLOG_ERR("1PPS not detected--no sync to GPS\n");
    return -ETIMEDOUT;
}

static int 
A2DSetGainAndOffset(struct A2DBoard* brd)
{
    int i;
    int ret;
    int repeat;

#ifdef DO_A2D_STATRD
    brd->FIFOCtl = A2DSTATEBL;   // Clear most of FIFO Control Word
#else
    brd->FIFOCtl = 0;            // Clear FIFO Control Word
#endif

    brd->OffCal = 0x0;

    // HACK! the CPLD logic needs to be fixed!	
    for (repeat = 0; repeat < 3; repeat++) {
	for (i = 0; i < MAXA2DS; i++)
	{
	    /*
	     * Set gain for requested channels
	     */
	    if (brd->requested[i] && (ret = A2DSetGain(brd, i)) != 0)
		    return ret;
	}
	outb(A2DIO_D2A1, brd->cmd_addr);
	mdelay(10);
	outb(A2DIO_D2A2, brd->cmd_addr);
	mdelay(10);
	outb(A2DIO_D2A1, brd->cmd_addr);
	mdelay(10);
    } 
    // END HACK!


    brd->cur_status.ser_num = getSerialNumber(brd);

    A2DSetOffset(brd);

    KLOG_DEBUG("success!\n");
    return 0;
}

/*
 * Perform board setup.
 */
static void
taskSetupBoard(unsigned long taskletArg)
{
    struct A2DBoard* brd = (struct A2DBoard*)taskletArg;
    int ret = 0;
    int haveMaster = 0;
    int i;

    KLOG_DEBUG("In taskSetupBoard\n");

    /*
     * Set the master now if we were given an explicit one
     */
    if (Master[BOARD_INDEX(brd)] >= 0)
    {
	if ((ret = A2DSetMaster(brd, Master[BOARD_INDEX(brd)])) < 0)
	    goto done;
	haveMaster = 1;
    }
    
    /*
     * Figure out requested channels and maximum sample rate.
     */
    brd->nRequestedChannels = 0;

    for (i = 0; i < MAXA2DS; i++)
    {
	if (brd->config.Hz[i] > brd->MaxHz)
	    brd->MaxHz = brd->config.Hz[i];
	/*
	 * Requested channels are those with Hz > 0
	 */
	brd->requested[i] = (brd->config.Hz[i] > 0);

	if (brd->requested[i]) 
	{
	    brd->nRequestedChannels++;
	    /*
	     * Make this channel the master if we don't have one yet
	     */
	    if (! haveMaster)
	    {
		if ((ret = A2DSetMaster(brd, i)) < 0)
		    goto done;
		haveMaster = 1;
	    }
	}
    }

    /*
     * Make sure nobody requests channel 7 on 'Vulcan'-ized cards.
     */
    if (NO_CHANNEL_7 && brd->requested[7])
    {
	KLOG_ERR("Channel 7 is not available on 'Vulcan'-ized cards!\n");
	ret = -EINVAL;
	goto done;
    }

    // Configure DAC gain codes
    if ((ret = A2DSetGainAndOffset(brd)) != 0)
	goto done;
    
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
    mdelay(20);

    // Then do a soft reset
    KLOG_DEBUG("Soft resetting A/D's\n");
    A2DStopReadAll(brd);

    // Configure the A/D's
    KLOG_DEBUG("Sending filter config data to A/Ds\n");
    if ((ret = A2DConfigAll(brd)) != 0) 
	goto done;

    // Reset the A/D's
    KLOG_DEBUG("Resetting A/Ds\n");
    A2DStopReadAll(brd);

    mdelay(1);  // Give A/D's a chance to load
    KLOG_DEBUG("A/Ds ready for synchronous start\n");

  done:
    if (ret != 0)
	KLOG_NOTICE("setup for board %d failed with state %d\n",
		    BOARD_INDEX(brd), ret);
    else
	KLOG_DEBUG("setup for board %d succeeded\n", BOARD_INDEX(brd));

    brd->setupStatus = ret;
    complete_all(&brd->setupCompletion);
}

/*
 * Function scheduled to be called from IRIG driver at 100Hz frequency.
 * Read the next DSM sample (which may contain a number of subsamples
 * if the card is sampling faster than 100Hz), and put it into the
 * kfifo buffer for later transfer to user space.
 */
static void 
ReadSampleCallback(void *ptr)
{
    struct A2DBoard* brd = (struct A2DBoard*) ptr;
    static int entrycount = 0;
    int preFlevel, postFlevel;
    static int consecutiveNonEmpty = 0;
    A2DSAMPLE dsmSample;

    /*
     * If a board reset is pending, we're done already.
     * If the last reset didn't work properly, stop the card and bail out.
     */
    if (brd->resetStatus == WAITING_FOR_RESET)
	goto done;
    else if (brd->resetStatus != 0)
    {
	KLOG_ERR("stopping board %d because of bad reset status %d\n",
		 BOARD_INDEX(brd), -brd->resetStatus);
	stopBoard(brd);
	goto done;
    }

    /*
     * Clear the card's FIFO if we were asked to discard a scan
     */
    if (brd->discardNextScan)
    {
	A2DClearFIFO(brd);
	brd->discardNextScan = 0;
	goto done;
    }

    /*
     * How full is the card's FIFO?
     */
    preFlevel = getA2DFIFOLevel(brd);
    brd->cur_status.preFifoLevel[preFlevel]++;

    /*
     * If FIFO is empty, just return
     */
    if (preFlevel == 0)
    {
	if (!(brd->cur_status.preFifoLevel[0] % 100))
	    KLOG_NOTICE("empty FIFO %d\n", brd->cur_status.preFifoLevel[0]);
	goto done;
    }
    
    /*
     * If FIFO is full, there's a problem
     */
    if (preFlevel == 5) 
    {
	KLOG_ERR("%d Restarting card with full FIFO @ %ld\n", entrycount,
		 GET_MSEC_CLOCK);
	brd->resetStatus = WAITING_FOR_RESET;
	tasklet_schedule(&brd->resetTasklet);
	goto done;
    }

    /*
     * Get the next DSM sample of data from the card
     */
    getDSMSampleData(brd, &dsmSample);
    
    /*
     * Build the DSM sample header, setting the time tag to now, and 
     * adjusting time tag to time of first subsample.
     */
    dsmSample.timetag = GET_MSEC_CLOCK;
    if (dsmSample.timetag < brd->ttMsecAdj)
	dsmSample.timetag += MSECS_PER_DAY;
    dsmSample.timetag -= brd->ttMsecAdj;

    dsmSample.length = DSMSampleSize(brd) - SIZEOF_DSM_SAMPLE_HEADER;

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
	consecutiveNonEmpty++;
    else
	consecutiveNonEmpty = 0;
    
    if (consecutiveNonEmpty == 100)
    {
	KLOG_NOTICE("Clearing card FIFO after %d consecutive non-empty ends\n",
		    consecutiveNonEmpty);
	consecutiveNonEmpty = 0;
	A2DClearFIFO(brd);
    }

    /*
     * Copy this DSM sample to the kfifo, if there's enough space for it
     */
    if ((brd->buffer->size - kfifo_len(brd->buffer)) >= DSMSampleSize(brd))
	kfifo_put(brd->buffer, (unsigned char*)&dsmSample, 
		  DSMSampleSize(brd));
    else
    {
	if (!(brd->skippedSamples % 100))
	    KLOG_WARNING("DSM sample @ %ld dropped "
			 "(not enough kfifo space %d < %d)\n",
			 dsmSample.timetag, 
			 brd->buffer->size - __kfifo_len(brd->buffer),
			 DSMSampleSize(brd));
	brd->skippedSamples++;
    }

    /*
     * Update stats every 10 seconds
     */
    if (!(brd->readCtr % (INTERRUPT_RATE * 10))) {
	brd->nbadScans = 0;

	/*
	 * copy current status to prev_status for access by ioctl
	 * A2D_GET_STATUS
	 */
	brd->cur_status.nbadFifoLevel = brd->nbadFifoLevel;
	brd->cur_status.fifoNotEmpty = brd->fifoNotEmpty;
	brd->cur_status.skippedSamples = brd->skippedSamples;
	brd->cur_status.resets = brd->resets;
	memcpy(&brd->prev_status, &brd->cur_status, sizeof(A2D_STATUS));
	memset(&brd->cur_status, 0, sizeof(A2D_STATUS));
    }

    /*
     * Get board temperature data if requested
     */
    if (brd->doTemp) {
	brd->tempSamp.timetag = GET_MSEC_CLOCK;
	brd->tempSamp.length = sizeof(short);
	brd->tempSamp.data = A2DTemp(brd);
	brd->tempReady = 1;
	brd->doTemp = 0;

	KLOG_NOTICE("Board temp. %d.%1d C\n", brd->tempSamp.data / 16, 
		    (10 * (brd->tempSamp.data % 16)) / 16);
    }

  done:
    /*
     * Wake up waiting reads
     */
    wake_up_interruptible(&(brd->rwaitq));

    entrycount++;
}


/*
 * Fill in the data portion of the given A2DSAMPLE from the next data
 * off the card.
 */
static void
getDSMSampleData(struct A2DBoard* brd, A2DSAMPLE* dsmSample)
{
    short *data;
    int s;
    int chan;
    
    data = dsmSample->data;

    /*
     * Read the data for the DSM sample from the card.  Note that a DSM
     * sample will contain brd->sampsPerCallback individual samples for
     * each requested channel.
     */
    outb(A2DIO_FIFO, brd->cmd_addr);    // Set up to read data

    for (s = 0; s < brd->sampsPerCallback; s++) 
    {
	for (chan = 0; chan < MAXA2DS; chan++) 
	{
	    short counts;
#ifdef DO_A2D_STATRD
	    /*
	     * Read (and ignore) the status word that precedes the data word.
	     */
	    inw(brd->base_addr);
#endif
	    /*
	     * Read the data word and stash it if it's from a requested 
	     * channel.  Note that inw on the Vulcan munges things to 
	     * local CPU (i.e. big-endian) order, and in the dsm_sample_t,
	     * the data should be in the order that came from the card, so we
	     * apply cpu_to_le16() to munge the bytes back...
	     */
	    counts = inw(brd->base_addr);
	    if (brd->requested[chan])
		*data++ = cpu_to_le16((brd->invertCounts) ? -counts : counts);
	}
    }
}


// Callback function to send I2C temperature data to user space.

//XX static void 
//XX temperatureIrigCallback(void *ptr)
//XX {
//XX     struct A2DBoard* brd = (struct A2DBoard*)ptr;
//XX     brd->doTemp = 1;
//XX }

//XX static int 
//XX ncar_a2dtemp_open(struct inode *inode, struct file *filp)
//XX {
//XX     struct A2DBoard *brd = BoardInfo + iminor(inode);
//XX 
//XX     brd->i2c = 0x3;
//XX 
//XX     register_irig_callback(temperatureIrigCallback, brd->tempRate, brd);
//XX 
//XX     return 0;
//XX }

//XX static int 
//XX ncar_a2dtemp_release(struct inode *inode, struct file *filp)
//XX {
//XX     struct A2DBoard *brd = BoardInfo + iminor(inode);
//XX 
//XX     unregister_irig_callback(temperatureIrigCallback, brd->i2cTempRate, brd);
//XX 
//XX     brd->doTemp = 0;
//XX     return 0;
//XX }

/*
 * User-side temperature read function
 */
//XX static ssize_t 
//XX ncar_a2dtemp_read(struct file *filp, char __user *buf, size_t count, 
//XX 		  loff_t *f_pos)
//XX {
//XX     struct A2DBoard *brd = (struct A2DBoard*)filp->private_data;
//XX     int n;
//XX     int retval = 0;
//XX 
//XX     /*
//XX      * Wait (or return for non-blocking read) if data aren't ready
//XX      */
//XX     while (! brd->tempReady)
//XX     {
//XX 	/*
//XX 	 * Return now for a non-blocking read
//XX 	 */
//XX 	if (filp->f_flags & O_NONBLOCK)
//XX 	    return -EAGAIN;
//XX 	/*
//XX 	 * Wait for data to be ready
//XX 	 */
//XX 	if (wait_event_interruptible(brd->rwaitq, brd->tempReady))
//XX 	    return -ERESTARTSYS;
//XX 	/*
//XX 	 * Regain our lock on the dev 
//XX 	 */
//XX     }
//XX     
//XX     n = (count > sizeof(I2C_TEMP_SAMPLE)) ? sizeof(I2C_TEMP_SAMPLE) : count;
//XX     retval = copy_to_user(buf, &(brd->tempSamp), n) ? -EFAULT : n;
//XX 
//XX     brd->tempReady = 0;
//XX     return retval;
//XX }

/**
 * Stop data collection on the selected board.
 */
static int
stopBoard(struct A2DBoard* brd)
{
    int ret = 0;

    brd->doTemp = 0;
    // interrupt the 1PPS or acquisition
    brd->interrupted = 1;

    // Turn off the callback routine
    unregister_irig_callback(ReadSampleCallback, IRIG_100_HZ, brd);

    // wake any waiting reads
    wake_up_interruptible(&(brd->rwaitq));
 
    AD7725StatusAll(brd);   // Read status and clear IRQ's
 
    A2DNotAuto(brd);     // Shut off auto mode (if enabled)
 
    // Abort all the A/D's
    A2DStopReadAll(brd);
 
    brd->busy = 0;       // Reset the busy flag
 
    return ret;
}

/* 
 * Reset the A2D.  This does an unregister_irig_callback, so it can't be
 * called from the irig callback function itself.  Schedule the board's
 * resetTasklet in that case.
 */
static int
resetBoard(struct A2DBoard* brd)
{
    int ret;

    unregister_irig_callback(ReadSampleCallback, IRIG_100_HZ, brd);

    // Sync with 1PPS
    KLOG_DEBUG("doing waitFor1PPS, GET_MSEC_CLOCK=%ld\n", GET_MSEC_CLOCK);
    if ((ret = waitFor1PPS(brd)) != 0)
	goto done;
    
    KLOG_DEBUG("Found initial PPS, GET_MSEC_CLOCK=%ld\n", GET_MSEC_CLOCK);
    
    A2DStopReadAll(brd);// Send Abort command to all A/Ds
    AD7725StatusAll(brd);	// Read status from all A/Ds

    A2DStartAll(brd);	// Start all the A/Ds
    AD7725StatusAll(brd);	// Read status again from all A/Ds

    A2DSetSYNC(brd);	// Stop A/D clocks
    A2DAuto(brd);	// Switch to automatic mode

    KLOG_DEBUG("Setting 1PPS Enable line\n");

    mdelay(20);
    A2DEnable1PPS(brd);// Enable sync with 1PPS

    KLOG_DEBUG("doing waitFor1PPS, GET_MSEC_CLOCK=%ld\n", GET_MSEC_CLOCK);
    if ((ret = waitFor1PPS(brd)) != 0)
	goto done;

    KLOG_DEBUG("Found second PPS, GET_MSEC_CLOCK=%ld\n", GET_MSEC_CLOCK);
    A2DClearFIFO(brd);  // Clear the board's FIFO...

    brd->discardNextScan = 1;  // whether to discard the initial scan
    brd->enableReads = 1;
    brd->interrupted = 0;
    brd->nbadScans = 0;
    brd->readCtr = 0;
    brd->nbadFifoLevel = 0;
    brd->fifoNotEmpty = 0;
    brd->skippedSamples = 0;
    brd->resets++;

    // start the IRIG callback routine at 100 Hz
    ret = register_irig_callback(ReadSampleCallback, IRIG_100_HZ, brd);
    if (ret != 0)
	KLOG_ERR("Error %d registering IRIG callback\n", ret);
    else
	brd->busy = 1;  // Set the busy flag
    KLOG_DEBUG("IRIG callback registered @ %ld\n", GET_MSEC_CLOCK);


  done:
    if (ret != 0)
	KLOG_WARNING("reset error %d for board %d\n", ret, BOARD_INDEX(brd));
    else
	KLOG_NOTICE("reset succeeded for board %d\n", BOARD_INDEX(brd));

    return ret;
}

/**
 * Start data collection on the selected board.  This is only called
 * via user space initiation (i.e., through the A2D_RUN ioctl).
 */
static int
startBoard(struct A2DBoard* brd)
{
    int ret;
    int nReadsPerCallback;
    
    KLOG_DEBUG("starting board %d\n", BOARD_INDEX(brd));
    
    brd->doTemp = 0;

    /*
     * Zero out status
     */
    memset(&brd->cur_status , 0, sizeof(A2D_STATUS));
    memset(&brd->prev_status, 0, sizeof(A2D_STATUS));

    /*
     * How many data values do we read per interrupt?
     */
    brd->sampsPerCallback = brd->MaxHz / INTERRUPT_RATE;
    nReadsPerCallback = MAXA2DS * brd->sampsPerCallback;
#ifdef DO_STAT_RD
    nReadsPerCallback *= 2; // twice as many reads if status is being sent
#endif

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
	(USECS_PER_SEC / INTERRUPT_RATE - USECS_PER_SEC / brd->MaxHz) /
	USECS_PER_MSEC;

    KLOG_DEBUG("sampsPerCallback=%d, ttMsecAdj=%d\n", brd->sampsPerCallback, 
	       brd->ttMsecAdj);

    brd->enableReads = 0;
    brd->interrupted = 0;

    /*
     * Finally reset, which will start collection.
     */
    brd->resetStatus = WAITING_FOR_RESET;
    tasklet_schedule(&brd->resetTasklet);
    ret = 0;

    if (ret == 0)
	KLOG_NOTICE("Board %d started\n", BOARD_INDEX(brd));
    else
	KLOG_WARNING("Start of board %d failed\n", BOARD_INDEX(brd));
  
    return ret;
}

/*
 * Task to perform a board reset.
 */
static void
taskResetBoard(unsigned long taskletArg)
{
    struct A2DBoard* brd = (struct A2DBoard*)taskletArg;

    brd->enableReads = 0;
    if ((brd->resetStatus = resetBoard(brd)) != 0)
	KLOG_WARNING("taskResetBoard() failed for board %d with status %d\n",
		     BOARD_INDEX(brd), brd->resetStatus);
}

/**
 * User-space open of the A/D device.
 */
static int 
ncar_a2d_open(struct inode *inode, struct file *filp)
{
    struct A2DBoard *brd = BoardInfo + iminor(inode);
    int errval;

    /*
     * Get the buffer to hold data eventually destined for user space
     */
    spin_lock_init(&brd->bufLock);
    brd->buffer = kfifo_alloc(A2D_BUFFER_SIZE, GFP_KERNEL, &brd->bufLock);
    if (IS_ERR(brd->buffer)) {
	errval = (int)brd->buffer;
	KLOG_ERR("Error %d from kfifo_alloc\n", errval);
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

    KLOG_NOTICE("Releasing board %d\n", BOARD_INDEX(brd));

    brd->doTemp = 0;
    brd->resets = 0;
    
    /*
     * interrupt the 1PPS or acquisition
     */
    brd->interrupted = 1;

    /*
     * Turn off the callback routine
     */
    stopBoard(brd);

    /*
     * Get rid of the FIFO to hold data destined for user space
     */
    kfifo_free(brd->buffer);
    brd->buffer = 0;

    return ret;
}

/**
 * Support for select/poll on the user device
 */
static unsigned int
ncar_a2d_poll(struct file *filp, poll_table *wait)
{
    struct A2DBoard *brd = (struct A2DBoard*)filp->private_data;

    poll_wait(filp, &brd->rwaitq, wait);

    if (kfifo_len(brd->buffer) >=  DSMSampleSize(brd))
	return(POLLIN | POLLRDNORM);  // ready to read
    else
	return 0;
}

/**
 * User-space read on the A/D device.
 */
static ssize_t 
ncar_a2d_read(struct file *filp, char __user *buf, size_t count, loff_t *pos)
{
    struct A2DBoard *brd = (struct A2DBoard*)filp->private_data;
    int nwritten;
    int fifo_len;
    int wlen;
    static int entrycount = 0;
    A2DSAMPLE dsmSamp;
    int ssize = DSMSampleSize(brd); // size of a DSM sample for this board

    if (brd->interrupted)
    {
        KLOG_DEBUG("returning EOF after board interrupt\n");
        return 0;
    }

    /*
     * Wait now for a sample to be available (or return EAGAIN for a
     * non-blocking read)
     */
    if (kfifo_len(brd->buffer) < ssize) {
        if (filp->f_flags & O_NONBLOCK) 
	    return -EAGAIN;
	if (wait_event_interruptible(brd->rwaitq, 
				     kfifo_len(brd->buffer) >= ssize))
	    return -ERESTARTSYS;
    }

    /*
     * How full is the buffer?
     */
    fifo_len = kfifo_len(brd->buffer);

    /*
     * Bytes to write to user
     */
    wlen = (count > fifo_len) ? fifo_len : count;

    /*
     * Adjust so we only send full samples
     */
    wlen = wlen - (wlen % ssize);
    
    /*
     * Copy to the user's buffer
     */
    nwritten = 0;
    while (nwritten < wlen) 
    {
        if (kfifo_get(brd->buffer, (void*)&dsmSamp, ssize) != ssize)
        {
            KLOG_WARNING("Did not get expected sample from kfifo\n");
            return -EFAULT;
        }

        if (copy_to_user(buf, &dsmSamp, ssize))
        {
            KLOG_WARNING("copy_to_user failure\n");
            return -EFAULT;
        }
        buf += ssize;

        nwritten += ssize;
    }

    return nwritten;
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
    int len = _IOC_SIZE(cmd);
    int ret = -EINVAL;
    
    KLOG_DEBUG("IOCTL for board %d: cmd=%x, len=%d\n", BOARD_INDEX(brd), 
	       cmd, len);

    switch (cmd)
    {
      case A2D_GET_STATUS:              /* user get of status */
	if (len != sizeof(A2D_STATUS)) 
	{
	    KLOG_ERR("A2D_GET_STATUS len %d != sizeof(A2D_STATUS)\n", len);
	    break;
	}
	ret = copy_to_user(userptr, &brd->prev_status, len);
	if (ret < 0)
	    break;
	ret = len;
	break;

      case A2D_SET_CONFIG:
	if (len != sizeof(A2D_SET)) 
	{
	    KLOG_ERR("A2D_SET_CONFIG len %d != sizeof(A2D_SET)\n", len);
	    break;     // invalid length
	}

	if (brd->busy) {
	    KLOG_WARNING("A/D card %d is running. Can't configure.\n", 
		BOARD_INDEX(brd));
	    ret = -EBUSY;
	    break;
	}
	ret = copy_from_user(&brd->config, userptr, len);
	if (ret < 0)
	    break;

	init_completion(&brd->setupCompletion);
	tasklet_schedule(&brd->setupTasklet);
	wait_for_completion(&brd->setupCompletion);

	ret = brd->setupStatus;
	break;

      case A2D_SET_CAL:
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

      case A2D_RUN:
	ret = startBoard(brd);
	break;

      case A2D_STOP:
	ret = stopBoard(brd);
	break;

      default:
	KLOG_ERR("Bad A2D ioctl 0x%x\n", cmd);
	break;
    }
    return ret;
}

static int 
ncar_a2dtemp_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, 
		   unsigned long arg)
{
    struct A2DBoard *brd = (struct A2DBoard*)filp->private_data;
    void __user* userptr = (void __user*)arg;
    int len = _IOC_SIZE(cmd);
    int rate;
    unsigned long flags;
    int ret = -EINVAL;

    KLOG_DEBUG("IOCTL cmd=%x board addr=0x%x len=%d\n",
	       cmd, brd->base_addr, len);

    switch (cmd)
    {
      case A2DTEMP_GET_TEMP:
	if (len != sizeof(short)) 
	    break;
	spin_lock_irqsave(&brd->bufLock, flags);
	ret = copy_to_user(userptr, &brd->tempSamp.data, len);
	spin_unlock_irqrestore(&brd->bufLock, flags);
	ret = sizeof(short);
	break;

      case A2DTEMP_SET_RATE:
	/*
	 * Set temperature query rate (using enum irigClockRates)
	 */
	if (len != sizeof(int)) 
	    break;
	copy_from_user(&rate, userptr, len);
	if (rate > IRIG_10_HZ)
	{
	    KLOG_WARNING("Illegal rate for A/D temp probe (> 10 Hz)\n");
	    break;
	}
	brd->tempRate = rate;
	break;

      default:
	KLOG_ERR("Bad A2DTEMP ioctl 0x%x\n", cmd);
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
    if (!BoardInfo) 
	return;

    unregister_chrdev_region(A2DDevStart, NumBoards);
    cdev_del(&A2DCdev);

    unregister_chrdev_region(I2CTempDevStart, NumBoards);
    cdev_del(&I2CTempCdev);


    for (ib = 0; ib < NumBoards; ib++) {
	struct A2DBoard* brd = BoardInfo + ib;

	stopBoard(brd);
	if (brd->buffer)
	{
	    kfifo_free(brd->buffer);
	    brd->buffer = 0;
	}

	AD7725StatusAll(brd);        // Read status and clear IRQ's

	if (brd->base_addr)
	    release_region(brd->base_addr, A2DIOWIDTH);
	brd->base_addr = 0;
    }

    kfree(BoardInfo);
    BoardInfo = 0;

    KLOG_NOTICE("NCAR A/D cleanup complete\n");

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
    {
	if (IoPort[ib] == 0) 
	    break;
#if defined(CONFIG_MACH_ARCOM_MERCURY)
	/* 
	 * Try to warn about 8-bit-only ioport values on the Vulcan.
	 * Since we can't query the settings for I/O window 1 for the
	 * PCI1520 CardBus bridge (which connects us to the PC/104 bus),
	 * this warning is only a guess.
	 */
	if (IoPort[ib] < 0x800)
	{
	    KLOG_WARNING("IoPort 0x%x for board %d is less than 0x800, "
			 "and this is probably an 8-bit-only port;\n",
			 IoPort[ib], ib);
	    KLOG_WARNING("(the port must lie in I/O window 1, as defined in "
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
    BoardInfo = kmalloc(NumBoards * sizeof(struct A2DBoard), GFP_KERNEL);
    if (!BoardInfo)
	goto err;

    /* initialize each A2DBoard structure */
    for (ib = 0; ib < NumBoards; ib++) {
	struct A2DBoard* brd = BoardInfo + ib;
	
	// initialize structure to zero, then initialize things
	// that are non-zero
	memset(brd, 0, sizeof(struct A2DBoard));

	/*
	 * Base address and command address
	 */
	brd->base_addr = IoPort[ib] + SYSTEM_ISA_IOPORT_BASE;
	brd->cmd_addr = brd->base_addr + A2DCMDADDR;

	// Request the necessary I/O region
	if (! request_region(brd->base_addr, A2DIOWIDTH, "NCAR A/D")) {
	    KLOG_ERR("ioport at 0x%x already in use\n", brd->base_addr);
	    goto err;
	}

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
	outw(AD7725_READDATA, brd->base_addr);	// start channel 0
	mdelay(20);				// wait a bit...
	outb(A2DIO_WRCMD, brd->cmd_addr);
	outw(AD7725_ABORT, brd->base_addr);	// stop channel 0
	outb(A2DIO_WRCMD, brd->cmd_addr);
	outw(AD7725_WRCONFIG, brd->base_addr);	// send WRCONFIG to channel 0
	// Make sure channel 0 status confirms receipt of AD7725_WRCONFIG cmd
	if (! A2DConfirmInstruction(brd, 0, AD7725_WRCONFIG)) 
	{
	    KLOG_ERR("Bad response on IoPort 0x%04x.  "
		     "Is there really an NCAR A/D card there?\n", IoPort[ib]);
	    goto err;
	}
	else
	    KLOG_NOTICE("NCAR A/D board confirmed at 0x%03x\n", 
			brd->base_addr);

	/*
	 * Do we tell the board to interleave status with data?
	 */
#ifdef DO_A2D_STATRD
	brd->FIFOCtl = A2DSTATEBL;
#else
	brd->FIFOCtl = 0;
#endif
	/*
	 * Default to 0.1 Hz rate for getting board temperature
	 */
	brd->tempRate = IRIG_0_1_HZ; // 0.1 Hz default rate for temp query

	brd->invertCounts = Invert[ib];

	/*
	 * Initialize the read wait queue
	 */
	init_waitqueue_head(&brd->rwaitq);

	/*
	 * Initialize our tasklets
	 */
	tasklet_init(&brd->setupTasklet, taskSetupBoard, (unsigned long)brd);
	tasklet_init(&brd->resetTasklet, taskResetBoard, (unsigned long)brd);

	/*
	 * Other initialization
	 */
	brd->nRequestedChannels = 0;
	memset(brd->requested, 0, sizeof(brd->requested));
    }

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

    KLOG_DEBUG("A2D init_module complete.\n");

    return 0;

  err:
    
    unregister_chrdev_region(I2CTempDevStart, NumBoards);
    cdev_del(&I2CTempCdev);

    unregister_chrdev_region(A2DDevStart, NumBoards);
    cdev_del(&A2DCdev);

    if (BoardInfo) {
	for (ib = 0; ib < NumBoards; ib++) {
	    struct A2DBoard* brd = BoardInfo + ib;

	    if (brd->base_addr)
		release_region(brd->base_addr, A2DIOWIDTH);
	    brd->base_addr = 0;
	}

    }

    kfree(BoardInfo);
    BoardInfo = 0;

    return error;
}
