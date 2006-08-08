/* xxxx_driver.h

   Time-stamp: <Wed 13-Apr-2005 05:52:09 pm>

   Header for test rtl driver.

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

#ifndef XXXX_DRIVER_H
#define XXXX_DRIVER_H

#include <nidas/core/dsm_sample.h>              // get dsm_sample typedefs

#ifndef __RTCORE_KERNEL__
/* User programs need this for the _IO macros, but kernel
 * modules get their's elsewhere.
 */
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define XXXX_MAGIC	'X'


/* sample structures that are passed via ioctls to/from this driver */
struct xxxx_get {
  char c[10];
};

struct xxxx_set {
  char c[20];
};

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define XXXX_GET_IOCTL _IOR(XXXX_MAGIC,0,struct xxxx_get)
#define XXXX_SET_IOCTL _IOW(XXXX_MAGIC,1,struct xxxx_set)

#ifdef __RTCORE_KERNEL__

#endif

#endif
