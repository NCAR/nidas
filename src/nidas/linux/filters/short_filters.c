/*  a2d_driver.c/

Driver and utility modules for Diamond System MM AT analog IO cards.

Copyright 2005 UCAR, NCAR, All Rights Reserved

Original author:	Gordon Maclean

Revisions:

*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <nidas/linux/klog.h>
#include <nidas/rtlinux/dsm_version.h>

#include <nidas/linux/filters/short_filters.h>

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
            kmalloc(sizeof(struct pickoff_filter),GFP_KERNEL);
        if (!this) return this;
        memset(this,0,sizeof(struct pickoff_filter));
        return this;
}

/**
 * Configure a pickoff filter. config parameter is not used.
 */
static int pickoff_config(void* obj,short id, int nvars, const int* vindices,int decimate, const void* cfg)
{
        struct pickoff_filter* this = (struct pickoff_filter*) obj;
        this->id = id;
        this->nvars = nvars;
        this->decimate = decimate;
        this->count = 0;
        this->vindices = (int*) kmalloc(nvars * sizeof(int),GFP_KERNEL);
        if (!this->vindices) return -ENOMEM;
        memcpy(this->vindices,vindices,nvars*sizeof(int));
        return 0;
}

/**
 * Pickoff filter method. Advanced math!
 */
static int pickoff_filter(void* obj,dsm_sample_time_t tt, const short* in,
    short_sample_t* out)
{
        struct pickoff_filter* this = (struct pickoff_filter*) obj;
        short* op = out->data;
        int i;
        if (this->count++ % this->decimate) return 0;
        this->count = 1;
        out->timetag = tt;
        out->length = this->nvars * sizeof(short);
        *op++ = this->id;
        for (i = 0; i < this->nvars; i++)
            *op++ = in[this->vindices[i]];
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
        struct boxcar_filter* this = kmalloc(sizeof(struct boxcar_filter),GFP_KERNEL);
        if (!this) return this;
        memset(this,0,sizeof(struct boxcar_filter));
        return this;
}

/**
 * Configure a boxcar filter.
 */
static int boxcar_config(void* obj,short id, int nvars, const int* vindices,int decimate, const void* cptr)
{
        struct boxcar_filter* this = (struct boxcar_filter*) obj;
        const struct boxcar_filter_config* cfg =
            (const struct boxcar_filter_config*) cptr;
        this->id = id;
        this->nvars = nvars;
        this->decimate = decimate;
        this->count = 0;
        this->npts = cfg->npts;
        this->vindices = (int*) kmalloc(nvars * sizeof(int),GFP_KERNEL);
        if (!this->vindices) return -ENOMEM;
        memcpy(this->vindices,vindices,nvars*sizeof(int));
        this->sums = (long*) kmalloc(nvars * sizeof(long),GFP_KERNEL);
        if (!this->sums) return -ENOMEM;
        memset(this->sums,0,nvars*sizeof(long));
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
    const short* in, short_sample_t* out)
{
        struct boxcar_filter* this = (struct boxcar_filter*) obj;
        int i;
        this->count++;

        if (this->count <= this->npts) {
                for (i = 0; i < this->nvars; i++)
                    this->sums[i] += in[this->vindices[i]];
                if (this->count == 1) this->tsave = tt;
                if (this->count == this->npts) {
                        short* op = out->data;
                        out->timetag = this->tsave +
                            (tt - this->tsave) / 2;     // middle time
                        out->length = this->nvars * sizeof(short);
                        *op++ = this->id;
                        for (i = 0; i < this->nvars; i++) {
                            *op++ = this->sums[i] / this->npts;
                            this->sums[i] = 0;
                        }
                        if (this->count == this->decimate) this->count = 0;
                        return 1;
                }
        }
        if (this->count == this->decimate) this->count = 0;
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
int short_filters_init(void)
{	
        // DSM_VERSION_STRING is found in dsm_version.h
        KLOG_NOTICE("version: %s\n",DSM_VERSION_STRING);
        return 0;
}
void short_filters_cleanup(void)
{
}

module_init(short_filters_init);
module_exit(short_filters_cleanup);
