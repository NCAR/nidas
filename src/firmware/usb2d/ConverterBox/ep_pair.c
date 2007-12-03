#pragma NOIV					// Do not generate interrupt vectors
//-----------------------------------------------------------------------------
//	File:		periph.c
//	Contents:	Hooks required to implement USB peripheral function.
//
//	Copyright (c) 2005 National Center For Atmospheric Research All rights reserved
//-----------------------------------------------------------------------------
#include "ezusb.h"
#include "ezregs.h"

#define	CDEBUG	0x8003
extern BOOL	GotSUD;			// Received setup data flag
extern BOOL Sleep;
extern void my_usb2d(void);

void TD_Poll(void);

BYTE	Configuration;		// Current configuration
BYTE	AlternateSetting;	// Alternate settings
//-----------------------------------------------------------------------------
// Task Dispatcher hooks
//	The following hooks are called by the task dispatcher.
//-----------------------------------------------------------------------------

void TD_Init(void) 				// Called once at startup
{
	// Enable endpoint 2 in, and endpoint 2 out
	IN07VAL = bmEP2;			// Validate all EP's
	OUT07VAL = bmEP2;

	// Enable double buffering on endpoint 2 in, and endpoint 2 out
	USBPAIR = 0x09;

	// Arm Endpoint 2 out to recieve data
	EPIO[OUT2BUF_ID].bytes = 0;

	// Setup breakpoint to trigger on TD_Poll()
	BPADDR = (WORD)TD_Poll;
	USBBAV |= bmBPEN;				// Enable the breakpoint
	USBBAV &= ~bmBPPULSE;
}

void TD_Poll(void) 				// Called repeatedly while the device is idle
{
	BYTE	count, i;

	if(	!(EPIO[OUT2BUF_ID].cntrl & bmEPBUSY) )		// Is there something in the OUT2BUF buffer,
		if( !(EPIO[IN2BUF_ID].cntrl & bmEPBUSY) )	// Is the IN2BUF available,
		{
			count = EPIO[OUT2BUF_ID].bytes;		// Then loopback the data
			for(i=0;i<count;++i)
				IN2BUF[i] = OUT2BUF[i];
  			EPIO[OUT2BUF_ID].bytes = 0;
	  		EPIO[IN2BUF_ID].bytes = count;
		}
}

BOOL TD_Suspend(void) 			// Called before the device goes into suspend mode
{
	// Turn off breakpoint light before entering suspend
	USBBAV |= bmBREAK;		// Clear the breakpoint
	return(TRUE);
}

BOOL TD_Resume(void) 			// Called after the device resumes
{
	return(TRUE);
}

//-----------------------------------------------------------------------------
// Device Request hooks
//	The following hooks are called by the end point 0 device request parser.
//-----------------------------------------------------------------------------

BOOL DR_GetDescriptor(void)
{
	return(TRUE);
}

BOOL DR_SetConfiguration(void)	// Called when a Set Configuration command is received
{
	Configuration = SETUPDAT[2];
	return(TRUE);				// Handled by user code
}

BOOL DR_GetConfiguration(void)	// Called when a Get Configuration command is received
{
	IN0BUF[0] = Configuration;
	EZUSB_SET_EP_BYTES(IN0BUF_ID,1);
	return(TRUE);				// Handled by user code
}

BOOL DR_SetInterface(void) 		// Called when a Set Interface command is received
{
	AlternateSetting = SETUPDAT[2];
	return(TRUE);				// Handled by user code
}

BOOL DR_GetInterface(void) 		// Called when a Set Interface command is received
{
	IN0BUF[0] = AlternateSetting;
	EZUSB_SET_EP_BYTES(IN0BUF_ID,1);
	return(TRUE);				// Handled by user code
}

BOOL DR_GetStatus(void)
{
	return(TRUE);
}

BOOL DR_ClearFeature(void)
{
	return(TRUE);
}

BOOL DR_SetFeature(void)
{
	return(TRUE);
}

BOOL DR_VendorCmnd(void)
{
	return(TRUE);
}

//-----------------------------------------------------------------------------
// USB Interrupt Handlers
//	The following functions are called by the USB interrupt jump table.
//-----------------------------------------------------------------------------

// Setup Data Available Interrupt Handler
void ISR_Sudav(void) interrupt 0
{

//BYTE  xdata	*debug;
/*
debug   = (BYTE xdata*)CDEBUG;
  	  *debug = 0x01;
*/
	GotSUD = TRUE;				// Set flag
	EZUSB_IRQ_CLEAR();
	USBIRQ = bmSUDAV;			// Clear SUDAV IRQ
}

// Setup Token Interrupt Handler
void ISR_Sutok(void) interrupt 0
{
/*
BYTE  xdata	*debug;
      debug   = (BYTE xdata*)CDEBUG;
  	  *debug = 0x01;
  	  *debug = 0x01;
*/
	EZUSB_IRQ_CLEAR();
	USBIRQ = bmSUTOK;			// Clear SUTOK IRQ
}

void ISR_Sof(void) interrupt 0
{
	EZUSB_IRQ_CLEAR();
	USBIRQ = bmSOF;				// Clear SOF IRQ
}

void ISR_Ures(void) interrupt 0
{
/*
BYTE  xdata	*debug;
      debug   = (BYTE xdata*)CDEBUG;
  	  *debug = 0x01;
  	  *debug = 0x01;
  	  *debug = 0x01;
*/
	EZUSB_IRQ_CLEAR();
	USBIRQ = bmURES;			// Clear URES IRQ
}

void ISR_IBN(void) interrupt 0
{
   // ISR for the IN Bulk NAK (IBN) interrupt.
}

void ISR_Susp(void) interrupt 0
{
/*
BYTE  xdata	*debug;

      debug   = (BYTE xdata*)CDEBUG;
  	  *debug = 0x01;
  	  *debug = 0x01;
  	  *debug = 0x01;
  	  *debug = 0x01;
*/
	Sleep = TRUE;
	EZUSB_IRQ_CLEAR();
	USBIRQ = bmSUSP;
}

void ISR_Ep0in(void) interrupt 0
{
}

void ISR_Ep0out(void) interrupt 0
{
}

void ISR_Ep1in(void) interrupt 0
{
}

void ISR_Ep1out(void) interrupt 0
{
}

void ISR_Ep2in(void) interrupt 0
{
}

void ISR_Ep2out(void) interrupt 0
{
}

void ISR_Ep3in(void) interrupt 0
{
}

void ISR_Ep3out(void) interrupt 0
{
}

void ISR_Ep4in(void) interrupt 0
{
}

void ISR_Ep4out(void) interrupt 0
{
}

void ISR_Ep5in(void) interrupt 0
{
}

void ISR_Ep5out(void) interrupt 0
{
}

void ISR_Ep6in(void) interrupt 0
{
}

void ISR_Ep6out(void) interrupt 0
{
}

void ISR_Ep7in(void) interrupt 0
{
}

void ISR_Ep7out(void) interrupt 0
{
}
