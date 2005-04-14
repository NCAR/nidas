/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

 Some routines for getting the current system time, accurate
 to milliseconds.

*/


#ifndef DSMTIME_H
#define DSMTIME_H

#include <sys/time.h>

// #include <dsm_sample.h>

#define CLOCK_SAMPLE_ID 1

#ifndef MSECS_PER_DAY
#define MSECS_PER_DAY 86400000
#endif

namespace dsm {

/** Milliseconds since Jan 1 1970, 00:00 UTC */
typedef long long dsm_sys_time_t;

/**
 * Return the current unix system time, in milliseconds.
 */
inline dsm_sys_time_t getCurrentTimeInMillis() {
    struct timeval tval;
    if (::gettimeofday(&tval,0) < 0) return 0L;   // shouldn't happen
    return (dsm_sys_time_t)(tval.tv_sec) * 1000 +
    	(tval.tv_usec + 500) / 1000;
}

/**
 * Return smallest dsm_sys_time_t that is an integral multiple of
 * delta, that isn't less than or equal to argument t.
 * Similar to to ceil() math function, except ceil() finds value
 * that isn't less than argument, not less-than-or-equal, i.e.
 * this function always returns a value greater than the arg.
 */
inline dsm_sys_time_t timeCeiling(dsm_sys_time_t t,unsigned long delta) {
    return ((t / delta) + 1) * delta;
}
/**
 * Return largest dsm_sys_time_t that is an integral multiple of
 * delta, that isn't greater than argument t.  Analogous to floor()
 * math function.
 */
inline dsm_sys_time_t timeFloor(dsm_sys_time_t t,unsigned long delta) {
    return (t / delta) * delta;
}

}

#endif
