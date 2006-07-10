// #define DEBUGFILTER
// #define PRINTIT
/*
testa2dX.c X = micro test

Time-stamp: <Wed 8-Sept-2004 12:18:00>

Original author:	Grant Gray

*/
#define		ISA_BASE	0xf7000000
#define		BOARD_OFF	0x03A0
#define		P_FIFO		0x0000
#define		P_A2DSTAT	0x0001
#define		P_A2DDATA	0x0002
#define		P_D2A0		0x0003
#define		P_D2A1		0x0004
#define		P_D2A2		0x0005
#define		P_SYSCTL	0x0006
#define		P_FIFOSTAT	0x0007
#define		CALCODELOW	48
#define		CALCODEHI	208

#define 	GAZILLION	10000
#define		DELAYNUM0	100
#define		DELAYNUM1	10000
#define		IOWIDTH		0x10
#define		US	unsigned short
#define		UC	unsigned char
#define		FILTER10KHZ	3
#define		FILTER5KHZ	2
#define		FILTER1KHZ	1
#define 	FILTER500HZ	0

/* RTLinux module includes...  */
#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_stdio.h>
#include <rtl_stdlib.h>
#include <rtl_pthread.h>
#include <rtl_unistd.h>
#include <irigclock.h>
#include <rtl_posixio.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <a2d_driver.h>
#include <filter10KHz.h>
#include <filter5KHz.h>
#include <filter1KHz.h>
#include <filter500Hz.h>

rtl_pthread_t aAthread;

int init_module(void);
void cleanup_module(void);
static void *TryA2D_thread(void *a);
//static unsigned char FIFOCtlRun = FIFOWREBL | A2DAUTO;
static unsigned char FIFOCtlRun = A2DAUTO;
static unsigned char FIFOCtlLoad = 0;
static unsigned char FIFOCtl;
static unsigned char a2dmaster = 0x0; // A2DMASTER;

volatile unsigned int isa_address = ISA_BASE+BOARD_OFF;
volatile unsigned int chan_addr;
volatile unsigned short *filterptr;

volatile long master = 0;
volatile long boot = 3;
volatile long ncycles = 2;
volatile long filter = 2;
volatile long gain = 1;
volatile long nsamp = 1;


RTLINUX_MODULE(trya2d);
MODULE_AUTHOR("Grant Gray <gray@ucar.edu>");
MODULE_DESCRIPTION("ISA bus test module");
MODULE_PARM(master, "1l");
MODULE_PARM_DESC(master, "Sets master A/D");
MODULE_PARM(boot, "1l");
MODULE_PARM_DESC(boot, "0=none, 1=BFIR, 2=header w/error masked, 3=header no mask");
MODULE_PARM(ncycles, "1l");
MODULE_PARM_DESC(ncycles, "Sets number of data acquistion cycles");
MODULE_PARM(filter, "1l");
MODULE_PARM_DESC(filter, "Sets filter to be loaded: 2-10Kz, 1-1KHz, 0-500Hz");
MODULE_PARM(gain, "1l");
MODULE_PARM_DESC(gain, "Set gain codes to 10*gain");
MODULE_PARM(nsamp, "1l");
MODULE_PARM_DESC(nsamp, "Number of output samples to average");

int init_module(void)
{
	// Set up filter pointer
	switch(filter)
	{
		case FILTER500HZ:
			filterptr = (US *)filter500Hz;
			break;

		case FILTER1KHZ:
			filterptr = (US *)filter1KHz;
			break;

		case FILTER5KHZ:
			filterptr = (US *)filter5KHz;
			break;

		case FILTER10KHZ:
			filterptr = (US *)filter10KHz;
			break;

		default:
	}
	rtl_printf("(%s) %s:\tCompiled on %s at %s\n",
		__FILE__, __FUNCTION__, __DATE__, __TIME__);

	rtl_printf("(%s) %s:\tISA address = 0x%08X\n", 
		__FILE__, __FUNCTION__,isa_address);

	request_region(isa_address, IOWIDTH, "TRYA2D");
	rtl_pthread_create(&aAthread, NULL, TryA2D_thread, (void *)0);
	rtl_printf("(%s) %s:\tTryA2D thread created\n", 
		__FILE__, __FUNCTION__);
	chan_addr = isa_address + 0x0000000F;
	rtl_printf("(%s): isa_address = 0x%08X; chan_addr = 0x%08X\n",
		__FILE__, isa_address, chan_addr);
	rtl_printf("(%s): initialized\n", __FILE__);
	return 0;
}

void cleanup_module(void)
{
	int i;
	US stat;
	UC ints;

	//Take CPLD out of AUTO mode 
	FIFOCtl = FIFOCtlLoad;
	outb((A2DIOFIFO), (UC *)chan_addr);
	outb((FIFOCtlLoad), (UC *)isa_address);
	rtl_printf("%s: Reset FIFOCtl = 0x%02X\n", __FILE__, FIFOCtl);

	// Read all the status lines
	outb((A2DSTATRD), (UC *)chan_addr);
	for(i = 0; i < MAXA2DS; i++)
	{
		stat = (inw((US *)isa_address + i));
		if(i == a2dmaster)rtl_printf("master stat = 0x%04X\n", stat);
	}

	// Read all the int lines
	outb((A2DIOSYSCTL), (UC *)chan_addr);
	ints = (inb((UC *)isa_address));
	rtl_printf("Int lines = 0x%02X\n", ints);
	
	// Shut down A/D's
	outb((A2DCMNDWR), (UC *)chan_addr);
	outw((A2DABORT), (US *)isa_address);

	rtl_pthread_cancel(aAthread);
	rtl_pthread_join(aAthread, NULL);
	release_region(isa_address, IOWIDTH);
	rtl_printf("(%s): all cleaned up\n", __FILE__);
	return;
}

static void *TryA2D_thread(void *t)
{
	int i, j, k = 0, l, m; 
	int ctr = 0, ndata = 800, datavol[] = {40, 80, 400, 800};
	int printmod = 100;
	US stat, ints;
	UC stat0, stat1;
	unsigned char intbit = 1;
	US data[1024];
	long avgs[MAXA2DS];

	ndata = datavol[filter];

	rtl_printf("%s: In p-thread, FIFOCtl = 0x%02X\n", __FILE__, FIFOCtl);
	// Reset A/D's don't change configuration

// Before doing ANYTHING, start then reset the A/D's

// Start the conversions
	rtl_printf("%s: Starting A/D's ", __FILE__);
   	outb((A2DCMNDWR), (UC *)chan_addr);
	for(j = 0; j < MAXA2DS; j++)
	{
   		outw((A2DREADDATA), (US *)isa_address + j);
		rtl_printf("%1d ", j);
	}
	rtl_usleep(10000); // Let them run a few milliseconds (10)
	rtl_printf("\n");

// Then do a soft reboot
	rtl_printf("%s: Soft resetting A/D's ", __FILE__);
	outb((A2DCMNDWR), (UC *)chan_addr);
	for(j = 0; j < MAXA2DS; j++)
	{
		outw((A2DABORT), (US *)isa_address + j);
		rtl_printf("%1d ", j);
	}
	rtl_printf("\n");

/* These didn't have the channel pointer set to A2DIOFIFO
//Clear SYNC 
	outb((FIFOCtl & ~A2DSYNC), (UC *)isa_address);
	outb(((FIFOCtl & ~A2DSYNC) | A2DSYNCCK), (UC *)isa_address);
	outb((FIFOCtl & ~A2DSYNC), (UC *)isa_address);

//Set SYNC and stop the A2D's
	outb((FIFOCtl | A2DSYNC), (UC *)isa_address);
	outb((FIFOCtl | A2DSYNC | A2DSYNCCK), (UC *)isa_address);
	outb((FIFOCtl | A2DSYNC), (UC *)isa_address);
*/
// Set FIFOCtl for load mode
	// Point at the FIFO control word
	//  and write FIFOCtlLoad into register on CPLD
	FIFOCtl = FIFOCtlLoad;
	outb((A2DIOFIFO), (UC *)chan_addr);
	outb((FIFOCtl), (UC *)isa_address);

// Set the gain codes to 10*gain
	outb(A2DIOGAIN03, (UC *)chan_addr);
	for(j = 0; j < MAXA2DS/2; j++)
		outb(10*gain, (UC *)isa_address + j);
	outb(A2DIOGAIN47, (UC *)chan_addr);
	for(j = 0; j < MAXA2DS/2; j++)
		outb(10*gain, (UC *)isa_address + j);

// Set master A/D
	// If the module parameter 'master' is non-zero, replace a2dmaster
	if(master != 0)a2dmaster = (UC)master;

	// Point at FIFOSTAT and set master A/D
	outb((A2DIOFIFOSTAT), (UC *)chan_addr);
	outb((a2dmaster), (UC *)isa_address);
	rtl_printf("%s: Setting a2dmaster = 0x%02X\n", __FILE__, a2dmaster);

	if(boot == 1)
	{
	// Configure A/D's from internal ROM
		outb((A2DCMNDWR), (UC *)chan_addr);
		for(j = 0; j < MAXA2DS; j++)
		{
			outw((A2DBFIR), (US *)isa_address + j);
		}
	}

	else if(boot == 2)
	{

		// Set A/D's to accept configuration data with errors masked
		outb((A2DCMNDWR), (UC *)chan_addr);

		for(k = 0; k < MAXA2DS; k++)
		{
			outw((A2DWRCONFEM), (US *)isa_address + k);
		}

		outb((A2DCONFWR), (UC *)chan_addr);

		for(k = 0; k < MAXA2DS; k++)
		{
			for(j = 0; j < CONFBLLEN*CONFBLOCKS + 1; j++)
			{
#ifdef DEBUGFILTER
				if(k == 0)
				{
					if((j % 10) == 0)rtl_printf("\n%03d:", j);
					rtl_printf(" %04X", filterptr[j]);	
				}
#endif
				outw((filterptr[j]), (US *)isa_address + k);
				rtl_usleep(20);
			}
		}
	}

	else if(boot == 3)
	{
		// Set A/D's to accept configuration data
		outb((A2DCMNDWR), (UC *)chan_addr);

		for(k = 0; k < MAXA2DS; k++)
		{
			outw((A2DWRCONFIG), (US *)isa_address + k);
		}


		intbit = 1;

		for(k = 0; k < MAXA2DS; k++)
		{
			for(j = 0; j < CONFBLLEN*CONFBLOCKS + 1; j++)
			{
#ifdef DEBUGFILTER
				if(k == 0)
				{
					if((j % 10) == 0)rtl_printf("\n%03d:", j);
					rtl_printf(" %04X", filterptr[j]);	
				}
#endif
				// Set channel pointer to Config write and
				//   write out configuration word
				outb((A2DCONFWR), (UC *)chan_addr);
				outw((filterptr[j]), (US *)isa_address + k);
				rtl_usleep(30);
	
				// Set channel pointer to sysctl to read int lines
				// Wait for interrupt bit to set
			
				outb((A2DIOSYSCTL), (UC *)chan_addr);
				while(((inb((UC *)isa_address)) & intbit) == 0)
				{
					rtl_usleep(30);	
					if(ctr++ > GAZILLION)
					{
						rtl_printf("INTERRUPT TIMEOUT! chip = %1d\n", k);
						goto byebye;
					}
				}
				// Read status word from target a/d to clear interrupt
				outb((A2DSTATRD), (UC *)chan_addr);
				stat = (inw((US *)isa_address + k));

				// Check status bits for errors
				if(stat & A2DCRCERR)
				{
					rtl_printf("CRC ERROR! chip = %1d, stat = 0x%04X\n",
						k, stat);
					break;
				}
			}

byebye:
			rtl_usleep(2000);
			intbit <<= 1;
		}
		// Set channel pointer back to FIFO
		outb((A2DIOFIFO), (UC *)chan_addr);
	}

	else if(boot == 0)
	{
		rtl_printf("%s: No boot action\n");
	}

	// Reset A/D's don't change configuration
	outb((A2DCMNDWR), (UC *)chan_addr);
	for(j = 0; j < MAXA2DS; j++)
	{
		outw((A2DABORT), (US *)isa_address + j);
	}

	rtl_usleep(1000);

//#ifdef DEBUGFILTER
	rtl_printf("\n\n");

	// Read status bits all a/d's
	outb((A2DSTATRD), (UC *)chan_addr); // Status read subchannel

	for(j = 0; j < MAXA2DS; j++)
	{
		stat = (inw((US *)isa_address + j));
		rtl_printf("Status: a/d%1d stat = 0x%04X\n", j, stat);
	}
//#endif

	rtl_usleep(DELAYNUM1);	// Give A/D's a chance to load

// Start the conversions
   	outb((A2DCMNDWR), (UC *)chan_addr);
	for(j = 0; j < MAXA2DS; j++)
	{
   		outw((A2DREADDATA), (US *)isa_address + j);
	}

// Do a read on status and data to clear interrupt on Master
	// Point channel pointer at A/D Stat 
	// and read the master a/d status
	outb((A2DSTATRD), (UC *)chan_addr);
	rtl_printf("%s: Pre-start stat: \n", __FILE__); 
	for(j = 0; j < MAXA2DS; j++)
	{
//		stat = (inw((US *)isa_address + a2dmaster));
		stat = (inw((US *)isa_address + j));
		rtl_printf("%01d:	0x%04x\n", j, stat);
	}

// Read the interrupt lines to see what's what
	outb((A2DIOSYSCTL), (UC *)chan_addr);
	ints = (inb((UC *)isa_address));
	rtl_printf("%s: Pre-start ints = 0x%02X\n", __FILE__, ints);

//Set SYNC and stop the A2D's
	rtl_printf("%s: Setting SYNC.  FIFOCtl = 0x%02X\n", __FILE__, FIFOCtl);
	outb((FIFOCtl | A2DSYNC), (UC *)isa_address);
	outb((FIFOCtl | A2DSYNC | A2DSYNCCK), (UC *)isa_address);
	outb((FIFOCtl | A2DSYNC), (UC *)isa_address);

//Clear FIFO
	FIFOCtl &= ~FIFOCLR;	// Make certain LSB of FIFOCtl is clear
	rtl_printf("%s: Clearing FIFO .  FIFOCtl = 0x%02X\n", __FILE__, FIFOCtl);
	outb((A2DIOFIFO), (UC *)chan_addr);
	outb((FIFOCtl), (UC *)isa_address);
	outb((FIFOCtl | FIFOCLR), (UC *)isa_address);
	outb((FIFOCtl), (UC *)isa_address);

// Read the FIFO status bits
	rtl_printf("%s: Reading FIFO status. FIFOCtl = 0x%02X\n", __FILE__, FIFOCtl);
	outb((A2DIOFIFOSTAT), (UC *)chan_addr);
	stat = (inb((UC *)isa_address));

// Read the interrupt lines to see what's what
	rtl_printf("%s: Checking int lines. FIFOCtl = 0x%02X\n", __FILE__, FIFOCtl);
	outb((A2DIOSYSCTL), (UC *)chan_addr);
	ints = (inb((UC *)isa_address));
	rtl_printf("%s: Pre-sync: ints = 0x%02X, fstat = 0x%02X\n", 
				__FILE__, ints, stat);

// Switch FIFOCtl to run mode
	FIFOCtl = FIFOCtlRun;
	rtl_printf("%s: Switching to RUN mode.  FIFOCtl = 0x%02X\n", 
					__FILE__, FIFOCtl);
	outb((A2DIOFIFO), (UC *)chan_addr);
	outb((FIFOCtl), (UC *)isa_address);

// Clear SYNC and let A/D's run
	rtl_printf("%s: Clearing SYNC. FIFOCtl = 0x%02X\n", __FILE__, FIFOCtl);
	outb((A2DIOFIFO), (UC *)chan_addr);
	outb((FIFOCtl & ~A2DSYNC), (UC *)isa_address);
	outb(((FIFOCtl & ~A2DSYNC) | A2DSYNCCK), (UC *)isa_address);
	outb((FIFOCtl & ~A2DSYNC), (UC *)isa_address);
	rtl_printf("%s: Clearing the A/D SYNC line\n", __FILE__);

// Take data!
 	
 	for(i = 0; i < ncycles; i++)	
	{
	  // Clear the averaging buffers
	  for(j = 0; j < MAXA2DS; j++)avgs[j] = 0;	// Clear avg registers

	  j = 0;
	  m = 0;

	  while(m < nsamp)
	  {
		// Wait .01 seconds
//		rtl_usleep(7500);
		rtl_usleep(10000);
	
		// Read the interrupt lines to see what's what
		outb((A2DIOSYSCTL), (UC *)chan_addr);
		ints = (inb((UC *)isa_address));

		// Read the FIFO status bits
		outb((A2DIOFIFOSTAT), (UC *)chan_addr);
		stat0 = (inb((UC *)isa_address));

		while(1)	
		{
			// Read the FIFO status bits
			outb((A2DIOFIFOSTAT), (UC *)chan_addr);
			stat1 = (inb((UC *)isa_address));
			if((stat1 & FIFONOTEMPTY) == 0)break;
			
			// Read data values from FIFO
			outb((A2DIOFIFO), (UC *)chan_addr);
			avgs[j%MAXA2DS] += (long)(inw((short *)isa_address));
			if(j%MAXA2DS == 0)m++;
			j++;
		}

	  }/* m (nsamp) loop */

	  for(j = 0; j < MAXA2DS; j++)avgs[j] /= nsamp;	//normalize
	  rtl_printf("\n%6d:fs=%02X/%02X:i=%02X", i, stat0, stat1, ints);
	  for(j = 0; j < MAXA2DS; j++)rtl_printf(" %04hx", avgs[j]);
	  rtl_printf("\n");
	  
	}/* i (ncycles)loop */
	return 0;
}

