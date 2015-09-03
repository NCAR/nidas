/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/* arinc.h

   Header for the CEI420a ARINC driver.

   Original Author: John Wasinger

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
#define ARINC_BIT        _IOW(ARINC_MAGIC,2, short)
#define ARINC_STAT       _IOR(ARINC_MAGIC,3, dsm_arinc_status)
#define ARINC_IOC_MAXNR 3

// These are copied from dsm/modules/CEI420A/Include/utildefs.h
#define AR_ODD                  0  /* Used to set ODD parity                  */
#define AR_EVEN                 1  /* Used to set EVEN parity                 */
#define AR_HIGH                 0  /* Used to set HIGH speed (100 Kbaud)      */
#define AR_LOW                  1  /* Used to set LOW speed (12.5 Kbaud)      */

#endif
