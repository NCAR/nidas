/*
   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   Driver for Diamond Systems Corp GPIO-MM series of Counter/Timer
    Digital I/O Cards.

   Original Author: Gordon Maclean

   Copyright 2008 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

/*
 * Each board has 10 16-bit counters and gobs of digital I/O pins.
 *
 * The implementation of a frequency counter uses 3 counters.
 * One counter on a board is used as a general timer clock.
 * Other driver modules can register with this clock to be
 * called back at periodic times.
 *
 * Therefore each board supports up to 3 frequency input
 * devices, or 9 pulse counters.
 *
 * Eventually the driver could also support a digital device,
 * for controlling all the I/O pins. Dig I/O interrupt
 * support is also available.
 *
 * So the following devices can be opened concurrently on a board:
 * 3 frequency counters, 1 dio
 * 2 frequency counters, 3 pulse counters, 1 dio
 * 1 frequency counters, 6 pulse counters, 1 dio
 * 0 frequency counters, 9 pulse counters, 1 dio
 *
 * Support for the pulse counters and dio has not been added yet....
 *      
 * Board #0: Device table:
 * device        devname                minor number
 * freq cntr 0:  /dev/gpiomm_fcntr0     0
 * freq cntr 1:  /dev/gpiomm_fcntr1     1
 * freq cntr 2:  /dev/gpiomm_fcntr2     2
 * digital i/O:  /dev/gpiomm_dio0       3
 * pulse cntr 0: /dev/gpiomm_cntr0      4
 * pulse cntr 1: /dev/gpiomm_cntr1      5
 * pulse cntr 2: /dev/gpiomm_cntr2      6
 * pulse cntr 3: /dev/gpiomm_cntr3      7
 * pulse cntr 4: /dev/gpiomm_cntr4      8
 * pulse cntr 5: /dev/gpiomm_cntr5      9
 * pulse cntr 6: /dev/gpiomm_cntr6     10
 * pulse cntr 7: /dev/gpiomm_cntr7     11
 * pulse cntr 8: /dev/gpiomm_cntr8     12
 *
 * Board #1: Device table:
 * device        devname                minor number (board0 + 13)
 * freq cntr 0:  /dev/gpiomm_fcntr3    13
 * freq cntr 1:  /dev/gpiomm_fcntr4    14
 * freq cntr 2:  /dev/gpiomm_fcntr5    15
 * digital i/O:  /dev/gpiomm_dio1      16
 * pulse cntr 0: /dev/gpiomm_cntr9     17
 * pulse cntr 1: /dev/gpiomm_cntr10    18
 * pulse cntr 2: /dev/gpiomm_cntr11    19
 * pulse cntr 3: /dev/gpiomm_cntr12    20
 * pulse cntr 4: /dev/gpiomm_cntr13    21
 * pulse cntr 5: /dev/gpiomm_cntr14    22
 * pulse cntr 6: /dev/gpiomm_cntr15    23
 * pulse cntr 7: /dev/gpiomm_cntr16    24
 * pulse cntr 8: /dev/gpiomm_cntr17    25
 */

#ifndef NIDAS_DIAMOND_GPIO_MM_H
#define NIDAS_DIAMOND_GPIO_MM_H

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
        unsigned int lostSamples;
        unsigned int pulseUnderflow;
        unsigned int badGateWarning;
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
#define GPIO_MM_FCNTR_GET_STATUS \
    _IOR(GPIO_MM_IOC_MAGIC,1,struct GPIO_MM_fcntr_status)

/* Maximum IOCTL number in above values */
#define GPIO_MM_IOC_MAXNR 1

#define GPIO_MM_CT_CLOCK_HZ 20000000

#ifdef __KERNEL__
/********  Start of definitions used by driver modules only **********/

#include <linux/cdev.h>
#include <nidas/linux/util.h>
#include <nidas/linux/diamond/gpio_mm_regs.h>

typedef void (*gpio_timer_callback_func_t) (void* privateData);

/**
 * Structure returned by register_gpio_timer_callback, and
 * passed to unregister_gpio_timer_callback. These fields
 * should be regarded as private, only changed by the gpio driver.
 */
struct gpio_timer_callback
{
        struct list_head list;
        gpio_timer_callback_func_t callbackFunc;
        void* privateData;
        unsigned int usecs;
        unsigned int tickModulus;
};

/**
 * Exposed function that allows other modules to register their callback
 * function to be called at the given interval. This capability is
 * implemented with a counter on the first GPIO-MM board in the system.
 *
 * register_gpio_timer_callback can be called at anytime, even from 
 * interrupt context.
 * The user's callback function is invoked from a tasklet in software
 * interrupt context.
 * register_gpio_timer_callback and unregister_gpio_timer_callback can be
 * called within a callback function itself.
 * @param usecs: callback interval, in micro-seconds.  A usecs value
 *          less than 1000 is not advised.
 */
extern struct gpio_timer_callback *register_gpio_timer_callback(
        gpio_timer_callback_func_t callback,unsigned int usecs,
        void *privateData, int *errp);
    
/**
 * Exposed function to un-register a callback.
 * A callback function can unregister itself, or other callback functions.
 * wait=1: do a wait_event_interruptible_timeout until it is certain
 *          that the callback will not be called again by the timer.
 *          The timeout is one second and cannot be changed.
 *          Do not using wait=1 when calling from interrupt context.
 * wait=0: don't wait. The callback might be called once more
 *          after this call to unregister_gpio_timer_callback.
 *          Set wait=0 if calling from interrupt context or software
 *          interrupt context where you must not sleep. 
 *          If a callback function is unregistering itself (in which
 *          case it is being called from software interrupt context)
 *          then it is guaranteed that it will not be called again,
 *          and so wait=1 is unnecessary (and wrong).
 * return:
 *   if wait==0, return will be 0.
 *   if wait != 0, the return will be value of
 *     wait_event_interruptible_timeout(). The return will be 0
 *     if the callback function was never deactivated,
 *     meaning the 1 second timeout occured while waiting
 *     for the callback to be deactivated
 *     Or, if the callback was deactivated, the return is the
 *     positive number of jiffies (1/HZ seconds) that remained
 *     in the timeout.
 */
extern long unregister_gpio_timer_callback(struct gpio_timer_callback *cb,
    int wait);

/* Size of ISA I/O space */
#define	GPIO_MM_DIO_IOPORT_WIDTH	8
#define	GPIO_MM_CT_IOPORT_WIDTH         16

#define MAX_GPIO_MM_BOARDS	5	// number of boards supported by driver

/* See device table in above comments */
#define GPIO_MM_MINORS_PER_BOARD 13

/* Use 3 counter timers to implement one frequency counter */
#define GPIO_MM_CNTR_PER_FCNTR 3

/* This will be 10/3=3 for GPIO-MM */
#define GPIO_MM_FCNTR_PER_BOARD (GPIO_MM_CNTR_PER_BOARD/GPIO_MM_CNTR_PER_FCNTR)

#define GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE 16

/* Which counter 0-9 to use for a general purpose timer */
#define GPIO_MM_TIMER_COUNTER 9

#define CALLBACK_POOL_SIZE 64  /* number of timer callbacks we can support */

/**
 * Sample from a frequency counter contains an unsigned 4 byte integer
 * counts and an unsigned 4 byte integer number of clock ticks.
 */
struct freq_sample
{
        dsm_sample_time_t timetag;    // timetag of sample
        dsm_sample_length_t length;       // number of bytes in data (8)
        int pulses;
        int ticks;
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
         * how often to report frequency samples
         */
        int outputPeriodUsec;

        atomic_t num_opened;                     // number of times opened

        struct gpio_timer_callback* timer_callback;

        struct dsm_sample_circ_buf samples;         // samples out of b.h.

        /**
         * User read & poll methods wait on this queue.
         */
        wait_queue_head_t rwaitq;

        struct sample_read_state read_state;

        /**
         * read latency in jiffies.
         */
        long latencyJiffies;

        /**
         * When were read & poll methods last woken.
         */
        unsigned long lastWakeup;

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
         * Timer sleep interval, in microseconds.
         * Greatest common divisor of requested timer intervals.
         * This will be non-zero if the timer is running.
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

        int scaler;

        /**
         * Bottom half of timer, calls client callback functions
         * after receipt of counter interrupt */
        struct tasklet_struct tasklet;

        /**
         * Has a new callback been added or removed but has not yet
         * activated by the timer tasklet?
         */
        volatile int callbacksChanged;

        /**
         * When unregistering a callback one can wait on this queue
         * to make sure that your callback will not be invoked.
         */
        wait_queue_head_t callbackWaitQ;

};

/**
 * Structure allocated for every GPIO_MM board in the system.
 */
struct GPIO_MM
{
        int num;                        // which board in system, from 0
        unsigned long dio_addr;         // Base address of 8255 DIO regs
        unsigned long ct_addr;        // Base address of 9513 cntr/timer regs
        int irqs[2];		        // values of ISA irq A and B
        int reqirqs[2];		        // requested system irqs A and B
        int irq_users[2];               // number of irq users, 0=A,1=B
        int cntr_used[GPIO_MM_CNTR_PER_BOARD];             // 0=unused, 1=used

	int boardID;

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
