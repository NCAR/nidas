/* a2d_driver.h

   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   RT-Linux driver for AccessIO 104-AIO16-16 card

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#ifndef AIO16_A2D_H
#define AIO16_A2D_H

#include <nidas/linux/util.h>		// get nidas typedefs

#ifndef __RTCORE_KERNEL__
/* User programs need this for the _IO macros, but kernel
 * modules get their's elsewhere.
 */
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

#define	NUM_AIO16_CHANNELS		16	// Num A/D channels per card

struct AIO16_Config
{
	int	gain[NUM_AIO16_CHANNELS];	// Gain settings, 1,2,5, or 10
	int	rate[NUM_AIO16_CHANNELS];	// Sample rate in Hz. 0 is off.
	int	bipolar[NUM_AIO16_CHANNELS];// 1=bipolar,0=unipolar
	long	latencyUsecs;	// buffer latency in micro-seconds
};

struct AIO16_Status
{
    unsigned char reg;			// contents of status register
    unsigned long missedSamples;
};

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define A2D_MAGIC 'a'

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define AIO16_CONFIG _IOW(A2D_MAGIC,0,struct AIO16_Config)
#define AIO16_STATUS _IOR(A2D_MAGIC,1,struct AIO16_Status)
#define AIO16_START _IO(A2D_MAGIC,2)
#define AIO16_STOP _IO(A2D_MAGIC,3)

/**
 * Definitions of bits in board status byte.
 */
#define AIO16_STATUS_FIFO_EMPTY		0x80	// bit is 1 when data fifo empty
#define AIO16_STATUS_IRQ_ENABLED	0x40	//  means irqs enabled
#define AIO16_STATUS_FIFO_HALF_FULL	0x20	// 1 means fifo > half full
#define AIO16_STATUS_DA5V		0x10	// 1 means DAC chan A 5 volt out
#define AIO16_STATUS_DB5V		0x08	// 1 means DAC chan B 5 volt out
#define AIO16_STATUS_GNH		0x04	// jumpers set to high gain
#define AIO16_STATUS_BIPOLAR		0x02	// jumpers set to bipolar
#define AIO16_STATUS_16SE		0x01	// jumpers set to singled ended

#ifdef __RTCORE_KERNEL__
/********  Start of definitions used by the driver module only **********/

#include <rtl_semaphore.h>
#include <nidas/rtlinux/dsm_viper.h>		// get SYSTEM_ISA_IOPORT_BASE

#define	AIO16_IOPORT_WIDTH	32	// Size of ISA I/O space

#define MAX_AIO16_BOARDS	4	// number of boards supported by driver

#define AIO16_HALF_FIFO_SIZE	1024	// number of samples in half an A2D fifo

/*
 * IOPORT definitions (page 21 of 104-AIO-16-16 Reference Manual.
 * AIO16_W_*	: write to the ioport
 * AIO16_R_*	: read from the ioport
 * AIO16_RW_*	: read/write from the ioport
 */

#define AIO16_W_START_CONF	0x0	// start converson
#define AIO16_R_FIFO		0x0	// read fifo
#define AIO16_W_FIFO_RESET	0x1	// data fifo reset
#define AIO16_W_CHANNELS	0x2	// first/last channel to scan
#define AIO16_W_BURST_MODE	0x3	// burst mode control
#define AIO16_W_SW_GAIN		0x4	// software gain select
#define AIO16_R_CONFIG_STATUS	0x8	// jumper configuration and status
#define AIO16_W_DAC_OUTPUTS	0x9	// DAC outputs
#define AIO16_R_INT_STATUS	0x9	// internal status
#define AIO16_RW_EEPROM		0xa	// EEPROM read/write
#define AIO16_R_AD_CHANNEL	0xa	// A/D channel read
#define AIO16_W_CAL_DATA	0xb	// calibration data write
#define AIO16_W_ENABLE_IRQ	0xc	// enable IRQ
#define AIO16_RW_DIO_0_7	0x10	// digital outputs 0-7
#define AIO16_RW_DIO_8_15	0x11	// digital outputs 8-15
#define AIO16_RW_COUNTER_0	0x14	// counter 0
#define AIO16_RW_COUNTER_1	0x15	// counter 1
#define AIO16_RW_COUNTER_2	0x16	// counter 2
#define AIO16_W_COUNTER_CTRL	0x17	// counter control
#define AIO16_W_RESET_DIG_IO	0x19	// reset digital I/O to input mode
#define AIO16_W_OVERSAMPLE	0x1a	// configure oversampling
#define AIO16_W_EXT_TRIG_SEL	0x1c	// external A/D trigger select
#define AIO16_W_AD_COUNTER_MD	0x1d	// A/D counter mode select
#define AIO16_W_ENABLE_AD_CNT	0x1e	// enable A/D counters
#define AIO16_W_AD_CLOCK_FREQ	0x1f	// A/D clock frequency
#define AIO16_R_BOARD_RESET	0x1f	// board reset

#define AIO16_TIMED		0x10	// set TIMED bit in AD_COUNTER_MD

#define AIO16_ENABLE_IRQ	0x10	// enable irq in W_ENABLE_IRQ
#define AIO16_DISABLE_IRQ	0x00	// disable irq in W_ENABLE_IRQ

#define AIO16_ENABLE_CTR0	0x80	// enable counter 0 in ENABLE_AD_CNT
#define AIO16_ENABLE_CTR12	0x40	// enable counter 1 & 2 in ENABLE_AD_CNT

/* Values for writing to AIO16_W_OVERSAMPLE */
#define AIO16_DISABLE_A2D	0x00	// disable A2D
#define AIO16_OVERSAMPLE_X1	0x11	// 1X oversample
#define AIO16_OVERSAMPLE_X2	0x91	// 2X oversample
#define AIO16_OVERSAMPLE_X8	0x10	// 8X oversample
#define AIO16_OVERSAMPLE_X16	0x90	// 16X oversample

#define AIO16_8254_CNTR_0	0x00
#define AIO16_8254_CNTR_1	0x40
#define AIO16_8254_CNTR_2	0x80
#define AIO16_8254_RWCMD	0xc0

#define AIO16_8254_LATCH	0x00	// latch the counter
#define AIO16_8254_RW_LS	0x10	// next r/w is least signif byte
#define AIO16_8254_RW_MS	0x20	// next r/w is most signif byte
#define AIO16_8254_RW_LS_MS	0x30	// next r/w is least followed by most

#define AIO16_8254_MODE_0	0x00
#define AIO16_8254_MODE_1	0x02
#define AIO16_8254_MODE_2	0x04
#define AIO16_8254_MODE_3	0x06
#define AIO16_8254_MODE_4	0x08
#define AIO16_8254_MODE_5	0x0a

#define AIO16_FIFO_QUEUE_SIZE	4	// 

#define OUT_BUFFER_SIZE		8192

#define THREAD_STACK_SIZE	8192

struct AIO16_Board {
    unsigned int addr;	// Base address of board
    int irq;		// requested IRQ

    int a2dfd;			// File descriptor of RTL FIFO for A2D data
    char* a2dFifoName;

    struct ioctlHandle* ioctlhandle;

    unsigned char requested[NUM_AIO16_CHANNELS];// 1=channel requested, 0=isn't
    int lowChan;		// lowest channel scanned
    int highChan;		// highest channel scanned
    int gainSetting;		// 
    int maxRate;		// Maximum requested A/D sample rate
    int overSample;		// number of oversamples (1,2,8,16)

    rtl_spinlock_t queuelock;
    rtl_sem_t fifoSem;		//
    struct dsm_sample_circ_buf fifoSamples;

    rtl_pthread_t sampleThread;	//
    void* sampleThreadStack;

    int ttMsecAdj;		// how much to adjust sample time-tags backwds
    int busy;
    int interrupted;
    int head;			// pointer to head of buffer
    int tail;			// pointer to tail of buffer
    size_t skippedSamples;	// discarded samples because of 
    				// RTL FIFO sluggishness.
    unsigned char buffer[OUT_BUFFER_SIZE];	// data buffer

    struct AIO16_Status status;
    dsm_sample_time_t lastWriteTT;
    long latencyMsecs;	// buffer latency in milli-seconds

    int junk;
};

#endif

#endif
