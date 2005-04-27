// #define BFIR
// #define DEBUGA2DGET
// #define DEBUGA2DGET2

/*  a2d_driver.c/


Time-stamp: <Wed 13-Apr-2005 05:57:57 pm>

Drivers and utility modules for NCAR/ATD/RAF DSM3 A/D card.

Copyright 2005 UCAR, NCAR, All Rights Reserved

Original author:	Grant Gray

Revisions:

*/

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

// A/D Configuration file parameters
#define CONFBLOCKS  12  	// 12 blocks as described below
#define CONFBLLEN   43  	// Block of 42 16-bit words plus a CRC word
#define	DELAYNUM0	10		// Used for usleep	
#define	DELAYNUM1	1000	// Used for usleep	

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


//Function templates
static void A2DIoctlDump(A2D_SET *a);
static void A2DReset(int a);
static void A2DResetAll(void);
static void A2DAuto(void);
static void A2DNotAuto(void);
static void A2DStart(int a);
static void A2DStartAll(void);
static short A2DConfig(int a, US *b);
static void A2DConfigAll(US *b);
static int  A2DFIFOEmpty(void);	// Tests FIFO empty flag
static int  A2DSetup(A2D_SET *a);
static void A2DGetData();		//Read hardware fifo
static void A2DPtrInit(A2D_SET *a);	//Initialize pointer to data areas
static US   A2DStatus(int a);		//Get A/D status
static void A2DStatusAll(US *a);		//Get A/D status into array a
// static void A2DCommand(int a, US b);	//Issue command b to A/D converter a
static UC   A2DSetGain(int a, int b);	//Set gains for individual A/D channels
static void A2DSetMaster(US a);	//Assign one of the A/D's as timing master
static int	A2DInit(A2D_SET *b);	//Initialize/load A/D converters
static void A2DSetCtr(A2D_SET *a);	//Reset the individual channel sample counters
static UC   A2DReadInts(void);		//Read the state of A/D interrupt lines
static UC   A2DSetVcal(int a);	//Set the calibration point voltage
static void A2DSetCal(A2D_SET *a);	//Set the cal enable bits for the 8 channels
static void A2DSetOffset (A2D_SET *a);	//Set the offset enable bits for the 8 channels
static void A2DReadFIFO(int a, A2DSAMPLE *b);	//Read a structs from the FIFO into buffer b
static void A2DReadDirect(int a,int b,US *c);	//Read b values dir to buf c from A/D a
static void A2DSetSYNC(void);		//Set the SYNC bit high-stops A/D conversion
static void A2DClearSYNC(void);	//Clear the SYNC bit
static void A2D1PPSEnable(void);	//Enables A/D start on next 1 PPS GPS transition
static void A2D1PPSDisable(void);	//Disable 1PPS sync
static void A2DClearFIFO(void);	//Clear (reset) the FIFO
static void A2DError(int a);		//A/D Card error handling

static int  init_module(void);
static void cleanup_module(void);

rtl_pthread_t SetupThread = 0;

static int ictr = 0, cctr = 0;
static 	US	CalOff = 0; 	//Static storage for cal and offset bits
static	US	FIFOCtl = 0;	//Static hardware FIFO control word storage
static char fifoname[50];
static 	int	fd_up = -1; 			// Data FIFO file descriptor

volatile unsigned int isa_address = A2DBASE;
volatile unsigned int chan_addr = A2DBASE + 0xF;
volatile unsigned int a2dsbusy = 0;

// These are just test values

UC		Cals;
UC		Offsets;


/****************  IOCTL Section ***************8*********/

static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,_IOC_SIZE(GET_NUM_PORTS) },
  { A2D_GET_IOCTL,_IOC_SIZE(A2D_GET_IOCTL) },
  { A2D_SET_IOCTL, sizeof(A2D_SET)  },
  { A2D_CAL_IOCTL, sizeof(A2D_SET)  },
  { A2D_RUN_IOCTL,_IOC_SIZE(A2D_RUN_IOCTL) },
  { A2D_STOP_IOCTL,_IOC_SIZE(A2D_STOP_IOCTL) },
  { A2D_RESTART_IOCTL,_IOC_SIZE(A2D_RESTART_IOCTL) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);

static struct ioctlHandle* ioctlhandle = 0;

/****************  End of IOCTL Section ******************/

static int boardNum = 0;
static int nports = 1;

/* module prameters (can be passed in via command line) */
//static unsigned int a2d_parm = 0;

MODULE_AUTHOR("Grant Gray <gray@ucar.edu>");
MODULE_DESCRIPTION("HIAPER A/D driver for RTLinux");
//MODULE_PARM(a2d_parm, "1l");

RTLINUX_MODULE(DSMA2D);

/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 */
static int A2DCallback(int cmd, int board, int port,
	void *buf, rtl_size_t len) 
{
  	int ret = -EINVAL;
	A2D_SET *ts;		// Pointer to A/D command struct
	A2D_GET *tg;


  	rtl_printf("\n%s: A2DCallback cmd=%x board=%d port=%d len=%d\n",
		__FILE__, cmd,board,port,len);
  	rtl_printf("%s: Size of A2D_SET = 0x%04X\n", __FILE__, sizeof(A2D_SET));

  	switch (cmd) 
	{
  	case GET_NUM_PORTS:		/* user get */
		{
		rtl_printf("%s: GET_NUM_PORTS\n", __FILE__);
		*(int *) buf = nports;
		ret = sizeof(int);
		}
  		break;

  	case A2D_GET_IOCTL:		/* user get */
		{
		tg = (A2D_GET *)buf;
		ret = len;
		}
    	break;

  	case A2D_SET_IOCTL:		/* user set */
		if(!a2dsbusy)
		{
			rtl_printf("%s: A2D_SET_IOCTL\n", __FILE__);
			ts = (A2D_SET *)buf;
			A2DSetup(ts);	//Call A2DSetup with structure pointer ts 
			rtl_printf("%s: Creating temp thread to start the ball rolling\n",
						__FILE__);
			rtl_pthread_create(&SetupThread, NULL, (void *)&A2DInit, (void *)ts);
			rtl_pthread_join(SetupThread, NULL);
			SetupThread = 0;
		    ret = sizeof(A2D_SET);
		}
		else {
			rtl_printf("A2D's running. Can't reset\n");
			ret = -EINVAL;
		}
   		break;

  	case A2D_CAL_IOCTL:		/* user set */
		{
		rtl_printf("%s: A2D_CAL_IOCTL\n", __FILE__);
		ts = (A2D_SET *)buf;
		A2DSetVcal((int)ts->vcalx8);
		A2DSetCal(ts);	//Call A2DSetup with structure pointer ts 
		ret = sizeof(A2D_SET);
		}
   		break;

  	case A2D_RUN_IOCTL:		/* user set */
		{
		    rtl_printf("%s: A2D_RUN_IOCTL\n", __FILE__);

			if (fd_up >= 0) rtl_close(fd_up);
			if((fd_up = rtl_open(fifoname, RTL_O_NONBLOCK | RTL_O_WRONLY)) < 0)
			{
				rtl_printf("Unable to open up FIFO\n");
				return -rtl_errno;
			}
			rtl_printf("%s: Up FIFO opened--fd = 0x%08x\n", __FILE__, fd_up);

		    US stat[MAXA2DS];
		    a2dsbusy = 1;	// Set the busy flag
		    rtl_printf("RUN command received \n");
		    // Start the IRIG callback routine at 100 Hz
		    register_irig_callback(&A2DGetData, 
					    IRIG_100_HZ, 
					    (void *)NULL);	
		    A2DResetAll();	// Send Abort command to all A/Ds
		    A2DStatusAll(stat);	// Read status from all A/Ds
		    A2DStartAll();	// Start all the A/Ds
		    A2DStatusAll(stat);	// Read status again from all A/Ds
		    A2DSetSYNC();	// Stop A/D clocks
		    A2DClearFIFO();	// Reset FIFO
		    A2DAuto();		// Switch to automatic mode
		    A2D1PPSEnable();// Enable sync with 1PPS
		    ret = 0;
		}
		break;

  	case A2D_STOP_IOCTL:		/* user set */
		{
		    US stat[MAXA2DS];
		    rtl_printf("%s: A2D_STOP_IOCTL\n", __FILE__);
		    rtl_printf("STOP command received\n");

			A2DStatusAll(stat); 	// Read status and clear IRQ's	

		    //Kill the callback routine
		    unregister_irig_callback(&A2DGetData, IRIG_100_HZ);

		    A2DNotAuto();	// Shut off auto mode (if enabled)

		    // Abort all the A/D's
		    A2DResetAll();

		    a2dsbusy = 0;	// Reset the busy flag
			if (fd_up >= 0) rtl_close(fd_up);
		    fd_up = -1;
		    ret = 0;
		}
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
  	char fname[20];
	int stat[MAXA2DS];

	A2DStatusAll(stat); 	// Read status and clear IRQ's	

  	/* Close my ioctl FIFOs, deregister my ioctlCallback function */
 	 closeIoctlFIFO(ioctlhandle);


  	/* Close and unlink the up fifo */

  	sprintf(fname, "/dev/dsma2d_in_0");
  	rtl_printf("%s : Destroying %s\n",__FILE__,  fname);
    if (fd_up >= 0) rtl_close(fd_up);
  	rtl_unlink("/dev/dsma2d_in_0");
  
	// Shut down the setup thread
    if (SetupThread) {
			rtl_pthread_cancel(SetupThread);
			rtl_pthread_join(SetupThread, NULL);
    }

	// Release the mapped memory
  	release_region(isa_address, A2DIOWIDTH);

  	rtl_printf("%s : Analog cleanup complete\n", __FILE__);

  return;
}

/*-----------------------Module------------------------------*/

int init_module()
{	
	int error;

  	rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
	     __FILE__, __FUNCTION__, __DATE__, __TIME__);

  	/* Open up my ioctl FIFOs, register my ioctlCallback function */
  	ioctlhandle = openIoctlFIFO("dsma2d",boardNum,A2DCallback,
  					nioctlcmds,ioctlcmds);

  	if (!ioctlhandle) return -RTL_EIO;

	// Open the fifo TO user space (up fifo)
	sprintf(fifoname, "/dev/dsma2d_in_0");
	rtl_printf("%s: Creating %s\n", __FILE__, fifoname);

	// remove broken device file before making a new one
	if (rtl_unlink(fifoname) < 0 && rtl_errno != RTL_ENOENT)
		return -rtl_errno;

	if((error = rtl_mkfifo(fifoname, 0666)))
		rtl_printf("Error creating fifo %s\n", fifoname);
	else
		rtl_printf("Up FIFO %s created for writing\n", fifoname);



	rtl_printf("%s: A2D initialization complete.\n", __FILE__);

	// Get the mapped board address
	request_region(isa_address, A2DIOWIDTH, "A2D_DRIVER");

	return 0;
}

static int A2DSetup(A2D_SET *a2d)
	{
	int A2DErrorCode; /* TotalErrors = 0; */
	int i;

	FIFOCtl = 0;		//Clear FIFO Control Word

	A2DErrorCode = 0;

	for(i = 0; i < MAXA2DS; i++)
	{	
		// Pass filter info to init routine
		A2DSetGain(i, a2d->gain[i]);
		a2d->status[i] = 0;	// Clear status
		A2DSetCtr(a2d);		// Set up the counters
	}

	A2DSetMaster(a2d->master);
	A2DSetOffset(a2d);
	A2DSetVcal((int)(a2d->vcalx8));	// Set the calibration voltage
	A2DSetCal(a2d);	// Switch appropriate channels to cal mode

	A2DClearSYNC();	// Start A/Ds synchronous with 1PPS from IRIG card

	A2D1PPSEnable();// DEBUG Do this just following a 1PPS transition
	A2DSetSYNC();	//	so that we don't step on the transition
	
	A2DPtrInit(a2d);// Initialize offset buffer pointers

	return 0;
}

/*-----------------------Utility------------------------------*/
// A2DInit loads the A/D designated by A2DDes with filter 
//   data from A2D structure
// NOTE: This must be called from within a real-time thread
// 			Otherwise the critical delays will not work properly

#define GAZILLION 10000

static int A2DInit(A2D_SET *a2d)
{
	US *filt;

	rtl_printf("In A2DInit\n");
	rtl_printf("Setting reference freq to 10KHz\n");
	
	setRate2Output(10000, 1);


// Initialize pointer to filter array

	filt = &a2d->filter[0];

// Make sure SYNC is cleared so clocks are running
	rtl_printf("%s: Clearing SYNC\n", __FILE__);
	A2DClearSYNC();

// Start then reset the A/D's

// Start conversions
	rtl_printf("%s: Starting A/D's \n", __FILE__);
   	A2DStartAll();

	rtl_usleep(10000); // Let them run a few milliseconds (10)

// Then do a soft reset
	rtl_printf("%s: Soft resetting A/D's\n ", __FILE__);
	A2DResetAll();

// Configure the A/D's
	rtl_printf("%s: Sending filter config data to A/Ds\n", __FILE__);
	A2DConfigAll(filt);
	
	rtl_printf("%s: Resetting A/Ds\n", __FILE__);
	// Reset the A/D's
	A2DResetAll();

	rtl_usleep(DELAYNUM1);	// Give A/D's a chance to load
	rtl_printf("%s: A/Ds ready for synchronous start \n", __FILE__);

	return(0);
}


// A2DPtrInit initializes the A2D->ptr[i] buffer offsets

static void A2DPtrInit(A2D_SET *a2d)
{
	int i;

	a2d->ptr[0] = 0;

	for(i = 1; i < 8; i++)
	{
		a2d->ptr[i] = a2d->ptr[i-1] + a2d->Hz[i-1]*sizeof(long);
	}

	return;
}

/*-----------------------Utility------------------------------*/
//Process A2D card error codes

static void A2DError(int Code)
{
	switch(Code)
	{
		case ERRA2DNOFILE:
			rtl_printf("%s: File cannot be opened\n", __FILE__);
			break;
		case ERRA2DCHAN:
			rtl_printf("%s: Non-existent A2D channel\n", __FILE__);
			break;
		case ERRA2DVCAL:
			rtl_printf("%s: Cal voltage out of limits\n", __FILE__);
			break;
		case ERRA2DGAIN:
			rtl_printf("%s: Gain value out of limits\n", __FILE__);
			break;
		case A2DLOADOK:
			break;
		default:
			break;
	}
	return;
}


/*-----------------------Utility------------------------------*/

//Read status of A2D chip specified by A2DDes 0-7

static US A2DStatus(int A2DSel)
{
	US stat;

	// Point at the A/D status channel
	outb(A2DSTATRD, (UC *)chan_addr);	// Point at Stat channel for read
	stat = inw((US *)isa_address + A2DSel);

	return(stat);
}

static void A2DStatusAll(US *stat)
{
	int i;
	for(i = 0; i < MAXA2DS; i++)
	{
		stat[i] = A2DStatus(i);
	}

	return;
}

/*-----------------------Utility------------------------------*/
// A2DCommand issues a command word to the A/D selected by A2DSel

static void A2DCommand(int A2DSel, US Command)
{
	// Point to A/D command write
	outb(A2DCMNDWR, (UC *)chan_addr);	

	// Write command to the selected A/D
	outw(Command, (US *)isa_address);
}

/*-----------------------Utility------------------------------*/
// A2DSetRate
// Didn't really need this since it is not a hardware function.
//	so left it just to setting a parameter in the A2D structure.

// This routine sets the individual A/D sample counters
static void A2DSetCtr(A2D_SET *a2d)
{
	int i;

	for(i = 0; i < 8; i++)
	{
		if(a2d->Hz[i] > 0)
		{
			// Set counter for channel
			a2d->ctr[i] = (US)(100/a2d->Hz[i]);
		}
		else
		{
			a2d->ctr[i] = -1;
		}
	}
	return;
}

/*-----------------------Utility------------------------------*/
// A2DSetGain sets the A/D Channel gain.
// Allowable gain values are 1 <= A2DGain <= 25.5
//   in 255 gain steps.
// The gain is calculated from gaincode (0-255) as:
//   gain = 5*5.12*gaincode/256 = .1*gaincode.
// The gain can go down to .7 before the system has saturation problems
// Check this 

/*-----------------------Utility------------------------------*/
static UC A2DSetGain(int A2DSel, int A2DGain)
{
	UC *DACAddr;
	int D2AChsel = -1;
	US GainCode = 1;

	//Check that gain is within limits
	if(A2DGain < 1 || A2DGain > 25)return(ERRA2DGAIN);


	DACAddr = (UC *)chan_addr + A2DSel; // this address

	if(A2DSel >= 0 && A2DSel <=3)	// If setting gain on channels 0-3
	{
		D2AChsel = A2DIOGAIN03;	// Use this channel select
	}

	if(A2DSel >= 4 && A2DSel <=7)	// Otherwise, for channels 4-7
	{
		D2AChsel = A2DIOGAIN47;		// Use this channel select and
		DACAddr = (UC *)chan_addr + A2DSel - 4; // this address
	}

	// If no A/D selected return error -1
	if(D2AChsel == -1)return(ERRA2DCHAN);	

	// Point to the appropriate DAC channel
	outb(D2AChsel, (UC *)chan_addr);		// Set the card channel pointer

	GainCode = (UC)(10 * A2DGain);	// Gain code = 10*gain

	// Write the code to the selected DAC
	outb(GainCode, DACAddr);

	return(GainCode);
}

/*-----------------------Utility------------------------------*/
//A2DSetMaster routes the interrupt signal from the target A/D chip 
//  to the ISA bus interrupt line.

static void A2DSetMaster(US A2DSel)
{
	if(A2DSel > 7)return;	// Error check return with no action


	// Point at the FIFO status channel
	outb(A2DIOFIFOSTAT, (UC *)chan_addr);	

	// Write the master to register
	outb(A2DSel, (UC *)isa_address);
}

/*-----------------------Utility------------------------------*/

//Read the A/D chip interrupt line states

static UC	A2DReadInts(void)
{
	//Point to the sysctl channel for read
	outb(A2DIOSYSCTL, (UC *)chan_addr);	

	// Read the interrupt word
	return(inb((UC *)isa_address));
}


/*-----------------------Utility------------------------------*/
//Set a calibration voltage
// Input is the calibration voltage * 8 expressed as an integer
//   i.e. -80 <= Vx8 <= +80 for -10 <= Calibration voltage <= +10

static UC A2DSetVcal(int Vx8)
{
	US Vcode = 1; 

	//Check that V is within limits
	if(Vx8 < -80 || Vx8 > 80)return(ERRA2DVCAL);

	// Point to the calibration DAC channel
	outb(A2DIOVCAL, (UC *)chan_addr);		

	Vcode = Vx8 + 128 ;//Convert Vx8 to DAC code

	// Write cal code to calibration D/A
	outb(Vcode, (UC *)isa_address);

	return(Vcode);
}

/*-----------------------Utility------------------------------*/

//Switch inputs specified by Chans bits to calibration mode: bits 8-15 -> chan 0-7
// Checked visually 5/22/04 GRG

static void A2DSetCal(A2D_SET *a2d)
{	
	US Chans = 0;
	int i;

	for(i = 0; i < MAXA2DS; i++)
	{
		Chans >>= 1; 
		if(a2d->calset[i] != 0)Chans += 0x80;
	}

	// Point at the system control input channel
	outb(A2DIOSYSCTL, (UC *)chan_addr);

	//Set the appropriate bits in CalOff
	CalOff |= (US)((Chans<<8) & 0xFF00);
	
	//Send CalOff word to system control word
	outw(CalOff, (US *)isa_address);

	return;
}

/*-----------------------Utility------------------------------*/

//Switch channels specified by Chans bits to offset mode: bits 0-7 -> chan 0-7
// TODO Check logic on this one

static void A2DSetOffset(A2D_SET *a2d)
{	
	US Chans = 0;
	int i;

	for(i = 0; i < MAXA2DS; i++)
	{
		Chans >>= 1;
		if(a2d->offset[i] != 0)Chans += 0x80;
	}

	outb(A2DIOVCAL, (UC *)chan_addr);

	CalOff |= (US)(Chans & 0x00FF);

	outw(CalOff, (US *)isa_address);
	
	return;
}

/*-----------------------Utility------------------------------*/
//Reads datactr status/data pairs from A2D FIFO
//

static void A2DReadFIFO(int datactr, A2DSAMPLE *buf)
{
	int i; 

	// Point to the FIFO read channel
	outb(A2DIOFIFO, (UC *)chan_addr);


	for(i = 0; i < datactr; i++)
	{
		buf->data[i] = inw((SS *)isa_address);
	}
	
}

/*-----------------------Utility------------------------------*/
//Read datactr data values from A/D A2DSel and store in dataptr.
//

static void A2DReadDirect(int A2DSel, int datactr, US *dataptr)
{
	int i;

	outb(A2DIODATA, (UC *)chan_addr);

	for(i = 0; i < datactr; i++)
	{
		*dataptr++ = inw((US *)isa_address + A2DSel);
	}
}

/*-----------------------Utility------------------------------*/
//Set A2D SYNC flip/flop.  This stops the A/D's until cleared
//  under program control or by a positive 1PPS transition
//  1PPS enable must be asserted in order to sync on 1PPS

static void A2DSetSYNC(void)
{
	outb(A2DIOFIFO, (UC *)chan_addr);
	 
	FIFOCtl |= A2DSYNC;	//Ensure that SYNC bit in FIFOCtl is set.

	//Cycle the sync clock while keeping SYNC bit high
	outb(FIFOCtl | A2DSYNC, (US *)isa_address);	
	outb(FIFOCtl | A2DSYNC | A2DSYNCCK, (UC *)isa_address);	
	outb(FIFOCtl | A2DSYNC, (US *)isa_address);							
	return;
}

/*-----------------------Utility------------------------------*/

// Clear the SYNC flag

static void A2DClearSYNC(void)
{
	outb(A2DIOFIFO, (UC *)chan_addr);

	FIFOCtl &= ~A2DSYNC;	//Ensure that SYNC bit in FIFOCtl is cleared.

	//Cycle the sync clock while keeping sync lowthe SYNC data line
	outb(FIFOCtl, (US *)isa_address);	
	outb(FIFOCtl | A2DSYNCCK, (UC *)isa_address);
	outb(FIFOCtl,(US *)isa_address);							
	return;
}

/*-----------------------Utility------------------------------*/
// Enable 1PPS sync 
static void A2D1PPSEnable(void)
{	
	// Point at the FIFO control byte
	outb(A2DIOFIFO, (UC *)chan_addr);
	
	// Set the 1PPS enable bit
	outb(FIFOCtl | A2D1PPSEBL, (UC *)isa_address);

	return;
}

/*-----------------------Utility------------------------------*/

//Disable 1PPS sync 
// Checked visually 5/22/04 GRG

static void A2D1PPSDisable(void)
{
	// Point to FIFO control byte
	outb(A2DIOFIFO, (UC *)chan_addr);
	
	// Clear the 1PPS enable bit
	outb(FIFOCtl & ~A2D1PPSEBL, (UC *)isa_address);

	return;
}

/*-----------------------Utility------------------------------*/

//Clear (reset) the data FIFO
// Checked visually 5/22/04 GRG

static void A2DClearFIFO(void)
{
	// Point to FIFO control byte
	outb(A2DIOFIFO, (UC *)chan_addr);

	FIFOCtl &= ~FIFOCLR; // Ensure that FIFOCLR bit is not set in FIFOCtl

	outb(FIFOCtl, (UC *)isa_address);
	outb(FIFOCtl | FIFOCLR, (UC *)isa_address);	// Cycle FCW bit 0 to clear FIFO
	outb(FIFOCtl, (UC *)isa_address);

	return;
}
/*-----------------------Utility------------------------------*/

// A2DGetData reads data from the A/D card hardware fifo and writes
// the data to the software up-fifo to user space.
static void A2DGetData()
{
//	US inbuf[HWFIFODEPTH];
	A2DSAMPLE buf;
	short *dataptr;

#ifdef DEBUGA2DGET
	if(ictr%100 == 0)rtl_printf("%s: %08d - In A2DGetData \n", __FILE__, ictr);
#endif
	ictr = 0;

//TODO get the timestamp and size in the first long word
//TODO put the size in the second entry (short)
//TODO make certain the write size is correct
//TODO check to see if fd_up is valid. If not, return an error
	
	// dataptr points to beginning of data section of A2DSAMPLE
	// has to be cast to a short *
	dataptr = (short *)buf.data;
	
#ifdef DEBUGA2DGET3
	ictr++;
#else
	while(!A2DFIFOEmpty())
	{
		// Point to FIFO read subchannel
		outb(A2DIOFIFO, (UC *)chan_addr);
		*dataptr++ = inw((US *)isa_address);		
		ictr++;	
	}
#endif

	buf.size = ictr*2;

#ifdef DEBUGA2DGET2
	if(cctr%100 == 0)rtl_printf("%s: %06d, size = %05d, datactr = %05d\n", 
				__FILE__, cctr, buf.size, ictr);
#else
    if (fd_up >= 0)
			rtl_write(fd_up, (void *)&buf, buf.size); // Write to up-fifo
#endif

	cctr++;

	return;
}

/*-----------------------Utility------------------------------*/
// A2DFIFOEmpty checks the FIFO empty status bit and returns
// -1 if empty, 0 if not empty

static int A2DFIFOEmpty()
{
	int stat;

	// Point at the FIFO status channel
	outb(A2DIOFIFOSTAT, (UC *)chan_addr);

	stat = (int)inb((UC *)isa_address);
	if(stat & FIFONOTEMPTY)return(0);
	else return(1);
}

/*-----------------------Utility------------------------------*/
// This routine sends the ABORT command to all A/D's.  
// The ABORT command amounts to a soft reset--they 
//  stay configured.

static void A2DReset(int A2DSel)
{
	//Point to the A2D command register
	outb(A2DCMNDWR, (UC *)chan_addr);

	// Send specified A/D the abort (soft reset) command
	outw(A2DABORT, (US *)isa_address + A2DSel);	
	return;
}

static void A2DResetAll()
{
	int i;

	for(i = 0; i < MAXA2DS; i++)A2DReset(i);
}

/*-----------------------Utility------------------------------*/
// This routine sets the A/D's to auto mode

static void A2DAuto(void)
{
	//Point to the FIFO Control word
	outb(A2DIOFIFO, (UC *)chan_addr);
	
	// Set Auto run bit and send to FIFO control byte
	FIFOCtl |=  A2DAUTO;
	outb(FIFOCtl, (UC *)isa_address);	
	return;
}

/*-----------------------Utility------------------------------*/
// This routine sets the A/D's to non-auto mode

static void A2DNotAuto(void)
{
	//Point to the FIFO Control word
	outb(A2DIOFIFO, (UC *)chan_addr);

	// Turn off the auto bit and send to FIFO control byte
	FIFOCtl &= ~A2DAUTO;	
	outb(FIFOCtl, (UC *)isa_address);	
	return;
}

/*-----------------------Utility------------------------------*/
// Start the selected A/D in acquisition mode

static void A2DStart(int A2DSel)
{
// Point at the A/D command channel
	outb(A2DCMNDWR, (UC *)chan_addr);

// Start the selected A/D
	outw(A2DREADDATA, (US *)isa_address + A2DSel);
	return;
}

static void A2DStartAll()
{
	int i;
	for(i = 0; i < MAXA2DS; i++)
	{
		A2DStart(i);
	}
}

/*-----------------------Utility------------------------------*/
// Configure A/D A2dSel with coefficient array 'filter'
static short A2DConfig(int A2DSel, US *filter)
{
	int j, ctr = 0;
	US stat;
	UC intmask=1, intbits[8] = {1,2,4,8,16,32,64,128};
	
// Point to the A/D write configuration channel
	outb(A2DCMNDWR, (UC *)chan_addr);

	// Set the interrupt mask 
	intmask = intbits[A2DSel];

// Set configuration write mode
	outw(A2DWRCONFIG, (US *)isa_address + A2DSel);

	for(j = 0; j < CONFBLLEN*CONFBLOCKS + 1; j++)
	{
		// Set channel pointer to Config write and
		//   write out configuration word
		outb(A2DCONFWR, (UC *)chan_addr);
		outw(filter[j], (US *)isa_address + A2DSel);
		rtl_usleep(30);

		// Set channel pointer to sysctl to read int lines
		// Wait for interrupt bit to set
		
		outb(A2DIOSYSCTL, (UC *)chan_addr);
		while((inb((UC *)isa_address) & intmask) == 0)
		{
			rtl_usleep(30);	
			if(ctr++ > GAZILLION)
			{
				rtl_printf("%s: \nINTERRUPT TIMEOUT! chip = %1d\n\n", 
						__FILE__, A2DSel);
				return(-2);
			}
		}
		// Read status word from target a/d to clear interrupt
		outb(A2DSTATRD, (UC *)chan_addr);
		stat = inw((US *)isa_address + A2DSel);

		// Check status bits for errors
		if(stat & A2DCRCERR)
		{
			rtl_printf("CRC ERROR! chip = %1d, stat = 0x%04X\n",
				A2DSel, stat);
			return(-1);
		}
	}
	rtl_usleep(2000);
	return(stat);
}

static void A2DConfigAll(US *filter)
{
	int i;
	for(i = 0; i < MAXA2DS; i++)A2DConfig(i, filter);
}

/*-----------------------Utility------------------------------*/
// Screen dump of critical command structure elements

static void A2DIoctlDump(A2D_SET *a2d)
{
	int i;
	for(i = 0;i < 8; i++)
		{
		rtl_printf("%s: \n", __FILE__);
		rtl_printf("Gain_%1d  = %3d\t", i, a2d->gain[i]);
		rtl_printf("Hz_%1d    = %3d\t", i, a2d->Hz[i]);
		rtl_printf("Offset  = 0x%02X\t", a2d->offset[i]);
		rtl_printf("Calset  = 0x%02X\n", a2d->calset[i]);
		}

	rtl_printf("Vcalx8  = %3d\n", a2d->vcalx8);
	rtl_printf("filter0 = 0x%04X\n", a2d->filter[0]);

	return;
}
