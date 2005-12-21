/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate$
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL$

  *******************************************************************

   structures defining samples which are sent from RT-Linux
   modules to user space.

*/

#ifndef DSM_SAMPLE_H
#define DSM_SAMPLE_H

/** Milliseconds since 00:00 UTC today */
typedef unsigned long dsm_sample_time_t;

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

struct dsm_sample {
  dsm_sample_time_t timetag;		/* timetag of sample */
  dsm_sample_length_t length;		/* number of bytes in data */
  char data[0];				/* space holder for the data */
};

#define SIZEOF_DSM_SAMPLE_HEADER \
	(sizeof(dsm_sample_time_t) + sizeof(dsm_sample_length_t))

#ifdef __RTCORE_KERNEL__
#include <linux/circ_buf.h>

/* Macros for manipulating sample circular buffers
 * (in addition to those in linux/circ_buf.h
 */
#define GET_HEAD(circbuf,size) \
    ((CIRC_SPACE(circbuf.head,circbuf.tail,size) > 0) ? \
        circbuf.buf[circbuf.head] : 0)

#define INCREMENT_HEAD(circbuf,size) \
        (circbuf.head = (circbuf.head + 1) & (size-1))

#define INCREMENT_TAIL(circbuf,size) \
        (circbuf.tail = (circbuf.tail + 1) & (size-1))

#define NEXT_HEAD(circbuf,size) \
        (INCREMENT_HEAD(circbuf,size), GET_HEAD(circbuf,size))

/* circular buffer of samples, compatible with macros in
   linux/circ_buf.h
*/
struct dsm_sample_circ_buf {
    struct dsm_sample **buf;
    int head;
    int tail;
};

#endif

#endif
