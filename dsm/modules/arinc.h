/* cei420a.h

   Header for the CEI420a ARINC driver.

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research

   Revisions:

     $LastChangedRevision: $
         $LastChangedDate: $
           $LastChangedBy: $
                 $HeadURL: $
*/

#ifndef ARINC_DRIVER_H
#define ARINC_DRIVER_H

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

/* The CEI420a supports up to 512 labels per buffer for a given channel. */
#define LPB         512

typedef unsigned long dsm_sample_timetag_t;  // TODO - remove duplicate typedef

struct dsm_arinc_sample {
  dsm_sample_timetag_t timetag; /* timetag of sample */
  unsigned long length;         /* number of bytes in data */
  unsigned long data[LPB];      /* the data */
};

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define ARINC_MAGIC 'K'

/* number of transmitters and receivers for the CEI-420A-42 (-42 = -RT) */
#define N_ARINC_RX 4
#define N_ARINC_TX 2

/* Structures that are passed via ioctls to/from this driver */
struct arinc_set {
  int channel;
  int label;
  int rate;
};

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define ARINC_SET       _IOW(ARINC_MAGIC,0,struct arinc_set)
#define ARINC_GO         _IO(ARINC_MAGIC,1)
#define ARINC_RESET      _IO(ARINC_MAGIC,2)
#define ARINC_STAT       _IO(ARINC_MAGIC,3)

#include <ioctl_fifo.h>

#ifdef __RTCORE_KERNEL__

#endif

#endif
