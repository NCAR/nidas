#define DEBUG_IO
// #define NO_IO

/*
vdio_module.c

Arcom VIPER Digital IO Driver

Time-stamp: <Tue 17-Aug-2004 10:12:00 am>

RTLinux digital I/O driver for the Viper digital I/O channel

Original Author:	Grant Gray

Copyright 2004 by the National Center for Atmospheric Research 

Revisions:

*/

/* Arcom Viper Digital I/O header */
#include "vdio.h"

pthread_t	aVthread;
sem_t		anVdioSemaphore;
unsigned short 	*pVdiord;

/* File pointer to FIFO to user space */
static unsigned char fp_Vdio;

/* Magic toggle buffer pointer */
extern int ptog;

RTLINUX_MODULE(Vdio);
MODULE_AUTHOR("Grant Gray <gray@ucar.edu>");
MODULE_DESCRIPTION("RTLinux Viper Digital I/O Driver");

/*--------------------------------MODULE-----------------------------------*/

void init_module(void)
{
	unsigned char dummy[2];
	char devstr[80];

	rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
		__FILE__, __FUNCTION__, __DATE__, __TIME__);

/* Initialize the semaphore to the 100 Hz thread */
	sem_init(&anVdioSemaphore, 1, 0);
	rtl_printf("(%s) %s:\t sem_init done\n", __FILE__, __FUNCTION__);

/* Create the data output FIFO */
	sprintf(devstr,"/dev/vdio_write");
	mkfifo(devstr, 0666);
	rtl_printf("FIFO %s initialized\n", devstr);

/* Create a command fifo */
	sprintf(devstr, "/dev/vdio_read");
	mkfifo(devstr, 0666);
	rtl_printf("FIFO %s initialized\n", devstr);
	
/* Create the 100 Hz thread */
	pthread_create(&aVthread, NULL, Vdio_100hz_thread, (void *)0);
	rtl_printf("(%s) %s:\t pthread_create done \n", __FILE__, __FUNCTION__);

/* Set up I/O port pointer */
	pVdiord = ioremap_nocache(VDIO_IN, 4);
	rtl_printf("Read pointer initialized to 0x%08X\n", 
				(unsigned long)pVdiord);

/* Allocate memory for control structure. */
	digio = (DIGIO *)dummy;

/*  Initially clear all the output bits. */

	ViperOutBits(0x00); 
	
	return;
}

/*----------------------------------Module-----------------------------------*/

void cleanup_module(void)
{
/* Close the data FIFO */
	close(fp_Vdio);
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
	/* The entire count sequence should take 2.56 seconds */
	ViperOutBits(vobits);
	vobits += 1;
	if(vobits%100 == 0)rtl_printf("DigiBits 0x%02x\n", vibits);
#else
	/* Send input bits up to user space via the FIFO */
	write(fp_Vdio, &vbits, sizeof(UC));
#endif
	}
}


/*-----------------------------------UTILITY-----------------------------------*/

/* Transfer a byte to the GPIO Digital Output port */

int ViperOutBits(unsigned char vbits)
{
	unsigned long *pVdioset, *pVdioclr;
	unsigned long biggy;

	pVdioset = (unsigned long *)ioremap_nocache(VDIO_OUT_SET, 4);
	pVdioclr = (unsigned long *)ioremap_nocache(VDIO_OUT_CLR, 4);

	biggy = 0x0FF00000 ^ (0x0FF00000 &(((unsigned long)vbits)<<20));
	rtl_printf("Biggy = 0x%08x\n", biggy);
	writel(biggy, pVdioclr);
	rtl_printf("writel works\n", biggy);

	biggy = (0x0FF00000 &(((unsigned long)vbits)<<20));
	rtl_printf("Biggy = 0x%08x\n", biggy);
	writel(biggy, pVdioset);
	rtl_printf("writel works!\n", biggy);

	return(VIPERDIOOK);
}

/*-----------------------------------UTILITY-----------------------------------*/

/* Read the Viper GPIO  input bits */

unsigned char ViperReadBits(void)
{
#ifndef NO_IO
	return((unsigned char)(0xFF & readw(pVdiord)));
	rmb();
#else
	return(0);
#endif
}
