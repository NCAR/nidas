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
/*
									      
  Simple filters for use in 16 bit digital sampling.

*/

#ifndef NIDAS_SHORT_FILTERS_KERNEL_H
#define NIDAS_SHORT_FILTERS_KERNEL_H

#include <nidas/linux/short_filters.h>

#include <nidas/linux/types.h>

#ifdef __KERNEL__
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

        /**
         * Sample identifier.
         */
        short id;

        short data[0];

} short_sample_t;

/**
 * Structure describing a filter and which channels it is
 * to be used for.
 */
struct short_filter_data {
        /**
         * Device name that data is being filtered for. Used in
         * log messages.
         */
        const char* deviceName;

        /**
         * Number of channels, aka variables, in each sample to be filtered.
         */
        int nchans;

        /**
         * The index of each channel in the input array.
         */
        int* channels;

        /**
         * Rate of the input samples to the filter.
         */
        int inputRate;

        /**
         * Requested output rate of the filter.
         */
        int outputRate;

        /**
         * pointer to filter's private data
         */
        void* filterObj;

        /**
         * sample index to put as first 16 bit word of data
         */
        short sampleIndex;
};

/**
 * init method, kmallocs and returns a pointer to the filter object,
 * or 0 if ENOMEM.
 * @return Pointer to the filter's private data.
 */
typedef void* (*shortfilt_init_method)(void);

/**
 * config method.
 * @param fdata Pointer to short_filter_data structure of filter configuration.
 * @param cfg Pointer to a configuration structure for the filter.
 * @param nbcfg Number of bytes in cfg structure.
 * @return 0: OK, or negative error value.
 */
typedef int (*shortfilt_config_method)(struct short_filter_data* fdata,
    const void* cfg,int nbcfg);

/**
 * Actual filter method.
 * @param obj Filter object which was kmalloc'd in init method.
 * @param tt Time tag of input data
 * @param in Input data
 * @param skip_factor Sometimes data is interspersed with things like a status word, in which case skip_factor would be 2 to skip the status.
 * @param out Output sample.
 * @return 1: Output sample is valid. 0: no output.
 */
typedef int (*shortfilt_filter_method)(void* obj,
    dsm_sample_time_t tt, const short* in, int skip_factor, short_sample_t* out);

/**
 * Destructor. kfree's the passed pointer to the filter object.
 */
typedef void (*shortfilt_cleanup_method)(void* obj);

/**
 * Exposed module function which returns a structure of filter methods
 * for a supported type.
 */
extern struct short_filter_methods get_short_filter_methods(enum nidas_short_filter type);

/** 
 * Configuration data needed for pickoff filter. Not much.
 */
struct pickoff_filter_config
{
};

/**
 * Configuration data needed for boxcar filter. Not much.
 */
struct boxcar_filter_config
{
        /**
         * Number of points to average.
         */
        int npts;
};

/**
 * Configuration data needed for a time average filter. Not much.
 */
struct timeavg_filter_config
{
        /**
         * Desired output rate.
         */
        int rate;
};

struct short_filter_methods {
        shortfilt_init_method init;
        shortfilt_config_method config;
        shortfilt_filter_method filter;
        shortfilt_cleanup_method cleanup;
};


struct short_filter_info {
        shortfilt_init_method finit;
        shortfilt_config_method fconfig;
        shortfilt_filter_method filter;
        shortfilt_cleanup_method fcleanup;
        struct short_filter_data data;
};

#endif  /* __KERNEL__ */

#endif
    
