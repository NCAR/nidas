/* a2d_driver.h

   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   Driver for Diamond Systems Corp MM AT series of Analog IO boards

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#ifndef NIDAS_DIAMOND_DMD_MMAT_H
#define NIDAS_DIAMOND_DMD_MMAT_H

#include <nidas/linux/filters/short_filters.h>

#ifndef __KERNEL__
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

#define	MAX_DMMAT_A2D_CHANNELS 32	// max num A/D channels per card

struct DMMAT_A2D_Config
{
        int gain[MAX_DMMAT_A2D_CHANNELS];   // Gain settings, 1,2,5, or 10
        int bipolar[MAX_DMMAT_A2D_CHANNELS];// 1=bipolar,0=unipolar
        int id[MAX_DMMAT_A2D_CHANNELS];     // sample id, 0,1, etc of each chan
        long latencyUsecs;                  // buffer latency in micro-sec
        int scanRate;                       // how fast to sample
};

struct DMMAT_A2D_Sample_Config
{
        int filterType;     // one of nidas_short_filter enum
        int rate;           // output rate
        int boxcarNpts;     // number of pts in boxcar avg
        short id;           // sample id
};

struct DMMAT_A2D_Status
{
        size_t missedSamples;
        size_t fifoOverflows;	// A2D FIFO has overflowed (error)
        size_t fifoUnderflows;	// A2D FIFO less than expected level (error)
        size_t fifoNotEmpty;	// A2D FIFO not empty after reading
        size_t irqsReceived;
};

#define DMM16AT_BOARD	0
#define DMM32XAT_BOARD	1

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define DMMAT_IOC_MAGIC 'd'

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define DMMAT_SET_A2D_CONFIG \
    _IOW(DMMAT_IOC_MAGIC,0,struct DMMAT_A2D_Config)
#define DMMAT_GET_A2D_STATUS \
    _IOR(DMMAT_IOC_MAGIC,1,struct DMMAT_A2D_Status)
#define DMMAT_A2D_START      _IO(DMMAT_IOC_MAGIC,2)
#define DMMAT_A2D_STOP       _IO(DMMAT_IOC_MAGIC,3)
#define DMMAT_GET_A2D_NCHAN  _IO(DMMAT_IOC_MAGIC,4)
#define DMMAT_SET_A2D_SAMPLE \
    _IOW(DMMAT_IOC_MAGIC,5,struct DMMAT_A2D_Sample_Config)

#define DMMAT_IOC_MAXNR 4

/**
 * Definitions of bits in board status byte.
 */
#define DMMAT_STATUS_A2D_BUSY		0x80	// 1=conv or scan in progress
#define DMMAT_STATUS_TIMER_INT		0x40	// interrupt from timer 0
#define DMMAT_STATUS_SINGLE_ENDED	0x20	// 1=SE jumpered, else diff.
#define DMMAT_STATUS_A2D_INT		0x10	// interrupt from A2D
#define DMMAT_STATUS_CHAN_MASK		0x0f	// current channel

#

#ifdef __KERNEL__
/********  Start of definitions used by the driver module only **********/

#include <linux/cdev.h>

/* Size of ISA I/O space (both MM16AT and MM32XAT) */
#define	DMMAT_IOPORT_WIDTH	16

#define MAX_DMMAT_BOARDS	4	// number of boards supported by driver

#define DMMAT_SAMPLE_QUEUE_SIZE 64

/* defines for analog config. These values are common to MM16AT and MM32XAT */
#define DMMAT_UNIPOLAR		0x04
#define DMMAT_BIPOLAR		0x00
#define DMMAT_RANGE_10V		0x08
#define DMMAT_RANGE_5V		0x00

#define DMMAT_GAIN_1		0x00
#define DMMAT_GAIN_2		0x01
#define DMMAT_GAIN_4		0x02
#define DMMAT_GAIN_8		0x03

/* defines for 8254 clock */
#define DMMAT_8254_CNTR_0       0x00	// select counter 0
#define DMMAT_8254_CNTR_1       0x40	// select counter 1
#define DMMAT_8254_CNTR_2       0x80	// select counter 2
#define DMMAT_8254_RWCMD        0xc0	// read-back command

#define DMMAT_8254_LATCH        0x00    // latch the counter
#define DMMAT_8254_RW_LS        0x10    // next r/w is least signif byte
#define DMMAT_8254_RW_MS        0x20    // next r/w is most signif byte
#define DMMAT_8254_RW_LS_MS     0x30    // next r/w is least followed by most

#define DMMAT_8254_MODE_0	0x00
#define DMMAT_8254_MODE_1	0x02
#define DMMAT_8254_MODE_2	0x04
#define DMMAT_8254_MODE_3	0x06
#define DMMAT_8254_MODE_4	0x08
#define DMMAT_8254_MODE_5	0x0a

struct DMMAT {
    int num;                            // which board in system, from 0
    unsigned long addr;                 // Base address of board
    int irq;		                // requested IRQ
    int irq_users;                      // how many users of irq

    unsigned long int_status_reg;	// addr of interrupt status register
    unsigned long int_ack_reg;		// addr of interrupt acknowledge reg

    unsigned char ad_int_mask;		// mask of A2D interrupt bit 
    unsigned char pctr_int_mask;	// mask of counter interrupt bit 
    unsigned char int_ack_val;		// value to write to int_act_reg

    struct DMMAT_A2D* a2d;              // pointer to DMMAT_A2D

    spinlock_t spinlock;                // for quick locks

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
    struct mutex mutex;                   // for slow locks
#else
    struct semaphore mutex;                   // for slow locks
#endif
};

struct a2d_sample
{
        dsm_sample_time_t timetag;    // timetag of sample
        dsm_sample_length_t length;       // number of bytes in data
        short data[MAX_DMMAT_A2D_CHANNELS];
};

struct a2d_tasklet_data
{
        struct a2d_sample saveSample;
};

struct DMMAT_A2D_Sample_Info {
        int nchans;
        int* channels;
        int decimate;
        enum nidas_short_filter filterType;
        shortfilt_init_method finit;
        shortfilt_config_method fconfig;
        shortfilt_filter_method filter;
        shortfilt_cleanup_method fcleanup;
        void* filterObj;
        short id;           // sample id
};

struct DMMAT_A2D
{
    struct DMMAT* brd;
    struct DMMAT_A2D_Status status;

    char* deviceName;
    struct cdev cdev;

    // methods
    int (*start)(struct DMMAT_A2D* a2d);	// a2d start method
    void (*stop)(struct DMMAT_A2D* a2d);	// a2d stop method
    int (*getFifoLevel)(struct DMMAT_A2D* a2d);
    int (*getNumChannels)(struct DMMAT_A2D* a2d);
    int (*selectChannels)(struct DMMAT_A2D* a2d);
    int (*getConvRateSetting)(struct DMMAT_A2D* a2d, unsigned char* val);
    int (*getGainSetting)(struct DMMAT_A2D* a2d,int gain, int bipolar,
                    unsigned char* val);
    void (*resetFifo)(struct DMMAT_A2D* a2d);
    void (*waitForA2DSettle)(struct DMMAT_A2D* a2d);

    int busy;                                   // a2d is running

    struct tasklet_struct tasklet;          // filter tasklet
    struct a2d_tasklet_data tl_data;       //

    spinlock_t spinlock;                // for quick locks
    struct dsm_sample_circ_buf fifo_samples;     // raw samples for tasklet
    struct dsm_sample_circ_buf samples;         // samples out of tasklet
    wait_queue_head_t read_queue;
    size_t sampBytesLeft;
    char* sampPtr;

    int maxFifoThreshold;
    int fifoThreshold;		
    
    int nsamples;               // how many different samples
    struct DMMAT_A2D_Sample_Info* sampleInfo;

    unsigned char requested[MAX_DMMAT_A2D_CHANNELS];// 1=channel requested, 0=isn't
    int lowChan;		// lowest channel scanned
    int highChan;		// highest channel scanned
    int nchans;
    unsigned char gainSetting;	// 
    int scanRate;		// A/D scan sample rate
    long scanDeltaT;

    int ttMsecAdj;		// how much to adjust sample time-tags backwds

    long latencyMsecs;	// buffer latency in milli-seconds

    struct list_head filters;
};

#endif

#endif
