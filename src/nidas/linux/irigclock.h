/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*-
 * vim: set shiftwidth=8 softtabstop=8 expandtab: */
/* irigclock.h

   Class for interfacing the PC104-SG time and frequency processor.

   Original Author: Mike Spowart
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   Revisions:

   $LastChangedRevision$
   $LastChangedDate$
   $LastChangedBy$
   $HeadURL$
*/

#ifndef IRIGCLOCK_H
#define IRIGCLOCK_H

#if defined(__KERNEL__)
#  include <linux/time.h>
#  include <nidas/linux/util.h>
#else
#  include <sys/time.h>
#endif

#include <nidas/linux/types.h>

/**
 * Enumeration of the callback rates supported by this module.
 * The enumeration values are a simple integer sequence from 0 by 1
 * rather than the actual rate value because they are used as an
 * index into an array of callback structures.
 */
enum irigClockRates {
        IRIG_0_1_HZ, IRIG_1_HZ,  IRIG_2_HZ,  IRIG_4_HZ,  IRIG_5_HZ,
        IRIG_10_HZ, IRIG_20_HZ, IRIG_25_HZ, IRIG_50_HZ,
        IRIG_100_HZ, IRIG_NUM_RATES, IRIG_ZERO_HZ = IRIG_NUM_RATES,
};

/**
 * Convert a rate in Hz to an enumerated value, rounding up to the next highest
 * supported rate, if necessary. This is a rather inefficient function and should
 * only be called at device open or module initialization.  We make it inline to avoid
 * "defined but not used" compiler warning. This is called by kernel and user code.
 */
static inline enum irigClockRates irigClockRateToEnum(unsigned int value)
{
        /* Round up to the next highest enumerated poll rate. */
        if      (value == 0)     return IRIG_0_1_HZ;
        else if (value <= 1)     return IRIG_1_HZ;
        else if (value <= 2)     return IRIG_2_HZ;
        else if (value <= 4)     return IRIG_4_HZ;
        else if (value <= 5)     return IRIG_5_HZ;
        else if (value <= 10)    return IRIG_10_HZ;
        else if (value <= 20)    return IRIG_20_HZ;
        else if (value <= 25)    return IRIG_25_HZ;
        else if (value <= 50)    return IRIG_50_HZ;
        else if (value <= 100)   return IRIG_100_HZ;
        else                     return IRIG_NUM_RATES;  /* invalid value given */
}

/**
 * Convert an enumerated value back to a rate in Hz.
 */
static inline unsigned int irigClockEnumToRate(enum irigClockRates value)
{
        static unsigned int rate[] = {0, 1, 2, 4, 5, 10, 20, 25, 50, 100, 0};    
        return rate[value];       
}

/*
 * fields in struct timeval on 64 bit machine are 64 bits,
 * so declare our own 2x32 bit timeval struct.
 */
struct timeval32 {
        int tv_sec;
        int tv_usec;
};

struct dsm_clock_data {
        struct timeval32 tval;
        unsigned char status;
};

struct dsm_clock_data_2 {
        struct timeval32 irigt;
        struct timeval32 unixt;
        unsigned char status;
        unsigned char seqnum;
        unsigned char syncToggles;
        unsigned char clockAdjusts;
        unsigned char max100HzBacklog;
};

struct dsm_clock_sample {
        dsm_sample_time_t timetag;		/* timetag of sample */
        dsm_sample_length_t length;		/* number of bytes in data */
        struct dsm_clock_data data;		/* must be no padding between
                                                 * length and data! */
};

struct dsm_clock_sample_2 {
        dsm_sample_time_t timetag;		/* timetag of sample */
        dsm_sample_length_t length;		/* number of bytes in data */
        struct dsm_clock_data_2 data;	/* must be no padding between
                                         * length and data! */
};

/**
 * Limits for how many ticks the 100Hz software clock can disagree
 * with the hardware clock before a reset is done. If the
 * clock difference is within this range, the software clock
 * is slewed by incrementing slower or faster, rather
 * than a step change reset.
 */
#define IRIG_MAX_DT_DIFF 20
#define IRIG_MIN_DT_DIFF -20

struct pc104sg_status {

        /**
         * Counts of the number of times that the IRIG
         * hardware clock has lost or regained sync.
         */
        uint32_t syncToggles;

        /**
         * Total counts of the number of times that the
         * sofware clock was reset with a step change.
         */
        uint32_t softwareClockResets;

        /**
         * Counts of the number of times that the software clock was
         * slewed, indexed by the number of delta-Ts is was out.
         */
        uint32_t slews[IRIG_MAX_DT_DIFF - IRIG_MIN_DT_DIFF + 1];

        /**
         * Value of extended status from PC104SG dual port RAM.
         * Bits:
         * 0: 1=On-board clock has not been verified to be within
         *          DP_Syncthr in last 5 seconds
         * 1: 1=Input time code unreadable.
         * 2: 1=PPS pulses not 1 second apart
         * 3: 1=Major time has not been set since counter rejam
         * 4: 1=Year not set
         */
        unsigned char extendedStatus;
};

/**
 * User IOCTLs that we support.
 */
#define IRIG_IOC_MAGIC 'I'	/* Unique(ish) char for IRIG ioctls */

#define IRIG_OPEN		_IO(IRIG_IOC_MAGIC, 0)
#define IRIG_CLOSE		_IO(IRIG_IOC_MAGIC, 1)
#define IRIG_GET_STATUS		_IOR(IRIG_IOC_MAGIC, 2, struct pc104sg_status)
#define IRIG_GET_CLOCK		_IOR(IRIG_IOC_MAGIC, 3, struct timeval32)
#define IRIG_SET_CLOCK		_IOW(IRIG_IOC_MAGIC, 4, struct timeval32)
#define IRIG_OVERRIDE_CLOCK	_IOW(IRIG_IOC_MAGIC, 5, struct timeval32)

/**********  Start of symbols used by kernel modules **********/

#if defined(__KERNEL__)

#include <linux/ioctl.h>
#include <linux/wait.h>

struct irigTime {
        int year;	/* actual year, eg: 2004 */
        int yday;	/* day of year, 1-366 */
        int hour;
        int min;
        int sec;
        int msec;
        int usec;
        int nsec;
};


/*
 * Clock ticker kept in RAM for reading (not writing!) by other kernel
 * modules via the GET_MSEC_CLOCK and GET_TMSEC_CLOCK macros.
 */
extern int TMsecClock[];
extern int ReadClock;

/**
 * Macro used by kernel modules to get the current clock value
 * in milliseconds since GMT 00:00:00.  Note that this value rolls
 * over from 86399999 to 0 at midnight. This is an efficient
 * clock for real-time use. Fetching the value is just a RAM
 * access - not an ISA bus access.
 * The memory barrier ensures that the store of the clock value
 * is complete before the load.
 */
#define GET_TMSEC_CLOCK \
        ({\
         int tmp = ReadClock;\
         smp_read_barrier_depends();\
         TMsecClock[tmp];\
         })

#define GET_MSEC_CLOCK (GET_TMSEC_CLOCK/10)

/**
 * For modules who want to know the resolution of the clock..
 */
extern int get_msec_clock_resolution(void);

/**
 * Fetch the IRIG clock value directly.  This is meant to be used for
 * debugging, rather than real-time time tagging.  It disables interrupts
 * and directly performs ISA bus transfers to/from the IRIG card.
 * Precision is better than 1 microsecond; the accuracy is
 * unknown and affected by ISA contention and interrupt activity.
 */
extern void irig_clock_gettime(struct timespec* tp);

typedef void irig_callback_func(void* privateData);

/**
 * Entry in a callback list.
 */
struct irig_callback {
        struct list_head list;
        irig_callback_func* callback;
        irig_callback_func* resyncCallback;
        void* privateData;
        enum irigClockRates rate;
};

/**
 * Schedule regular callbacks of a particular function.
 * These callbacks are executed by the pc104sg driver from
 * a software interrupt, hence they cannot do anything
 * that would sleep, like calling wait_event, or lock a
 * semaphore, or allocate memory with anything other than
 * GFP_ATOMIC, or call schedule.
 *
 * In general, an irig callback should act like a hardware
 * interrupt handler, doing a minimum of essential operations,
 * before scheduling a work_queue for further processing,
 * or notifying the user side that data is available if further
 * processing is not necessary.
 *
 * printks (and KLOG_* macros) cause delays, so keep
 * those to a minimum in these callbacks.
 *
 * The callbacks are performed starting with the highest
 * requested rate, typcally 100Hz, in the sequence that
 * they were registered.  Therefore, the execution time
 * of a callback delays the execution of other
 * callbacks at the same rate that were registered later,
 * and all callbacks that are registered at lower rates.
 * So, keep then quick to reduce effects on other modules.
 *
 * register_irig_callback should not be called from hardware
 * interrupt context.
 */
extern struct irig_callback* register_irig_callback(
                irig_callback_func* func,
                irig_callback_func* resync,
                enum irigClockRates rate,
                void* privateData,int *errp);

/**
 * Remove a callback from the list.
 * A callback function can remove itself, or any other
 * callback function.
 * @return 1: OK, callback will never be called again
 *      0: callback might be called once more. If not in a callback,
 *         or in hardware interrupt context, do flush_irig_callback
 *         to wait until the irig driver is finished with the current
 *         set of callbacks, to ensure that the callback will not be
 *         called again.
 *      <0: errno
 *
 * unregister_irig_callback should not be called from hardware
 * interrupt context.
 */
extern int unregister_irig_callback(struct irig_callback*);

/**
 * This function does a wait_event_interruptible() until the
 * pc104sg module is not calling its callbacks.  Note:
 * this function cannot be called from an irig callback, since
 * 1. it would wait forever
 * 2. waits are not allowed from software interrupt context.
 *
 * It is actually not necessary to call from an irig callback
 * since if an irig callback unregisters itself, then
 * it will not be called again after a return.
 *
 * Since flush_irig_callbacks does a wait, it should also not
 * be called from a hardware interrupt handler.
 *
 * It is useful to use from a device release fops (user close),
 * which is in user context, to ensure that a certain
 * callback will not be called again:
 * int i = unregister_irig_callback(my_callback);
 * if (i < 0) return i;
 * else if (!i) flush_irig_callbacks();
 *
 * @return  0: OK,
 *      -ERESTARTSYS: wait was interrupted by a signal.
 */
extern int flush_irig_callbacks(void);

/**
 * Function that can be called to set the rate of the auxillary output
 * on the PC104SG card.
 */
void setRate2Output(int rate);

#endif	/* defined(__KERNEL__) */

#endif
