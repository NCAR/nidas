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

typedef unsigned long dsm_sample_time_t;
typedef unsigned short dsm_small_sample_length_t;
typedef unsigned long dsm_large_sample_length_t;

/* a small-ish data sample - one whose data length fits
 * in an unsigned short, ie. less than 65536.
 * 
 * The data member array length is 0, which looks strange.
 * It allows one to create varying length samples.
 * In actual use one will create and use a dsm_small_sample
 * as follows:
    struct dsm_small_sample* samp =
 	kmalloc(sizeof(struct dsm_small_sample) + SPACE_ENOUGH_FOR_DATA,
		GFP_KERNEL);
    ...
    samp->timetag = xxx;
    if (len > SPACE_ENOUGH_FOR_DATA) we_ve_got_trouble();
    samp->length = len;
    memcpy(samp->data,buffer,len);
 */

struct dsm_small_sample {
  dsm_sample_time_t timetag;		/* timetag of sample */
  dsm_small_sample_length_t length;	/* number of bytes in data */
  char data[0];				/* space holder for the data */
};

/* a big honk'in data sample. */
struct dsm_large_sample {
  dsm_sample_time_t timetag;		/* timetag of sample */
  dsm_large_sample_length_t length;	/* number of bytes in data */
  char data[0];				/* space holder for the data */
};

#endif
