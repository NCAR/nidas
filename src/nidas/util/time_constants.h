// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    Macro values for time conversions.

 ********************************************************************
*/

#ifndef NIDAS_UTIL_TIMECONSTANTS_H
#define NIDAS_UTIL_TIMECONSTANTS_H

#ifndef SECS_PER_HOUR
#define SECS_PER_HOUR 3600
#endif

#ifndef SECS_PER_DAY
#define SECS_PER_DAY 86400
#endif

#ifndef MSECS_PER_SEC
#define MSECS_PER_SEC 1000
#endif

#ifndef MSECS_PER_DAY
#define MSECS_PER_DAY 86400000
#define MSECS_PER_HALF_DAY 43200000
#endif

/* Some NIDAS driver modules report time in 1/10 milliseconds */
#ifndef TMSECS_PER_SEC
#define TMSECS_PER_SEC 10000
#endif

#ifndef TMSECS_PER_DAY
#define TMSECS_PER_DAY 864000000
#define TMSECS_PER_HALF_DAY 432000000
#endif

#ifndef USECS_PER_TMSEC
#define USECS_PER_TMSEC 100
#endif

#ifndef USECS_PER_MSEC
#define USECS_PER_MSEC 1000
#endif

#ifndef USECS_PER_SEC
#define USECS_PER_SEC 1000000
#endif

#ifndef USECS_PER_HOUR
#define USECS_PER_HOUR 3600000000LL
#endif

#ifndef USECS_PER_DAY
#define USECS_PER_DAY 86400000000LL
#define USECS_PER_HALF_DAY 43200000000LL
#endif

#ifndef NSECS_PER_USEC
#define NSECS_PER_USEC 1000
#endif

#ifndef NSECS_PER_TMSEC
#define NSECS_PER_TMSEC 100000
#endif

#ifndef NSECS_PER_MSEC
#define NSECS_PER_MSEC 1000000
#endif

#ifndef NSECS_PER_SEC
#define NSECS_PER_SEC 1000000000
#endif

#endif
