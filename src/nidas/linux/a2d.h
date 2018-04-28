/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
/* nidas_analog.h

   Common ioctl definitions for analog boards.

   Original Author: Gordon Maclean

*/

#ifndef NIDAS_LINUX_A2D_FILTERS_H
#define NIDAS_LINUX_A2D_FILTERS_H

#include <nidas/linux/filters/short_filters.h>

#ifndef __KERNEL__
/* User programs need this for the _IO macros, but kernel
 * modules get their's elsewhere.
 */
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

/* This header is also included from user-side code that
 * wants to get the values of the ioctl commands, and
 * the definition of the structures.
 */

/*
 * Set this to the maximum number of A2D channels on any
 * board that uses this header file.
 * The diamond MM32XAT has 32 channels, so it should be at least 32.
 */
#define	MAX_A2D_CHANNELS 32	// max num A/D channels per card

/**
 * Fields common to all A2D configurations.
 */
struct nidas_a2d_config
{
        int scanRate;                       // how fast to sample
        int latencyUsecs;                  // buffer latency in micro-sec
};

/**
 * Information for configuring a sample from an A2D.
 */
struct nidas_a2d_sample_config
{
        int sindex;         // sample index, 0,1,etc
        int nvars;          // number of variables in sample
        int rate;           // sample rate
        int filterType;     // one of nidas_short_filter enum
        int channels[MAX_A2D_CHANNELS];  // which channel for each variable
        int gain[MAX_A2D_CHANNELS];     // gain setting for the channel
        int bipolar[MAX_A2D_CHANNELS];// 1=bipolar,0=unipolar
        int nFilterData;        // number of bytes in filterData;
        char filterData[0];     // data for filter
};

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define NIDAS_A2D_IOC_MAGIC 'n'

/*
 * IOCTLS that are supported on all A2D cards.
 */
#define NIDAS_A2D_GET_NCHAN  _IOR(NIDAS_A2D_IOC_MAGIC,0,int)
#define NIDAS_A2D_SET_CONFIG \
    _IOW(NIDAS_A2D_IOC_MAGIC,1,struct nidas_a2d_config)
#define NIDAS_A2D_CONFIG_SAMPLE \
    _IOW(NIDAS_A2D_IOC_MAGIC,2,struct nidas_a2d_sample_config)

#endif
