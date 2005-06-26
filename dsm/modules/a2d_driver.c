// #define TEMPDEBUG
// #define BFIR
// #define DEBUGA2DGET
// #define DEBUGA2DGET2
#define DEBUGTIMING
#define DOA2DSTATRD

/*  a2d_driver.c/


Time-stamp: <Wed 13-Apr-2005 05:57:57 pm>

Drivers and utility modules for NCAR/ATD/RAF DSM3 A/D card.

Copyright 2005 UCAR, NCAR, All Rights Reserved

Original author:	Grant Gray

Revisions:

*/
//Clock/data line bits for i2c interface
#define		I2CSCL		0x2
#define		I2CSDA		0x1

//#define INBUFWR		// Turn on simulated data array printout
//#define	DEBUGDATA 	// Turns on printout in data simulator
#define	INTERNAL	// Uses internal A/D filter code
#define	QUIETOUTB	// Shut off printout
#define	QUIETINB	// Shut off printout
#define	QUIETOUTW	// Shut off printout
#define	QUIETINW	// Shut off printout

// #define		DEBUG0		//Activates specific debug functions
// #define NOIRIG		// DEBUG mode, running without IRIG card
#define		USE_RTLINUX 	//If not defined I/O is in debug mode

#define	DELAYNUM0	10		// Used for usleep	
#define	DELAYNUM1	1000	// Used for usleep	

#define	PPSTIMEOUT	240000	// Number of times to try for 1PPS low
#define GAZILLION 10000

/* RTLinux module includes...  */

#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_stdio.h>
#include <rtl_stdlib.h>
#include <rtl_string.h>
#include <rtl_fcntl.h>
#include <rtl_posixio.h>
#include <rtl_pthread.h>
#include <rtl_unistd.h>
#include <sys/rtl_stat.h>
#include <linux/ioport.h>

#include <ioctl_fifo.h>
#include <irigclock.h>
#include <a2d_driver.h>

/* ioport addresses of installed boards, 0=noboard */
static int ioport[MAX_A2D_BOARDS] = { 0x3A0, 0, 0, 0 };

/* number of A2D boards in system (number of non-zero ioport values) */
static int numboards = 0;

MODULE_AUTHOR("Grant Gray <gray@ucar.edu>");
MODULE_DESCRIPTION("HIAPER A/D driver for RTLinux");
RTLINUX_MODULE(DSMA2D);

MODULE_PARM(ioport, "1-" __MODULE_STRING(MAX_A2D_BOARDS) "i");
MODULE_PARM_DESC(ioport, "ISA port address of each board, e.g.: 0x3A0");

static struct A2DBoard* boardInfo = 0;

static const char* devprefix = "dsma2d";

/* number of devices on a board. This is the number of
 * /dev/dsma2d* devices, from the user's point of view, that one
 * board represents.  Device 0 is the 8 A2D
 * channels, device 1 is the temperature sensor.
 */
#define NDEVICES 2

int  init_module(void);
void cleanup_module(void);

/****************  IOCTL Section *************************/

static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,_IOC_SIZE(GET_NUM_PORTS) },
  { A2D_STATUS_IOCTL,_IOC_SIZE(A2D_STATUS_IOCTL) },
  { A2D_SET_IOCTL, sizeof(A2D_SET)  },
  { A2D_CAL_IOCTL, sizeof(A2D_CAL)  },
  { A2D_RUN_IOCTL,_IOC_SIZE(A2D_RUN_IOCTL) },
  { A2D_STOP_IOCTL,_IOC_SIZE(A2D_STOP_IOCTL) },
  { A2D_OPEN_I2CT,_IOC_SIZE(A2D_OPEN_I2CT) },
  { A2D_CLOSE_I2CT,_IOC_SIZE(A2D_CLOSE_I2CT) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);

/****************  End of IOCTL Section ******************/

void getIRIGClock(dsm_sample_time_t* msecp,long *nsecp)
{
    struct rtl_timespec tnow;
    irig_clock_gettime(&tnow);
    *msecp = (tnow.tv_sec % 86400) * 1000 + tnow.tv_nsec / 1000000;
    *nsecp = tnow.tv_nsec % 1000000;
}

/*-----------------------Utility------------------------------*/
// I2C serial bus control utilities
 
static inline void i2c_clock_hi(struct A2DBoard* brd)
{
	brd->i2c |= I2CSCL; // Set clock bit hi
	outb(A2DIOSTAT, brd->chan_addr);	// Clock high	
	outb(brd->i2c, (UC *)brd->addr);
	rtl_usleep(DELAYNUM0);
	return;	
}

static inline void i2c_clock_lo(struct A2DBoard* brd)
{
	brd->i2c &= ~I2CSCL; // Set clock bit low
	outb(A2DIOSTAT, brd->chan_addr);	// Clock low	
	outb(brd->i2c, brd->addr);
	rtl_usleep(DELAYNUM0);
	return;	
}

static inline void i2c_data_hi(struct A2DBoard* brd)
{
	brd->i2c |= I2CSDA; // Set data bit hi
	outb(A2DIOSTAT, brd->chan_addr);	// Data high	
	outb(brd->i2c, brd->addr);
	rtl_usleep(DELAYNUM0);
	return;	
}

static inline void i2c_data_lo(struct A2DBoard* brd)
{
	brd->i2c &= ~I2CSDA; // Set data bit lo
	outb(A2DIOSTAT, brd->chan_addr);	// Data high	
	outb(brd->i2c, brd->addr);
	rtl_usleep(DELAYNUM0);
	return;	
}
/*-----------------------End I2C Utils -----------------------*/

/*-----------------------Utility------------------------------*/
// Read on-board LM92 temperature sensor via i2c serial bus
// The signed short returned is weighted .0625 deg C per bit

static short A2DTemp(struct A2DBoard* brd)
{
   	unsigned char b1;
	unsigned char b2;
	unsigned char t1;
   	short x;
   	unsigned char i, address = 0x48;	// Address of temperature register
	
	
	// shift the address over one, and set the READ indicator
	b1 = (address << 1) | 1;

	// a start state is indicated by data going from hi to lo,
   	// when clock is high.
   	i2c_data_hi(brd);
   	i2c_clock_hi(brd);
   	i2c_data_lo(brd);
   	i2c_clock_lo(brd);
	i2c_data_hi(brd);	

   // Shift out the address/read byte
   	for (i = 0; i < 8; i++) 
	{
    	// set data line
    	if (b1 & 0x80) 
		{
           	i2c_data_hi(brd);
    	} 
		else 
		{
           	i2c_data_lo(brd);
     	}
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
	i2c_data_hi(brd);

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
	rtl_printf("b1=0x%02X, b2=0x%02X, b1b2>>3 ", b1, b2);
	rtl_printf("0x%04X, degC = %d.%1d\n", x, x/16, (10*(x%16))/16);
#endif
	return x;
}


/*-----------------------Utility------------------------------*/

//Read status of A2D chip specified by A2DSel 0-7

static US A2DStatus(struct A2DBoard* brd, int A2DSel)
{
	US stat;

	// Point at the A/D status channel
	outb(A2DSTATRD, brd->chan_addr);	// Point at Stat channel for read
	stat = inw(brd->addr + A2DSel*2);

	return(stat);
}

static void A2DStatusAll(struct A2DBoard* brd)
{
	int i;
	for(i = 0; i < MAXA2DS; i++)
	{
		brd->status.status[i] = A2DStatus(brd,i);
	}

	return;
}

/*-----------------------Utility------------------------------*/
// A2DCommand issues a command word to the A/D selected by A2DSel

static void A2DCommand(struct A2DBoard* brd,int A2DSel, US Command)
{
	// Point to A/D command write
	outb(A2DCMNDWR, brd->chan_addr);	

	// Write command to the selected A/D
	outw(Command, brd->addr);
}

/*-----------------------Utility------------------------------*/
// A2DSetGain sets the A/D Channel gain.
// Allowable gain values are 1 <= A2DGain <= 25.5
//   in 255 gain steps.
// The gain is calculated from gaincode (0-255) as:
//   gain = 5*5.12*gaincode/256 = .1*gaincode.
// The gain can go down to .7 before the system has saturation problems
// Check this 

static int A2DSetGain(struct A2DBoard* brd, int A2DSel, int A2DGain)
{
	unsigned int DACAddr;
	int D2AChsel = -1;
	UC GainCode = 1;

	//Check that gain is within limits
	if(A2DGain < 1 || A2DGain > 255) {
	    rtl_printf("bad gain value: %d\n",A2DGain);
	    return -EINVAL;
	}

	// If no A/D selected return error -1
	if(A2DSel < 0 || A2DSel >= MAXA2DS) return -EINVAL;

	if(A2DSel < 4)	// If setting gain on channels 0-3
	{
		D2AChsel = A2DIOGAIN03;	// Use this channel select
		DACAddr = brd->addr + A2DSel; // this address
	}
	else {
		D2AChsel = A2DIOGAIN47;		// Use this channel select and
		DACAddr = brd->addr + A2DSel - 4; // this address
	}

	// Point to the appropriate DAC channel
	outb(D2AChsel, brd->chan_addr);		// Set the card channel pointer

	GainCode = (UC)A2DGain;

	// Write the code to the selected DAC
	outb(GainCode, DACAddr);

	return 0;
}

/*-----------------------Utility------------------------------*/
//A2DSetMaster routes the interrupt signal from the target A/D chip 
//  to the ISA bus interrupt line.

static int A2DSetMaster(struct A2DBoard* brd,int A2DSel)
{
	if(A2DSel < 0 || A2DSel >= MAXA2DS) {
	    rtl_printf("A2DSetMaster, bad chip number: %d\n",
	    	A2DSel);
	    return -EINVAL;
	}
    A2DSel = 7;	 //DEBUG jamming this to 7
    rtl_printf("A2DSetMaster, master=%d\n",A2DSel);

	// Point at the FIFO status channel
	outb(A2DIOFIFOSTAT, brd->chan_addr);	

	// Write the master to register
	outb((char)A2DSel, brd->addr);
	return 0;
}

/*-----------------------Utility------------------------------*/

//Read the A/D chip interrupt line states

static UC A2DReadInts(struct A2DBoard* brd)
{
	//Point to the sysctl channel for read
	outb(A2DIOSYSCTL, brd->chan_addr);	

	// Read the interrupt word
	return inb(brd->addr);
}


/*-----------------------Utility------------------------------*/
//Set a calibration voltage
// Input is the calibration voltage * 8 expressed as an integer
//   i.e. -80 <= Vx8 <= +80 for -10 <= Calibration voltage <= +10

static int A2DSetVcal(struct A2DBoard* brd)
{
	US Vcode = 1; 

	//Check that V is within limits
	if(brd->cal.vcalx8 < -80 || brd->cal.vcalx8 > 80) return -EINVAL;

	// Point to the calibration DAC channel
	outb(A2DIOVCAL, brd->chan_addr);		

	Vcode = brd->cal.vcalx8 + 128 ;//Convert Vx8 to DAC code

	// Write cal code to calibration D/A
	outb(Vcode, brd->addr);

	return 0;
}

/*-----------------------Utility------------------------------*/

//Switch inputs specified by Chans bits to calibration mode:
// Cal bits are in lower byte

static void A2DSetCal(struct A2DBoard* brd)
{	
	US Chans = 0;
	int i;

	for(i = 0; i < MAXA2DS; i++)
	{
		Chans >>= 1; 
		if(brd->cal.calset[i] != 0)Chans += 0x80;
	}

	// Point at the system control input channel
	outb(A2DIOSYSCTL, brd->chan_addr);

	//Set the appropriate bits in OffCal
	brd->OffCal |= (US)((Chans) & 0x00FF);
	rtl_printf("A2DSetCAl OffCal=0x%04x\n",brd->OffCal);
	
	//Send OffCal word to system control word
	outw(brd->OffCal, brd->addr);

	return;
}

/*-----------------------Utility------------------------------*/

//Switch channels specified by Chans bits to offset mode: bits 0-7 -> chan 0-7
// Offset bits are in upper byte

static void A2DSetOffset(struct A2DBoard* brd)
{	
	US Chans = 0;
	int i;

	for(i = 0; i < MAXA2DS; i++)
	{
		Chans >>= 1;
//		if(brd->config.offset[i] != 0)Chans += 0x80;
		if(brd->config.offset[i] == 0)Chans += 0x80;	// Inverted bits
	}

	outb(A2DIOCALOFF, brd->chan_addr);

	brd->OffCal |= (US)((Chans <<8) & 0xFF00);
	brd->OffCal = 0;
	rtl_printf("A2DSetOffset OffCal=0x%04x\n",brd->OffCal);

	outw(brd->OffCal, brd->addr);
	
	return;
}

/*-----------------------Utility------------------------------*/
//Reads datactr status/data pairs from A2D FIFO
//

static void A2DReadFIFO(struct A2DBoard* brd, int datactr, A2DSAMPLE *buf)
{
	int i; 

	// Point to the FIFO read channel
	outb(A2DIOFIFO, brd->chan_addr);

	for(i = 0; i < datactr; i++)
	{
		buf->data[i] = inw(brd->addr);
	}
	
}

/*-----------------------Utility------------------------------*/
//Read datactr data values from A/D A2DSel and store in dataptr.
//

static void A2DReadDirect(struct A2DBoard* brd, int A2DSel, int datactr, US *dataptr)
{
	int i;

	outb(A2DIODATA, brd->chan_addr);

	for(i = 0; i < datactr; i++)
	{
		*dataptr++ = inw(brd->addr + A2DSel*2);
	}
}

/*-----------------------Utility------------------------------*/
//Set A2D SYNC flip/flop.  This stops the A/D's until cleared
//  under program control or by a positive 1PPS transition
//  1PPS enable must be asserted in order to sync on 1PPS

static void A2DSetSYNC(struct A2DBoard* brd)
{
	outb(A2DIOFIFO, brd->chan_addr);
	 
	brd->FIFOCtl |= A2DSYNC;	//Ensure that SYNC bit in FIFOCtl is set.

	//Cycle the sync clock while keeping SYNC bit high
	outb(brd->FIFOCtl, brd->addr);	
	outb(brd->FIFOCtl | A2DSYNCCK, brd->addr);	
	outb(brd->FIFOCtl, brd->addr);							
	return;
}

/*-----------------------Utility------------------------------*/

// Clear the SYNC flag

static void A2DClearSYNC(struct A2DBoard* brd)
{
	outb(A2DIOFIFO, brd->chan_addr);

	brd->FIFOCtl &= ~A2DSYNC;	//Ensure that SYNC bit in FIFOCtl is cleared.

	//Cycle the sync clock while keeping sync lowthe SYNC data line
	outb(brd->FIFOCtl, brd->addr);	
	outb(brd->FIFOCtl | A2DSYNCCK, brd->addr);
	outb(brd->FIFOCtl,brd->addr);							
	return;
}

/*-----------------------Utility------------------------------*/
// Enable 1PPS sync 
static void A2D1PPSEnable(struct A2DBoard* brd)
{	
	// Point at the FIFO control byte
	outb(A2DIOFIFO, brd->chan_addr);
	
	// Set the 1PPS enable bit
	outb(brd->FIFOCtl | A2D1PPSEBL, brd->addr);

	return;
}

/*-----------------------Utility------------------------------*/

//Disable 1PPS sync 

static void A2D1PPSDisable(struct A2DBoard* brd)
{
	// Point to FIFO control byte
	outb(A2DIOFIFO, brd->chan_addr);
	
	// Clear the 1PPS enable bit
	outb(brd->FIFOCtl & ~A2D1PPSEBL, brd->addr);

	return;
}

/*-----------------------Utility------------------------------*/

//Clear (reset) the data FIFO

static void A2DClearFIFO(struct A2DBoard* brd)
{
	// Point to FIFO control byte
	outb(A2DIOFIFO, brd->chan_addr);

	brd->FIFOCtl &= ~FIFOCLR; // Ensure that FIFOCLR bit is not set in FIFOCtl

	outb(brd->FIFOCtl, brd->addr);
	outb(brd->FIFOCtl | FIFOCLR, brd->addr);	// Cycle FCW bit 0 to clear FIFO
	outb(brd->FIFOCtl, brd->addr);

	return;
}

/*-----------------------Utility------------------------------*/
// Grab the h/w FIFO data flags for posterity
static inline void A2DGetFIFOStatus(struct A2DBoard* brd)
{
	unsigned short stat;
	outb(A2DIOFIFOSTAT,brd->chan_addr);
	stat = inw(brd->addr);

	brd->status.ser_num = (stat & 0xFFC0)>>6; // S/N is upper 10 bits  

	// If FIFONOTFULL is 0, fifo IS full
	if((stat & FIFONOTFULL) == 0) brd->status.fifofullctr++;	// FIFO is full

	// If FIFONOTEMPTY is 0, fifo IS empty
	else if((stat & FIFONOTEMPTY) == 0) brd->status.fifoemptyctr++; 

	// Figure out which 1/4 of the 1024 FIFO words we're filled to
	switch(stat&0x03) // Switch on stat's 2 LSB's
	{
		case 3:		//FIFO half full (bit0) and allmost full (bit 1)
			brd->status.fifo44ctr++; // 3/4 <= FIFO < full
			break;
		case 2:		//FIFO not half full and allmost empty
			brd->status.fifo14ctr++; // empty < FIFO <= 1/4
			break;
		case 1:		//FIFO half full and not almost full
			brd->status.fifo34ctr++; // 1/2 <= FIFO < 3/4
			break;
		case 0:
			brd->status.fifo24ctr++; // 1/4 < FIFO <= 1/2
		default:
			break;
	}
}
	
/*-----------------------Utility------------------------------*/
// A2DFIFOEmpty checks the FIFO empty status bit and returns
// 1 if empty, 0 if not empty

static inline int A2DFIFOEmpty(struct A2DBoard* brd)
{
	unsigned short stat;

	// Point at the FIFO status channel
	outb(A2DIOFIFOSTAT,brd->chan_addr);
	stat = inw(brd->addr);

	return (stat & FIFONOTEMPTY) == 0;
}

/*-----------------------Utility------------------------------*/
// This routine sends the ABORT command to all A/D's.  
// The ABORT command amounts to a soft reset--they 
//  stay configured.

static void A2DReset(struct A2DBoard* brd, int A2DSel)
{
	//Point to the A2D command register
	outb(A2DCMNDWR, brd->chan_addr);

	// Send specified A/D the abort (soft reset) command
	outw(A2DABORT, brd->addr + A2DSel*2);	
	return;
}

static void A2DResetAll(struct A2DBoard* brd)
{
	int i;

	for(i = 0; i < MAXA2DS; i++) A2DReset(brd,i);
}

/*-----------------------Utility------------------------------*/
// This routine sets the A/D's to auto mode

static void A2DAuto(struct A2DBoard* brd)
{
	//Point to the FIFO Control word
	outb(A2DIOFIFO, brd->chan_addr);
	
	// Set Auto run bit and send to FIFO control byte
	brd->FIFOCtl |=  A2DAUTO;
	outb(brd->FIFOCtl, brd->addr);	
	return;
}

/*-----------------------Utility------------------------------*/
// This routine sets the A/D's to non-auto mode

static void A2DNotAuto(struct A2DBoard* brd)
{
	//Point to the FIFO Control word
	outb(A2DIOFIFO, brd->chan_addr);

	// Turn off the auto bit and send to FIFO control byte
	brd->FIFOCtl &= ~A2DAUTO;	
	outb(brd->FIFOCtl, brd->addr);	
	return;
}

/*-----------------------Utility------------------------------*/
// Start the selected A/D in acquisition mode

static void A2DStart(struct A2DBoard* brd,int A2DSel)
{
// Point at the A/D command channel
	outb(A2DCMNDWR, brd->chan_addr);

// Start the selected A/D
	outw(A2DREADDATA, brd->addr + A2DSel*2);
	return;
}

static void A2DStartAll(struct A2DBoard* brd)
{
	int i;
	for(i = 0; i < MAXA2DS; i++)
	{
		A2DStart(brd,i);
	}
}

/*-----------------------Utility------------------------------*/
// Configure A/D A2dSel with coefficient array 'filter'
static int A2DConfig(struct A2DBoard* brd, int A2DSel)
{
	int j, ctr = 0;
	US stat;
	UC intmask=1, intbits[8] = {1,2,4,8,16,32,64,128};
	if(A2DSel < 0 || A2DSel >= MAXA2DS) return -EINVAL;
	
// Point to the A/D write configuration channel
	outb(A2DCMNDWR, brd->chan_addr);

	// Set the interrupt mask 
	intmask = intbits[A2DSel];

// Set configuration write mode
	outw(A2DWRCONFIG, brd->addr + A2DSel*2);

	for(j = 0; j < CONFBLLEN*CONFBLOCKS + 1; j++)
	{
		// Set channel pointer to Config write and
		//   write out configuration word
		outb(A2DCONFWR, brd->chan_addr);
		outw(brd->config.filter[j], brd->addr + A2DSel*2);
		rtl_usleep(30);

		// Set channel pointer to sysctl to read int lines
		// Wait for interrupt bit to set
		
		outb(A2DIOSYSCTL, brd->chan_addr);
		while((inb(brd->addr) & intmask) == 0)
		{
			rtl_usleep(30);	
			if(ctr++ > GAZILLION)
			{
				rtl_printf("%s: INTERRUPT TIMEOUT! chip = %1d\n\n", 
						__FILE__, A2DSel);
				// return -ETIMEDOUT;
				break;
			}
		}
		// Read status word from target a/d to clear interrupt
		outb(A2DSTATRD, brd->chan_addr);
		stat = inw(brd->addr + A2DSel*2);

		// Check status bits for errors
		if(stat & A2DCRCERR)
		{
			rtl_printf("CRC ERROR! chip = %1d, stat = 0x%04X\n",
				A2DSel, stat);
			brd->status.status[A2DSel] = stat; // Error status word
			// return -EIO;
		}
	}
	brd->status.status[A2DSel] = stat; // Final status word following load
	rtl_usleep(2000);
	return 0;
}

/*-----------------------Utility------------------------------*/
// Configure all A/D's with same filter

static int A2DConfigAll(struct A2DBoard* brd)
{
	int ret;
	int i;
	for(i = 0; i < MAXA2DS; i++)
	    if ((ret = A2DConfig(brd,i)) < 0) return ret;
	return 0;
}

static int getSerialNumber(struct A2DBoard* brd)
{
	unsigned short stat;
	// fetch serial number
	outb(A2DIOFIFOSTAT, brd->chan_addr);
	stat = inw(brd->addr);
	return (stat & 0xFFC0)>>6;  // S/N is upper 10 bits  
}


/* Utility function to wait for INV1PPS to be zero.
 * Since it uses rtl_usleep, it must be called from a
 * real-time thread.
 * Return: negative Linux (not RTLinux) errno, or 0=OK.
 */
static int waitFor1PPS(struct A2DBoard* brd) 
{
	unsigned short stat;
	int timeit = 0;
	// Point at the FIFO status channel
	outb(A2DIOFIFOSTAT, brd->chan_addr);
	while(timeit++ < PPSTIMEOUT)	
	{
	    if (brd->interrupted) return -EINTR;
	    // Read status, check INV1PPS bit
	    stat = inw(brd->addr);
	    if((stat & INV1PPS) == 0) return 0;
	    rtl_usleep(50); 	// Wait 50 usecs and try again
	}
	rtl_printf("1PPS not detected--no sync to GPS\n");
	return -ETIMEDOUT;
}
static int A2DSetup(struct A2DBoard* brd)
{
	A2D_SET *a2d = &brd->config;
	int  i;
	int ret;

#ifdef DOA2DSTATRD
	brd->FIFOCtl = A2DSTATEBL;	//Clear most of FIFO Control Word
#else
	brd->FIFOCtl = 0;		//Clear FIFO Control Word
#endif

	brd->OffCal = 0;		//

	for(i = 0; i < MAXA2DS; i++)
	{	
		// Pass filter info to init routine
		if ((ret = A2DSetGain(brd,i,a2d->gain[i])) < 0) return ret;
		if(a2d->Hz[i] > brd->MaxHz) brd->MaxHz = a2d->Hz[i];	// Find maximum rate
	}

	brd->status.ser_num = getSerialNumber(brd);
	
	if ((ret = A2DSetMaster(brd,a2d->master)) < 0) return ret;

	A2DSetOffset(brd);

	return 0;
}

/*--------------------- Thread function ----------------------*/
// A2DThread loads the A/D designated by A2DDes with filter 
// data from A2D structure.
//   
// NOTE: This must be called from within a real-time thread
// 			Otherwise the critical delays will not work properly

static void* A2DSetupThread(void *thread_arg)
{
	struct A2DBoard* brd = (struct A2DBoard*) thread_arg;
	int ret = 0;

	ret = A2DSetup(brd);
	if (ret < 0) return (void*)-ret;

// Make sure SYNC is cleared so clocks are running
	rtl_printf("%s: Clearing SYNC\n", __FILE__);
	A2DClearSYNC(brd);


    // If starting from a cold boot, one needs to 
    // let the A2Ds run for a bit before downloading
    // the filter data.

    // Start then reset the A/D's
    // Start conversions
	rtl_printf("%s: Starting A/D's \n", __FILE__);
   	A2DStartAll(brd);

	rtl_usleep(10000); // Let them run a few milliseconds (10)

// Then do a soft reset
	rtl_printf("%s: Soft resetting A/D's\n ", __FILE__);
	A2DResetAll(brd);
// Configure the A/D's
	rtl_printf("%s: Sending filter config data to A/Ds\n", __FILE__);
	if ((ret = A2DConfigAll(brd)) < 0) return (void*)-ret;
	
	rtl_printf("%s: Resetting A/Ds\n", __FILE__);
	// Reset the A/D's
	A2DResetAll(brd);

	rtl_usleep(DELAYNUM1);	// Give A/D's a chance to load
	rtl_printf("%s: A/Ds ready for synchronous start \n", __FILE__);

	return (void*)ret;
}

/*--------------------- Thread function ----------------------*/
/* Waits for 1PPS neg-going pulse from IRIG card
 * Return: positive Linux errno (not RTLinux error) cast
 *	as a pointer to void.
 */

static void* A2DWait1PPSThread(void *arg)
{
	struct A2DBoard* brd = (struct A2DBoard*) arg;
	int ret;
	if ((ret = waitFor1PPS(brd)) < 0) return (void*)-ret;
	return 0;
}

/* A2D callback function. Simply post the semaphore
 * so the A2DGetDataThread can do its thing
 */
static void a2dIrigCallback(void *brd)
{
	rtl_sem_post(&((struct A2DBoard*)brd)->acq_sem);
}

static void i2cTempIrigCallback(void *ptr);

static int openI2CTemp(struct A2DBoard* brd,int rate)
{
	// limit rate to something reasonable
	if (rate > IRIG_10_HZ) {
	    rtl_printf("Illegal rate for I2C temperature probe. Exceeds 10Hz\n");
	    return -EINVAL;
	}
	brd->i2c = 0x3;

	if (brd->i2cTempfd >= 0) rtl_close(brd->i2cTempfd);
	if((brd->i2cTempfd = rtl_open(brd->i2cTempFifoName,
		RTL_O_NONBLOCK | RTL_O_WRONLY)) < 0)
	{
	    rtl_printf("%s error: opening %s: %s\n",
		    __FILE__,brd->i2cTempFifoName,rtl_strerror(rtl_errno));
	    return -convert_rtl_errno(rtl_errno);
	}
	brd->i2cTempRate = rate;
	register_irig_callback(&i2cTempIrigCallback,rate, brd);

	return 0;
}

static int closeI2CTemp(struct A2DBoard* brd)
{
	unregister_irig_callback(&i2cTempIrigCallback,brd->i2cTempRate,brd);

	int fd = brd->i2cTempfd;
	brd->i2cTempfd = 0;
	if (fd >= 0) rtl_close(fd);
	return 0;
}

/* Callback function to send I2C temperature data to user space.
 */
static void i2cTempIrigCallback(void *ptr)
{
    struct A2DBoard* brd = (struct A2DBoard*)ptr;
    I2C_TEMP_SAMPLE samp;
    samp.timestamp = GET_MSEC_CLOCK;
    samp.size = sizeof(short);
    samp.data = A2DTemp(brd);
    rtl_printf("Brd temp %d.%1d degC\n", samp.data/16, (10*(samp.data%16))/16);

    if (brd->i2cTempfd >= 0) {
	// Write to up-fifo
	if (rtl_write(brd->i2cTempfd, &samp,
	    SIZEOF_DSM_SAMPLE_HEADER + samp.size) < 0) {
	    rtl_printf("%s error: write %s: %s\n",
		    __FILE__,brd->i2cTempFifoName,rtl_strerror(rtl_errno));
	    rtl_printf("%s shutting down this FIFO\n",__FILE__);
	    rtl_close(brd->i2cTempfd);
	    brd->i2cTempfd = 0;
	}
    }
}
/*------------------ Main A2D thread -------------------------*/
//	0 does necessary initialization, then loops
//	1 waits on a semaphore from the 100Hz callback
//	2 does a sufficient sleep so that all the data for
//	  the past 10 msec is ready in the A2D fifo.
//	3 reads the A2D fifo
//	4 repeat 1-4
static void* A2DGetDataThread(void *thread_arg)
{
	struct A2DBoard* brd = (struct A2DBoard*) thread_arg;

	A2DSAMPLE buf;
	int i;
	int nreads = brd->MaxHz*MAXA2DS/INTRP_RATE;
	unsigned short stat;
	static int closeA2D(struct A2DBoard* brd,int joinAcqThread);

	// The A2Ds should be done writing to the FIFO in
	// 2 * 8 * 800 nsec = 12.8 usec
	struct rtl_timespec usec20;
	struct rtl_timespec usec100;

	usec20.tv_sec = 0;
	usec20.tv_nsec = 20000;

	usec100.tv_sec = 0;
	usec100.tv_nsec = 100000;

	rtl_printf("A2DGetDataThread starting, nreads=%d, GET_MSEC_CLOCK=%d\n",
		nreads,GET_MSEC_CLOCK);

	if ((i = waitFor1PPS(brd)) < 0) return (void*)-i;

	rtl_printf("Found 1PPS, GET_MSEC_CLOCK=%d\n",
		GET_MSEC_CLOCK);

	// Zero the semaphore, then start the IRIG callback routine at 100 Hz
	rtl_sem_init(&brd->acq_sem,0,0);
	register_irig_callback(&a2dIrigCallback,IRIG_100_HZ, brd);

	// wait for 10 msec semaphore
	rtl_sem_wait(&brd->acq_sem);
	if (brd->interrupted) return 0;

	rtl_printf("Got 100Hz semaphore, GET_MSEC_CLOCK=%d\n",
		GET_MSEC_CLOCK);

	rtl_nanosleep(&usec20,0);
	A2DClearFIFO(brd);	// Reset FIFO

	if (!A2DFIFOEmpty(brd)) {
	    int ngood = 0;
	    int nbad = 0;
	    rtl_printf("fifo not empty\n");

	    // toss the initial data
	    do {
		// Point to FIFO read subchannel
		outb(A2DIOFIFO,brd->chan_addr);
		for (i = 0; i < MAXA2DS; i++) {
#ifdef DOA2DSTATRD
		    stat = inw(brd->addr);		
		    short d = inw(brd->addr);		
		    // check for acceptable looking status value
		    if ((stat & A2DSTATMASK) != A2DEXPSTATUS)
		        nbad++;
		    else ngood++;
#else
		    short d = inw(brd->addr);		
#endif
		}
	    } while(!A2DFIFOEmpty(brd));

	    rtl_printf("Cleared FIFO by reading, GET_MSEC_CLOCK=%d, ngood=%d,nbad=%d\n",
		    GET_MSEC_CLOCK,ngood,nbad);

	}

	rtl_printf("Starting data-acq loop, GET_MSEC_CLOCK=%d\n",
		GET_MSEC_CLOCK);

	// Here's the acquisition loop
	for (;;) {

	    rtl_sem_wait(&brd->acq_sem);
	    if (brd->interrupted) break;

	    buf.timestamp = GET_MSEC_CLOCK;

	    // rtl_nanosleep(&usec20,0);

	    // A2DGetFIFOStatus(brd);

	    // dataptr points to beginning of data section of A2DSAMPLE
	    register SS *dataptr = buf.data;

	    int ngood = 0;
	    int nbad = 0;

	    outb(A2DIOFIFO,brd->chan_addr);

	    for (i = 0; i < nreads; i++) {
#ifdef DOA2DSTATRD
		stat = inw(brd->addr);		
		// check for acceptable looking status value
		if ((stat & A2DSTATMASK) != A2DEXPSTATUS) {
		    nbad++;
		    brd->nbad[i % MAXA2DS]++;
		    brd->bad[i % MAXA2DS] = stat;
		}
		else {
		    ngood++;
		    brd->status.status[i % MAXA2DS] = stat;
		}

		*dataptr++ = inw(brd->addr);
#else
		*dataptr++ = inw(brd->addr);
#endif
	    }

	    if (nbad > 0) {
	        brd->nbadBufs++;
		A2DClearFIFO(brd);	// Reset FIFO
	    }
	    else if (!A2DFIFOEmpty(brd)) {
	        if (!(brd->fifoNotEmpty++ % 100))
		    rtl_printf("fifo not empty %d times\n",brd->fifoNotEmpty);
		A2DClearFIFO(brd);	// Reset FIFO
	    }

	    if (!(++brd->readCtr % 100)) {
		dsm_sample_time_t tnow = GET_MSEC_CLOCK;
		if (!(brd->readCtr % 10000) || brd->nbadBufs) {
		    rtl_printf("GET_MSEC_CLOCK=%d\n",tnow);
		    rtl_printf("last good status= %04x %04x %04x %04x %04x %04x %04x %04x\n",
			brd->status.status[0],
			brd->status.status[1],
			brd->status.status[2],
			brd->status.status[3],
			brd->status.status[4],
			brd->status.status[5],
			brd->status.status[6],
			brd->status.status[7]);
		    rtl_printf("last bad status=  %04x %04x %04x %04x %04x %04x %04x %04x\n",
			brd->bad[0],
			brd->bad[1],
			brd->bad[2],
			brd->bad[3],
			brd->bad[4],
			brd->bad[5],
			brd->bad[6],
			brd->bad[7]);
		    rtl_printf("num  bad status=  %4d %4d %4d %4d %4d %4d %4d %4d: ",
			brd->nbad[0],
			brd->nbad[1],
			brd->nbad[2],
			brd->nbad[3],
			brd->nbad[4],
			brd->nbad[5],
			brd->nbad[6],
			brd->nbad[7]);
		    brd->readCtr = 0;

		}
		brd->nbadBufs = 0;
		for (i = 0; i < MAXA2DS; i++) {
		    brd->status.status[i] = 0;
		    brd->bad[i] = 0;
		    brd->nbad[i] = 0;
		}
	    }

	    buf.size = (char*)dataptr - (char*)buf.data;

	    if (brd->a2dfd >= 0 && buf.size > 0) {
		// Write to up-fifo
		if (rtl_write(brd->a2dfd, &buf,
		    SIZEOF_DSM_SAMPLE_HEADER + buf.size) < 0) {
		    int ierr = rtl_errno;	// save err
		    rtl_printf("%s error: write %s: %s\n",
			    __FILE__,brd->a2dFifoName,rtl_strerror(rtl_errno));
		    rtl_printf("%s shutting down this A2D\n",__FILE__);
		    closeA2D(brd,0);		// close, but don't join this thread
		    return (void*) convert_rtl_errno(ierr);
		}
	    }
	}
	rtl_printf("Exiting A2DGetDataThread\n");
	return 0;
}

static int openA2D(struct A2DBoard* brd)
{
	void* thread_status;
	brd->busy = 1;	// Set the busy flag
	brd->interrupted = 0;

	if (brd->a2dfd >= 0) rtl_close(brd->a2dfd);
	if((brd->a2dfd = rtl_open(brd->a2dFifoName,
		RTL_O_NONBLOCK | RTL_O_WRONLY)) < 0)
	{
	    rtl_printf("%s error: opening %s: %s\n",
		    __FILE__,brd->a2dFifoName,rtl_strerror(rtl_errno));
	    return -convert_rtl_errno(rtl_errno);
	}

	// Establish a RT thread to allow syncing with 1PPS
	rtl_printf("1PPSThread starting, GET_MSEC_CLOCK=%d\n",
		GET_MSEC_CLOCK);
	if (rtl_pthread_create(&brd->pps_thread, NULL, A2DWait1PPSThread, brd) < 0)
	    return -convert_rtl_errno(rtl_errno);
	if (rtl_pthread_join(brd->pps_thread, &thread_status) < 0) {
	    brd->pps_thread = 0;
	    return -convert_rtl_errno(rtl_errno);
	}
	brd->pps_thread = 0;
	if (thread_status != (void*)0) return -(int)thread_status;
	rtl_printf("1PPSThread done, GET_MSEC_CLOCK=%d\n",
		GET_MSEC_CLOCK);

	A2DResetAll(brd);	// Send Abort command to all A/Ds
	A2DStatusAll(brd);	// Read status from all A/Ds

	A2DStartAll(brd);	// Start all the A/Ds
	A2DStatusAll(brd);	// Read status again from all A/Ds

	A2DSetSYNC(brd);	// Stop A/D clocks
	A2DAuto(brd);		// Switch to automatic mode

	rtl_printf("Final FIFO Clear\n");
	A2DClearFIFO(brd);	// Reset FIFO

	rtl_printf("Setting 1PPS Enable line\n");
	A2D1PPSEnable(brd);// Enable sync with 1PPS

	// Start data acquisition thread
	brd->interrupted = 0;
	if (rtl_pthread_create(&brd->acq_thread, NULL, A2DGetDataThread, brd) < 0)
		return -convert_rtl_errno(rtl_errno);
	return 0;
}

/**
 * @param joinAcqThread 1 means do a pthread_join of the acquisition thread.
 *		0 means don't do pthread_join (to avoid a deadlock)
 * @return negative UNIX errno
 */
static int closeA2D(struct A2DBoard* brd,int joinAcqThread) 
{
	int ret = 0;
	void* thread_status;

	// interrupt the 1PPS or acquisition thread
	brd->interrupted = 1;

	// Shut down the setup thread
	if (brd->setup_thread) {
	    rtl_pthread_cancel(brd->setup_thread);
	    rtl_pthread_join(brd->setup_thread, NULL);
	    brd->setup_thread = 0;
	}

	// Shut down the pps thread
	if (brd->pps_thread) {
	    rtl_pthread_cancel(brd->pps_thread);
	    rtl_pthread_join(brd->pps_thread, NULL);
	    brd->pps_thread = 0;
	}

	if (joinAcqThread && brd->acq_thread) {
	    rtl_pthread_join(brd->acq_thread, &thread_status);
	    brd->acq_thread = 0;
	    if (thread_status != (void*)0) ret = -(int)thread_status;
	}

	//Turn off the callback routine
	unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ,brd);

	A2DStatusAll(brd); 	// Read status and clear IRQ's	

	A2DNotAuto(brd);	// Shut off auto mode (if enabled)

	// Abort all the A/D's
	A2DResetAll(brd);

	if (brd->a2dfd >= 0) {
	    int fdtmp = brd->a2dfd;
	    brd->a2dfd = -1;
	    rtl_close(fdtmp);
	}
	brd->busy = 0;	// Reset the busy flag

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
	void* thread_status;

	// paranoid check if initialized (probably not necessary)
	if (!boardInfo) return ret;

	struct A2DBoard* brd = boardInfo + board;

#ifdef DEBUG
  	rtl_printf("\n%s: ioctlCallback cmd=%x board=%d port=%d len=%d\n",
		__FILE__, cmd,board,port,len);
#endif

  	switch (cmd) 
	{
  	case GET_NUM_PORTS:		/* user get */
		if (len != sizeof(int)) break;
		rtl_printf("%s: GET_NUM_PORTS\n", __FILE__);
		*(int *) buf = NDEVICES;	
		ret = sizeof(int);
  		break;

  	case A2D_STATUS_IOCTL:		/* user get */
		if (port != 0) break;	// port 0 is the A2D, port 1 is I2C temp
		if (len != sizeof(A2D_STATUS)) break;
		memcpy(buf,&brd->status,len);
		ret = len;
		break;

  	case A2D_SET_IOCTL:		/* user set */
		if (port != 0) break;	// port 0 is the A2D, port 1 is I2C temp
		if (len != sizeof(A2D_SET)) break;	// invalid length
		if(brd->busy) {
			rtl_printf("A2D's running. Can't reset\n");
			ret = -EBUSY;
			break;
		}

		rtl_printf("%s: A2D_SET_IOCTL\n", __FILE__);
		memcpy(&brd->config,(A2D_SET*)buf,sizeof(A2D_SET));

		rtl_printf("%s: Starting setup thread\n", __FILE__);
		rtl_pthread_create(&brd->setup_thread, NULL, A2DSetupThread, brd);
		rtl_pthread_join(brd->setup_thread, &thread_status);
		rtl_printf("%s: Setup thread finished\n", __FILE__);
		brd->setup_thread = 0;

		if (thread_status != (void*)0) ret = -(int)thread_status;
		else ret = 0;		// OK
   		break;

  	case A2D_CAL_IOCTL:		/* user set */
		rtl_printf("%s: A2D_CAL_IOCTL\n", __FILE__);
		if (port != 0) break;	// port 0 is the A2D, port 1 is I2C temp
		if (len != sizeof(A2D_CAL)) break;	// invalid length
		memcpy(&brd->cal,(A2D_CAL*)buf,sizeof(A2D_CAL));
		A2DSetVcal(brd);
		A2DSetCal(brd);	//Call A2DSetup with structure pointer ts 
		ret = 0;
   		break;

  	case A2D_RUN_IOCTL:

		if (port != 0) break;	// port 0 is the A2D, port 1 is I2C temp
		// clean up acquisition thread if it was left around
		if (brd->acq_thread) {
		    brd->interrupted = 1;
		    rtl_pthread_cancel(brd->acq_thread);
		    rtl_pthread_join(brd->acq_thread, &thread_status);
		    brd->acq_thread = 0;
		}

		rtl_printf("%s: A2D_RUN_IOCTL\n", __FILE__);
		ret = openA2D(brd);
		rtl_printf("A2D_RUN_IOCTL finished\n");
		break;

  	case A2D_STOP_IOCTL:
		if (port != 0) break;	// port 0 is the A2D, port 1 is I2C temp
		rtl_printf("%s: A2D_STOP_IOCTL\n", __FILE__);
		ret = closeA2D(brd,1);
		break;
  	case A2D_OPEN_I2CT:
		rtl_printf("%s: A2D_OPEN_I2CT\n", __FILE__);
		if (port != 1) break;	// port 0 is the A2D, port 1 is I2C temp
		if (len != sizeof(int)) break;	// invalid length
		int rate = *(int*)buf;
		ret = openI2CTemp(brd,rate);
		break;
  	case A2D_CLOSE_I2CT:
		rtl_printf("%s: A2D_CLOSE_I2CT\n", __FILE__);
		if (port != 1) break;	// port 0 is the A2D, port 1 is I2C temp
		ret = closeI2CTemp(brd);
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
	int ib;
	if (!boardInfo) return;

	for (ib = 0; ib < numboards; ib++) {
	    struct A2DBoard* brd = boardInfo + ib;

	    // remove the callback routine (does nothing if it isn't registered)
	    unregister_irig_callback(&a2dIrigCallback, IRIG_100_HZ,brd);

	    A2DStatusAll(brd); 	// Read status and clear IRQ's	

	    // Shut down the setup thread
	    if (brd->setup_thread) {
		rtl_pthread_cancel(brd->setup_thread);
		rtl_pthread_join(brd->setup_thread, NULL);
		brd->setup_thread = 0;
	    }

	    // Shut down the pps thread
	    if (brd->pps_thread) {
		rtl_pthread_cancel(brd->pps_thread);
		rtl_pthread_join(brd->pps_thread, NULL);
		brd->pps_thread = 0;
	    }

	    // Shut down the acquisition thread
	    if (brd->acq_thread) {
		rtl_pthread_cancel(brd->acq_thread);
		rtl_pthread_join(brd->acq_thread, NULL);
		brd->acq_thread = 0;
	    }

	    // close and remove A2D fifo
	    if (brd->a2dfd >= 0) rtl_close(brd->a2dfd);
	    if (brd->a2dFifoName) {
		rtl_unlink(brd->a2dFifoName);
		rtl_gpos_free(brd->a2dFifoName);
	    }
	    rtl_sem_destroy(&brd->acq_sem);

	    // close and remove temperature fifo
	    if (brd->i2cTempfd >= 0) rtl_close(brd->i2cTempfd);
	    if (brd->i2cTempFifoName) {
		rtl_unlink(brd->i2cTempFifoName);
		rtl_gpos_free(brd->i2cTempFifoName);
	    }

	    if (brd->ioctlhandle) closeIoctlFIFO(brd->ioctlhandle);
	    brd->ioctlhandle = 0;

	    if (brd->addr)
		release_region(brd->addr, A2DIOWIDTH);
	    brd->addr = 0;
	}

        rtl_gpos_free(boardInfo);
        boardInfo = 0;

  	rtl_printf("%s : Analog cleanup complete\n", __FILE__);

	return;
}

/*-----------------------Module------------------------------*/

int init_module()
{	
	int error = -EINVAL;
	int ib;

	boardInfo = 0;

  	rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
	     __FILE__, __FUNCTION__, __DATE__, __TIME__);

	/* count non-zero ioport addresses, gives us the number of boards */
	for (ib = 0; ib < MAX_A2D_BOARDS; ib++)
	    if (ioport[ib] == 0) break;
	numboards = ib;
	if (numboards == 0) {
	    rtl_printf("%s: No boards configured, all ioport[]==0\n", __FILE__);
	    goto err;
	}

	error = -ENOMEM;
	boardInfo = rtl_gpos_malloc( numboards * sizeof(struct A2DBoard) );
	if (!boardInfo) goto err;

	/* initialize each A2DBoard structure */
	for (ib = 0; ib < numboards; ib++) {
	    struct A2DBoard* brd = boardInfo + ib;
	    memset(brd,0,sizeof(struct A2DBoard));

	    brd->addr = 0;
	    brd->chan_addr = 0;

	    brd->setup_thread = 0;
	    brd->pps_thread = 0;
	    brd->acq_thread = 0;
	    brd->interrupted = 0;
	    brd->busy = 0;

	    rtl_sem_init(&brd->acq_sem,0,0);
	    brd->a2dFifoName = 0;
	    brd->a2dfd = -1;
	    brd->i2cTempFifoName = 0;
	    brd->i2cTempfd = -1;
	    brd->ioctlhandle = 0;
	    memset(&brd->config,0,sizeof(A2D_SET));
	    memset(&brd->cal,0,sizeof(A2D_CAL));
	    memset(&brd->status,0,sizeof(A2D_STATUS));
	    memset(&brd->bad,0,sizeof(brd->bad));
	    memset(&brd->nbad,0,sizeof(brd->nbad));
	    brd->OffCal = 0;
#ifdef DOA2DSTATRD
	    brd->FIFOCtl = A2DSTATEBL;
#else
	    brd->FIFOCtl = 0;
#endif
	    brd->MaxHz = 0;
	    brd->readCtr = 0;
	    brd->nbadBufs = 0;
	    brd->fifoNotEmpty = 0;
	    brd->i2c = 0x3;
	}

	/* allocate necessary members in each A2DBoard structure */
	for (ib = 0; ib < numboards; ib++) {
	    struct A2DBoard* brd = boardInfo + ib;

	    error = -EBUSY;
	    unsigned int addr =  ioport[ib] + SYSTEM_ISA_IOPORT_BASE;
	    // Get the mapped board address
	    if (check_region(addr, A2DIOWIDTH)) {
		rtl_printf("%s: ioports at 0x%x already in use\n",
			__FILE__,addr);
		goto err;
	    }

	    request_region(addr, A2DIOWIDTH, "A2D_DRIVER");
	    brd->addr = addr;
	    brd->chan_addr = addr + 0xF;

	    /* Open up my ioctl FIFOs, register my ioctlCallback function */
	    error = -EIO;
	    brd->ioctlhandle =
			openIoctlFIFO("dsma2d",ib,ioctlCallback,
					    nioctlcmds,ioctlcmds);

	    if (!brd->ioctlhandle) goto err;

	    // Open the A2D fifo to user space
	    error = -ENOMEM;
	    brd->a2dFifoName = makeDevName(devprefix,"_in_",ib*NDEVICES);
            if (!brd->a2dFifoName) goto err;

	    // remove broken device file before making a new one
	    if ((rtl_unlink(brd->a2dFifoName) < 0 && rtl_errno != RTL_ENOENT)
	    	|| rtl_mkfifo(brd->a2dFifoName, 0666) < 0) {
		rtl_printf("%s error: unlink/mkfifo %s: %s\n",
			__FILE__,brd->a2dFifoName,rtl_strerror(rtl_errno));
		error = -convert_rtl_errno(rtl_errno);
		goto err;
	    }

	    // Open the fifo for I2C temperature data to user space
	    error = -ENOMEM;
	    brd->i2cTempFifoName = makeDevName(devprefix,"_in_",ib*NDEVICES+1);
            if (!brd->i2cTempFifoName) goto err;

	    // remove broken device file before making a new one
	    if ((rtl_unlink(brd->i2cTempFifoName) < 0 && rtl_errno != RTL_ENOENT)
	    	|| rtl_mkfifo(brd->i2cTempFifoName, 0666) < 0) {
		rtl_printf("%s error: unlink/mkfifo %s: %s\n",
			__FILE__,brd->i2cTempFifoName,rtl_strerror(rtl_errno));
		error = -convert_rtl_errno(rtl_errno);
		goto err;
	    }
	}

	rtl_printf("%s: A2D init_module complete.\n", __FILE__);

	return 0;
err:

	if (boardInfo) {
	    for (ib = 0; ib < numboards; ib++) {
		struct A2DBoard* brd = boardInfo + ib;

		rtl_sem_destroy(&brd->acq_sem);
		if (brd->a2dFifoName) {
		    rtl_unlink(brd->a2dFifoName);
		    rtl_gpos_free(brd->a2dFifoName);
		}

		if (brd->i2cTempFifoName) {
		    rtl_unlink(brd->i2cTempFifoName);
		    rtl_gpos_free(brd->i2cTempFifoName);
		}

		if (brd->ioctlhandle) closeIoctlFIFO(brd->ioctlhandle);
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

