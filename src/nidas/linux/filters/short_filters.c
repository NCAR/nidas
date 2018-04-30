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
 * Simple filters for A2D data: boxcar and time averaging, and pickoff.
 * Original author:	Gordon Maclean
*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/slab.h>		/* kmalloc, kfree */
#include <nidas/linux/klog.h>
#include <nidas/linux/Revision.h>    // REPO_REVISION

#include "short_filters_kernel.h"

#ifndef REPO_REVISION
#define REPO_REVISION "unknown"
#endif

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Some simple filters for A2D data");
MODULE_VERSION(REPO_REVISION);

#define F_MALLOC(x) kmalloc(x,GFP_KERNEL)

/**
 * Private data used by a pickoff filter.
 */
struct pickoff_filter
{
        int decimate;
        int nvars;
        int* vindices;
        int count;
        short sampleIndex;
};

/**
 * Constructor for a pickoff filter.
 */
static void* pickoff_init(void)
{
        struct pickoff_filter* this =
            (struct pickoff_filter*) F_MALLOC(sizeof(struct pickoff_filter));
        if (!this) return this;
        memset(this,0,sizeof(struct pickoff_filter));
        return this;
}

/**
 * Configure a pickoff filter.
 */
static int pickoff_config(struct short_filter_data* fdata,
        const void* cfg, int nbcfg)
{
        struct pickoff_filter* this = (struct pickoff_filter*) fdata->filterObj;

        if (fdata->inputRate % fdata->outputRate) {
                KLOG_ERR("%s: pickoff filter, inputRate=%d is not a multiple of the rate=%d for sample %d\n",
                    fdata->deviceName, fdata->inputRate, fdata->outputRate,
                    fdata->sampleIndex);
                return -EINVAL;
        }

        this->sampleIndex = fdata->sampleIndex;
        this->nvars = fdata->nchans;
        this->decimate = fdata->inputRate / fdata->outputRate;
        this->count = 0;
        this->vindices = (int*) F_MALLOC(this->nvars * sizeof(int));
        if (!this->vindices) return -ENOMEM;
        memcpy(this->vindices,fdata->channels,this->nvars*sizeof(int));
        return 0;
}

/**
 * Pickoff filter method. Advanced math!
 */
static int pickoff_filter(void* obj,dsm_sample_time_t tt, const short* in,
    int skip_factor,short_sample_t* out)
{
        struct pickoff_filter* this = (struct pickoff_filter*) obj;
        short* op = out->data;
        int i;
        if (this->count++ % this->decimate) return 0;
        this->count = 1;
        out->timetag = tt;
	out->id = this->sampleIndex;
        for (i = 0; i < this->nvars; i++)
            *op++ = in[this->vindices[i] * skip_factor];
        out->length = (op - &out->id) * sizeof(short);
        return 1;
}

/**
 * Destructor for a pickoff filter.
 */
static void pickoff_cleanup(void* obj)
{
        struct pickoff_filter* this = (struct pickoff_filter*) obj;
        if (this) kfree(this->vindices);
        kfree(this);
}

/**
 * Private data used by a boxcar filter.
 */
struct boxcar_filter
{
        int decimate;
        int nvars;
        int* vindices;
        int32_t* sums;
        int npts;
        int count;
        dsm_sample_time_t tsave;
        short sampleIndex;
};

/**
 * Constructor for a boxcar filter.
 */
static void* boxcar_init(void)
{

        struct boxcar_filter* this =
            (struct boxcar_filter*) F_MALLOC(sizeof(struct boxcar_filter));
        if (!this) return this;
        memset(this,0,sizeof(struct boxcar_filter));
        return this;
}

/**
 * Configure a boxcar filter.
 */
static int boxcar_config(struct short_filter_data* fdata, const void* cfg, int nbcfg)
{
        struct boxcar_filter* this = (struct boxcar_filter*) fdata->filterObj;
        const struct boxcar_filter_config* bcfg =
            (const struct boxcar_filter_config*) cfg;

        if (fdata->inputRate % fdata->outputRate) {
                KLOG_ERR("%s: boxcar filter, inputRate=%d is not a multiple of the rate=%d for sample %d\n",
                    fdata->deviceName, fdata->inputRate, fdata->outputRate,
                    fdata->sampleIndex);
                return -EINVAL;
        }

        this->sampleIndex = fdata->sampleIndex;
        this->nvars = fdata->nchans;
        this->decimate = fdata->inputRate / fdata->outputRate;
        this->count = 0;
        if (nbcfg != sizeof(int)) return -EINVAL;
        this->npts = bcfg->npts;
        this->vindices = (int*) F_MALLOC(this->nvars * sizeof(int));
        if (!this->vindices) return -ENOMEM;
        memcpy(this->vindices,fdata->channels,this->nvars*sizeof(int));
        this->sums = (int32_t*) F_MALLOC(this->nvars * sizeof(int32_t));
        if (!this->sums) return -ENOMEM;
        memset(this->sums,0, this->nvars*sizeof(int32_t));
        KLOG_INFO("%s: boxcar filter, id=%d, decimate=%d, npts=%d\n",
            fdata->deviceName, this->sampleIndex,this->decimate,this->npts);
        return 0;
}

/**
 * Boxcar filter method. More advanced math!
 * Uses decimate and number of points parameters.
 * Example:
 *  input rate 1000/s. Requested output rate 100/s, which
 *  is a decimate value of 10.  npts=4.
 * Boxcar will average first 4 input samples and generate
 * an output. Then it will skip the next 6 samples, then
 * average 4, etc.
 *  
 */
static int boxcar_filter(void* obj, dsm_sample_time_t tt,
    const short* in, int skip_factor, short_sample_t* out)
{
        struct boxcar_filter* this = (struct boxcar_filter*) obj;
        int i;
        this->count++;
        KLOG_DEBUG("boxcar filter, count=%d, npts=%d,decimate=%d\n",
            this->count,this->npts, this->decimate);

        if (this->count <= this->npts) {
                for (i = 0; i < this->nvars; i++)
                    this->sums[i] += in[this->vindices[i] * skip_factor];
                if (this->count == 1) this->tsave = tt;
                if (this->count == this->npts) {
                        short* op = out->data;
                        int tdiff = (tt - this->tsave);
                        // midnight rollover
                        if (tdiff < 0) {
                                if (tdiff < -TMSECS_PER_SEC / 2)
                                        tdiff += TMSECS_PER_SEC;
                                else if (tdiff < -MSECS_PER_SEC / 2)
                                        tdiff += MSECS_PER_SEC;
                        }

                        // middle time
                        out->timetag = this->tsave + tdiff / 2;
                        out->id = this->sampleIndex;
                        for (i = 0; i < this->nvars; i++) {
                            *op++ = this->sums[i] / this->npts;
                            this->sums[i] = 0;
                        }
                        out->length = (op - &out->id) * sizeof(short);
                        if (this->count == this->decimate) this->count = 0;
                        KLOG_DEBUG("boxcar filter return sample, count=%d, npts=%d,decimate=%d\n",
                            this->count,this->npts, this->decimate);
                        return 1;
                }
        }
        if (this->count == this->decimate) this->count = 0;
        KLOG_DEBUG("boxcar filter return nothing, count=%d, npts=%d,decimate=%d\n",
            this->count,this->npts, this->decimate);
        return 0;
}

/**
 * Destructor for a boxcar filter.
 */
static void boxcar_cleanup(void* obj)
{
        struct boxcar_filter* this = (struct boxcar_filter*) obj;
        if (this) {
                kfree(this->vindices);
                kfree(this->sums);
        }
        kfree(this);
}

/**
 * Private data used by a timeavg filter.
 */
struct timeavg_filter
{
        int decimate;
        int deltaTmsecs;
        dsm_sample_time_t endTime;
        unsigned int nout;
        int nvars;
        int* vindices;
        int32_t* sums;
        int nsum;
        short sampleIndex;
};

/**
 * Constructor for a time average filter.
 */
static void* timeavg_init(void)
{

        struct timeavg_filter* this =
            (struct timeavg_filter*) F_MALLOC(sizeof(struct timeavg_filter));
        if (!this) return this;
        memset(this,0,sizeof(struct timeavg_filter));
        this->endTime = INT_MIN;
        return this;
}

/**
 * Configure a time average filter.
 */
static int timeavg_config(struct short_filter_data* fdata,
        const void* cfg,int nbcfg)
{
        struct timeavg_filter* this = (struct timeavg_filter*) fdata->filterObj;

        const struct timeavg_filter_config* bcfg =
            (const struct timeavg_filter_config*) cfg;

        this->sampleIndex = fdata->sampleIndex;
        this->nvars = fdata->nchans;
        this->nout = 0;
        if (nbcfg != sizeof(int)) return -EINVAL;

        if (bcfg->rate % fdata->outputRate) {
                KLOG_ERR("%s: timeavg rate=%d is not a multiple of the rate=%d for sample %d\n",
                    fdata->deviceName, bcfg->rate, fdata->outputRate,
                    this->sampleIndex);
                return -EINVAL;
        }


        this->deltaTmsecs = TMSECS_PER_SEC / bcfg->rate;
        this->decimate = bcfg->rate / fdata->outputRate;

        this->vindices = (int*) F_MALLOC(fdata->nchans * sizeof(int));
        if (!this->vindices) return -ENOMEM;
        memcpy(this->vindices,fdata->channels,fdata->nchans*sizeof(int));
        this->sums = (int32_t*) F_MALLOC(fdata->nchans * sizeof(int32_t));
        if (!this->sums) return -ENOMEM;
        memset(this->sums,0,fdata->nchans*sizeof(int32_t));
        KLOG_INFO("%s: timeavg filter, id=%d, rate=%d, deltaTmsecs=%d, decimate=%d\n",
                fdata->deviceName, this->sampleIndex, bcfg->rate,
                this->deltaTmsecs,this->decimate);
        return 0;
}

/**
 * Time average filter method.
 * Example:
 *  input scan rate 1000/s.
 *  time average rate 100/s
 *  output rate 20/s
 *  Time averager will average data over 0.01 seconds intervals
 *  but then only 1 out of 5 of those averages will be output.
 */
static int timeavg_filter(void* obj, dsm_sample_time_t tt,
    const short* in, int skip_factor, short_sample_t* out)
{
        struct timeavg_filter* this = (struct timeavg_filter*) obj;
        int i, result = 0;
        KLOG_DEBUG("timeavg filter, count=%d, nsum=%d,decimate=%d\n",
            this->count,this->nsum, this->decimate);

        if (tt >= TMSECS_PER_DAY) tt -= TMSECS_PER_DAY;
        else if (tt < 0) tt += TMSECS_PER_DAY;

        if (tt >= this->endTime) {
                if (!(this->nout++ % this->decimate) && this->nsum > 0) {
                        short* op = out->data;
                        dsm_sample_time_t tout = this->endTime - this->deltaTmsecs / 2;
                        // midnight rollover
                        if (tout < 0) tout += TMSECS_PER_DAY;

                        out->timetag = tout;
                        out->id = this->sampleIndex;
                        for (i = 0; i < this->nvars; i++) {
                            *op++ = this->sums[i] / this->nsum;
                        }
                        out->length = (op - &out->id) * sizeof(short);
                        KLOG_DEBUG("timeavg filter return sample, count=%d, nsum=%d,decimate=%d\n",
                                this->count,this->nsum, this->decimate);
                        result = 1;
                }
                memset(this->sums,0, this->nvars*sizeof(int32_t));
                this->nsum = 0;
                this->endTime += this->deltaTmsecs;
                if (this->endTime >= TMSECS_PER_DAY) this->endTime -= TMSECS_PER_DAY;

                // roll forward
                if (tt >= this->endTime)
                        this->endTime = tt + this->deltaTmsecs - tt % this->deltaTmsecs;
        }

        for (i = 0; i < this->nvars; i++)
            this->sums[i] += in[this->vindices[i] * skip_factor];
        this->nsum++;
        return result;
}

/**
 * Destructor for a timeavg filter.
 */
static void timeavg_cleanup(void* obj)
{
        struct timeavg_filter* this = (struct timeavg_filter*) obj;
        if (this) {
                kfree(this->vindices);
                kfree(this->sums);
        }
        kfree(this);
}

/**
 * Return structure of pointers to the methods for a given filter.
 * Pointers will be 0 if the filter is not supported.
 * This structure is passed back by value and does not need to
 * be kfree'd. This function is exposed to other driver modules.
 */
struct short_filter_methods get_short_filter_methods(enum nidas_short_filter which)
{
        struct short_filter_methods meths;
        switch(which) {
        case NIDAS_FILTER_PICKOFF:
                meths.init = pickoff_init;
                meths.config = pickoff_config;
                meths.filter = pickoff_filter;
                meths.cleanup = pickoff_cleanup;
                break;
        case NIDAS_FILTER_BOXCAR:
                meths.init = boxcar_init;
                meths.config = boxcar_config;
                meths.filter = boxcar_filter;
                meths.cleanup = boxcar_cleanup;
                break;
        case NIDAS_FILTER_TIMEAVG:
                meths.init = timeavg_init;
                meths.config = timeavg_config;
                meths.filter = timeavg_filter;
                meths.cleanup = timeavg_cleanup;
                break;
        default:
                meths.init = 0;
                meths.config = 0;
                meths.filter = 0;
                meths.cleanup = 0;
                break;
        }
        return meths;
}

EXPORT_SYMBOL(get_short_filter_methods);

static int __init short_filters_init(void)
{	
        KLOG_NOTICE("version: %s\n",REPO_REVISION);
        return 0;
}
static void __exit short_filters_cleanup(void)
{
}

module_init(short_filters_init);
module_exit(short_filters_cleanup);
