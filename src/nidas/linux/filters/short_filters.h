/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate$
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL$

  *******************************************************************

  Filters for use in digital sampling.

*/

#ifndef NIDAS_SHORT_FILTERS_H
#define NIDAS_SHORT_FILTERS_H

#include <nidas/core/dsm_sample.h>

/**
 * Enumeration of supported filter types.
 */
enum nidas_short_filter {
    NIDAS_FILTER_UNKNOWN,
    NIDAS_FILTER_PICKOFF,
    NIDAS_FILTER_BOXCAR,
};

/**
 * Form of data sample that the short filters operate on.
 */
typedef struct short_sample
{
    /**
     * timetag of sample. Signed tenths of milliseconds since 00:00 UTC.
     */
    dsm_sample_time_t timetag;
    /**
     * length of sample, in bytes.
     */
    dsm_sample_length_t length;
    short data[0];
} short_sample_t;

#ifdef __KERNEL__

/**
 * init method, kmallocs and returns a pointer to the filter object,
 * or 0 if ENOMEM.
 */
typedef void* (*shortfilt_init_method)(void);

/**
 * config method.
 * @param id  Numberic id which is written to first word of output
 *      samples - so that downstream code can differentiate between
 *      samples from different filters.
 * @param obj Filter object which was kmalloc'd in init method.
 * @param cfg Pointer to a configuration structure for the filter.
 * @param decimate The decimation factor.
 * @param nvars Number of variables in each sample to be filtered.
 * @param vindices Integer indices of variables in sample to be filtered.
 * @cfg Pointer to configuration struct for the given filter.
 */
typedef int (*shortfilt_config_method)(void* obj, short id, int nvars,
    const int* vindices, int decimate,const void* cfg);

/**
 * Actual filter method.
 * @param obj Filter object which was kmalloc'd in init method.
 * @param in Input sample.
 * @param out Output sample.
 * @return 1: Output sample is valid. 0: no output.
 */
typedef int (*shortfilt_filter_method)(void* obj,
    dsm_sample_time_t tt, const short* in, short_sample_t* out);

/**
 * Destructor. kfree's the passed pointer to the filter object.
 */
typedef void (*shortfilt_cleanup_method)(void* obj);

struct short_filter_methods {
        shortfilt_init_method init;
        shortfilt_config_method config;
        shortfilt_filter_method filter;
        shortfilt_cleanup_method cleanup;
};

/**
 * Exposed module function which returns a structure of filter methods
 * for a supported type.
 */
struct short_filter_methods get_short_filter_methods(enum nidas_short_filter type);

/**
 * Configuration data needed for boxcar filter. Not much.
 */
struct boxcar_filter_config
{
      int npts;       // number of points to average.
};

#endif

#endif
    
