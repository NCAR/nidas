/* arinc.h

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

#include <dsm_sample.h>

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
#define ARINC_MAGIC 'K'

/**
 * The CEI420a supports up to 512 labels per buffer
 * for a given channel (265 when timetaged).
 */
#define LPB         256

/**
 * number of transmitters and receivers for the
 * CEI-420A-42 (-42 = -RT)
 */
#define N_ARINC_RX 4
#define N_ARINC_TX 2

/**
 * ARINC configuration structure
 */
typedef struct {
  short label;
  short rate;
} arcfg_t;

/**
 * ARINC time tagged sample structure
 */
typedef struct {
  unsigned long time;
  unsigned long data;
} tt_data_t;

/**
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */
#define ARINC_SET       _IOW(ARINC_MAGIC,0, arcfg_t)  // channel
#define ARINC_GO         _IO(ARINC_MAGIC,1)           // channel
#define ARINC_RESET      _IO(ARINC_MAGIC,2)           // channel
#define ARINC_SIM_XMIT   _IO(ARINC_MAGIC,3)           // channel
#define ARINC_BIT       _IOW(ARINC_MAGIC,4, short)    // channel
#define ARINC_STAT       _IO(ARINC_MAGIC,5)           // channel
#define ARINC_MEASURE    _IO(ARINC_MAGIC,6)           // channel

#include <ioctl_fifo.h>

#ifdef __RTCORE_KERNEL__

#include <utildefs.h>

#endif

#endif
