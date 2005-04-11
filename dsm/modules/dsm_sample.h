/*
  *******************************************************************
  Copyright by the National Center for Atmospheric Research
									      
  $LastChangedDate: 2004-10-26 10:33:08 -0600 (Tue, 26 Oct 2004) $
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL: http://orion/svn/hiaper/ads3/dsm/modules/dsm_serial.h $

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

#endif
