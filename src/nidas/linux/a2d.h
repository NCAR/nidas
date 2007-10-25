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

/**
 * Fields common to all A2D configurations.
 */
struct nidas_a2d_config
{
        int scanRate;                       // how fast to sample
        long latencyUsecs;                  // buffer latency in micro-sec
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
        void* filterObj;       // pointer to filter's private data
        short index;           // sample index to put as first 16 bit word of data
};

#endif

#endif
