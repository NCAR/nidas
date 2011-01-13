/* a2d_driver.h

   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   Driver for Diamond Systems Corp MM AT series of Analog IO boards

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#ifndef NIDAS_DIAMOND_DMD_MMAT_H
#define NIDAS_DIAMOND_DMD_MMAT_H

/// #include <nidas/linux/filters/short_filters.h>
#include <nidas/linux/util.h>
#include <nidas/linux/a2d.h>

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

#define MAX_DMMAT_BOARDS	4	// number of boards supported by driver
#define	MAX_DMMAT_A2D_CHANNELS 32	// max num A/D channels per card
#define DMMAT_D2A_OUTPUTS_PER_BRD 4
#define	MAX_DMMAT_D2A_OUTPUTS (MAX_DMMAT_BOARDS * DMMAT_D2A_OUTPUTS_PER_BRD)

struct DMMAT_A2D_Status
{
        unsigned int missedSamples;
        unsigned int fifoOverflows;	// A2D FIFO has overflowed (error)
        unsigned int fifoUnderflows;	// A2D FIFO less than expected level (error)
        unsigned int fifoEmpty;	// A2D FIFO empty (error)
        unsigned int fifoNotEmpty;	// A2D FIFO not empty after reading
        unsigned int irqsReceived;
};



/* Supported board types */
#define DMM16AT_BOARD	0
#define DMM32XAT_BOARD	1
/* The 32DXAT version of the board has a 16 bit D2A. According to the manual
 * it is supposed to have jumpers called DAC_SZ0/1, which allow one to configure
 * it to behave like a DMM32XAT with a 12 bit D2A. However, they are soldered
 * 0 ohm resistors, not jumpers, and so there is no way for the user to force
 * the card to behave like a 32XAT. In the user forum at diamondsystems.com ,
 * Diamond says, as of Aug 2009, that they are working on a FPGA fix to allow
 * the user to control the D2A behaviour. This FPGA change isn't in the 2 cards
 * that we have on the GV CVI system. There is no easy way for the driver to
 * detect which card is present, so the user must pass this board type integer
 * to the driver.
 */
#define DMM32DXAT_BOARD	2

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

/** A2D Ioctls in addition to those in nidas/linux/a2d.h */
#define DMMAT_A2D_GET_STATUS \
    _IOR(DMMAT_IOC_MAGIC,0,struct DMMAT_A2D_Status)
#define DMMAT_A2D_START      _IO(DMMAT_IOC_MAGIC,1)
#define DMMAT_A2D_STOP       _IO(DMMAT_IOC_MAGIC,2)
#define DMMAT_A2D_DO_AUTOCAL    _IO(DMMAT_IOC_MAGIC,3)
#define DMMAT_A2D_START_CONVERSION \
		_IO(DMMAT_IOC_MAGIC, 4)
#define DMMAT_A2D_GET_VALUE \
		_IOR(DMMAT_IOC_MAGIC, 5, short)

/** Counter Ioctls */
#define DMMAT_CNTR_START \
    _IOW(DMMAT_IOC_MAGIC,6,struct DMMAT_CNTR_Config)
#define DMMAT_CNTR_STOP       _IO(DMMAT_IOC_MAGIC,7)
#define DMMAT_CNTR_GET_STATUS \
    _IOR(DMMAT_IOC_MAGIC,8,struct DMMAT_CNTR_Status)

/**
 * D2A Ioctls
 * This D2A driver does not enforce any exclusive use policy:
 * multiple threads can each have the same device open and set
 * any of the output voltages.
 */
#define DMMAT_D2A_GET_NOUTPUTS \
    _IO(DMMAT_IOC_MAGIC,9)
#define DMMAT_D2A_GET_CONVERSION \
    _IOR(DMMAT_IOC_MAGIC,10,struct DMMAT_D2A_Conversion)
#define DMMAT_D2A_SET \
    _IOW(DMMAT_IOC_MAGIC,11,struct DMMAT_D2A_Outputs)
#define DMMAT_D2A_GET \
    _IOR(DMMAT_IOC_MAGIC,12,struct DMMAT_D2A_Outputs)



/**
 * D2D IOCTLs
 */

#define DMMAT_D2D_CONFIG \
		_IOW(DMMAT_IOC_MAGIC, 13, struct D2D_Config)
#define DMMAT_D2D_START \
		_IO(DMMAT_IOC_MAGIC, 14)
#define DMMAT_D2D_STOP \
		_IO(DMMAT_IOC_MAGIC, 15)
		
/**
 * Multi device IOCTLs
 */

// Works for D2D or D2A device
#define DMMAT_ADD_WAVEFORM \
		_IOW(DMMAT_IOC_MAGIC, 16, struct waveform)



/**
 * Used to define read and write to specific addresses.
 * Not currently implemented.
 */

#define DMMAT_READ_ADDR \
 		_IOR(DMMAT_IOC_MAXNR, 17, struct command)
#define DMMAT_WRITE_ADDR \
 		_IOWR(DMMAT_IOC_MAXNR, 18, struct command)


#define DMMAT_IOC_MAXNR 18

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


struct DMMAT_CNTR_Config
{
        int msecPeriod;         // how long to count for
};

struct DMMAT_CNTR_Status
{
        unsigned int lostSamples;
        unsigned int irqsReceived;
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
        int vmin[MAX_DMMAT_D2A_OUTPUTS];
        int vmax[MAX_DMMAT_D2A_OUTPUTS];
        int cmin[MAX_DMMAT_D2A_OUTPUTS];
        int cmax[MAX_DMMAT_D2A_OUTPUTS];
};

struct DMMAT_D2A_Outputs
{
        int active[MAX_DMMAT_D2A_OUTPUTS];     // 1=set, 0=ignore
        int counts[MAX_DMMAT_D2A_OUTPUTS];      // counts value
        int nout;
};

struct command {
	unsigned char value;
	unsigned char address;
};

/**
 * Structures used by D2D device to set up the waveform to ship 
 * over the D2A
 */
struct D2D_Config{
	int num_chan_out;
	int waveform_scan_rate;
};

struct waveform{
	int channel;
	int size;
	int point[0];
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

/*
 * Board #0
 * A2D: 1 device, minor number 0, /dev/dmmat_a2d0
 * CNTR: 1 device, minor number 1 /dev/dmmat_cntr0
 * D2A: 1 device, minor number 2, /dev/dmmat_d2a0
 * DIO: 1 device, minor number 3, /dev/dmmat_dio0
 * D2D: 1 device, minor number 4, /dev/dmmat_d2d0
 *
 * Board #1
 * A2D: 1 device, minor number 5, /dev/dmmat_a2d1
 * CNTR: 1 device, minor number 6 /dev/dmmat_cntr1
 * D2A: 1 device, minor number 7, /dev/dmmat_d2a1
 * DIO: 1 device, minor number 8, /dev/dmmat_dio1
 * D2D: 1 device, minor number 9, /dev/dmmat_d2d1
 */
#define DMMAT_DEVICES_PER_BOARD 5  // See the next entries
#define DMMAT_DEVICES_A2D_MINOR 0
#define DMMAT_DEVICES_CNTR_MINOR 1
#define DMMAT_DEVICES_D2A_MINOR 2
#define DMMAT_DEVICES_DIO_MINOR 3
#define DMMAT_DEVICES_D2D_MINOR 4



#define DMMAT_FIFO_SAMPLE_QUEUE_SIZE 64
#define DMMAT_A2D_SAMPLE_QUEUE_SIZE 1024
#define DMMAT_D2A_WAVEFORM_SIZE 1024
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

struct a2d_sample
{
        dsm_sample_time_t timetag;    // timetag of sample
        dsm_sample_length_t length;       // number of bytes in data
        short data[MAX_DMMAT_A2D_CHANNELS+1];	// room for id too
};


/**
 * Structure allocated for every DMMAT board in the system.
 */
struct DMMAT {
        int num;                        // which board in system, from 0
				int type;                       // 0 = 16, 1 = 32X, 2 = 32DX
        unsigned long addr;             // Base address of board
        int irq;		                    // requested IRQ
        int irq_users[2];               // number of irq users each type:
                                        // a2d=0, cntr=1
        unsigned long itr_status_reg;	  // addr of interrupt status register
        unsigned long itr_ack_reg;	    // addr of interrupt acknowledge reg

        unsigned char ad_itr_mask;	    // mask of A2D interrupt bit 
        unsigned char cntr_itr_mask;	  // mask of counter interrupt bit 
        unsigned char itr_ack_val;	    // value to write to int_act_reg
        unsigned char itr_ctrl_val;     // interrupt control register value

        struct DMMAT_A2D* a2d;          // pointer to A2D device struct
        struct DMMAT_CNTR* cntr;        // pointer to CNTR device struct
        struct DMMAT_D2A* d2a;          // pointer to D2A device struct
				struct DMMAT_D2D* d2d;          // pointer to D2D device struct

        spinlock_t reglock;             // lock when accessing board registers
                                        // to avoid messing up the board.

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        struct mutex irqreq_mutex;      // when setting up irq handler
#else
        struct semaphore irqreq_mutex;  // when setting up irq handler
#endif
};

struct a2d_bh_data
{
        struct a2d_sample saveSample;
};

/**
 * Information maintained for the A2D.
 */
struct DMMAT_A2D
{
        struct DMMAT* brd;

        struct DMMAT_A2D_Status status;

        char deviceName[32];
        struct cdev cdev;

        // methods which may have a different implementation
        // for each board type
        int (*start)(struct DMMAT_A2D* a2d,int lock);	// a2d start method
        void (*stop)(struct DMMAT_A2D* a2d,int lock);	// a2d stop method
        int  (*getFifoLevel)(struct DMMAT_A2D* a2d);
        int  (*getNumChannels)(struct DMMAT_A2D* a2d);
        int  (*selectChannels)(struct DMMAT_A2D* a2d);
        int  (*getConvRateSetting)(struct DMMAT_A2D* a2d, unsigned char* val);
        int  (*getGainSetting)(struct DMMAT_A2D* a2d,int gain, int bipolar,
                        unsigned char* val);
        void (*resetFifo)(struct DMMAT_A2D* a2d);
        void (*waitForA2DSettle)(struct DMMAT_A2D* a2d);

        int running;                                   // a2d is running

        atomic_t num_opened;                     // number of times opened

#ifdef USE_TASKLET
        struct tasklet_struct tasklet;          // filter tasklet
#else
        struct work_struct worker;
#endif
        struct a2d_bh_data bh_data;       // data for use by bottom half

        struct dsm_sample_circ_buf fifo_samples;     // samples for bottom half
        struct dsm_sample_circ_buf samples;         // samples out of b.h.

        wait_queue_head_t read_queue;   // user read & poll methods wait on this

        struct sample_read_state read_state;

        int maxFifoThreshold;       // maximum hardware fifo threshold
        int fifoThreshold;	        // current hardware fifo threshold
        
        int nfilters;               // how many different output filters

        struct a2d_filter_info* filters;

        int gain;               // all channels have same gain
        int bipolar;            // all channels have same polarity
        int lowChan;		// lowest channel scanned
        int highChan;		// highest channel scanned
        int nchans;
        unsigned char gainConvSetting;	// gain and conversion rate setting
        int scanRate;		// A/D scan sample rate
        int scanDeltaT;

        int latencyMsecs;	        // buffer latency in milli-seconds
        int latencyJiffies;	// buffer latency in jiffies
        unsigned long lastWakeup;   // when were read & poll methods last woken

};

/**
 * Sample from a pulse counter contains an unsigned 4 byte integer
 * counts.
 */
struct cntr_sample {
    dsm_sample_time_t timetag;
    dsm_sample_length_t length;
    unsigned int data;
};

/**
 * Circular buffer of pulse samples.
 */
struct cntr_sample_circ_buf {
    struct cntr_sample *buf[DMMAT_CNTR_QUEUE_SIZE];
    volatile int head;
    volatile int tail;
    int size;
};


/**
 * Information maintained for the counter device.
 */
struct DMMAT_CNTR {

        struct DMMAT* brd;

        struct cdev cdev;

        char deviceName[32];

        struct DMMAT_CNTR_Status status;

        struct timer_list timer;

        int  (*start)(struct DMMAT_CNTR* cntr);	// cntr start method

        void (*stop)(struct DMMAT_CNTR* cntr);	// cntr stop method

        volatile unsigned int rolloverSum;     // current counter sum

        int jiffiePeriod;                       // how often to wake up
                                                // and create an output

        int firstTime;                          // first count is bad

        int shutdownTimer;                      // set to 1 to stop counting

        int lastVal;                            // previous value in the
                                                // counter register

        struct dsm_sample_circ_buf samples;    // samples for read method

        struct sample_read_state read_state;

        wait_queue_head_t read_queue;           // user read & poll methods
                                                // wait on this queue

        int busy;

};

struct D2APt {
	unsigned int value;
	unsigned int channel;
};

struct DMMAT_D2A {

        struct DMMAT* brd;

        struct cdev cdev;

        char deviceName[32];

        struct DMMAT_D2A_Outputs outputs;

        int (*setD2A)(struct DMMAT_D2A* d2a,struct DMMAT_D2A_Outputs* set,int i);

				int (*sendD2A)(struct DMMAT_D2A* d2a, struct D2APt* sent);
				
				int (*loadD2A)(struct DMMAT_D2A* d2a, struct D2APt* sent);
				
				int (*addWaveform)(struct DMMAT_D2A* d2a, struct waveform* waveform);
				
				int (*loadWaveformIntoBuffer)(struct DMMAT_D2A* d2a);

        int vmin;

        int vmax;

        int cmin;

        int cmax;
				
				unsigned int num_chan_out;
				
				unsigned int channels_set;
				
				unsigned int waveform_scan_rate;								
				
				unsigned int waveform_max; // Up to 1023
												
				struct D2APt waveform[DMMAT_D2A_WAVEFORM_SIZE]; // For loading D2A waveform.
				
				atomic_t running;		

};


struct DMMAT_D2D {
	struct DMMAT* brd;
	
	struct cdev cdev;
  
	char deviceName[32];
	
	int (*start)(struct DMMAT_D2D* d2a);
	int (*stop)(struct DMMAT_D2D* d2d);

	
	int waveform_scan_rate;
	
	// Use D2A struct for this information
	// struct D2APt waveform[DMMAT_D2A_WAVEFORM_SIZE]; // Use D2A code
	// unsigned int waveform_max;
	
	atomic_t running; // Set with void atomic_set(atomic_t *v, int i)
										// read with int atomic_read(atomic_t *v)
};

#endif

#endif
