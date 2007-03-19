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

/* A2D Ioctls */
#define DMMAT_A2D_SET_CONFIG \
    _IOW(DMMAT_IOC_MAGIC,0,struct DMMAT_A2D_Config)
#define DMMAT_A2D_GET_STATUS \
    _IOR(DMMAT_IOC_MAGIC,1,struct DMMAT_A2D_Status)
#define DMMAT_A2D_START      _IO(DMMAT_IOC_MAGIC,2)
#define DMMAT_A2D_STOP       _IO(DMMAT_IOC_MAGIC,3)
#define DMMAT_A2D_GET_NCHAN  _IO(DMMAT_IOC_MAGIC,4)
#define DMMAT_A2D_SET_SAMPLE \
    _IOW(DMMAT_IOC_MAGIC,5,struct DMMAT_A2D_Sample_Config)

#define DMMAT_CNTR_START \
    _IOW(DMMAT_IOC_MAGIC,6,struct DMMAT_CNTR_Config)
#define DMMAT_CNTR_STOP       _IO(DMMAT_IOC_MAGIC,7)

/* D2A Ioctls */
#define DMMAT_D2A_GET_NOUTPUTS \
    _IO(DMMAT_IOC_MAGIC,8)
#define DMMAT_D2A_GET_CONVERSION \
    _IOR(DMMAT_IOC_MAGIC,9,struct DMMAT_D2A_Conversion)
#define DMMAT_D2A_SET \
    _IOW(DMMAT_IOC_MAGIC,10,struct DMMAT_D2A_Outputs)
#define DMMAT_D2A_GET \
    _IOR(DMMAT_IOC_MAGIC,11,struct DMMAT_D2A_Outputs)

#define DMMAT_IOC_MAXNR 11

/**
 * Definitions of bits in board status byte.
 */
#define DMMAT_STATUS_A2D_BUSY		0x80	// 1=conv or scan in progress
#define DMMAT_STATUS_TIMER_INT		0x40	// interrupt from timer 0
#define DMMAT_STATUS_SINGLE_ENDED	0x20	// 1=SE jumpered, else diff.
#define DMMAT_STATUS_A2D_INT		0x10	// interrupt from A2D
#define DMMAT_STATUS_CHAN_MASK		0x0f	// current channel

/**
 * Enumeration of supported jumper configurations for the D2A.
 * Currently don't support the programmable mode.
 */
enum dmmat_d2a_jumpers {
        DMMAT_D2A_UNI_5,        // unipolar, 0-5V
        DMMAT_D2A_UNI_10,       // unipolar, 0-10V
        DMMAT_D2A_BI_5,         // bipolar, -5-5V
        DMMAT_D2A_BI_10        // bipolar, -10-10V
};

#define DMMAT_D2A_OUTPUTS 4

struct DMMAT_CNTR_Config
{
        int msecPeriod;         // how long to count for
};

struct DMMAT_CNTR_Status
{
        size_t lostSamples;
        size_t irqsReceived;
};

/**
 * Structure describing the linear relation of counts
 * and D2A voltage.  User does an ioctl query to get this
 * from the D2A, and uses it to compute a count for a desired
 * voltage, and sets the output by passing an integer count value
 * via an ioctl. Alas, floating point is too much to expect from a
 * kernel module...
 */
struct DMMAT_D2A_Conversion
{
        int vmin;
        int vmax;
        int cmin;
        int cmax;
};

struct DMMAT_D2A_Outputs
{
        int active[DMMAT_D2A_OUTPUTS];     // 1=set, 0=ignore
        int counts[DMMAT_D2A_OUTPUTS];      // counts value
};

#ifdef __KERNEL__
/********  Start of definitions used by the driver module only **********/

#include <linux/cdev.h>
// #include <linux/timer.h>

/*
 * To use a tasklet to do the bottom half processing, define USE_TASKLET.
 * Otherwise a work queue is used.  Define USE_MY_WORK_QUEUE to create a
 * single thread work queue for dmd_mmat, otherwise the shared
 * work queue will be used.
 * Testing in March 2007, on a viper, running 2.6.16.28-arcom1-2-viper,
 * doing 2KHz samping of 9 channels on a MM32XAT saw no difference 
 * between the 3 methods.  We'll use a single thread, non-shared queue.
 * It's cool because it shows up in a "ps" listing.
 * We don't need the atomicity (word?) of tasklets - I guess they are
 * run as a software interrupt.
 */
// #define USE_TASKLET

#ifndef USE_TASKLET
#define USE_MY_WORK_QUEUE
#include <linux/workqueue.h>
#endif

/* Size of ISA I/O space (both MM16AT and MM32XAT) */
#define	DMMAT_IOPORT_WIDTH	16

#define MAX_DMMAT_BOARDS	4	// number of boards supported by driver

/*
 * Board #0
 * A2D: 1 device, minor number 0, /dev/dmmat_a2d0
 * CNTR: 1 device, minor number 1 /dev/dmmat_cntr0
 * D2A: 1 device, minor number 2, /dev/dmmat_d2a0
 * DIO: 1 device, minor number 3, /dev/dmmat_dio0
 *
 * Board #1
 * A2D: 1 device, minor number 0, /dev/dmmat_a2d1
 * CNTR: 1 device, minor number 1 /dev/dmmat_cntr1
 * D2A: 1 device, minor number 2, /dev/dmmat_d2a1
 * DIO: 1 device, minor number 3, /dev/dmmat_dio1
 */
#define DMMAT_DEVICES_PER_BOARD 4

#define DMMAT_FIFO_SAMPLE_QUEUE_SIZE 64
#define DMMAT_A2D_SAMPLE_QUEUE_SIZE 2048
#define DMMAT_CNTR_QUEUE_SIZE 16

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

/**
 * Structure allocated for every DMMAT board in the system.
 */
struct DMMAT {
        int num;                        // which board in system, from 0
        unsigned long addr;             // Base address of board
        int irq;		        // requested IRQ
        int irq_users[2];               // number of irq users each type:
                                        // a2d=0, cntr=1
        unsigned long itr_status_reg;	// addr of interrupt status register
        unsigned long itr_ack_reg;	// addr of interrupt acknowledge reg

        unsigned char ad_itr_mask;	// mask of A2D interrupt bit 
        unsigned char cntr_itr_mask;	// mask of counter interrupt bit 
        unsigned char itr_ack_val;	// value to write to int_act_reg
        unsigned char itr_ctrl_val;     // interrupt control register value

        struct DMMAT_A2D* a2d;          // pointer to A2D device struct
        struct DMMAT_CNTR* cntr;        // pointer to CNTR device struct
        struct DMMAT_D2A* d2a;          // pointer to D2A device struct

        spinlock_t reglock;             // when accessing board registers

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        struct mutex irqreq_mutex;         // when setting up irq handler
#else
        struct semaphore irqreq_mutex;     // when setting up irq handler
#endif
};

struct a2d_sample
{
        dsm_sample_time_t timetag;    // timetag of sample
        dsm_sample_length_t length;       // number of bytes in data
        short data[MAX_DMMAT_A2D_CHANNELS];
};

struct a2d_bh_data
{
        struct a2d_sample saveSample;
};

/**
 * From the user point of view a sample is a group of 
 * A2D channels which the user wants output at a given
 * rate, using a certain filter.  This structure describes
 * such a sample.
 */
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

        // methods which may have a different implementation
        // for each board type
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

#ifdef USE_TASKLET
        struct tasklet_struct tasklet;          // filter tasklet
#else
        struct work_struct worker;
#ifdef USE_MY_WORK_QUEUE
        struct workqueue_struct* work_queue;
#endif
#endif
        struct a2d_bh_data bh_data;       // data for use by bottom half

        struct dsm_sample_circ_buf fifo_samples;     // samples for bottom half
        struct dsm_sample_circ_buf samples;         // samples out of b.h.

        wait_queue_head_t read_queue;   // user read & poll methods wait on this
        size_t sampBytesLeft;       // bytes left to copy to user in last sample
        char* sampPtr;              // pointer into last sample for copy to user

        int maxFifoThreshold;       // maximum hardware fifo threshold
        int fifoThreshold;	        // current hardware fifo threshold
        
        int nsamples;               // how many different output sample groups
        struct DMMAT_A2D_Sample_Info* sampleInfo;

        unsigned char requested[MAX_DMMAT_A2D_CHANNELS];// 1=channel requested, 0=isn't
        int lowChan;		// lowest channel scanned
        int highChan;		// highest channel scanned
        int nchans;
        unsigned char gainSetting;	// 
        int scanRate;		// A/D scan sample rate
        long scanDeltaT;

        long latencyMsecs;	        // buffer latency in milli-seconds
        long latencyJiffies;	// buffer latency in jiffies
        unsigned long lastWakeup;   // when were read & poll methods last woken

        unsigned long delayedWork;

};

struct cntr_sample {
    dsm_sample_time_t timetag;
    dsm_sample_length_t length;
    unsigned long data;
};

struct cntr_sample_circ_buf {
    struct cntr_sample *buf[DMMAT_CNTR_QUEUE_SIZE];
    volatile int head;
    volatile int tail;
};


struct DMMAT_CNTR {

        struct DMMAT* brd;

        struct cdev cdev;

        char* deviceName;

        struct DMMAT_CNTR_Status status;

        struct timer_list timer;

        int (*start)(struct DMMAT_CNTR* cntr);	// cntr start method

        void (*stop)(struct DMMAT_CNTR* cntr);	// cntr stop method

        volatile unsigned long rolloverSum;     // current counter sum

        int jiffiePeriod;                       // how often to re-submit

        int firstTime;

        int shutdownTimer;

        int lastVal;

        struct cntr_sample_circ_buf samples;     // samples for read method

        wait_queue_head_t read_queue;   // user read & poll methods wait on this

        int busy;

};

struct DMMAT_D2A {

        struct DMMAT* brd;

        struct cdev cdev;

        char* deviceName;

        struct DMMAT_D2A_Conversion conversion;

        struct DMMAT_D2A_Outputs outputs;

        int (*setD2A)(struct DMMAT_D2A* d2a,struct DMMAT_D2A_Outputs* set);

};

#endif

#endif
