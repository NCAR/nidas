/*  a2d_driver.c/


Time-stamp: <Sat 13-Jan-2005 12:48:04 pm>

Drivers and utility modules for NCAR/ATD/RAF DSM3 A/D card.

Copyright NCAR September 2004.

Original author:	Grant Gray

Revisions:

*/

//#define INBUFWR		// Turn on simulated data array printout
//#define	DEBUGDATA 	// Turns on printout in data simulator
#define	INTERNAL	// Uses internal A/D filter code
#define FIX_A2D		// Reverse LS byte of ISA bus
#define	QUIETOUTB	// Shut off printout
#define	QUIETINB	// Shut off printout
#define	QUIETOUTW	// Shut off printout
#define	QUIETINW	// Shut off printout

// #define		DEBUG0		//Activates specific debug functions
// #define NOIRIG		// DEBUG mode, running without IRIG card
#define		USE_RTLINUX 	//If not defined I/O is in debug mode
#define		FREERUN		// Start A/D's without waiting for 1PPS
#define 	SYNC1PPS	// Enable start sync to 1PPS leading edge

// Enumerated A2D_RUN_IOCTL messages
#define	RUN	1
#define STOP	2
#define	RESTART	3

// A/D Configuration file parameters
#define CONFBLOCKS  12  // 12 blocks as described below
#define CONFBLLEN   43  // Block of 42 16-bit words plus a CRC word

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <fcntl.h>

/* RTLinux module includes...  */

#include <rtl.h>
#include <rtl_posixio.h>
#include <pthread.h>
#include <unistd.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <asm/io.h>

#include <ioctl_fifo.h>
#include <irigclock.h>
#include <a2d_driver.h>


void ggoutw(US a, US *b);
void ggoutb(UC a, UC *b);
US   gginw(US *a);
UC   gginb(UC *a);
void A2DIoctlDump(A2D_SET *a);
US   fix_a2d(US a);
UC   bit_rvs(UC a);

static 	US	CalOff = 0; 	//Static storage for cal and offset bits
static 	UL	CardBase;	// A/D card address
static	int	CurChan;	// Current channel value
static	US	FIFOCtl;	//Static hardware FIFO control word storage
static 	int	fd_up; 		// Data FIFO file descriptor
static 	int	a2drun = RUN;	// Internal flag for debugging
static  UL	ktime = 0;	// phoney timestamp for debugging

volatile unsigned int isa_address = A2DBASE;

pthread_t aAthread;

// These are just test values

UC		Cals;
UC		Offsets;


/****************  IOCTL Section ***************8*********/

static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,_IOC_SIZE(GET_NUM_PORTS) },
  { A2D_GET_IOCTL,_IOC_SIZE(A2D_GET_IOCTL) },
  { A2D_SET_IOCTL, sizeof(A2D_SET)  },
  { A2D_CAL_IOCTL, sizeof(A2D_SET)  },
  { A2D_RUN_IOCTL, sizeof(int)       },
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
	void *buf, size_t len) 
{
  int ret = -EINVAL;

  rtl_printf("\n%s: A2DCallback\n   cmd=%d\n   board=%d\n   port=%d\n   len=%d\n",
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
		A2D_GET *tg = (A2D_GET *)buf;
		tg->size = sizeof(A2D_GET) - 8;
		ret = len;
		}
    		break;

  	case A2D_SET_IOCTL:		/* user set */
		{
		rtl_printf("%s: A2D_SET_IOCTL\n", __FILE__);
		A2D_SET *ts = (A2D_SET *)buf;
#ifdef PRINTIOCTL
		A2DIoctlDump(ts);
#endif
		A2DSetup(ts);	//Call A2DSetup with structure pointer ts 
		ret = sizeof(A2D_SET);
		}
   		break;

  	case A2D_CAL_IOCTL:		/* user set */
		{
		rtl_printf("%s: A2D_CAL_IOCTL\n", __FILE__);
		A2D_SET *ts = (A2D_SET *)buf;
#ifdef PRINTIOCTL
		A2DIoctlDump(ts);
#endif
		A2DSetVcal((int)ts->vcalx8);
		A2DSetCal(ts);	//Call A2DSetup with structure pointer ts 
		ret = sizeof(A2D_SET);
		}
   		break;

  	case A2D_RUN_IOCTL:		/* user set */
		{
		rtl_printf("%s: A2D_RUN_IOCTL\n", __FILE__);
		int *tm =  (int *)buf;
		rtl_printf("Run message = 0x%08X\n", *tm);

	 	if((*tm & 0x0000000F) == RUN)
		{
			a2drun = 1;
			A2DSetSYNC();	//Stop A/D's
			A2DClearFIFO();	// Reset FIFO
			A2D1PPSEnable();// Enable sync with 1PPS
#ifdef NOIRIG
// If no IRIG card present, set up an independent thread to send 
//  simulated data to user space.
			rtl_printf("%s: Creating data simulation thread\n", 
					__FILE__);
			pthread_create(&aAthread, NULL, A2DGetDataSim, NULL);
			rtl_printf("(%s) %s:\tA2DGetDataSim thread created\n",
					__FILE__, __FUNCTION__);
#else
			register_irig_callback(&A2DGetData, 
						IRIG_100_HZ, 
						(void *)NULL);	
#endif
		}

		if((*tm & 0x0000000F) == STOP)
		{
			a2drun = STOP;
			A2DSetSYNC();	// Stop the A/D's
#ifndef NOIRIG
			unregister_irig_callback(&A2DGetData, IRIG_100_HZ);
#endif
		}
		ret = sizeof(long);
		}
		break;
  	}
  	return ret;
}


/*-----------------------Module------------------------------*/
// Stops the A/D and releases reserved memory
void cleanup_module(void)
{
  char fname[20];

  /* Close my ioctl FIFOs, deregister my ioctlCallback function */
  closeIoctlFIFO(ioctlhandle);


  /* Close and unlink the up fifo */

  sprintf(fname, "/dev/dsma2d_in_0");
  rtl_printf("%s : Destroying %s\n",__FILE__,  fname);
  close(fd_up);
  unlink("/dev/dsma2d_in_0");
  
#ifdef NOIRIG
	pthread_cancel(aAthread);
	pthread_join(aAthread, NULL);
#endif
  release_region(isa_address, A2DIOWIDTH);

  rtl_printf("%s : Analog cleanup complete\n", __FILE__);

  return;
}

/*-----------------------Module------------------------------*/

int init_module()
{	
	char fifoname[50];
	int error;

  	rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
	     __FILE__, __FUNCTION__, __DATE__, __TIME__);

  	/* Open up my ioctl FIFOs, register my ioctlCallback function */
  	ioctlhandle = openIoctlFIFO("dsma2d",boardNum,A2DCallback,
  					nioctlcmds,ioctlcmds);

  	if (!ioctlhandle) return -EIO;
	// Open the fifo TO user space (up fifo)
	sprintf(fifoname, "/dev/dsma2d_in_0");
	rtl_printf("%s: Creating %s\n", __FILE__, fifoname);
	if((error = mkfifo(fifoname, 0666)))
		rtl_printf("Error opening fifo %s for write\n", fifoname);
	else
		rtl_printf("Fifo %s opened for write\n", fifoname);
	fd_up = open(fifoname, O_NONBLOCK | O_WRONLY);

	rtl_printf("%s: Up FIFO fd = 0x%08x\n", __FILE__, fd_up);


	rtl_printf("%s: A2D initialization complete.\n", __FILE__);

	// Get the mapped board address
	request_region(isa_address, A2DIOWIDTH, "A2D_DRIVER");

	return 0;
}

int A2DSetup(A2D_SET *a2d)
	{
	int A2DErrorCode; /* TotalErrors = 0; */
	int i;

	FIFOCtl = 0;		//Clear FIFO Control Word
	CardBase = A2DBASE;	//Set CardBase to base IO address of card 

	A2DErrorCode = 0;

	for(i = 0; i < 8; i++)
	{	
		// Pass filter info to init routine
		A2DError(A2DInit(i, &a2d->filter[0]));
		A2DSetGain(i, a2d->gain[i]);
		a2d->status[i] = 0;	// Clear status
		A2DSetCtr(a2d);		// Set up the counters
	}

	A2DSetMaster(a2d->master);
//TODO Change argument of A2DSetVcal to A2D_SET *a2d
	A2DSetVcal((int)(a2d->vcalx8));	// Set the calibration voltage
	A2DSetCal(a2d);	// Switch appropriate channels to cal mode

	A2DClearSYNC();	// Start A/Ds synchronous with 1PPS from IRIG card

#ifdef SYNC1PPS
	A2D1PPSEnable();// DEBUG Do this just following a 1PPS transition
	A2DSetSYNC();	//	so that we don't step on the transition
#endif
	
	A2DPtrInit(a2d);// Initialize offset buffer pointers


	return 0;
}

/*-----------------------Utility------------------------------*/
// A2DInit loads the A/D designated by A2DDes with filter 
//   data from A2D structure

// DEBUG checkout the cardaddress and such for this routine (old)

int A2DInit(int A2DDes, US *filter)
{
	int 	j, k;
	US	*filt;
	US	*A2DAddr, A2DStat;

	filt = (US *)filter;

	A2DAddr = (US *)(CardBase + A2DDes);

	A2DChSel(A2DIOSTAT);		// Point at command channel	
	ggoutw(A2DWRCONFEM, A2DAddr);	// Write with masked errors

	A2DChSel(A2DIODATA);		// Point to A/D data channel
  	//Transfer configuration file to
  	for(j = 0; j < CONFBLOCKS; j++)
  	{
   	   for(k = 0; k < CONFBLLEN; k++)
   	   	{
   	       	ggoutw(filt[j*CONFBLLEN+k],A2DAddr);// Send forth the filter coefficients
   	   	}
  	}
   	if(A2DStatus(A2DDes) != A2DCONFIGEND)
   	   	rtl_printf("A/D%02X load error. A2DStatus = 0x%04x\n", 
			A2DDes, A2DStatus(A2DDes));
	else
		rtl_printf("A/D%02x Loaded OK\n", A2DDes);

	A2DStat = A2DStatus(A2DDes);

	switch(A2DStat)
	{
	case	A2DIDERR:		//Chip ID error
			A2DError(ERRA2DCHIPID);
			break;

	case	A2DCRCERR:		//Data corrupted--CRC error
			A2DError(ERRA2DCRC);
			break;

	case	A2DDATAERR:		//Conversion data invalid
			A2DError(ERRA2DCONV);
			break;

	case	A2DCONFIGEND:	// Good load
			A2DError(A2DLOADOK);
			break;

	default:
		break;
	}

	return(A2DLOADOK);
}


//TODO Make this work in with AnalogTable
// A2DPtrInit initializes the A2D->ptr[i] buffer offsets

void A2DPtrInit(A2D_SET *a2d)
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

void A2DError(int Code)
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
//Set initial card channel 0-7

void A2DChSel(int chan)
{
	UC *A2DAddr;

	if(chan < 0 || chan > 7)return;	//Error check, return without action

	A2DAddr = (UC *)(CardBase +  A2DIOLOAD);

	CurChan = chan;

	ggoutb((UC)CurChan, A2DAddr);
	return;
}

/*-----------------------Utility------------------------------*/

//Read status of A2D chip specified by A2DDes 0-7

US A2DStatus(int A2DDes)
{
	US *A2DAddr, stat;

	A2DAddr = (US *)(CardBase + A2DDes);

	A2DChSel(A2DIOSTAT);	// Point at Stat channel for read
	stat = gginw(A2DAddr);
	return(stat);
//	return(A2DCONFIGEND);

}

/*-----------------------Utility------------------------------*/
// A2DCommand issues a command word to the A/D selected by A2DSel

void A2DCommand(int A2DSel, US Command)
{
	US *A2DAddr;

	A2DChSel(A2DIOSTAT);	// Get ready to write to A/D Command channel

	A2DAddr = (US *)(CardBase + A2DSel);

	ggoutw(Command, A2DAddr);
}

/*-----------------------Utility------------------------------*/
// A2DSetRate
// Didn't really need this since it is not a hardware function.
//	so left it just to setting a parameter in the A2D structure.

// This routine sets the individual A/D sample counters
void A2DSetCtr(A2D_SET *a2d)
{
	int i;

	for(i = 0; i < 8; i++)
	{
		if(a2d->Hz[i] > 0)
		{
			// Set counter for channel
			a2d->ctr[i] = (US)((100/a2d->Hz[i]) +0.5);
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
UC A2DSetGain(int A2DSel, int A2DGain)
{
	int D2AChsel = -1;
	US GainCode = 1;
	UC *A2DAddr;

	//Check that gain is within limits
	if(A2DGain < 1 || A2DGain > 25)return(ERRA2DGAIN);

	A2DAddr = (UC *)CardBase + A2DSel;//    and this address

	if(A2DSel >= 0 && A2DSel <=3)	// If setting gain on channels 0-3
	{
		D2AChsel = A2DIOGAIN03;	// Use this channel select
	}

	if(A2DSel >= 4 && A2DSel <=7)	// Otherwise, for channels 4-7
	{
		D2AChsel = A2DIOGAIN47;		// Use this channel select and
		A2DAddr = (UC *)(CardBase + A2DSel - 4); // this address
	}

	// If no A/D selected return error -1
	if(D2AChsel == -1)return(ERRA2DCHAN);	

	A2DChSel(D2AChsel);		// Set the card channel pointer

	GainCode = (UC)(10 * A2DGain);	// Gain code = 10*gain

	ggoutb(GainCode, A2DAddr);

	return(GainCode);
}

/*-----------------------Utility------------------------------*/
//A2DSetMaster routes the interrupt signal from the target A/D chip 
//  to the ISA bus interrupt line.
// Checked visually 5/22/04 GRG

void A2DSetMaster(US A2DMr)
{
	UC *A2DAddr;

	if(A2DMr > 7)return;	// Error check return with no action

	A2DAddr = (UC *)(CardBase);	// Compute write address

	A2DChSel(A2DIOFIFOSTAT);	//Select the FIFO status channel (7)

	ggoutb(A2DMr, A2DAddr);
}

/*-----------------------Utility------------------------------*/

//Read the A/D chip interrupt line states
// Checked visually 5/22/04 GRG

UC	A2DReadInts(void)
{
	A2DChSel(A2DIOSYSCTL);	//Select the sysctl channel for read

	return(gginb((UC *)CardBase) & 0x00FF);
}


/*-----------------------Utility------------------------------*/
//Set a calibration voltage
// Input is the calibration voltage * 8 expressed as an integer
//   i.e. -80 <= Vx8 <= +80 for -10 <= Calibration voltage <= +10
// Checked visually 5/22/04 GRG
UC A2DSetVcal(int Vx8)
{
	US Vcode = 1; 
	UC *A2DAddr;

	//Check that V is within limits
	if(Vx8 < -80 || Vx8 > 80)return(ERRA2DVCAL);

	A2DAddr = (UC *)(CardBase);

	A2DChSel(A2DIOVCAL);		// Set the channel pointer to VCAL

	Vcode = Vx8 + 128 ;//Convert Vx8 to DAC code

	ggoutb(Vcode, A2DAddr);

	return(Vcode);
}

/*-----------------------Utility------------------------------*/

//Switch inputs specified by Chans bits to calibration mode: bits 8-15 -> chan 0-7
// Checked visually 5/22/04 GRG

void A2DSetCal(A2D_SET *a2d)
{	
	US *A2DAddr;
	US Chans = 0;
	int i;

	for(i = 0; i < MAXA2DS; i++)
	{
		Chans >>= 1; 
		if(a2d->calset[i] != 0)Chans += 0x80;
	}
	
	A2DChSel(A2DIOSYSCTL);
	A2DAddr = (US *)(CardBase);

	CalOff |= (US)((Chans<<8) & 0xFF00);
	
	ggoutw(CalOff, A2DAddr);

	return;
}

/*-----------------------Utility------------------------------*/

//Switch channels specified by Chans bits to offset mode: bits 0-7 -> chan 0-7
// Checked visually 5/22/04 GRG

void A2DSetOffset(A2D_SET *a2d)
{	
	US *A2DAddr;
	US Chans = 0;
	int i;

	for(i = 0; i < MAXA2DS; i++)
	{
		Chans >>= 1;
		if(a2d->offset[i] != 0)Chans += 0x80;
	}

	A2DChSel(A2DIOVCAL);
	A2DAddr = (US *)(CardBase);

	CalOff |= (US)(Chans & 0x00FF);

	ggoutw(CalOff, A2DAddr);
	
	return;
}

/*-----------------------Utility------------------------------*/
//Reads datactr status/data pairs from A2D FIFO
//

void A2DReadFIFO(int datactr, US *dataptr)
{
	US *A2DAddr;

	A2DChSel(A2DIOFIFO);

	A2DAddr = (US *)CardBase;		// Read the FIFO
	
	*dataptr = gginw(A2DAddr);
	
}

/*-----------------------Utility------------------------------*/
//Read datactr data values from A/D A2DSel and store in dataptr.
//

void A2DReadDirect(int A2DSel, int datactr, US *dataptr)
{
	US *A2DAddr;
	int i;

	A2DAddr = (US *)(CardBase + A2DSel);
	A2DChSel(A2DIODATA);

	for(i = 0; i < datactr; i++)
	{
		*dataptr++ = gginw(A2DAddr);
	}
}

/*-----------------------Utility------------------------------*/
//Set A2D SYNC flip/flop.  This stops the A/D's until cleared
//  under program control or by a positive 1PPS transition
// Checked visually 5/22/04 GRG

void A2DSetSYNC(void)
{
	UC *A2DAddr;

	A2DChSel(A2DIOFIFO);
	A2DAddr = (UC *)(CardBase);
	 
	FIFOCtl &= ~A2DSYNC;	//Ensure that SYNC bit in FIFOCtl is cleared.

	ggoutb(FIFOCtl | A2DSYNC, A2DAddr);	// Set the SYNC data line
	ggoutb(FIFOCtl | A2DSYNC | A2DSYNCCK, A2DAddr);	//Cycle the sync clock 
							// while keeping sync high
	ggoutb(FIFOCtl | A2DSYNC, A2DAddr);							
	return;
}

/*-----------------------Utility------------------------------*/

// Clear the SYNC flag
// Checked visually 5/22/04 GRG

void A2DClearSYNC(void)
{
	UC *A2DAddr;

	A2DChSel(A2DIOFIFO);
	A2DAddr = (UC *)CardBase;
	FIFOCtl &= ~A2DSYNC;	//Ensure that SYNC bit in FIFOCtl is cleared.
	ggoutb(FIFOCtl, A2DAddr);	// Clear the SYNC data line
	ggoutb(FIFOCtl | A2DSYNCCK, A2DAddr);	//Cycle the sync clock while keeping sync low
	ggoutb(FIFOCtl, A2DAddr);							
	return;
}

/*-----------------------Utility------------------------------*/
// Enable 1PPS sync 
// Checked visually 5/22/04 GRG
void A2D1PPSEnable(void)
{	
	UC *A2DAddr;

	A2DChSel(A2DIOFIFO);
	A2DAddr = (UC *)CardBase;
	
	ggoutb(FIFOCtl | A2D1PPSEBL, A2DAddr);

	return;
}

/*-----------------------Utility------------------------------*/

//Disable 1PPS sync 
// Checked visually 5/22/04 GRG

void A2D1PPSDisable(void)
{
	US *A2DAddr;

	A2DChSel(A2DIOFIFO);
	A2DAddr = (US *)(CardBase + A2DIOFIFO);
	
	ggoutw(FIFOCtl & ~A2D1PPSEBL, A2DAddr);

	return;
}

/*-----------------------Utility------------------------------*/

//Clear (reset) the data FIFO
// Checked visually 5/22/04 GRG

void A2DClearFIFO(void)
{
	UC *A2DAddr;

	A2DChSel(A2DIOFIFO);
	A2DAddr = (UC *)CardBase;

	FIFOCtl &= ~FIFOCLR; // Ensure that FIFOCLR bit is not set in FIFOCtl

	ggoutb(FIFOCtl, A2DAddr);
	ggoutb(FIFOCtl | FIFOCLR, A2DAddr);	// Cycle FCW bit 0 to clear FIFO
	ggoutb(FIFOCtl, A2DAddr);

	return;
}
/*-----------------------Utility------------------------------*/

// A2DGetData reads data from the A/D card hardware fifo and writes
// the data to the software up-fifo to user space.
void A2DGetData()
{
	US *A2DAddr;
//	US inbuf[HWFIFODEPTH];
	A2DSAMPLE buf;
	int i, j;

	A2DAddr = (US *)CardBase;
	A2DChSel(A2DIOFIFO);

	for(i = 0; i < INTRP_RATE; i++)
	{
		for(j = 0; j < MAXA2DS; j++)
		{
#ifdef DEBUG_FIFO
		buf.data[i][j] = i-HWFIFODEPTH/2; //Ramp
#else
		buf.data[i][j] = gginw(A2DAddr);	
#endif
//TODO get the timestamp and size in the first long word
//TODO put the size in the second entry (short)
//TODO make certain the write size is correct
//TODO check to see if fd_up is valid. If not, return an error
		}
		if((short)A2DFIFOEmpty())break;
	}
	write(fd_up, &buf, sizeof(A2DSAMPLE)); // Write to up-fifo
	return;
}
/*-----------------------Utility------------------------------*/
// A2DFIFOEmpty checks the FIFO empty status bit and returns
// -1 if empty, 0 if not empty

int A2DFIFOEmpty()
{
	int stat;
	UC *A2DAddr;
	A2DChSel(A2DIOFIFOSTAT);

	A2DAddr = (UC *)CardBase;
	
	stat = gginb(A2DAddr);
	if(stat & FIFONOTEMPTY)return(0);
	else return(-1);
}
/*-----------------------Utility------------------------------*/
// A2DGetDataSim is a debug tool which is activated by the 
// compiler switch NOIRIG.  I simply produces a ramp of data to 
// the up-fifo.

void A2DGetDataSim(void)
{
	A2DSAMPLE buf;
	int i, j, sign = 1; 
	size_t nbytes;
	
	rtl_printf("%s: Starting simulated data acquisition thread\n", __FILE__);	
	while(a2drun == RUN)
	{
		rtl_usleep(10000);
		for(i = 0; i < INTRP_RATE; i++)
		{
#ifdef INBUFWR
			rtl_printf("0x%05x: ", i*MAXA2DS);
#endif
			for(j = 0; j < MAXA2DS; j++)
			{
			buf.data[i][j] = sign*(1000*j + (j+1)*(j+1)); 
			sign *= -1;
#ifdef INBUFWR
			rtl_printf("%06d  ", buf.data[i][j]);
#endif
			}
#ifdef INBUFWR			
		rtl_printf("\n");
#endif
		}
#ifdef NOIRIG
		buf.timestamp = ktime;
#else
		buf.timestamp = GET_MSEC_CLOCK;
#endif
		buf.size = (dsm_sample_length_t)sizeof(buf.data);

/* rtl_printf("buf.size = 0x%04X\n", buf.size); */

#ifndef DEBUGFIFOWRITE
		nbytes = write(fd_up, &buf, sizeof(A2DSAMPLE)); //Write to up-fifo 
#endif
#ifdef DEBUGDATA
		errno = 0;
		if(!(ktime%10))rtl_printf("%6d: nbytes 0x%08X, errno 0x%08x, size %6d, time 0x%08d\n",
			 ktime, nbytes, errno, buf.size, buf.timestamp);
#endif
		ktime++;
	}
return;
}
/*-----------------------Utility------------------------------*/

// The following I/O routines only simulate the corresponding 
// RTLinux routines.  The print out where and what is being 
// written/read.


void ggoutb(UC data, UC *addr)
{
#ifndef USE_RTLINUX
#ifndef QUIETOUTB
	rtl_printf("Write	0x%02x 	 to  0x%08x\n", data, addr);
#endif

#else
	outb(data, addr);
#endif
	return;
}

UC gginb(UC *addr)
{
#ifndef USE_RTLINUX
#ifndef QUIETINB
	rtl_printf("Read             from 0x%08x\n", addr);
#endif
	return(1);
#else
	return(inb(addr));
#endif
}

void ggoutw(US data, US *addr)
{
#ifndef USE_RTLINUX
#ifndef QUIETOUTW
	rtl_printf("Write	0x%04x to  0x%08x\n", data, addr);
#endif

#else
	outw(fix_a2d(data), addr);
#endif

	return;
}

US gginw(US *addr)
{
#ifndef USE_RTLINUX
#ifndef QUIETINW
	rtl_printf("Read          from 0x%08x\n", addr);
#endif
	return(2);

#else
	return(fix_a2d(inw(addr)));
#endif	
}


void A2DIoctlDump(A2D_SET *a2d)
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

/* 
fix_a2d reverses the bits in the LS byte of the input unsigned short
Calls bit_rvs(unsigned char a);
*/

US fix_a2d(US viper)
{
#ifndef FIX_A2D
	return(viper);
#else
	return((viper &0xFF00) | (US)bit_rvs(viper & 0x00FF));
#endif
}


/* 
bit_rvs reverses the bits in a UC
*/
UC bit_rvs(UC incoming)
{
	UC masknum, locnum = incoming, revnum = 0;
	int i;

	masknum = 1;

	for(i = 0; i < 8; i++)
		{
		revnum <<= 1;
		if(locnum & masknum)revnum += 1;
		masknum <<=1;
		}
	return(revnum);	
}
