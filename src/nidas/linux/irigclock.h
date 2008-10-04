/* irigclock.h

   Class for interfacing the PC104-SG time and frequency processor.

   Original Author: Mike Spowart
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   Revisions:

     $LastChangedRevision: 3648 $
     $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $
     $LastChangedBy: cjw $
     $HeadURL: http://svn.atd.ucar.edu/svn/nids/trunk/src/nidas/rtlinux/irigclock.h $
*/

#ifndef IRIGCLOCK_H
#define IRIGCLOCK_H

#if defined(__KERNEL__)
#  include <linux/time.h>
#include <nidas/linux/util.h>
#else
#  include <sys/time.h>
#endif

#include <nidas/linux/types.h>

/**
 * Enumeration of the callback rates supported by this module.
 */
enum irigClockRates {
    IRIG_0_1_HZ, IRIG_1_HZ,  IRIG_2_HZ,  IRIG_4_HZ,  IRIG_5_HZ,
    IRIG_10_HZ, IRIG_20_HZ, IRIG_25_HZ, IRIG_50_HZ,
    IRIG_100_HZ, IRIG_NUM_RATES, IRIG_ZERO_HZ = IRIG_NUM_RATES,
};

/**
 * Convert a rate in Hz to an enumerated value.
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
    else                    return IRIG_NUM_RATES;  /* invalid value given */
}

/**
 * Convert an enumerated value back to a rate in Hz.
 */
static inline unsigned int irigClockEnumToRate(enum irigClockRates value)
{
    static unsigned int rate[] = {0, 1, 2, 4, 5, 10, 20, 25, 50, 100, 0};
    return rate[value];
}

struct dsm_clock_data {
    struct timeval tval;
    unsigned char status;
};

struct dsm_clock_sample {
    dsm_sample_time_t timetag;		/* timetag of sample */
    dsm_sample_length_t length;		/* number of bytes in data */
    struct dsm_clock_data data;		/* must be no padding between
    					 * length and data! */
};

/**
 * User IOCTLs that we support.
 */
#define IRIG_IOC_MAGIC 'I'	/* Unique(ish) char for IRIG ioctls */

#define IRIG_OPEN		_IO(IRIG_MAGIC, 0)
#define IRIG_CLOSE		_IO(IRIG_MAGIC, 1)
#define IRIG_GET_STATUS		_IOR(IRIG_IOC_MAGIC, 2, unsigned char)
#define IRIG_GET_CLOCK		_IOR(IRIG_IOC_MAGIC, 3, struct timeval)
#define IRIG_SET_CLOCK		_IOW(IRIG_IOC_MAGIC, 4, struct timeval)
#define IRIG_OVERRIDE_CLOCK	_IOW(IRIG_IOC_MAGIC, 5, struct timeval)

/**********  Start of symbols used by kernel modules **********/

#if defined(__KERNEL__)

#include <asm/semaphore.h>
#include <linux/ioctl.h>
#include <linux/wait.h>

extern volatile unsigned long MsecClock[];
extern volatile unsigned char ReadClock;

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

/**
 * Macro used by kernel modules to get the current clock value
 * in milliseconds since GMT 00:00:00.  Note that this value rolls
 * over from 86399999 to 0 at midnight. This is an efficient
 * clock for real-time use. Fetching the value is just a RAM
 * access - not an ISA bus access.
 */
#define GET_MSEC_CLOCK (MsecClock[ReadClock])

/**
 * For modules who want to know the resolution of the clock..
 */
int get_msec_clock_resolution(void);

/**
 * Fetch the IRIG clock value directly.  This is meant to be used for
 * debugging, rather than real-time time tagging.  It disables interrupts
 * and directly performs ISA bus transfers to/from the IRIG card.
 * Precision is better than 1 microsecond; the accuracy is
 * unknown and is probably affected by ISA contention.
 */
void irig_clock_gettime(struct timespec* tp);

typedef void irig_callback_func(void* privateData);

void setRate2Output(int rate);

/**
 * Entry in a callback list.
 */
struct irig_callback {
    struct list_head list;
    irig_callback_func* callback;
    void* privateData;
    enum irigClockRates rate;
    int enabled;
};

/**
 * Schedule timed regular callbacks of a particular function.
 * These callbacks are executed from a software interrupt.
 */
extern struct irig_callback* register_irig_callback(
    irig_callback_func* func, enum irigClockRates rate,
    void* privateData,int *errp);

/**
 * Remove a callback from the queue.
 * A callback function can remove itself, or any other
 * callback function.
 * @return 1: OK, callback will never be called again
 *      0: callback might be called once more. Do flush_irig_callback
 *         to wait until it is definitely finished.
 *      <0: errno
 */
extern int unregister_irig_callback(struct irig_callback*);

extern int flush_irig_callbacks(void);

#endif	/* defined(__KERNEL__) */

#endif
