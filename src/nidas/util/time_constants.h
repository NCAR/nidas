// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/*

    Macro values for time conversions.

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
