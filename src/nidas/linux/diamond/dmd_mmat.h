/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*-
 *  * vim: set shiftwidth=8 softtabstop=8 expandtab: */
/* a2d_driver.h

   Driver for Diamond Systems Corp MM AT series of Analog IO boards

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#ifndef NIDAS_DIAMOND_DMD_MMAT_H
#define NIDAS_DIAMOND_DMD_MMAT_H

// #include <nidas/linux/filters/short_filters.h>
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
        unsigned int fifoEmpty;         // A2D FIFO empty (error)
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
#define DMMAT_START      _IO(DMMAT_IOC_MAGIC,1)
#define DMMAT_STOP       _IO(DMMAT_IOC_MAGIC,2)
#define DMMAT_A2D_DO_AUTOCAL    _IO(DMMAT_IOC_MAGIC,3)

/** Counter Ioctls */
#define DMMAT_CNTR_START \
        _IOW(DMMAT_IOC_MAGIC,4,struct DMMAT_CNTR_Config)
#define DMMAT_CNTR_STOP       _IO(DMMAT_IOC_MAGIC,5)
#define DMMAT_CNTR_GET_STATUS \
        _IOR(DMMAT_IOC_MAGIC,6,struct DMMAT_CNTR_Status)

/**
 * D2A Ioctls
 * This D2A driver does not enforce any exclusive use policy:
 * multiple threads can each have the same device open and set
 * any of the output voltages.
 */
#define DMMAT_D2A_GET_NOUTPUTS \
        _IO(DMMAT_IOC_MAGIC,7)
#define DMMAT_D2A_GET_CONVERSION \
        _IOR(DMMAT_IOC_MAGIC,8,struct DMMAT_D2A_Conversion)
#define DMMAT_D2A_SET \
        _IOW(DMMAT_IOC_MAGIC,9,struct DMMAT_D2A_Outputs)
#define DMMAT_D2A_GET \
        _IOR(DMMAT_IOC_MAGIC,10,struct DMMAT_D2A_Outputs)
#define DMMAT_ADD_WAVEFORM \
        _IOW(DMMAT_IOC_MAGIC, 11, struct D2A_Waveform)
#define DMMAT_D2A_SET_CONFIG \
        _IOW(DMMAT_IOC_MAGIC, 12, struct D2A_Config)

#define DMMAT_IOC_MAXNR 12

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
enum dmmat_d2a_jumpers
{
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

/**
 * Structures used by D2A device to set up sending a repeated waveform.
 */
struct D2A_Config
{
        /** D2A output rate in Hz:  waveforms/sec */
	int waveformRate;
};

struct D2A_Waveform
{
        /** output D2A channel number, starting at 0. */
	int channel;

        /** number of values in points array */
	int size;

        /** waveform D2A values, for the channel */
	int point[0];
};

#ifdef __cplusplus

#include <cstdlib>
/**
* C++ wrapper class for a D2A_Waveform, so that the D2A_Waveform C struct
* is automatically freed when it goes out of scope, and a convienient constructor
* that creates the struct with a given number of points.
*/
class D2A_WaveformWrapper {
public:
        D2A_WaveformWrapper(int channel, int size)
        {
                _csize = sizeof(struct D2A_Waveform) + sizeof(_waveform->point[0]) * size;
                _waveform = (struct D2A_Waveform*) ::malloc(_csize);
                _waveform->channel = channel;
                _waveform->size = size;
        }

        ~D2A_WaveformWrapper() { ::free(_waveform); }

        /// pointer to D2A_Waveform
        D2A_Waveform* c_ptr() { return _waveform; }

        /**
         * Number of bytes in struct D2A_Waveform
         */
        int c_size() const { return _csize; }
private:
        struct D2A_Waveform* _waveform;
        int _csize;
};

#endif

#ifdef __KERNEL__
/********  Start of definitions used by the driver module only **********/

#include <linux/cdev.h>
// #include <linux/timer.h>

#include <linux/workqueue.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
#define STRUCT_MUTEX struct mutex
#else
#define STRUCT_MUTEX struct semaphore
#endif

/* Size of ISA I/O space (both MM16AT and MM32XAT) */
#define	DMMAT_IOPORT_WIDTH	16

/*
 * Board #0
 * A2D: 1 device, minor number 0, /dev/dmmat_a2d0
 * CNTR: 1 device, minor number 1 /dev/dmmat_cntr0
 * D2A: 1 device, minor number 2, /dev/dmmat_d2a0
 * D2D: 1 device, minor number 3, /dev/dmmat_d2d0
 *
 * Board #1
 * A2D: 1 device, minor number 4, /dev/dmmat_a2d1
 * CNTR: 1 device, minor number 5 /dev/dmmat_cntr1
 * D2A: 1 device, minor number 6, /dev/dmmat_d2a1
 * D2D: 1 device, minor number 7, /dev/dmmat_d2d1
 */
#define DMMAT_DEVICES_PER_BOARD 4  // See the next entries
#define DMMAT_DEVICES_A2D_MINOR 0
#define DMMAT_DEVICES_CNTR_MINOR 1
#define DMMAT_DEVICES_D2A_MINOR 2
#define DMMAT_DEVICES_D2D_MINOR 3

/* Max size of D2A waveform on DMM32XAT and DMM32DXAT */
#define DMMAT_D2A_WAVEFORM_SIZE 1024

/* Number of samples in the output circular buffer for
 * the pulse counter device. Size of the circular
 * TODO: make this dynamic, dependent on the
 * requested sample rate.
 */
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

/* all boards support this rate for the input to counter 1 */
#define DMMAT_CNTR1_INPUT_RATE  10000000

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
        /** room for sample id too */
        short data[MAX_DMMAT_A2D_CHANNELS+1];
};

/**
 * The A2D uses the cascaded counter/timer 1/2 to time the conversions.
 * If the D2A is generating a waveform, it also uses counter/timer 1/2.
 * They must agree on the outputRate, so we keep that information here
 * in one place.
 */
struct counter12
{
        int inputRate;
        int outputRate;
        atomic_t userCount;
        spinlock_t lock;
};

/**
 * Structure allocated for every DMMAT board in the system.
 */
struct DMMAT
{
        /** which board in system, from 0 */
        int num;

        /** Base address of board */
        unsigned long addr;

        /** requested IRQ */
        int irq;

        /** number of irq users each type: a2d=0, cntr=1 */
        int irq_users[2];

        /** address of interrupt status register */
        unsigned long itr_status_reg;

        /** addr of interrupt acknowledge reg */
        unsigned long itr_ack_reg;	   

        /** mask of A2D interrupt bit */
        unsigned char ad_itr_mask;

        /** mask of counter interrupt bit */
        unsigned char cntr_itr_mask;

        /** value to write to int_act_reg */
        unsigned char itr_ack_val;

        /** interrupt control register value */
        unsigned char itr_ctrl_val;

        struct counter12 clock12;       // counter/timer 1/2
        struct DMMAT_A2D* a2d;          // pointer to A2D device struct
        struct DMMAT_CNTR* cntr;        // pointer to CNTR device struct
        struct DMMAT_D2A* d2a;          // pointer to D2A device struct
        struct DMMAT_D2D* d2d;          // pointer to D2D device struct

        /**
         * Method for setting the input rate for counter/timer 1.
         * Must be implemented for each board.
         */
        int (*setClock1InputRate)(struct DMMAT* brd,int rate);

        /**
         * lock when accessing board registers to avoid intermingling
         * register accesses between threads.
         */
        spinlock_t reglock;

        STRUCT_MUTEX irqreq_mutex;      // when setting up irq handler
};

/** stuff needed by bottom half for creating normal A2D samples */
struct a2d_bh_data
{
        struct a2d_sample saveSample;
};

/** stuff needed for creating A2D samples in waveform mode */
struct waveform_bh_data
{
        /**
         * output waveform samples
         */
        struct short_sample* owsamp[MAX_DMMAT_A2D_CHANNELS];

        /**
         * current counter into each output waveform sample
         */
        int waveSampCntr;

};

/**
 * Information maintained for the A2D.
 */
struct DMMAT_A2D
{
        struct DMMAT* brd;

        struct DMMAT_A2D_Status status;

        /** Which mode is A2D operating in */
        enum a2dmode { A2D_NORMAL, A2D_WAVEFORM } mode;

        /** deviceName for each mode */
        char deviceName[2][32];

        struct cdev cdev;

        STRUCT_MUTEX mutex;

        atomic_t num_opened;                     // number of times opened
        atomic_t running;                       // a2d is running

        /**
         * If the hardware fifo overflows in waveform mode, the interrupt
         * handler must signal the bottom half of the situation.
         */
        int overflow;

        /** methods which may have a different implementation for each board type */
        void (*start)(struct DMMAT_A2D* a2d);	// a2d start method
        void (*stop)(struct DMMAT_A2D* a2d);	// a2d stop method
        int  (*getFifoLevel)(struct DMMAT_A2D* a2d);

        /**
         * Return the level of the A2D hardware fifo at which interrupts
         * are generated. Depends on the board capabilities, the
         * requested A2D scan rate, number of scanned channels, and
         * the requested latency.
         */
        int  (*getA2DThreshold)(struct DMMAT_A2D* a2d);

        int  (*getNumA2DChannels)(struct DMMAT_A2D* a2d);
        int  (*selectA2DChannels)(struct DMMAT_A2D* a2d);
        int  (*getConvRateSetting)(struct DMMAT_A2D* a2d, unsigned char* val);
        int  (*getGainSetting)(struct DMMAT_A2D* a2d,int gain, int bipolar,
                        unsigned char* val);
        void (*resetFifo)(struct DMMAT_A2D* a2d);
        void (*waitForA2DSettle)(struct DMMAT_A2D* a2d);

        /**
         * bipolar gains
         *      gain    range
         *      1       +-10V
         *      2       +-5V
         *      4       +-2.5V
         *      8       +-1.25V
         *      16      +-0.625V
         * unipolar gains
         *      gain    range
         *      1       0-20V   not avail
         *      2       0-10V
         *      4       0-5V
         *      8       0-2.5
         *      16      0-1.25
         * all channels must have same gain
         */
        int gain;

        /**
         * all channels must have same polarity
         */
        int bipolar;

        /**
         * lowest channel scanned, from 0
         */
        int lowChan;

        /**
         * highest channel scanned, from 0. User may not be requesting
         * values from all channels between lowChan and highChan,
         * but the card must scan all channels sequentially in that range.
         */
        int highChan;

        /**
         * Number of channels scanned: highChan-lowChan+1.
         */
        int nchanScanned;

        /**
         * gain and conversion rate register setting
         */
        unsigned char gainConvSetting;

        /**
         * A2D scan rate, Hz.
         */
        int scanRate;

        /**
         * time delta between A2D scans, in 1/10ths of milliseconds
         */
        int scanDeltaT;

        /**
         * maximum hardware fifo threshold
         */
        int maxFifoThreshold;

        /**
         * current hardware fifo threshold
         */
        int fifoThreshold;

        /**
         * Total of the requested sample output rates.
         */
        int totalOutputRate;
        
        /** bottom half worker for normal processing */
        struct work_struct worker;

        /** bottom half worker for waveforms */
        struct work_struct waveform_worker;

        /** data for use by normal bottom half */
        struct a2d_bh_data bh_data;

        /** data for use by waveform bottom half */
        struct waveform_bh_data waveform_bh_data;

        /**
         * Raw, timetagged samples exactly as read out of the A2D hardware fifo,
         * that are passed to bottom half for further processing.
         */
        struct dsm_sample_circ_buf fifo_samples;

        /**
         * Processed samples out of bottom half, ready for user reading.
         */
        struct dsm_sample_circ_buf samples;

        /**
         * wait queue for user read & poll methods.
         */
        wait_queue_head_t read_queue;

        /**
         * saved state of user reads
         */
        struct sample_read_state read_state;

        /**
         * how many different output filters
         */
        int nfilters;

        /**
         * the filters the user has requested.
         */
        struct a2d_filter_info* filters;

        /**
         * input channels requested in waveform mode
         */
        int waveformChannels[MAX_DMMAT_A2D_CHANNELS];

        /**
         * number of channels sampled in waveform mode
         */
        int nwaveformChannels;

        /**
         * size of the waveform samples.
         */
        int wavesize;

        /**
         * Requested buffering latency in milli-seconds. User-side reads
         * should be satisfied in roughly this amount of time if
         * there is any data available.
         */
        int latencyMsecs;

        /**
         * buffering latency in jiffies.
         */
        int latencyJiffies;

        /**
         * When were read & poll methods last woken. Used in the implementation
         * of the latency limit.
         */
        unsigned long lastWakeup;

};

/**
 * Sample from a pulse counter contains an unsigned 4 byte integer
 * counts.
 */
struct cntr_sample
{
        dsm_sample_time_t timetag;
        dsm_sample_length_t length;
        unsigned int data;
};

/**
 * Information maintained for the counter device.
 */
struct DMMAT_CNTR
{

        struct DMMAT* brd;

        struct cdev cdev;

        char deviceName[32];

        atomic_t running;

        atomic_t num_opened;

        STRUCT_MUTEX mutex;

        struct DMMAT_CNTR_Status status;

        struct timer_list timer;

        int  (*start)(struct DMMAT_CNTR* cntr);	// cntr start method

        void (*stop)(struct DMMAT_CNTR* cntr);	// cntr stop method

        unsigned int rolloverSum;     // current counter sum

        int jiffiePeriod;                       // how often to wake up
                                                // and create an output

        int firstTime;                          // first count is bad

        atomic_t shutdownTimer;                      // set to 1 to stop counting

        int lastVal;                            // previous value in the
                                                // counter register

        struct dsm_sample_circ_buf samples;    // samples for read method

        struct sample_read_state read_state;

        wait_queue_head_t read_queue;           // user read & poll methods
                                                // wait on this queue

};

/**
 * The D2A can also operate in two modes. In normal mode, it sets
 * the individual analog outputs as requested, via ioctls.
 * In waveform mode, the user passes via ioctls, one or more
 * waveform arrays, each containing the desired scaled, integer,
 * output values for an analog output channel. The desired output
 * rate of the waveforms is also specified.
 */
struct DMMAT_D2A
{
        struct DMMAT* brd;

        struct cdev cdev;

        char deviceName[32];

        atomic_t num_opened;                     // number of times opened

        atomic_t waveform_running;		

        STRUCT_MUTEX waveform_mutex;

        struct DMMAT_D2A_Outputs outputs;

        int (*setD2A)(struct DMMAT_D2A* d2a,struct DMMAT_D2A_Outputs* set,int i);

        int (*addWaveform)(struct DMMAT_D2A* d2a, struct D2A_Waveform* waveform);

        int (*loadWaveforms)(struct DMMAT_D2A* d2a);

        void (*startWaveforms)(struct DMMAT_D2A* d2a);

        void (*stopWaveforms)(struct DMMAT_D2A* d2a);

        /** minimum settable output voltage */
        int vmin;

        /** maximum settable output voltage */
        int vmax;

        /** integer count value associated with the minimum output. */
        int cmin;

        /** integer count value associated with the maximum output. */
        int cmax;
				
        /**
         * Output rate of waveform, in Hz (waveforms/sec)
         */
        int waveformRate;								

        /**
         * waveforms that the user has requested.
         */
        struct D2A_Waveform *waveforms[DMMAT_D2A_OUTPUTS_PER_BRD];

        /**
         * Number of output waveforms.
         */
        int nWaveforms;

        /**
         * Length of the waveforms for the output channels. All the
         * waveforms must have the same size.
         */
        int wavesize;

};

/**
 * A "D2D" is a combination of an A2D and a D2A, both operating in
 * waveform mode.
 */
struct DMMAT_D2D
{
	struct DMMAT* brd;
	
	struct cdev cdev;
  
	char deviceName[32];
	
	void (*start)(struct DMMAT_D2D* d2d);
	void (*stop)(struct DMMAT_D2D* d2d);
	
        STRUCT_MUTEX mutex;

        atomic_t num_opened;
};

#endif

#endif
