/* xxxx_driver.h

   Time-stamp: <Thu 26-Aug-2004 06:48:04 pm>

   Header for test rtl driver.

   Original Author: Gordon Maclean

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

*/

#ifndef XXXX_DRIVER_H
#define XXXX_DRIVER_H

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define XXXX_MAGIC 'X'


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

#include <ioctl_fifo.h>

#ifdef __KERNEL__

#endif

#endif
