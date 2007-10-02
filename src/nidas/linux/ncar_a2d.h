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
// #include <nidas/linux/filters/short_filters.h>

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
#define NCAR_A2D_TEMPERATURE_INDEX 999

#define NUM_NCAR_A2D_CHANNELS         8       // Number of A/D's per card
#define A2DGAIN_MUL     9       // multiplies GainCode
#define A2DGAIN_DIV     10      // divides GainCode

/*
 * Frequency for getting data from the card's FIFO (Hz)
 */
#define A2D_POLL_RATE  100

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
#define A2D_GET_STATUS _IOR(A2D_MAGIC, 0, A2D_STATUS)
#define A2D_SET_CONFIG _IOW(A2D_MAGIC, 1, A2D_SET)
#define A2D_SET_CAL    _IOW(A2D_MAGIC, 2, A2D_CAL)
#define A2D_RUN        _IO(A2D_MAGIC, 3)
#define A2D_STOP       _IO(A2D_MAGIC, 4)
#define A2DTEMP_GET_TEMP _IOR(A2D_MAGIC, 5, short)
#define A2DTEMP_SET_RATE _IOW(A2D_MAGIC, 6, int)
/* obsolete RTLinux ioctls (reference by unused open method in 
 * raf/A2DBoardTempSensor */
#define A2DTEMP_OPEN   _IOW(A2D_MAGIC, 7, int)
#define A2DTEMP_CLOSE  _IO(A2D_MAGIC, 8)


/*
 * A/D filter configuration
 */
#define CONFBLOCKS      12  // 12 blocks as described below
#define CONFBLLEN       43  // 42 data words plus 1 CRCC

typedef struct
{
    int  gain[NUM_NCAR_A2D_CHANNELS];    // Gain settings
    int  gainMul[NUM_NCAR_A2D_CHANNELS]; // Gain Code multiplier
    int  gainDiv[NUM_NCAR_A2D_CHANNELS]; // Gain Code divider
    // Sample rate in Hz. 0 is off. Not used in new version
    int  Hz[NUM_NCAR_A2D_CHANNELS];
    int  offset[NUM_NCAR_A2D_CHANNELS];  // Offset flags
    int  sampleIndex[NUM_NCAR_A2D_CHANNELS];  // index of corresponding sample
    int latencyUsecs;     // buffer latency in micro-seconds (UNUSED)
    unsigned short filter[CONFBLOCKS*CONFBLLEN+1]; // Filter data
    int scanRate;                       // how fast to sample
} A2D_SET;

/* A2D status info */
typedef struct
{
    // fifoLevel indices 0-5 correspond to:
    // 	0: empty
    //  1: <= 1/4 full
    //  2: < 1/2 full
    //  3: < 3/4 full
    //  4: < full
    //  5: full
    size_t preFifoLevel[6];   // counters for fifo level, pre-read
    size_t postFifoLevel[6];  // counters for fifo level, post-read
    size_t nbad[NUM_NCAR_A2D_CHANNELS];     // number of bad status words in last 100 scans
    unsigned short badval[NUM_NCAR_A2D_CHANNELS];   // value of last bad status word
    unsigned short goodval[NUM_NCAR_A2D_CHANNELS];  // value of last good status word
    unsigned short    ser_num;        // A/D card serial number
    size_t nbadFifoLevel;     // #times hw fifo not at expected level pre-read
    size_t fifoNotEmpty;      // #times hw fifo not empty post-read
    size_t skippedSamples;    // discarded samples because of slow RTL fifo
    int resets;               // number of board resets since last open
} A2D_STATUS;

/*
 * Calibration structure
 */
typedef struct
{
    int  calset[NUM_NCAR_A2D_CHANNELS];  // Calibration flags
    int  vcalx8;           // Calibration voltage:
    // 128=0, 48=-10, 208 = +10, .125 V/bit
} A2D_CAL;

#endif
