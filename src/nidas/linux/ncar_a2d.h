/* ncar_a2d.h

$LastChangedRevision: 3648 $
$LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $
$LastChangedBy: cjw $
$HeadURL: http://svn.atd.ucar.edu/svn/nids/trunk/src/nidas/rtlinux/ncar_a2d.h $

Copyright 2005 UCAR, NCAR, All Rights Reserved

*/

/* 
 * This header is shared from user-side code that wants to get the
 * values of the ioctl commands.
 */

#ifndef NCAR_A2D_H
#define NCAR_A2D_H

#include <nidas/linux/types.h>              // get nidas typedefs
#include <nidas/linux/a2d.h>

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
