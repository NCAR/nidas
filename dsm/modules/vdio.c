// #define DEBUG_OUTPUT
// #define DEBUG_IO
// #define NO_IO

/*
vdio.c

Arcom VIPER Digital IO Driver

Time-stamp: <Tue 17-Aug-2004 10:12:00 am>

RTLinux digital I/O driver for the Viper digital I/O channel

Original Author:	Grant Gray

Copyright 2004 by the National Center for Atmospheric Research 

Revisions:

*/


/* Arcom Viper Digital I/O header */
#include "vdio.h"

rtl_pthread_t   aVthread;
rtl_sem_t       anVdioSemaphore;
unsigned short 	*pVdiord;
unsigned long 	*pVdioset, *pVdioclr;

/* File pointer to FIFO to user space */
static unsigned char fp_Vdio;

/* Magic toggle buffer pointer */
extern int ptog, gtob;

RTLINUX_MODULE(Vdio);
MODULE_AUTHOR("Grant Gray <gray@ucar.edu>");
MODULE_DESCRIPTION("RTLinux Viper Digital I/O Driver");

/*--------------------------------MODULE-----------------------------------*/

void init_module(void)
{
	int i = 1;
	unsigned char dummy[2];
	char devstr[80];
	unsigned char phoney = 0xAA;

	rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
		__FILE__, __FUNCTION__, __DATE__, __TIME__);

/* Initialize the semaphore to the 100 Hz thread */
	sem_init(&anVdioSemaphore, 1, 0);
	rtl_printf("(%s) %s:\t sem_init done\n", __FILE__, __FUNCTION__);

/* Create the data output FIFO */
	sprintf(devstr,"/dev/vdio_write");

        // remove broken device file before making a new one
        rtl_unlink(devstr);
        if ( rtl_errno != RTL_ENOENT ) return;

	rtl_mkfifo(devstr, 0666);
	rtl_printf("FIFO %s initialized\n", devstr);

/* Create a command fifo */
	sprintf(devstr, "/dev/vdio_read");

        // remove broken device file before making a new one
        rtl_unlink(devstr);
        if ( rtl_errno != RTL_ENOENT ) return;

	rtl_mkfifo(devstr, 0666);
	rtl_printf("FIFO %s initialized\n", devstr);
	
/* Create the 100 Hz thread */
	rtl_pthread_create(&aVthread, NULL, Vdio_100hz_thread, (void *)0);
	rtl_printf("(%s) %s:\t rtl_pthread_create done \n", __FILE__, __FUNCTION__);

/* Set up I/O port pointer */

	pVdiord = ioremap_nocache(VDIO_IN, 4);
	pVdioset = (unsigned long *)ioremap_nocache(VDIO_OUT_SET, 4);
	pVdioclr = (unsigned long *)ioremap_nocache(VDIO_OUT_CLR, 4);

	rtl_printf("Read pointer initialized to 0x%08X\n", 
				(unsigned long)pVdiord);

/* Allocate memory for control structure. */
	digio = (DIGIO *)dummy;

/*  Initially clear all the output bits. */

	ViperOutBits(0x00);  	// Clear output bits

#ifdef DEBUG_OUTPUT

/* 
DEBUG_OUTPUT:
This loop cycles the output bits through a binary sequence and reads
and prints the input bits.
*/

       int iii;
	unsigned char b = 0x40;

	while(b&0x40)
	{
	ViperOutBits(phoney); 
	phoney ^= 0xFF;
	rtl_printf("Vdio read 0x%02X\n", b = ViperReadBits());
	for(iii = 0; iii < 100000; iii++);
	}
#endif
	
	return;
}

/*----------------------------------Module-----------------------------------*/

void cleanup_module(void)
{
/* Set the data bits back to zero */
	ViperOutBits(0x00);

/* Close the data FIFO */
	rtl_close(fp_Vdio);
	return;

}
/*-----------------------------------Linux-----------------------------------*/
/* 100 Hz thread for Viper Digital I/0 */
static unsigned char vobits = 0;

static void *Vdio_100hz_thread(void *t)
{
	unsigned char vibits;
	while(1)
	{
	/* Wait for the 100 Hz semaphore to be set */
	sem_wait(&anVdioSemaphore);

	/* Load the current bits */
	vibits = ViperReadBits();

#ifdef DEBUG_IO
	/* DEBUG write successive bits to output port */
	/* The entire count sequence should take 2.56 seconds at 100 Hz */
	ViperOutBits(vobits);
	vobits += 1;
	if(vobits%100 == 0)rtl_printf("DigiBits 0x%02x\n", vibits);
#else
	/* Send input bits up to user space via the FIFO */
	rtl_write(fp_Vdio, &vbits, sizeof(UC));
#endif
	}
}


/*-----------------------------------UTILITY-----------------------------------*/

/* Transfer a byte to the GPIO Digital Output port */

int ViperOutBits(unsigned char vbits)
{
	unsigned long biggy;


	biggy = 0x0FF00000 ^ (0x0FF00000 &(((unsigned long)vbits)<<20));
	writel(biggy, pVdioclr);

	biggy = (0x0FF00000 &(((unsigned long)vbits)<<20));
	writel(biggy, pVdioset);

	return(VIPERDIOOK);
}

/*-----------------------------------UTILITY-----------------------------------*/

/* Read the Viper GPIO  input bits */

unsigned char ViperReadBits(void)
{
#ifndef NO_IO
	return((unsigned char)(0xFF & readw(pVdiord)));
//	rmb();
#else
	return(0);
#endif
}
