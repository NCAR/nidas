/*  a2d_driver.c

Time-stamp: <Sun 25-Oct-2004 12:48:04 pm>

Drivers and utility modules for NCAR/ATD/RAF DSM3 A/D card.

Copyright NCAR September 2004.

Original author:	Grant Gray

Revisions:

*/

#define	DEBUGIT(a)	rtl_printf("DEBUGIT at %d\n", a);
// #define		DEBUG0		//Activates specific debug functions
// #define		USE_RTLINUX 	//If not defined I/O is in debug mode

#include	<stdio.h>
#include	<stdlib.h>
#include	<math.h>
#include	<string.h>
#include 	<a2d_driver.h>


/* RTLinux module includes...  */
#include <rtl.h>
#include <rtl_posixio.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>

void ggoutw(US a, US *b);
void ggoutb(UC a, UC *b);
US gginw(US *a);
UC gginb(UC *a);
void phoney(A2D_SET *a2d);

// These are just test values

UC		Cals;
UC		Offsets;

/****************  IOCTL Section ***************8*********/

static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,_IOC_SIZE(GET_NUM_PORTS) },
  { A2D_GET_IOCTL,_IOC_SIZE(A2D_GET_IOCTL) },
  { A2D_SET_IOCTL,_IOC_SIZE(A2D_SET_IOCTL) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);

static struct ioctlHandle* ioctlhandle = 0;

/****************  End of IOCTL Section ******************/

static int boardNum = 0;
static int nports = 1;

/* module prameters (can be passed in via command line) */
static unsigned int a2d_parm = 0;
MODULE_PARM(a2d_parm, "1l");

RTLINUX_MODULE(A2D);
MODULE_AUTHOR("Grant Gray <gray@ucar.edu>");
MODULE_DESCRIPTION("A2D driver");

/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 */
static int A2DCallback(int cmd, int board, int port,
	void *buf, size_t len) {

  rtl_printf("A2DCallback, cmd=%d, board=%d, port=%d,len=%d\n",
		cmd,board,port,len);

  int ret = -EINVAL;
  switch (cmd) {
  case GET_NUM_PORTS:		/* user get */
    {
	rtl_printf("GET_NUM_PORTS\n");
	*(int *) buf = nports;
	ret = sizeof(int);
    }
    break;

  case A2D_GET_IOCTL:		/* user get */
    	{
	A2D_GET *tg = (A2D_GET *)buf;
	strncpy(tg->c,"0123456789",len);
	tg->c[len-1] = 0;
	ret = len;
    	}
    	break;

  case A2D_SET_IOCTL:		/* user set */
      	{
	A2D_SET *ts = (A2D_SET *)buf;
	rtl_printf("received A2D_SET\n");
	A2DSetup(ts);	//Call A2DSetup with structure pointer ts 
	ret = len;
    	}
   break;
  }
  return ret;
}


/*-----------------------Module------------------------------*/
// Stops the A/D and releases reserved memory
void cleanup_module(void)
{
  /* Close my ioctl FIFOs, deregister my ioctlCallback function */
  closeIoctlFIFO(ioctlhandle);
  //TODO rtl_unregister_dev("/dev/a2d");
  rtl_printf("Analog cleanup complete\n");
  return;
}

/*-----------------------Module------------------------------*/

int init_module()
{	
  	rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
	     __FILE__, __FUNCTION__, __DATE__, __TIME__);

  	/* Open up my ioctl FIFOs, register my ioctlCallback function */
  	ioctlhandle = openIoctlFIFO("a2d",boardNum,A2DCallback,
  					nioctlcmds,ioctlcmds);
rtl_printf("handle = 0x%08x\n");
rtl_printf("A2D initialization was just peachy!\n");

  	if (!ioctlhandle) return -EIO;
	else return 0;
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
		a2d->flag[i] = 0;	// Clear flags
		a2d->status[i] = 0;	// Clear status
		A2DSetCtr(a2d);		// Set up the counters
	}

	A2DSetMaster(a2d->master);

	A2DSetVcal((int)(8*a2d->vcalx8));	// Set the calibration voltage
	A2DSetCal(a2d);	// Switch appropriate channels to cal mode

	A2DClearSYNC();	// Start A/Ds synchronous with 1PPS from IRIG card
	A2D1PPSEnable();// DEBUG Do this just following a 1PPS transition
	A2DSetSYNC();	//	so that we don't step on the transition
	
	A2DPtrInit(a2d);// Initialize offset buffer pointers
	A2DRun(a2d);	// Infinite loop data acquisition

	return 0;
}

/*-----------------------Utility------------------------------*/
// A2DInit loads the A/D designated by A2DDes with filter 
//   data from A2D structure

// DEBUG checkout the cardaddress and such for this routine (old)

int A2DInit(int A2DDes, US *filter)
{
	int 	j, count;
	US	*filt;
	US	*A2DAddr, A2DStat;

	filt = (US *)filter;

	count = *filt++;
	A2DAddr = (US *)(CardBase + A2DDes);

	A2DChSel(A2DIOSTAT);		// Point at command channel	
	ggoutw(A2DWRCONFEM, A2DAddr);	// Write with masked errors
	A2DChSel(A2DIODATA);		// Point to A/D data channel

	for(j = 0; j < count; j++)
	{
		ggoutw(*filt++,A2DAddr);// Send forth the filter coefficients
	}

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
			rtl_printf("File cannot be opened\n");
			break;
		case ERRA2DCHAN:
			rtl_printf("Non-existent A2D channel\n");
			break;
		case ERRA2DVCAL:
			rtl_printf("Cal voltage out of limits\n");
			break;
		case ERRA2DGAIN:
			rtl_printf("Gain value out of limits\n");
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
	US *A2DAddr;

	if(chan < 0 || chan > 7)return;	//Error check, return without action

	A2DAddr = (US *)(CardBase + A2DIOLOAD);

	CurChan = chan;

	ggoutw((US)CurChan, A2DAddr);
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
	//	return(stat);
	return(A2DCONFIGEND);

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
	US *A2DAddr;

	//Check that gain is within limits
	if(A2DGain < 1 || A2DGain > 25)return(ERRA2DGAIN);

	if(A2DSel >= 0 && A2DSel <=3)	// If setting gain on channels 0-3
	{
		D2AChsel = A2DIOGAIN03;	// Use this channel select
		A2DAddr = (US *)(CardBase + A2DSel);//    and this address
	}

	if(A2DSel >= 4 && A2DSel <=7)	// Otherwise, for channels 4-7
	{
		D2AChsel = A2DIOGAIN47;		// Use this channel select and
		A2DAddr = (US *)(CardBase + A2DSel - 4); // this address
	}

	// If no A/D selected return error -1
	if(D2AChsel == -1)return(ERRA2DCHAN);	

	A2DChSel(D2AChsel);		// Set the card channel pointer

	GainCode = (US)(10 * A2DGain);	// Gain code = 10*gain

	ggoutw(GainCode, A2DAddr);

	return(GainCode);
}

/*-----------------------Utility------------------------------*/
//A2DSetMaster routes the interrupt signal from the target A/D chip 
//  to the ISA bus interrupt line.
// Checked visually 5/22/04 GRG

void A2DSetMaster(US A2DMr)
{
	US *A2DAddr;

	if(A2DMr > 7)return;	// Error check return with no action

	A2DAddr = (US *)(CardBase);	// Compute write address

	A2DChSel(A2DIOFIFOSTAT);	//Select the FIFO status channel (7)

	ggoutw(A2DMr, A2DAddr);
}

/*-----------------------Utility------------------------------*/

//Read the A/D chip interrupt line states
// Checked visually 5/22/04 GRG

US	A2DReadInts()
{
	US *A2DAddr;

	A2DAddr = (US *)(CardBase + A2DIOSYSCTL);

	return(gginw(A2DAddr) & 0x00FF);
}


/*-----------------------Utility------------------------------*/
//Set a calibration voltage
// Input is the calibration voltage * 8 expressed as an integer
//   i.e. -80 <= Vx8 <= +80 for -10 <= Calibration voltage <= +10
// Checked visually 5/22/04 GRG
US A2DSetVcal(int Vx8)
{
	US Vcode = 1; 
	US *A2DAddr;

	//Check that V is within limits
	if(Vx8 < -80 || Vx8 > 80)return(ERRA2DVCAL);

	A2DAddr = (US *)(CardBase);

	A2DChSel(A2DIOVCAL);		// Set the channel pointer to VCAL

	Vcode = Vx8 + 128 ;//Convert Vx8 to DAC code

	ggoutw(Vcode, A2DAddr);

	return(Vcode);
}

/*-----------------------Utility------------------------------*/

//Switch inputs specified by Chans bits to calibration mode: bits 8-15 -> chan 0-7
// Checked visually 5/22/04 GRG
void A2DSetCal(A2D_SET *a2d)
{	
	US *A2DAddr;
	US Chans;

	Chans = a2d->calset;
	
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
	US Chans;

	Chans = a2d->offset;

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
	
	*dataptr++ = gginw(A2DAddr);
	
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
	US *A2DAddr;

	A2DChSel(A2DIOFIFO);
	A2DAddr = (US *)(CardBase);
	 
	FIFOCtl &= ~A2DSYNC;	//Ensure that SYNC bit in FIFOCtl is cleared.

	ggoutw(FIFOCtl | A2DSYNC, A2DAddr);				// Set the SYNC data line
	ggoutw(FIFOCtl | A2DSYNC | A2DSYNCCK, A2DAddr);	//Cycle the sync clock while keeping sync high
	ggoutw(FIFOCtl | A2DSYNC, A2DAddr);							
	return;
}

/*-----------------------Utility------------------------------*/
// Clear the SYNC flag
// Checked visually 5/22/04 GRG

void A2DClearSYNC(void)
{
	US *A2DAddr;

	A2DChSel(A2DIOFIFO);
	A2DAddr = (US *)(CardBase + A2DIOFIFO);
	FIFOCtl &= ~A2DSYNC;					//Ensure that SYNC bit in FIFOCtl is cleared.
	ggoutw(FIFOCtl, A2DAddr);				// Clear the SYNC data line
	ggoutw(FIFOCtl | A2DSYNCCK, A2DAddr);	//Cycle the sync clock while keeping sync low
	ggoutw(FIFOCtl, A2DAddr);							
	return;
}

/*-----------------------Utility------------------------------*/
// Enable 1PPS sync 
// Checked visually 5/22/04 GRG
void A2D1PPSEnable(void)
{	
	US *A2DAddr;

	A2DChSel(A2DIOFIFO);
	A2DAddr = (US *)(CardBase + A2DIOFIFO);
	
	FIFOCtl |= A2D1PPSEBL;		//Set the FCW 1PPS enable bit

	ggoutw(FIFOCtl, A2DAddr);

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
	
	FIFOCtl &= ~A2D1PPSEBL;		//Clear the FCW 1PPS enable bit

	ggoutw(FIFOCtl, A2DAddr);

	return;
}

/*-----------------------Utility------------------------------*/

//Clear (reset) the data FIFO
// Checked visually 5/22/04 GRG

void A2DClearFIFO(void)
{
	US *A2DAddr;

	A2DChSel(A2DIOFIFO);
	A2DAddr = (US *)(CardBase + A2DIOFIFO);

	ggoutw(FIFOCtl, A2DAddr);
	ggoutw(FIFOCtl | FIFOCLR, A2DAddr);	// Cycle FCW bit 0 to clear FIFO
	ggoutw(FIFOCtl, A2DAddr);

	return;
}
/*-----------------------Utility------------------------------*/

// This will be the primary data acquisition loop which will be
// triggered by a 100 Hz semaphore.

void A2DRun(A2D_SET *a)
{
	while(1)
	{
	usleep(100000); 
	}
}
/*-----------------------Utility------------------------------*/

// The following I/O routines only simulate the corresponding 
// RTLinux routines.  The print out where and what is being 
// written/read.


void ggoutb(UC data, UC *addr)
{
#ifndef USE_RTLINUX
	rtl_printf("Write	0x%02x 	 to  0x%08x\n", data, addr);
#else
	outb(data, addr);
#endif
	return;
}

UC gginb(UC *addr)
{
#ifndef USE_RTLINUX
	rtl_printf("Read             from 0x%08x\n", addr);
	return(1);
#else
	return(inb(addr));
#endif
}

void ggoutw(US data, US *addr)
{
#ifndef USE_RTLINUX
	rtl_printf("Write	0x%04x to  0x%08x\n", data, addr);
#else
	outw(data, addr);
#endif

	return;
}

US gginw(US *addr)
{
#ifndef USE_RTLINUX
	rtl_printf("Read          from 0x%08x\n", addr);
	return(2);

#else
	return(inw(addr));
#endif	
}


void phoney(A2D_SET *a2d)
{
int i;
for(i = 0; i < 8; i++)
	{
	a2d->gain[i] = 10;
	a2d->Hz[i] = 50;
	a2d->flag[i] = 0;
	}
DEBUGIT(20);
	return;
}
