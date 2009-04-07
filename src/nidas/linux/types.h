/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate$
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL$

  *******************************************************************

   C structures defining samples which are sent from RT-Linux
   modules to user space.

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

/** tenths of milliseconds since 00:00 UTC today */
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
    memcpy(user_buffer,samp,SIZEOF_DSM_SAMPLE_HEADER + samp->length)
 */

typedef struct dsm_sample {
  dsm_sample_time_t timetag;		/* timetag of sample */
  dsm_sample_length_t length;		/* number of bytes in data */
  char data[0];				/* space holder for the data */
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
#define MSECS_PER_DAY 86400000L
#endif

#ifndef TMSECS_PER_DAY
#define TMSECS_PER_DAY 864000000L
#endif

#ifndef SECS_PER_DAY
#define SECS_PER_DAY 86400
#endif

#endif
