/* arinc.h

   Header for the CEI420a ARINC driver.

   Original Author: John Wasinger

   Copyright 2005 UCAR, NCAR, All Rights Reserved

   Revisions:

     $LastChangedRevision: 3990 $
         $LastChangedDate: 2007-10-01 23:10:50 -0600 (Mon, 01 Oct 2007) $
           $LastChangedBy: maclean $
                 $HeadURL: http://svn/svn/nidas/trunk/src/nidas/linux/arinc.h $
*/

#ifndef ARINC_H
#define ARINC_H

#include <nidas/linux/types.h>

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

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
 * ARINC label configuration structure
 */
typedef struct {
  short label;
  short rate;
} arcfg_t;

/**
 * ARINC channel configuration structure
 */
typedef struct {
  unsigned int speed;
  unsigned int parity;
} archn_t;

/**
 * ARINC time tagged sample structure
 */
typedef struct {
  unsigned int time;
  int          data;
} tt_data_t;

/**
 * ARINC channel status
 */
typedef struct {
  unsigned int lps_cnt; // Labels Per Second
  unsigned int lps;     // Labels Per Second
  unsigned int pollRate;	// Hz
  unsigned int overflow;
  unsigned int underflow;
  unsigned int nosync;
} dsm_arinc_status;

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define ARINC_MAGIC 'K'

/**
 * The enumeration of IOCTLs that this driver supports.
 */
#define ARINC_SET        _IOW(ARINC_MAGIC,0, arcfg_t)
#define ARINC_OPEN       _IOW(ARINC_MAGIC,1, archn_t)
#define ARINC_CLOSE       _IO(ARINC_MAGIC,2)
#define ARINC_SIM_XMIT    _IO(ARINC_MAGIC,3)
#define ARINC_BIT        _IOW(ARINC_MAGIC,4, short)
#define ARINC_STAT       _IOR(ARINC_MAGIC,5, dsm_arinc_status)
#define ARINC_MEASURE     _IO(ARINC_MAGIC,6)
#define ARINC_IOC_MAXNR 6

// These are copied from dsm/modules/CEI420A/Include/utildefs.h
#define AR_ODD                  0  /* Used to set ODD parity                  */
#define AR_EVEN                 1  /* Used to set EVEN parity                 */
#define AR_HIGH                 0  /* Used to set HIGH speed (100 Kbaud)      */
#define AR_LOW                  1  /* Used to set LOW speed (12.5 Kbaud)      */

#endif
