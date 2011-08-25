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
#include <nidas/util/IOException.h>
#include <nidas/util/time_constants.h>

namespace nidas { namespace core {

/** Microseconds since Jan 1 1970, 00:00 UTC */
typedef long long dsm_time_t;

/**
 * Return the current unix system time, in microseconds 
 * since Jan 1, 1970, 00:00 GMT
 */
inline dsm_time_t getSystemTime() {
    struct timeval tval;
    if (::gettimeofday(&tval,0) < 0) return 0;   // shouldn't happen
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

/**
 * Utility function, sleeps until the next even period + offset.
 * Returns true if interrupted.
 */
bool sleepUntil(unsigned int periodMsec,unsigned int offsetMsec=0)
    throw(nidas::util::IOException);

}}	// namespace nidas namespace core

#endif
