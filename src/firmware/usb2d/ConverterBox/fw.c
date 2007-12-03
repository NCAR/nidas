//-----------------------------------------------------------------------------
//	File:		fw.c
//	Contents:	Task dispatcher and device request parser
//				source.
//
//	Copyright (c) 2005 National Center For Atmospheric Research All rights reserved
//-----------------------------------------------------------------------------
#include "ezusb.h"
#include "ezregs.h"
#include <stdio.h>

//-----------------------------------------------------------------------------
// Random Macros
//-----------------------------------------------------------------------------
#define	min(a,b) (((a)<(b))?(a):(b))
#define	max(a,b) (((a)>(b))?(a):(b))
#define	CDEBUG		0x8003
#define HRESET  	0x8005
#define HADVAN  	0x8006
#define HOUSE0  	0x8007
#define HOUSE1  	0x8008
#define SHADW0  	0x8009
#define SHADW1  	0x800A
#define	AOUTA  		0x7F96
#define	AOEA		  0x7F9C
#define LEDRED		0xfe
#define LEDGREEN	0xfd
//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------
extern void my_usb2d(void);
extern void my_usb2d_init(void);
extern void settas(void);
extern void stoptas(void);
extern void setpot(void);
extern void setled(void);
extern void put_crlf(void);
extern void putchars(void);
extern BYTE	potdata;
extern BYTE ndiv;
extern BYTE color;
extern BYTE gstatus;
extern BYTE hksend;
extern BYTE busy;
extern bit tasstop;


volatile BOOL	GotSUD;
BOOL		Rwuen;
BOOL		Selfpwr;
volatile BOOL	Sleep;						// Sleep mode enable flag

WORD	pDeviceDscr;	// Pointer to Device Descriptor; Descriptors may be moved
WORD	pConfigDscr;	
WORD	pStringDscr;	
BYTE  xdata *h_advan;
BYTE  xdata *h_reset;
BYTE  xdata *hread0;
BYTE  xdata *hread1;
BYTE  xdata *sread0;
BYTE  xdata *sread1;
BYTE  count_hk = 0;
unsigned char  hdata0, hdata1, scount, shadow[10];
unsigned char  serial_char[2];

//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
void SetupCommand(void);
void TD_Init(void);
void TD_Poll(void);
void get_shadwor(void);
void get_house(void);
void send_house(void);
BOOL TD_Suspend(void);
BOOL TD_Resume(void);

BOOL DR_GetDescriptor(void);
BOOL DR_SetConfiguration(void);
BOOL DR_GetConfiguration(void);
BOOL DR_SetInterface(void);
BOOL DR_GetInterface(void);
BOOL DR_GetStatus(void);
BOOL DR_ClearFeature(void);
BOOL DR_SetFeature(void);
BOOL DR_VendorCmnd(void);

//-----------------------------------------------------------------------------
// Code
//-----------------------------------------------------------------------------
// Task dispatcher
void main(void)
{
  BYTE  xdata *debug;
  BYTE  xdata *aouta;
  BYTE  xdata *aoea;
  BYTE	count;
  BYTE  nmsec;
  DWORD	i;
  WORD	offset;
  WORD  count_led = 0;
  DWORD	DevDescrLen;
  DWORD	j=0;
  WORD	IntDescrAddr;
  WORD	ExtDescrAddr;

  // Initialize Global States
  Sleep = FALSE;					// Disable sleep mode
  Rwuen = FALSE;					// Disable remote wakeup
  Selfpwr = FALSE;				// Disable self powered
  GotSUD = FALSE;					// Clear "Got setup data" flag

  debug   = (BYTE xdata*)CDEBUG;
  h_advan = (BYTE xdata*)HADVAN;
  h_reset = (BYTE xdata*)HRESET;
  hread0  = (BYTE xdata*)HOUSE0;
  hread1  = (BYTE xdata*)HOUSE1;
  sread0  = (BYTE xdata*)SHADW0;
  sread1  = (BYTE xdata*)SHADW1;

  aoea  = (BYTE xdata*)AOEA;
  aouta = (BYTE xdata*)AOUTA;

  // Initialize user device
  TD_Init();
  // The following section of code is used to relocate the descriptor table. 
  // Since the SUDPTRH and SUDPTRL are assigned the address of the descriptor 
  // table, the descriptor table must be located in on-part memory.
  // The 4K demo tools locate all code sections in external memory.
  // The descriptor table is relocated by the frameworks ONLY if it is found 
  // to be located in external memory.
  pDeviceDscr = (WORD)&DeviceDscr;
  pConfigDscr = (WORD)&ConfigDscr;
  pStringDscr = (WORD)&StringDscr;
  if ((WORD)&DeviceDscr & 0xe000)
  {
    IntDescrAddr = INTERNAL_DSCR_ADDR;
    ExtDescrAddr = (WORD)&DeviceDscr;
    DevDescrLen = (WORD)&UserDscr - (WORD)&DeviceDscr + 2;
    for (i = 0; i < DevDescrLen; i++)
      *((BYTE xdata *)IntDescrAddr+i) = 0xCD;
    for (i = 0; i < DevDescrLen; i++)
      *((BYTE xdata *)IntDescrAddr+i) = *((BYTE xdata *)ExtDescrAddr+i);
    pDeviceDscr = IntDescrAddr;
    offset = (WORD)&DeviceDscr - INTERNAL_DSCR_ADDR;
    pConfigDscr -= offset;
    pStringDscr -= offset;
  }

  EZUSB_IRQ_ENABLE();				// Enable USB interrupt (INT2)
  EZUSB_ENABLE_RSMIRQ();				// Wake-up interrupt

  // The 8051 is responsible for all USB events, even those that have happened
  // before this point.  We cannot ignore pending USB interrupts.
  // The chip will come out of reset with the flags all cleared.
//  USBIRQ = 0xff;				// Clear any pending USB interrupt requests
  PORTACFG = 0x0;	  			// Turn off port A Alternate functions 
  PORTBCFG = 0x0;	  			// Turn off port B Alternate functions 
  PORTCCFG |= 0xc0;				// Turn on r/w lines for external memory 
  *aoea = 0x80;
  *aouta = 0x00;

  USBBAV = USBBAV | 1 & ~bmBREAK;	// Disable breakpoints and autovectoring
  USBIEN |= bmSUDAV | bmSUTOK | bmSUSP | bmURES;	// Enable selected interrupts

  #ifndef NO_RENUM
    // Note: at full speed, high speed hosts may take 5 sec to detect device
    EZUSB_Discon(TRUE); // Renumerate
  #endif


  PORTCCFG |= 0xcf;				// Turn on r/w lines for external memory 
  CKCON = (CKCON&(~bmSTRETCH)) | FW_STRETCH_VALUE; // Set stretch to 0 (after renumeration)

  my_usb2d_init();

  IT0 = 1;	//Neg. edge triggered INT0 
  IT1 = 1;	//Neg. edge triggered INT1
  EX0 = 1;	//Unmask INT0
  EX1 = 1;	//Unmask INT1
  EA = 1;	//Enable all unmasked 8051 interrupts

  EPIO[IN2BUF_ID].cntrl = 0x02;	// Clear busy bit

  color = LEDGREEN;
  setled();
  // Task Dispatcher
  while(TRUE)					// Main Loop
  {
    if(GotSUD) {			// Wait for SUDAV
      SetupCommand();	 		// Implement setup command
      GotSUD = FALSE;		   	// Clear SUDAV flag
    }
 
    // Poll User Device
    // NOTE: Idle mode stops the processor clock.  There are only two
    // ways out of idle mode, the WAKEUP pin, and detection of the USB
    // resume state on the USB bus.  The timers will stop and the
    // processor will not wake up on any other interrupts.
  
    if (Sleep) {
      if(TD_Suspend()) { //periph.c, always true
        Sleep = FALSE;	   		// Clear the "go to sleep" flag.  Do it here to prevent any race condition between wakeup and the next sleep.
        do {
       	  EZUSB_Susp();			// Place processor in idle mode. Library
    	  } while(!Rwuen && EZUSB_EXTWAKEUP()); // ezusb.h, Always false
        // Must continue to go back into suspend if the host has disabled remote wakeup
        // *and* the wakeup was caused by the external wakeup pin.
                
    	// 8051 activity will resume here due to USB bus or Wakeup# pin activity.
        EZUSB_Resume();	// If source is the Wakeup# pin, signal the host to Resume.
				              // EZUSB_Resume does nothing as Wakeup# pin grounded. Library
    	  TD_Resume(); //Always returns TRUE but doesn't do anything, periph.c
      }   
    }  

    if(	!(EPIO[OUT2BUF_ID].cntrl & bmEPBUSY) ) {	// Is there something in the OUT2BUF buffer,
      count = EPIO[OUT2BUF_ID].bytes;		// Then loopback the data
//          *aouta = 0x80;
//          *aouta = 0x00;

      if(count == 3) {
//        for(i=0; i<8; i++) {
//          hksend = 0x00;   
//          putchars();		// This is just a crude time delay
//        }
        if ((potdata != OUT2BUF[0]) || (ndiv != OUT2BUF[1])) {
    	    potdata = OUT2BUF[0];
          ndiv = OUT2BUF[1];
          setpot();
        }
        nmsec = OUT2BUF[2];

        if(nmsec % 2) get_shadwor() ;
        if(nmsec == 0) {
          *aouta = 0x80;
          *aouta = 0x00;
          get_house();
          send_house();
        }

      } 

      EPIO[OUT2BUF_ID].bytes = 0;
    }

    if( !(EPIO[IN2BUF_ID].cntrl & bmEPBUSY) )	// Is the IN2BUF available, .ctrl=IN2CS
    {

//      USBIEN = 0x00;	// Disable USB interrupts
      if(tasstop) {
        settas();
      }
      my_usb2d();

      if(busy & bmBIT0) {
        if(color == LEDGREEN) {
          color = LEDRED;
          setled();
        }
      }
      else {
        if(color == LEDRED) {
          count_led += 1;
          if(count_led == 1024) {
            color = LEDGREEN;
            count_led = 0;
            setled();
          }
        }
      }

    }
//      USBIEN |= bmSUDAV | bmSUTOK | bmSUSP | bmURES;	// Enable USB interrupts

//  	  *debug = 0x01;
//  	  *debug = 0x01;
  }
}

// Get ShadowOr
void get_shadwor(void)
{
  shadow[scount++] = *sread1;
  shadow[scount++] = *sread0;
  if(scount == 10) scount = 0;
}

// Get Housekeeping
void get_house(void)
{
  hdata0 = *hread0;
  hdata1 = *hread1;
  *h_advan = 0x0;
  count_hk += 1;
  if(count_hk == 8) {
    *h_reset = 0x0;
    count_hk = 0;
  }
}

// Send Housekeeping
void send_house(void)
{
  int i;

  for(i=0; i<5; i++) {
    hksend = sprintf(serial_char,"%02x",shadow[i*2]);
    hksend = serial_char[1];
    putchars();
    hksend = serial_char[0];   
    putchars();
    hksend = sprintf(serial_char,"%02x",shadow[i*2 + 1]);
    hksend = serial_char[1];
    putchars();
    hksend = serial_char[0];   
    putchars();
    hksend = 0x20;
    putchars();
  }

  hksend = sprintf(serial_char,"%02x",count_hk);
  hksend = serial_char[1];
  putchars();
  hksend = serial_char[0];   
  putchars();
  hksend = 0x20;
  putchars();

  hksend = sprintf(serial_char,"%02x",hdata1);
  hksend = serial_char[1];
  putchars();
  hksend = serial_char[0];   
  putchars();

  hksend = sprintf(serial_char,"%02x",hdata0);
  hksend = serial_char[1];
  putchars();
  hksend = serial_char[0];   
  putchars();

  put_crlf();

}

// Device request parser
void SetupCommand(void)
{
	void	*dscr_ptr;
	DWORD	i;

	switch(SETUPDAT[1])
	{
		case SC_GET_DESCRIPTOR:						// *** Get Descriptor
			if(DR_GetDescriptor())
				switch(SETUPDAT[3])			
				{
					case GD_DEVICE:				// Device
						SUDPTRH = MSB(pDeviceDscr);
						SUDPTRL = LSB(pDeviceDscr);
						break;
					case GD_CONFIGURATION:			// Configuration
						if(dscr_ptr = (void *)EZUSB_GetConfigDscr(SETUPDAT[2]))
						{
							SUDPTRH = MSB(dscr_ptr);
							SUDPTRL = LSB(dscr_ptr);
						}
						else
							EZUSB_STALL_EP0(); 	// Stall End Point 0
						break;
					case GD_STRING:				// String
						if(dscr_ptr = (void *)EZUSB_GetStringDscr(SETUPDAT[2]))
						{
							// Workaround for rev D errata number 8
							// If you're certain that you will never run on rev D,
							// you can just do this:
							// SUDPTRH = MSB(dscr_ptr);
							// SUDPTRL = LSB(dscr_ptr);
							STRINGDSCR *sdp;
							BYTE len;

							sdp = dscr_ptr;

							len = sdp->length;
							if (len > SETUPDAT[6]) 
								len = SETUPDAT[6]; //limit to the requested length
							
							while (len)
							{
								for(i=0; i<min(len,64); i++)
									*(IN0BUF+i) = *((BYTE xdata *)sdp+i);

								//set length and arm Endpoint
								EZUSB_SET_EP_BYTES(IN0BUF_ID,min(len,64));	
								len -= min(len,64);
                        (BYTE *)sdp += 64;

								// Wait for it to go out (Rev C and above)
								while(EP0CS & 0x04)
									;
							}

							// Arm a 0 length packet just in case.  There was some reflector traffic about
							// Apple hosts asking for too much data.  This will keep them happy and will
							// not hurt valid hosts because the next SETUP will clear this.
							EZUSB_SET_EP_BYTES(IN0BUF_ID,0);	
							// Clear the HS-nak bit
							EP0CS = bmHS;
						}
						else 
							EZUSB_STALL_EP0();	// Stall End Point 0
						break;
					default:				// Invalid request
						EZUSB_STALL_EP0();		// Stall End Point 0
				}
			break;
		case SC_GET_INTERFACE:						// *** Get Interface
			DR_GetInterface();
			break;
		case SC_SET_INTERFACE:						// *** Set Interface
			DR_SetInterface();
			break;
		case SC_SET_CONFIGURATION:					// *** Set Configuration
			DR_SetConfiguration();
			break;
		case SC_GET_CONFIGURATION:					// *** Get Configuration
			DR_GetConfiguration();
			break;
		case SC_GET_STATUS:						// *** Get Status
			if(DR_GetStatus())
				switch(SETUPDAT[0])
				{
					case GS_DEVICE:				// Device
						IN0BUF[0] = ((BYTE)Rwuen << 1) | (BYTE)Selfpwr;
						IN0BUF[1] = 0;
						EZUSB_SET_EP_BYTES(IN0BUF_ID,2);
						break;
					case GS_INTERFACE:			// Interface
						IN0BUF[0] = 0;
						IN0BUF[1] = 0;
						EZUSB_SET_EP_BYTES(IN0BUF_ID,2);
						break;
					case GS_ENDPOINT:			// End Point
						IN0BUF[0] = EPIO[EPID(SETUPDAT[4])].cntrl & bmEPSTALL;
						IN0BUF[1] = 0;
						EZUSB_SET_EP_BYTES(IN0BUF_ID,2);
						break;
					default:				// Invalid Command
						EZUSB_STALL_EP0();		// Stall End Point 0
				}
			break;
		case SC_CLEAR_FEATURE:						// *** Clear Feature
			if(DR_ClearFeature())
				switch(SETUPDAT[0])
				{
					case FT_DEVICE:				// Device
						if(SETUPDAT[2] == 1)
							Rwuen = FALSE; 		// Disable Remote Wakeup
						else
							EZUSB_STALL_EP0();	// Stall End Point 0
						break;
					case FT_ENDPOINT:			// End Point
						if(SETUPDAT[2] == 0)
                  {
							EZUSB_UNSTALL_EP( EPID(SETUPDAT[4]) );
                     EZUSB_RESET_DATA_TOGGLE( SETUPDAT[4] );
                  }
						else
							EZUSB_STALL_EP0();	// Stall End Point 0
						break;
				}
			break;
		case SC_SET_FEATURE:						// *** Set Feature
			if(DR_SetFeature())
				switch(SETUPDAT[0])
				{
					case FT_DEVICE:				// Device
						if(SETUPDAT[2] == 1)
							Rwuen = TRUE;		// Enable Remote Wakeup
						else
							EZUSB_STALL_EP0();	// Stall End Point 0
						break;
					case FT_ENDPOINT:			// End Point
						if(SETUPDAT[2] == 0)
							EZUSB_STALL_EP( EPID(SETUPDAT[4]) );
						else
							EZUSB_STALL_EP0();	 // Stall End Point 0
						break;
				}
			break;
		default:							// *** Invalid Command
			if(DR_VendorCmnd())
				EZUSB_STALL_EP0();				// Stall End Point 0
	}

	// Acknowledge handshake phase of device request
	// Required for rev C does not effect rev B
	EP0CS |= bmBIT1;
}

// Wake-up interrupt handler
void resume_isr(void) interrupt WKUP_VECT
{
	EZUSB_CLEAR_RSMIRQ();
}
