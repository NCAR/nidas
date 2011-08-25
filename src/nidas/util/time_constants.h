/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2011-03-16 13:27:19 -0600 (Wed, 16 Mar 2011) $

    $LastChangedRevision: 6037 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/util/UTime.h $

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
