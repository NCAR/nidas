/*
Arcom VIPER Digital IO:  header file for vdio.c/vdio.cc

Time-stamp: <Tue 17-Aug-2004 10:12:00 am>

RTLinux digital I/O driver for the Viper digital I/O channel

Original Author:	Grant Gray

Copyright 2004 by the National Center for Atmospheric Research 

Revisions:

*/

#ifndef VDIO_H
#define VDIO_H

/* Conveniences */

#ifndef		US
#define		US	unsigned short
#endif
#ifndef		UL
#define		UL	unsigned long
#endif
#ifndef		UC
#define		UC	unsigned char
#endif

/*  Error values */
#define		VIPERDIOOK	   	 0
#define		ERRDIOSTART		-1
#define		ERRDIOEND		-2

/* Some necessary physical addresses */

#define 	VDIO_IN			0x14500000
#define		VDIO_OUT_CLR		0x40E00024
#define		VDIO_OUT_SET		0x40E00018
#define		__ISA_DEMUX__

#include 	<main.h>

/* RTLinux headers */
#include <rtl.h>
#include <rtl_posixio.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
//#include <rtl_unistd.h>
#include <unistd.h>
#include <semaphore.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <rtl_conf.h>
#include <rtl_buffer.h>
#include <asm-arm/io.h>

typedef struct
{
	unsigned char	dout;				// 8 bits of output
	unsigned char	din;				// 8 bits of input
}DIGIO;

/* Some usefule global variables */
/* static UC outbits; */
static DIGIO *digio;

/* Prototypes for DIO functions */

void		init_module(void);		/* DIO 'main' */
void		cleanup_module(void);
static void 	*Vdio_100hz_thread(void *a);	/* 100 hz loop */
int		ViperOutBits(unsigned char a);
unsigned char	ViperReadBits(void);

#ifdef __cplusplus

class Vdio
{
public:
	Vdio();				// Constructor
	unsigned char Vdio_read();	// Get bits
	void Vdio_write();	 	// Write bits
private:
	VFifo_init(int a, int b);
	int fp_2usr, fp_2krnl;
};

#endif	/* __cplusplus */

#endif /* #define VDIO_H */
