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

#ifndef DSM_SAMPLE_H
#define DSM_SAMPLE_H

/** tenths of milliseconds since 00:00 UTC today */
typedef long dsm_sample_time_t;

/** length of data portion of sample. */
typedef unsigned long dsm_sample_length_t;

/*
 * A data sample as it is passed from kernel-level drivers
 * to user space.
 *
 * The data member array length is 0, allowing one to create
 * varying length samples.
 * In actual use one will create and use a dsm_sample
 * as follows:
    struct dsm_sample* samp =
 	rtl_gpos_malloc(SIZEOF_DSM_SAMPLE_HEADER + SPACE_ENOUGH_FOR_DATA);
    ...
    samp->timetag = xxx;
    if (len > SPACE_ENOUGH_FOR_DATA) we_ve_got_trouble();
    samp->length = len;
    memcpy(samp->data,buffer,len);
    ...

    rtl_write(fifofd,samp,SIZEOF_DSM_SAMPLE_HEADER + len);
 */

typedef struct dsm_sample {
  dsm_sample_time_t timetag;		/* timetag of sample */
  dsm_sample_length_t length;		/* number of bytes in data */
  char data[0];				/* space holder for the data */
} dsm_sample_t;

#define SIZEOF_DSM_SAMPLE_HEADER \
	(sizeof(dsm_sample_time_t) + sizeof(dsm_sample_length_t))

#if defined(__RTCORE_KERNEL__) || defined(__KERNEL__)
#include <linux/circ_buf.h>

/* Macros for manipulating sample circular buffers
 * (in addition to those in linux/circ_buf.h */
 
/*
 * GET_HEAD accesses the head index twice.  The idea is that
 * the space will only stay the same or get bigger between
 * the CIRC_SPACE check and the return of buf[head] element,
 * because only the calling thread should be changing head.
 * If a separate consumer thread is messing with tail, CIRC_SPACE,
 * will only stay the same or get bigger, not smaller, and
 * and the head element will not become invalid.
 */
#define GET_HEAD(circbuf,size) \
    ((CIRC_SPACE(circbuf.head,circbuf.tail,size) > 0) ? \
        circbuf.buf[circbuf.head] : 0)

/*
 * use barrier() to disable optimizations so that head is always OK,
 * just in case the compiler would build code like the following:
 *  head = head + 1;
 *  head = head & (size -1);
 * which leaves head in a bad state for a moment.
 */
#define INCREMENT_HEAD(circbuf,size) \
        ({\
            int tmp = (circbuf.head + 1) & ((size) - 1);\
            barrier();\
            circbuf.head = tmp;\
        })

/*
 * use barrier() to disable optimizations so that tail is always OK.
 */
#define INCREMENT_TAIL(circbuf,size) \
        ({\
            int tmp = (circbuf.tail + 1) & ((size) - 1);\
            barrier();\
            circbuf.tail = tmp;\
        })

#define NEXT_HEAD(circbuf,size) \
        (INCREMENT_HEAD(circbuf,size), GET_HEAD(circbuf,size))

/* circular buffer of samples, compatible with macros in
   linux/circ_buf.h
*/
struct dsm_sample_circ_buf {
    struct dsm_sample **buf;
    volatile int head;
    volatile int tail;
};

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

/*
 * Return time in milliseconds since 00:00 UTC.
 */
inline dsm_sample_time_t getSystemTimeMsecs(void)
{
    struct timeval tv;
    do_gettimeofday(&tv);
    return (tv.tv_sec % 86400) * MSECS_PER_SEC +
        tv.tv_usec / USECS_PER_MSEC;
}


/*
 * Return time in tenths of milliseconds since 00:00 UTC.
 */
inline dsm_sample_time_t getSystemTimeTMsecs(void)
{
    struct timeval tv;
    do_gettimeofday(&tv);
    return (tv.tv_sec % 86400) * TMSECS_PER_SEC +
        tv.tv_usec / USECS_PER_TMSEC;
}

#endif

#endif
