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

enum irigClockRates {
    IRIG_1_HZ,  IRIG_2_HZ,  IRIG_4_HZ,  IRIG_5_HZ,
    IRIG_10_HZ, IRIG_20_HZ, IRIG_25_HZ, IRIG_50_HZ,
    IRIG_100_HZ, IRIG_NUM_RATES
};

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

static inline unsigned int irigClockEnumToRate(enum irigClockRates value)
{
    static unsigned int rate[] = {1,2,4,5,10,20,25,50,100};
    return rate[value];
}


#ifdef __KERNEL__

extern unsigned long msecClock[];
extern unsigned char readClock;

#define GET_MSEC_CLOCK (msecClock[readClock])

#define MSEC_IN_DAY 86400000

typedef void irig_callback_t(void* privateData);

int register_irig_callback(irig_callback_t* func, enum irigClockRates rate,
	void* privateData);

void unregister_irig_callback(irig_callback_t* func, enum irigClockRates rate);

#endif		/* __KERNEL__ */

#endif
