/* irigclock.h

   Class for interfacing the PC104-SG time and frequency processor.

   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research

   Revisions:

     $LastChangedRevision: $
         $LastChangedDate: $
           $LastChangedBy: $
                 $HeadURL: $
*/

#ifndef IRIGCLOCK_H
#define IRIGCLOCK_H

#include <dsm_sample.h>

#define MSECS_PER_DAY 86400000

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
    IRIG_100_HZ, IRIG_NUM_RATES
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

#define IRIG_MAGIC 'I'
#define IRIG_OPEN _IO(IRIG_MAGIC,0)
#define IRIG_CLOSE _IO(IRIG_MAGIC,1)
#define IRIG_GET_STATUS _IOR(IRIG_MAGIC,2,unsigned char)
#define IRIG_GET_CLOCK _IOR(IRIG_MAGIC,3,struct rtl_timeval)
#define IRIG_SET_CLOCK _IOW(IRIG_MAGIC,4,struct rtl_timeval)
#define IRIG_OVERRIDE_CLOCK _IOW(IRIG_MAGIC,5,struct rtl_timeval)

#ifdef __RTCORE_KERNEL__

/* External symbols used by kernel modules */

extern unsigned long msecClock[];
extern unsigned char readClock;

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
 * over from 86399999 to 0 at midnight.
 */
#define GET_MSEC_CLOCK (msecClock[readClock])

typedef void irig_callback_t(void* privateData);

int register_irig_callback(irig_callback_t* func, enum irigClockRates rate,
	void* privateData);

void unregister_irig_callback(irig_callback_t* func, enum irigClockRates rate);

struct irig_port {
    char* inFifoName;
    int inFifoFd;
    struct dsm_clock_sample samp;
};


#endif		/* __RTCORE_KERNEL__ */

#endif
