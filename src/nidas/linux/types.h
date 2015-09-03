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
#include <sys/types.h>
#include <stdint.h>
#endif

/**
 * Depending on the module, either tenths of milliseconds, or
 * milliseconds since 00:00 UTC today. The user-side code
 * that interprets the samples must know the time units.
 */
typedef int dsm_sample_time_t;

/** length of data portion of sample. */
typedef unsigned int dsm_sample_length_t;

/*
 * A data sample as it is passed from kernel-level drivers
 * to user space.
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

        /** timetag of sample */
        dsm_sample_time_t timetag;

        /** number of bytes in data */
        dsm_sample_length_t length;

        /** space holder for the data */
        char data[0];
} dsm_sample_t;

#define SIZEOF_DSM_SAMPLE_HEADER \
        (sizeof(dsm_sample_time_t) + sizeof(dsm_sample_length_t))

#ifndef NSECS_PER_SEC
#define NSECS_PER_SEC 1000000000
#endif

#ifndef NSECS_PER_MSEC
#define NSECS_PER_MSEC 1000000
#endif

#ifndef NSECS_PER_USEC
#define NSECS_PER_USEC 1000
#endif

#ifndef USECS_PER_MSEC
#define USECS_PER_MSEC 1000
#endif

/* TMSEC is a tenth of a millisecond */
#ifndef USECS_PER_TMSEC
#define USECS_PER_TMSEC 100
#endif

#ifndef TMSECS_PER_MSEC
#define TMSECS_PER_MSEC 10
#endif

#ifndef USECS_PER_SEC
#define USECS_PER_SEC 1000000
#endif

#ifndef MSECS_PER_SEC
#define MSECS_PER_SEC 1000
#endif

#ifndef TMSECS_PER_SEC
#define TMSECS_PER_SEC 10000
#endif

#ifndef MSECS_PER_DAY
#define MSECS_PER_DAY 86400000
#endif

#ifndef TMSECS_PER_DAY
#define TMSECS_PER_DAY 864000000
#endif

#ifndef SECS_PER_DAY
#define SECS_PER_DAY 86400
#endif

#endif
