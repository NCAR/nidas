/* Pc104sg.h
   Class for interfacing the PC104-SG time and frequency processor.

   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research

   Revisions:

*/

#ifndef IRIGCLOCK_H
#define IRIGCLOCK_H

extern unsigned long msecClock[];
extern unsigned char readClock;

#define GET_MSEC_CLOCK (msecClock[readClock])

#define MSEC_IN_DAY 86400000

enum irigClockRates { IRIG_1_HZ, IRIG_5_HZ, IRIG_10_HZ, IRIG_25_HZ,
	IRIG_50_HZ, IRIG_100_HZ, IRIG_NUM_RATES };

typedef void irig_callback_t(void* privateData);

void register_irig_callback(irig_callback_t* func, enum irigClockRates rate,
	void* privateData);

void unregister_irig_callback(irig_callback_t* func, enum irigClockRates rate);

#endif
