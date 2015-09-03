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
 * Simple filters for A2D data: boxcar averaging, and pickoff.
 * Original author:	Gordon Maclean
*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/slab.h>		/* kmalloc, kfree */
#include <nidas/linux/klog.h>
#include <nidas/linux/SvnInfo.h>    // SVNREVISION

#include <nidas/linux/filters/short_filters.h>

#ifndef SVNREVISION
#define SVNREVISION "unknown"
#endif

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Some simple filters for A2D data");
MODULE_VERSION(SVNREVISION);

#define F_MALLOC(x) kmalloc(x,GFP_KERNEL)

/**
 * Data object for the implementation of a pickoff filter.
 */
struct pickoff_filter
{
        int decimate;
        int nvars;
        int* vindices;
        int count;
        short id;
};

/** 
 * Configuration data needed for pickoff filter. Not much.
 */
struct pickoff_filter_config
{
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
 * Configure a pickoff filter. Pointer to config structure is not used.
 */
static int pickoff_config(void* obj,short id, int nvars, const int* vindices,int decimate, const void* cfg, int nbcfg)
{
        struct pickoff_filter* this = (struct pickoff_filter*) obj;
        this->id = id;
        this->nvars = nvars;
        this->decimate = decimate;
        this->count = 0;
        this->vindices = (int*) F_MALLOC(nvars * sizeof(int));
        if (!this->vindices) return -ENOMEM;
        memcpy(this->vindices,vindices,nvars*sizeof(int));
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
	out->id = this->id;
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
 * Data object for the implementation of a boxcar filter.
 */
struct boxcar_filter
{
        int decimate;
        int nvars;
        int* vindices;
        long* sums;
        int npts;
        int count;
        dsm_sample_time_t tsave;
        short id;
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
static int boxcar_config(void* obj,short id, int nvars, const int* vindices,int decimate, const void* cfg,int nbcfg)
{
        struct boxcar_filter* this = (struct boxcar_filter*) obj;
        const struct boxcar_filter_config* bcfg =
            (const struct boxcar_filter_config*) cfg;
        this->id = id;
        this->nvars = nvars;
        this->decimate = decimate;
        this->count = 0;
        if (nbcfg != sizeof(int)) return -EINVAL;
        this->npts = bcfg->npts;
        this->vindices = (int*) F_MALLOC(nvars * sizeof(int));
        if (!this->vindices) return -ENOMEM;
        memcpy(this->vindices,vindices,nvars*sizeof(int));
        this->sums = (long*) F_MALLOC(nvars * sizeof(long));
        if (!this->sums) return -ENOMEM;
        memset(this->sums,0,nvars*sizeof(long));
        KLOG_INFO("boxcar filter, id=%d, decimate=%d, npts=%d\n",
            id,this->decimate,this->npts);
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
                        out->timetag = this->tsave +
                            (tt - this->tsave) / 2;     // middle time
                        out->id = this->id;
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
        KLOG_NOTICE("version: %s\n",SVNREVISION);
        return 0;
}
static void __exit short_filters_cleanup(void)
{
}

module_init(short_filters_init);
module_exit(short_filters_cleanup);
