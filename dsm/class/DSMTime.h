/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

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
#include <math.h>

namespace dsm {

#ifdef TIME_IS_LONG_LONG
  typedef long long dsm_sys_time_t;
#else
  typedef double dsm_sys_time_t;
#endif

#ifdef TIME_IS_LONG_LONG
  /**
   * Return the current unix system time, in milliseconds.
   */
  static dsm_sys_time_t getCurrentTimeInMillis() {
    struct timeval tval;
    if (::gettimeofday(&tval,0) < 0) return 0L;   // shouldn't happen
    return (dsm_sys_time_t)(tval.tv_sec) * 1000 +
    	(tval.tv_usec + 500) / 1000;
  }
#else
  /**
   * Return the current unix system time, in milliseconds.
   */
  static dsm_sys_time_t getCurrentTimeInMillis() {
    struct timeval tval;
    if (::gettimeofday(&tval,0) < 0) return 0.0;   // shouldn't happen
    return (dsm_sys_time_t)tval.tv_sec * 1000.0 + tval.tv_usec / 1000.0;
  }
#endif


#endif
