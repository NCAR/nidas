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

#include <nidas/linux/types.h>

#ifndef MSECS_PER_DAY
#define MSECS_PER_DAY 86400000
#endif

#ifndef SECS_PER_DAY
#define SECS_PER_DAY 86400
#endif

#ifndef MSECS_PER_SEC
#define MSECS_PER_SEC 1000
#endif

#ifndef USECS_PER_SEC
#define USECS_PER_SEC 1000000
#endif

#ifndef USECS_PER_MSEC
#define USECS_PER_MSEC 1000
#endif

#ifndef NSECS_PER_SEC
#define NSECS_PER_SEC 1000000000
#endif

#ifndef NSECS_PER_MSEC
#define NSECS_PER_MSEC 1000000
#endif

#ifndef NSECS_PER_USEC
#define NSECS_PER_USEC 1000
#endif

#ifndef __RTCORE_KERNEL__
#include <sys/time.h>
#define rtl_timeval timeval
#endif

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
    if      (value <= 1)     return IRIG_1_HZ;
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
    static unsigned int rate[] = {0,1,2,4,5,10,20,25,50,100,0};
    return rate[value];
}

struct dsm_clock_data {
    struct rtl_timeval tval;
    unsigned char      status;
};

struct dsm_clock_sample {
    dsm_sample_time_t timetag;		/* timetag of sample */
    dsm_sample_length_t length;		/* number of bytes in data */
    struct dsm_clock_data data;		/* must be no padding between
    					 * length and data! */
};

#define IRIG_MAGIC	'I'
#define IRIG_OPEN	_IO(IRIG_MAGIC,0)
#define IRIG_CLOSE	_IO(IRIG_MAGIC,1)
#define IRIG_GET_STATUS	_IOR(IRIG_MAGIC,2,unsigned char)
#define IRIG_GET_CLOCK	_IOR(IRIG_MAGIC,3,struct rtl_timeval)
#define IRIG_SET_CLOCK	_IOW(IRIG_MAGIC,4,struct rtl_timeval)
#define IRIG_OVERRIDE_CLOCK _IOW(IRIG_MAGIC,5,struct rtl_timeval)


/****************  Start of symbols used by kernel modules **************************/

#ifdef __RTCORE_KERNEL__

extern volatile unsigned long msecClock[];
extern volatile unsigned char readClock;

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
#define GET_MSEC_CLOCK (msecClock[readClock])

/**
 * Fetch the IRIG clock value.  This is meant to be used for
 * debugging, rather than real-time time tagging.  It involves
 * ISA bus transfers to/from the IRIG card. However, it has
 * a precision of better than a micro-second. The accuracy
 * is unknown and is probably effected by ISA contention.
 */
void irig_clock_gettime(struct rtl_timespec* tp);

/**
 * For modules who want to know the resolution of the clock
 */
int get_msec_clock_resolution();

typedef void irig_callback_t(void* privateData);

void setRate2Output (int rate, int isRT);

int register_irig_callback(irig_callback_t* func, enum irigClockRates rate,
	void* privateData);

void unregister_irig_callback(irig_callback_t* func, enum irigClockRates rate,
	void* privateData);

struct irig_port {
    char* inFifoName;
    int inFifoFd;
    struct dsm_clock_sample samp;
};


#endif		/* __RTCORE_KERNEL__ */

#endif
