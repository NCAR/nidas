/* a2d_driver.h

   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   RT-Linux driver for Diamond Systems Corp (DSC) MM-16-AT card.

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#ifndef DSC_A2D_H
#define DSC_A2D_H

#include <dsm_sample.h>		// get dsm_sample typedefs

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

#define	NUM_DSC_CHANNELS 32	// max num A/D channels per card

struct DSC_Config
{
    int gain[NUM_DSC_CHANNELS];	// Gain settings, 1,2,5, or 10
    int rate[NUM_DSC_CHANNELS];	// Sample rate in Hz. 0 is off.
    int bipolar[NUM_DSC_CHANNELS];// 1=bipolar,0=unipolar
    long latencyUsecs;	// buffer latency in micro-seconds
};

struct DSC_Status
{
    size_t missedSamples;
    size_t fifoOverflows;	// A2D FIFO has overflowed (error)
    size_t fifoUnderflows;	// A2D FIFO less than expected level (error)
    size_t fifoNotEmpty;	// A2D FIFO not empty after reading
};

#define DSC_MM16AT_BOARD	0
#define DSC_MM32XAT_BOARD	1

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define A2D_MAGIC 'd'

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define DSC_CONFIG _IOW(A2D_MAGIC,0,struct DSC_Config)
#define DSC_STATUS _IOR(A2D_MAGIC,1,struct DSC_Status)
#define DSC_START _IO(A2D_MAGIC,2)
#define DSC_STOP _IO(A2D_MAGIC,3)
#define DSC_GET_NCHAN _IOR(A2D_MAGIC,4,int)

/**
 * Definitions of bits in board status byte.
 */
#define DSC_STATUS_A2D_BUSY		0x80	// 1=conv or scan in progress
#define DSC_STATUS_TIMER_INT		0x40	// interrupt from timer 0
#define DSC_STATUS_SINGLE_ENDED	0x20	// 1=SE jumpered, else diff.
#define DSC_STATUS_A2D_INT		0x10	// interrupt from A2D
#define DSC_STATUS_CHAN_MASK		0x0f	// current channel

#

#ifdef __RTCORE_KERNEL__
/********  Start of definitions used by the driver module only **********/

#include <rtl_semaphore.h>
#include <dsm_viper.h>		// get SYSTEM_ISA_IOPORT_BASE

/* Size of ISA I/O space (both MM16AT and MM32XAT) */
#define	DSC_IOPORT_WIDTH	16

#define MAX_DSC_BOARDS	4	// number of boards supported by driver

#define DSC_SAMPLE_QUEUE_SIZE 16

/* defines for analog config. These values are common to MM16AT and MM32XAT */
#define DSC_UNIPOLAR		0x04
#define DSC_BIPOLAR		0x00
#define DSC_RANGE_10V		0x08
#define DSC_RANGE_5V		0x00

#define DSC_GAIN_1		0x00
#define DSC_GAIN_2		0x01
#define DSC_GAIN_4		0x02
#define DSC_GAIN_8		0x03

/* defines for 8254 clock */
#define DSC_8254_CNTR_0       0x00	// select counter 0
#define DSC_8254_CNTR_1       0x40	// select counter 1
#define DSC_8254_CNTR_2       0x80	// select counter 2
#define DSC_8254_RWCMD        0xc0	// read-back command

#define DSC_8254_LATCH        0x00    // latch the counter
#define DSC_8254_RW_LS        0x10    // next r/w is least signif byte
#define DSC_8254_RW_MS        0x20    // next r/w is most signif byte
#define DSC_8254_RW_LS_MS     0x30    // next r/w is least followed by most

#define DSC_8254_MODE_0	0x00
#define DSC_8254_MODE_1	0x02
#define DSC_8254_MODE_2	0x04
#define DSC_8254_MODE_3	0x06
#define DSC_8254_MODE_4	0x08
#define DSC_8254_MODE_5	0x0a

#define OUT_BUFFER_SIZE		8192

#define THREAD_STACK_SIZE	8192

struct DSC_Board {
    int irq;		// requested IRQ
    unsigned long addr;	// Base address of board
    unsigned long int_status_reg;	// addr of interrupt status register
    unsigned long int_ack_reg;		// addr of interrupt acknowledge reg

    unsigned char ad_int_mask;		// mask of A2D interrupt bit 
    unsigned char int_ack_val;		// value to write to int_act_reg

    rtl_spinlock_t boardlock;
    rtl_spinlock_t queuelock;

    int maxFifoThreshold;
    int fifoThreshold;		

    int (*start)(struct DSC_Board* brd);	// board start method
    void (*stop)(struct DSC_Board* brd);	// board stop method
    int (*getNumChannels)(struct DSC_Board* brd);
    int (*selectChannels)(struct DSC_Board* brd);
    int (*getConvRateSetting)(struct DSC_Board* brd, unsigned char* val);
    int (*getGainSetting)(struct DSC_Board* brd,int gain, int bipolar,
                    unsigned char* val);
    int (*getFifoLevel)(struct DSC_Board* brd);
    void (*resetFifo)(struct DSC_Board* brd);
    void (*waitForA2DSettle)(struct DSC_Board* brd);

    unsigned char requested[NUM_DSC_CHANNELS];// 1=channel requested, 0=isn't
    int lowChan;		// lowest channel scanned
    int highChan;		// highest channel scanned
    int nchans;
    unsigned char gainSetting;	// 
    int maxRate;		// Maximum requested A/D sample rate

    struct dsm_sample_circ_buf samples;
    rtl_sem_t sampleSem;

    int ttMsecAdj;		// how much to adjust sample time-tags backwds
    int busy;
    int interrupted;
    int head;			// pointer to head of buffer
    int tail;			// pointer to tail of buffer
    unsigned char buffer[OUT_BUFFER_SIZE];	// data buffer

    struct ioctlHandle* ioctlhandle;
    int outfd;
    char* outFifoName;

    rtl_pthread_t sampleThread;
    void* sampleThreadStack;

    struct DSC_Status status;
    dsm_sample_time_t lastWriteTT;
    long latencyMsecs;	// buffer latency in milli-seconds
};

#endif

#endif
