/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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
/* lams.h

   Header for the NCAR/EOL Laser Air Motion Sensor (LAMS) interface.
   Original Author: Mike Spowart
   Copyright by the National Center for Atmospheric Research 2004
 */

#ifndef NIDAS_LAMS_LAMSX_H
#define NIDAS_LAMS_LAMSX_H

#include <nidas/linux/types.h>

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */
#define LAMS_SPECTRA_SIZE  512 

/**
 * Enumeration of LAMS sample types.
 */
#define LAMS_SPECAVG_SAMPLE_TYPE  0
#define LAMS_SPECPEAK_SAMPLE_TYPE  1

//#define LAMS_PATTERN             0x5555
//#define NUM_ARRAYS               128
//#define N_LAMS                   3

#ifdef __KERNEL__
struct lams_avg_sample {
       dsm_sample_time_t timetag;       // timetag of sample
       dsm_sample_length_t length;      // number of bytes in data
       unsigned int type;               // sample type
       unsigned int data[LAMS_SPECTRA_SIZE];   // the averages
};
struct lams_peak_sample {
       dsm_sample_time_t timetag;       // timetag of sample
       dsm_sample_length_t length;      // number of bytes in data
       unsigned int type;               // sample type
       unsigned short data[LAMS_SPECTRA_SIZE]; // the peaks
};
#else
// For user-space programs, the data portion of a spectral average sample
struct lams_avg_sample {
       unsigned int type;
       unsigned int data[LAMS_SPECTRA_SIZE];   // the averages
};

// For user-space programs, the data portion of a spectral peak sample
struct lams_peak_sample {
       unsigned int type;
       unsigned short data[LAMS_SPECTRA_SIZE]; // the peaks
};
#endif


struct lams_status {
        unsigned int missedISRSamples;
        unsigned int missedOutSamples;
};

/** not needed in new driver. */
struct lams_set {
        int channel;
};

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * a ioctl to the wrong device.
 */
#define LAMS_MAGIC              'L'

// The enumeration of IOCTLs that this driver supports.
#define LAMS_SET_CHN     _IOW(LAMS_MAGIC,0, struct lams_set)    // not needed in new driver
#define LAMS_N_AVG       _IOW(LAMS_MAGIC,1, unsigned int)       // put navg and npeak in a struct
#define LAMS_N_PEAKS     _IOW(LAMS_MAGIC,2, unsigned int)
#define LAMS_N_SKIP      _IOW(LAMS_MAGIC,3, unsigned int)
#define LAMS_GET_STATUS  _IOR(LAMS_MAGIC,4, struct lams_status)
#define LAMS_TAS_BELOW   _IO(LAMS_MAGIC,5)
#define LAMS_TAS_ABOVE   _IO(LAMS_MAGIC,6)
#define LAMS_IOC_MAXNR  6

#endif // NIDAS_LAMS_LAMSX_H
