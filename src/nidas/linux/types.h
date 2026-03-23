/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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

 C structures defining samples which are sent from Linux modules to user space.

*/

#ifndef NIDAS_LINUX_TYPES_H
#define NIDAS_LINUX_TYPES_H

/* get other types while we're at it. */
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h> // uint32_t, int32_t, int64_t
#endif

/**
 * Depending on the module, either tenths of milliseconds, or
 * milliseconds since 00:00 UTC today. The user-side code
 * that interprets the samples must know the time units.
 */
typedef int dsm_sample_time_t;

/** length of data portion of sample. */
typedef uint32_t dsm_sample_length_t;

/*
 * A data sample as it is passed from kernel-level drivers
 * to user space.
 *
 * The time tag is a 4-byte time, relative to 00:00 UTC, which
 * is converted to an 8-byte absolute time after the sample
 * is read in real-time.
 *
 * The data member array length is 0, allowing one to create
 * varying length samples.
 * In actual use one will create and use a dsm_sample
 * as follows:
 struct dsm_sample* samp =
 kmalloc(SIZEOF_DSM_SAMPLE_HEADER + SPACE_ENOUGH_FOR_DATA,GFP_KERNEL);
 ...
 samp->timetag = xxx;
 if (len > SPACE_ENOUGH_FOR_DATA) we_ve_got_trouble();
 samp->length = len;
 memcpy(samp->data,buffer,len);
 ...
 *
 * When sample is read on the user side:
 struct dsm_sample header;
 read(fd,&header,SIZEOF_DSM_SAMPLE_HEADER);
 char* data = (char*) malloc(header.length);
 * read data portion
 read(fd,data,header.length);
 */

typedef struct dsm_sample {

        /** 4-byte relative timetag of sample */
        dsm_sample_time_t timetag;

        /** number of bytes in data */
        dsm_sample_length_t length;

        /** space holder for the data */
        char data[0];
} dsm_sample_t;

#define SIZEOF_DSM_SAMPLE_HEADER \
        (sizeof(dsm_sample_time_t) + sizeof(dsm_sample_length_t))

#ifndef SECS_PER_HOUR
#define SECS_PER_HOUR 3600
#endif

#ifndef SECS_PER_DAY
#define SECS_PER_DAY (24*SECS_PER_HOUR)
#endif

#ifndef MSECS_PER_SEC
#define MSECS_PER_SEC 1000
#endif

#ifndef USECS_PER_MSEC
#define USECS_PER_MSEC 1000
#endif

#ifndef NSECS_PER_MSEC
#define NSECS_PER_MSEC 1000000LL
#endif

#ifndef NSECS_PER_SEC
#define NSECS_PER_SEC (NSECS_PER_MSEC * MSECS_PER_SEC)
#endif

/* TMSEC is a tenth of a millisecond */
#ifndef NSECS_PER_TMSEC
#define NSECS_PER_TMSEC 100000LL
#endif

#ifndef NSECS_PER_USEC
#define NSECS_PER_USEC 1000
#endif

#ifndef USECS_PER_TMSEC
#define USECS_PER_TMSEC 100
#endif

#ifndef TMSECS_PER_MSEC
#define TMSECS_PER_MSEC 10
#endif

#ifndef USECS_PER_SEC
#define USECS_PER_SEC 1000000LL
#endif

/**
 * Use USECS_PER_SEC_LONG on 32-bit targets to do math with USECS_PER_SEC when
 * the result must fit in a long or long long division must be avoided, like
 * in a kernel module.  It is tempting to define USECS_PER_SEC as a long on
 * 32-bit targets, but then lots of other expressions overflow.  This at least
 * makes it explicit that a more limited type is being used.
 */
#ifndef USECS_PER_SEC_LONG
#define USECS_PER_SEC_LONG 1000000L
#endif

/* Some NIDAS driver modules report time in 1/10 milliseconds */
#ifndef TMSECS_PER_SEC
#define TMSECS_PER_SEC 10000L
#endif

#ifndef MSECS_PER_DAY
#define MSECS_PER_DAY (SECS_PER_DAY * MSECS_PER_SEC)
#define MSECS_PER_HALF_DAY (MSECS_PER_DAY / 2)
#endif

#ifndef TMSECS_PER_DAY
#define TMSECS_PER_DAY (SECS_PER_DAY * TMSECS_PER_SEC)
#define TMSECS_PER_HALF_DAY (TMSECS_PER_DAY / 2)
#endif

#ifndef USECS_PER_HOUR
#define USECS_PER_HOUR (SECS_PER_HOUR * USECS_PER_SEC)
#endif

#ifndef USECS_PER_DAY
#define USECS_PER_DAY (SECS_PER_DAY * USECS_PER_SEC)
#define USECS_PER_HALF_DAY (USECS_PER_DAY / 2)
#endif

/**
 * Macro to create a timespec from milliseconds. The timespec is the whole
 * number of seconds in MSEC, then the nanoseconds fields is the remainder, in
 * nanoseconds.  The NSECS_PER_MSEC is cast to long for 32-bit targets where
 * tv_nsec is a long, since the remainder in nanoseconds can never overlow a
 * long anyway.
 */
#define TIMESPEC_MSEC(MSEC) \
    timespec{ \
        MSEC / MSECS_PER_SEC, \
        (MSEC % MSECS_PER_SEC) * (long)NSECS_PER_MSEC \
    }


#endif
