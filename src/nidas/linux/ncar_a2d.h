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
/* ncar_a2d.h

*/

/* 
 * This header is shared from user-side code that wants to get the
 * values of the ioctl commands.
 */

#ifndef NCAR_A2D_H
#define NCAR_A2D_H

#include "types.h"              // get nidas typedefs
#include "a2d.h"

/* 
 * User programs need these for the _IO macros, but kernel modules get
 * theirs elsewhere.
 */
#ifndef __KERNEL__
#  include <sys/ioctl.h>
#  include <sys/types.h>
#endif

/*
 * Board temperature samples will have this index value.
 */
#define NCAR_A2D_TEMPERATURE_INDEX 255

#define NUM_NCAR_A2D_CHANNELS         8       // Number of A/D's per card

/*
 * A/D filter configuration
 */
#define CONFBLOCKS      12  // 12 blocks as described below
#define CONFBLLEN       43  // 42 data words plus 1 CRCC

/**
 * Data used to configure on-chip filters.
 */
struct ncar_a2d_ocfilter_config
{
    unsigned short filter[CONFBLOCKS*CONFBLLEN+1]; // Filter data
};

/* A2D status info */
struct ncar_a2d_status
{
    // fifoLevel indices 0-5 correspond to:
    //  0: empty
    //  1: <= 1/4 full
    //  2: < 1/2 full
    //  3: < 3/4 full
    //  4: < full
    //  5: full
    unsigned int preFifoLevel[6];   // counters for fifo level, pre-read

    unsigned short goodval[NUM_NCAR_A2D_CHANNELS];  // value of last good status word

    unsigned short    ser_num;        // A/D card serial number

    unsigned int skippedSamples;    // discarded samples because of slow RTL fifo
    int resets;               // number of board resets since last open

    // following members are used only by RTLinux version
    unsigned int postFifoLevel[6];  // counters for fifo level, post-read
    unsigned int nbad[NUM_NCAR_A2D_CHANNELS];     // number of bad status words in last 100 scans
    unsigned short badval[NUM_NCAR_A2D_CHANNELS];   // value of last bad status word
    unsigned int nbadFifoLevel;     // #times hw fifo not at expected level pre-read
    unsigned int fifoNotEmpty;      // #times hw fifo not empty post-read

};

/* This structure is used to copy a brief description of the current board
 * configuration back to user space via the NCAR_A2D_GET_SETUP ioctl.
 */
struct ncar_a2d_setup
{
    int   gain[NUM_NCAR_A2D_CHANNELS];  // gain settings
    int offset[NUM_NCAR_A2D_CHANNELS];  // Offset flags
    int calset[NUM_NCAR_A2D_CHANNELS];  // cal voltage channels
    int vcal;                           // cal voltage
};

/* Calibration structure
 */
struct ncar_a2d_cal_config
{
    int calset[NUM_NCAR_A2D_CHANNELS];  // channels
    int state;                          // off: 0   on: 1
    int vcal;                           // voltage
};

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define A2D_MAGIC 'A'

/*
 * IOCTLs that this driver supports.
 */
#define  NCAR_A2D_GET_STATUS        _IOR(A2D_MAGIC,  0, struct ncar_a2d_status)
#define  NCAR_A2D_SET_OCFILTER      _IOW(A2D_MAGIC,  1, struct ncar_a2d_ocfilter_config)
#define  NCAR_A2D_GET_SETUP         _IOR(A2D_MAGIC,  2, struct ncar_a2d_setup)
#define  NCAR_A2D_SET_CAL           _IOW(A2D_MAGIC,  3, struct ncar_a2d_cal_config)
#define  NCAR_A2D_RUN                _IO(A2D_MAGIC,  4)
#define  NCAR_A2D_STOP               _IO(A2D_MAGIC,  5)
#define  NCAR_A2D_GET_TEMP          _IOR(A2D_MAGIC,  6, short)
#define  NCAR_A2D_SET_TEMPRATE      _IOW(A2D_MAGIC,  7, int)

#endif
