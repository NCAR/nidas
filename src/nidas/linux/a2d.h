/* nidas_analog.h

   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   Common ioctl definitions for analog boards.

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

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

struct nidas_a2d_config
{
        int gain[MAX_A2D_CHANNELS];     // gain setting for the channel
        int bipolar[MAX_A2D_CHANNELS];// 1=bipolar,0=unipolar
        // sample index, 0,1, etc of each channel variable, -1 if not used
        int sampleIndex[MAX_A2D_CHANNELS];
        long latencyUsecs;                  // buffer latency in micro-sec
        int scanRate;                       // how fast to sample
};

struct nidas_a2d_filter_config
{
        int filterType;     // one of nidas_short_filter enum
        int rate;           // output rate
        int boxcarNpts;     // number of pts in boxcar avg
        short index;        // sample index, 0,1,...
};

struct nidas_a2dx_config
{
        long latencyUsecs;                  // buffer latency in micro-sec
        int scanRate;                       // how fast to sample
};

struct nidas_a2d_sample_config
{
        int sindex;         // sample index, 0,1,etc
        int nvars;          // number of variables in sample
        int rate;           // sample rate
        int filterType;     // one of nidas_short_filter enum
        int boxcarNpts;     // number of pts in boxcar avg
        int channels[MAX_A2D_CHANNELS];  // which channel for each variable
        int gain[MAX_A2D_CHANNELS];     // gain setting for the channel
        int bipolar[MAX_A2D_CHANNELS];// 1=bipolar,0=unipolar
};

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define NIDAS_A2D_IOC_MAGIC 'n'

/*
 * The enumeration of IOCTLs that this driver supports.
 * See pages 130-132 of Linux Device Driver's Manual 
 */

/** A2D Ioctls */
#define NIDAS_A2D_GET_NCHAN  _IOR(NIDAS_A2D_IOC_MAGIC,0,int)
#define NIDAS_A2D_SET_CONFIG \
    _IOW(NIDAS_A2D_IOC_MAGIC,1,struct nidas_a2d_config)
#define NIDAS_A2D_ADD_FILTER \
    _IOW(NIDAS_A2D_IOC_MAGIC,2,struct nidas_a2d_filter_config)
#define NIDAS_A2D_CONFIG_SAMPLE \
    _IOW(NIDAS_A2D_IOC_MAGIC,3,struct nidas_a2d_sample_config)

#ifdef __KERNEL__

/**
 * Structure describing a filter and which channels it is
 * to be used for.
 */
struct a2d_filter_info {
        int nchans;
        int* channels;
        int decimate;
        enum nidas_short_filter filterType;
        shortfilt_init_method finit;        // filter methods
        shortfilt_config_method fconfig;
        shortfilt_filter_method filter;
        shortfilt_cleanup_method fcleanup;
        void* filterObj;                    // pointer to filter's private data
        short index;           // sample index to put as first 16 bit word of data
};

#endif

#endif
