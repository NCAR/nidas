/*
   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   Driver for Diamond Systems Corp GPIO-MM series of Counter/Timer
    Digital I/O Cards

   Original Author: Gordon Maclean

   Copyright 2008 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#ifndef NIDAS_DIAMOND_GPIO_MM_H
#define NIDAS_DIAMOND_GPIO_MM_H

#include <nidas/linux/util.h>

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

struct GPIO_MM_fcntr_config
{
        int outputPeriodUsec;   // how often to report
        int numPulses;             // how many pulses to time
        int latencyUsecs;
};

struct GPIO_MM_fcntr_status
{
        size_t lostSamples;
        size_t pulseUnderflow;
};

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define GPIO_MM_IOC_MAGIC 'g'

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */

/** Counter Ioctls */
#define GPIO_MM_FCNTR_START \
    _IOW(GPIO_MM_IOC_MAGIC,0,struct GPIO_MM_fcntr_config)
#define GPIO_MM_FCNTR_STOP       _IO(GPIO_MM_IOC_MAGIC,1)
#define GPIO_MM_FCNTR_GET_STATUS \
    _IOR(GPIO_MM_IOC_MAGIC,2,struct GPIO_MM_fcntr_status)

#define GPIO_MM_IOC_MAXNR 2

#ifdef __KERNEL__
/********  Start of definitions used by the driver module only **********/

#include <linux/cdev.h>
#include <nidas/linux/diamond/gpio_mm_regs.h>

typedef void (*gpio_timer_callback_func_t) (void* privateData);

struct gpio_timer_callback
{
        struct list_head list;
        gpio_timer_callback_func_t callbackFunc;
        void* privateData;
        unsigned int usecs;
        unsigned int tickModulus;
};

extern struct gpio_timer_callback *register_gpio_timer_callback(
        gpio_timer_callback_func_t callback,unsigned int usecs,
        void *privateData, int *errp);
    
extern long unregister_gpio_timer_callback(struct gpio_timer_callback *cb,
    int wait);

/* Size of ISA I/O space */
#define	GPIO_MM_DIO_IOPORT_WIDTH	8
#define	GPIO_MM_CT_IOPORT_WIDTH         16

#define MAX_GPIO_MM_BOARDS	5	// number of boards supported by driver

/*
 * Board #0
 * FREQ0: 1 device, minor number 0, /dev/gpiomm_fcntr0
 * FREQ1: 1 device, minor number 1, /dev/gpiomm_fcntr1
 * FREQ2: 1 device, minor number 2, /dev/gpiomm_fcntr2
 * DIO:   1 device, minor number 3, /dev/gpiomm_dio0
 * CNTR0: 1 device, minor number 4, /dev/gpiomm_cntr0
 * CNTR1: 1 device, minor number 5, /dev/gpiomm_cntr1
 * CNTR2: 1 device, minor number 6, /dev/gpiomm_cntr2
 * CNTR3: 1 device, minor number 7, /dev/gpiomm_cntr3
 * CNTR4: 1 device, minor number 8, /dev/gpiomm_cntr4
 * CNTR5: 1 device, minor number 9, /dev/gpiomm_cntr5
 * CNTR6: 1 device, minor number 10, /dev/gpiomm_cntr6
 * CNTR7: 1 device, minor number 11, /dev/gpiomm_cntr7
 * CNTR8: 1 device, minor number 12, /dev/gpiomm_cntr8
 * CNTR9: 1 device, minor number 13, /dev/gpiomm_cntr9
 *
 * Board #1
 * FREQ0: 1 device, minor number 20, /dev/gpiomm_fcntr3
 * FREQ0: 1 device, minor number 21, /dev/gpiomm_fcntr4
 * FREQ0: 1 device, minor number 22, /dev/gpiomm_fcntr5
 * DIO: 1 device, minor number 23,   /dev/gpiomm_dio1
 * CNTR0: 1 device, minor number 24, /dev/gpiomm_cntr10
 * CNTR1: 1 device, minor number 25, /dev/gpiomm_cntr11
 * CNTR2: 1 device, minor number 26, /dev/gpiomm_cntr12
 * CNTR3: 1 device, minor number 27, /dev/gpiomm_cntr13
 * CNTR4: 1 device, minor number 28, /dev/gpiomm_cntr14
 * CNTR5: 1 device, minor number 29, /dev/gpiomm_cntr15
 * CNTR6: 1 device, minor number 30, /dev/gpiomm_cntr16
 * CNTR7: 1 device, minor number 31, /dev/gpiomm_cntr17
 * CNTR8: 1 device, minor number 32, /dev/gpiomm_cntr18
 * CNTR9: 1 device, minor number 33, /dev/gpiomm_cntr19
 */
#define GPIO_MM_MINORS_PER_BOARD 20

/* Use 3 counter timers to implement one frequency counter */
#define GPIO_MM_CNTR_PER_FCNTR 3

/* This will be 3 for GPIO-MM */
#define GPIO_MM_FCNTR_PER_BOARD (GPIO_MM_CNTR_PER_BOARD/GPIO_MM_CNTR_PER_FCNTR)

#define GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE 8

#define CALLBACK_POOL_SIZE 64  /* number of timer callbacks we can support */

/**
 * Sample from a frequency counter contains an unsigned 4 byte integer
 * counts and an unsigned 4 byte integer number of 40Mh clock ticks.
 */
struct freq_sample
{
        dsm_sample_time_t timetag;    // timetag of sample
        dsm_sample_length_t length;       // number of bytes in data
        int pulses;
        int ticks40Mhz;
};

struct GPIO_MM;

/**
 * Information maintained for a frequency counter device.
 */
struct GPIO_MM_fcntr
{
        struct GPIO_MM* brd;

        int num;                            // which freq counter on board

        struct GPIO_MM_fcntr_status status;

        char deviceName[32];
        struct cdev cdev;

        /**
         * number of pulses to count.
         */
        int numPulses;

        /**
         * how often to report frequency
         */
        int outputPeriodUsec;

        int running;

        atomic_t num_opened;                     // number of times opened

        struct gpio_timer_callback* timer_callback;

        struct dsm_sample_circ_buf samples;         // samples out of b.h.

        // user read & poll methods wait on this
        wait_queue_head_t rwaitq;

        struct sample_read_state read_state;

        long latencyMsecs;	        // buffer latency in milli-seconds
        long latencyJiffies;	// buffer latency in jiffies
        unsigned int lastWakeup;   // when were read & poll methods last woken

        unsigned int irqsReceived;
};

/*
 * The GPIO timer.
 */
struct GPIO_MM_timer
{
        struct GPIO_MM* brd;

        /**
         * The active list of timer callbacks
         */
        struct list_head callbackList;

        /**
         * A pre-allocated pool of callbacks that are available.
         */
        struct list_head callbackPool;

        /**
         * Newly registered callbacks that are not active yet.
         */
        struct list_head pendingAdds;

        /**
         * callbacks that are to be removed next.
         */
        struct gpio_timer_callback *pendingRemoves[CALLBACK_POOL_SIZE];

        int nPendingRemoves;

        /**
         * Used to enforce exclusive access to the callbackPool, pendingAdds
         * and pendingRemoves callback lists.
         */
        spinlock_t callbackLock;

        /**
         * Greatest common divisor of requested timer intervals.
         */
        unsigned int usecs;

        volatile unsigned int irqsReceived;

        /**
         * The current timer tick.
         */
        volatile unsigned int tick;

        /**
         * Timer tick rollover.
         */
        unsigned int tickLimit;

        /**
         * Bottom half of timer, calls client callback functions
         * after receipt of counter interrupt */
        struct tasklet_struct tasklet;

        /**
         * Has a new callback been added or removed but has not yet
         * activated by the timer tasklet?
         */
        atomic_t callbacksChanged;

        /**
         * When unregistering a callback one can wait on this queue
         * to make sure that your callback will not be invoked.
         */
        wait_queue_head_t callbackWaitQ;

        /** is this timer running? */
        atomic_t running;
};

/**
 * Structure allocated for every GPIO_MM board in the system.
 */
struct GPIO_MM
{
        int num;                        // which board in system, from 0
        unsigned long dio_addr;         // Base address of 8255 DIO regs
        unsigned long ct_addr;        // Base address of 9513 cntr/timer regs
        int irqs[2];		        // ISA irq A and B
        int reqirqs[2];		        // requested system irqs A and B
        int irq_users[2];               // number of irq users, 0=A,1=B
        int cntr_used[GPIO_MM_CNTR_PER_BOARD];             // 0=unused, 1=used

        /**
         * Settings of master mode registers for both 9513 chips
         */
        unsigned char mmode_lsb[2];
        unsigned char mmode_msb[2];

        struct GPIO_MM_timer* timer;

        /**
         * Pointer to frequency counter device structures.
         */
        struct GPIO_MM_fcntr* fcntrs;

        /**
         * lock when accessing board registers
         *  to avoid confusing the board.
         */
        spinlock_t reglock;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        struct mutex brd_mutex;         // lock for non-interrupt board ops
#else
        struct semaphore brd_mutex;     // lock for non-interrupt board ops
#endif
};

#endif

#endif
