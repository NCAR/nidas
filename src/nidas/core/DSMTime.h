/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

 Some routines for getting the current system time, with 
 microsecond precision, but not microsecond accuracy.
*/


#ifndef NIDAS_CORE_DSMTIME_H
#define NIDAS_CORE_DSMTIME_H

#include <sys/time.h>

#include <nidas/core/Sample.h>

#ifndef SECS_PER_DAY
#define SECS_PER_DAY 86400
#endif

#ifndef MSECS_PER_DAY
#define MSECS_PER_DAY 86400000
#define MSECS_PER_HALF_DAY 43200000
#endif

#ifndef USECS_PER_DAY
#define USECS_PER_DAY 86400000000LL
#define USECS_PER_HALF_DAY 43200000000LL
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

#ifndef NSECS_PER_USEC
#define NSECS_PER_USEC 1000
#endif

#ifndef NSECS_PER_MSEC
#define NSECS_PER_MSEC 1000000
#endif


namespace nidas { namespace core {

/**
 * Return the current unix system time, in microseconds 
 * since Jan 1, 1970, 00:00 GMT
 */
inline dsm_time_t getSystemTime() {
    struct timeval tval;
    if (::gettimeofday(&tval,0) < 0) return 0L;   // shouldn't happen
    return (dsm_time_t)(tval.tv_sec) * USECS_PER_SEC + tval.tv_usec;
}

/**
 * Return smallest dsm_time_t that is an integral multiple of
 * delta, that isn't less than or equal to argument t.
 * Similar to to ceil() math function, except ceil() finds value
 * that isn't less than argument, not less-than-or-equal, i.e.
 * this function always returns a value greater than the arg.
 */
inline dsm_time_t timeCeiling(dsm_time_t t,long long delta) {
    return ((t / delta) + 1) * delta;
}

/**
 * Return largest dsm_time_t that is an integral multiple of
 * delta, that isn't greater than argument t.  Analogous to floor()
 * math function.
 */
inline dsm_time_t timeFloor(dsm_time_t t,long long delta) {
    return (t / delta) * delta;
}

}}	// namespace nidas namespace core

#endif
