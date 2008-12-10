/*  a2d_driver.c/

Driver and utility modules for Diamond System MM AT analog IO cards.

Copyright 2005 UCAR, NCAR, All Rights Reserved

Original author:	Gordon Maclean

Revisions:

*/

#include <linux/types.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/fs.h>           /* everything... */
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <nidas/linux/diamond/dmd_mmat.h>
#include <nidas/rtlinux/dsm_version.h>
// #define DEBUG
#include <nidas/linux/klog.h>
#include <nidas/linux/isa_bus.h>

/* SA_SHIRQ is deprecated starting in 2.6.22 kernels */
#ifndef IRQF_SHARED
# define IRQF_SHARED SA_SHIRQ
#endif

/* ioport addresses of installed boards, 0=no board installed */
static unsigned long ioports[MAX_DMMAT_BOARDS] = { 0x380, 0, 0, 0 };
/* number of DMMAT boards in system (number of non-zero ioport values) */
static int numboards = 0;

/* ISA irqs, required for each board. Can be shared. */
static int irqs[MAX_DMMAT_BOARDS] = { 3, 0, 0, 0 };
static int numirqs = 0;

/* board types: 0=DMM16AT, 1=DMM32XAT 
 * See #defines for DMM_XXXXX_BOARD)
 * Doesn't seem to be an easy way to auto-detect the board type,
 * but it's probably do-able.
 */
static int types[MAX_DMMAT_BOARDS] = { DMM32XAT_BOARD, 0, 0, 0 };
static int numtypes = 0;

/*
 * How is the D2A jumpered? Bipolar, unipolar, 5 or 10 volts.
 */
static int d2aconfig[MAX_DMMAT_BOARDS] = { DMMAT_D2A_UNI_5, 0, 0, 0 };
static int numd2aconfig = 0;

#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(ioports,ulong,&numboards,0);
module_param_array(irqs,int,&numirqs,0);
module_param_array(types,int,&numtypes,0);
module_param_array(d2aconfig,int,&numd2aconfig,0);
#else
module_param_array(ioports,ulong,numboards,0);
module_param_array(irqs,int,numirqs,0);
module_param_array(types,int,numtypes,0);
module_param_array(d2aconfig,int,numd2aconfig,0);
#endif

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_LICENSE("Dual BSD/GPL");

/*
 * Holds the major number of all DMMAT devices.
 */
static dev_t dmmat_device = MKDEV(0,0);

/*
 * Pointer to first of dynamically allocated structures containing
 * all data pertaining to the configured DMMAT boards on the system.
 */
static struct DMMAT* board = 0;

#ifdef USE_MY_WORK_QUEUE
static struct workqueue_struct* work_queue = 0;
#endif

/*********** Board Utility Functions *******************/
/**
 * Set counter value in a 82C54 clock.
 * Works on both MM16AT and MM32XAT, assuming both have set
 * page bit(s) to 0:
 *	MM16AT: bit 6 in base+10 is the page bit
 *	MM32XAT: bits 0-2 in base+8 are page bits
 */
static void setTimerClock(struct DMMAT* brd,
	int clock, int mode, unsigned short val)
{
        unsigned char lobyte;
        unsigned char hibyte;

        /*
         * 82C54 control word:
         * bits 7,6: 0=select ctr 0, 1=ctr 1, 2=ctr 2, 3=read-back cmd
         * bits 5,4: 0=ctr latch, 1=r/w lsbyte only, 2=r/w msbyte, 3=lsbtye,msbyte
         * bits 3,2,1: mode: 0=interrupt on terminal count, 2=rate generator
         * bit 0: 0=binary 16 bit counter, 1=4 decade BCD
         */
        unsigned char ctrl = (clock << 6) + 0x30 + (mode << 1);

        outb(ctrl,brd->addr + 15);

        lobyte = val & 0xff;
        outb(lobyte,brd->addr + 12 + clock);
        hibyte = val >> 8;
        outb(hibyte,brd->addr + 12 + clock);

        KLOG_DEBUG("ctrl=%#x, lobyte=%d,hibyte=%d\n",
            (int)ctrl,(int)lobyte,(int)hibyte);
}

/**
 * Read counter value in a 82C54 clock.
 * Works on both MM16AT and MM32XAT, assuming both have set
 * page bit(s) to 0:
 *	MM16AT: bit 6 in base+10 is the page bit
 *	MM32XAT: bits 0-2 in base+8 are page bits
 */
static unsigned long readLatchedTimer(struct DMMAT* brd,int clock)
{
        unsigned char lobyte;
        unsigned int hibyte;
        int value;

        /*
         * 82C54 control word:
         * bits 7,6: 0=select ctr 0, 1=ctr 1, 2=ctr 2, 3=read-back cmd
         * bits 5,4: 0=ctr latch
         * bits 3-0: don't care
         */
        unsigned char ctrl = clock << 6;

        outb(ctrl,brd->addr + 15);  // latch counter

        lobyte = inb(brd->addr + 12 + clock);
        hibyte = inb(brd->addr + 12 + clock);
        value = (hibyte << 8) + lobyte;

        KLOG_DEBUG("ctrl=%#x, lobyte=%d,hibyte=%d,value=%d\n",
            (int)ctrl,(int)lobyte,(int)hibyte,value);

        return value;
}

static void initializeA2DClock(struct DMMAT_A2D* a2d)
{
        unsigned long ticks =
            (USECS_PER_SEC * 10 + a2d->scanRate/2) / a2d->scanRate;
        unsigned short c1;
        if (ticks % 2) ticks--;
        // maximum sampling rate should divide evenly into 10MHz
        // Also, since the minimum counter value for a 82C54 clock chip
        // in (mode 3) is 2, then 10MHz/scanRate must be even.
        // the
        if ((USECS_PER_SEC * 10) % (a2d->scanRate * 2)) {
            KLOG_WARNING("maximum sampling rate=%d Hz does not divide evenly into 10 MHz. Actual sampling rate will be %ld Hz\n",
                    a2d->scanRate,(USECS_PER_SEC*10)/ticks);
        }

        KLOG_DEBUG("clock ticks=%ld,scanRate=%d\n",ticks,a2d->scanRate);
        c1 = 1;

        while (ticks > 65535) {
            if (!(ticks % 2)) {
                c1 *= 2;
                ticks /= 2;
            }
            else if (!(ticks % 5)) {
                c1 *= 5;
                ticks /= 5;
            }
        }
        // clock counters must be > 1
        if (c1 < 2) {
            c1 = 2;
            ticks /= 2;
        }
        KLOG_DEBUG("clock ticks=%d,%ld\n", c1,ticks);
        setTimerClock(a2d->brd,1,2,c1);
        setTimerClock(a2d->brd,2,2,ticks);
}

/**
 * Determine number of available channels - depends
 * on single-ended vs differential jumpering.
 */
static int getNumA2DChannelsMM16AT(struct DMMAT_A2D* a2d)
{
        struct DMMAT* brd = a2d->brd;
        int nchan = 8;
        unsigned long flags;
        unsigned char status;

        spin_lock_irqsave(&brd->reglock,flags);
        status = inb(brd->addr + 8);
        spin_unlock_irqrestore(&brd->reglock,flags);

        if (status & 0x20) nchan += 8;	// single-ended
        return nchan;
}

/**
 * Determine number of available channels - depends
 * on single-ended vs differential jumpering.
 */
static int getNumA2DChannelsMM32XAT(struct DMMAT_A2D* a2d)
{
        struct DMMAT* brd = a2d->brd;
        unsigned long flags;
        unsigned char status;
        int nchan;

        // Jumpered options:
        //   1. channels 0-31 single-ended
        //   2. channels 0-15 differential
        //   3. 0-7 DI, 8-15 SE, 24-31 SE (not continuous)
        //   4. 0-7 SE, 8-15 DI, 16-23 SE
        spin_lock_irqsave(&brd->reglock,flags);
        status = inb(brd->addr + 8);
        spin_unlock_irqrestore(&brd->reglock,flags);

        nchan = 16;
        if (status & 0x20) nchan += 8;	// 0-7, 16-23 single-ended
        if (status & 0x40) nchan += 8;	// 8-15, 24-31 single-ended
        return nchan;
}

/**
 * returns: 0: ok,  -EINVAL: channels out of range.
 */
static int selectA2DChannelsMM16AT(struct DMMAT_A2D* a2d)
{

        unsigned long flags;
        int nchan = a2d->getNumChannels(a2d);
        unsigned char chanNibbles;

        if (a2d->lowChan < 0) return -EINVAL;
        if (a2d->highChan >= nchan ||
            a2d->highChan < a2d->lowChan) return -EINVAL;
        if (a2d->lowChan > a2d->highChan) return -EINVAL;
          
        chanNibbles = (a2d->highChan << 4) + a2d->lowChan;
        KLOG_DEBUG("highChan=%d,lowChan=%d,nib=0x%x\n",
            a2d->highChan,a2d->lowChan,(int)chanNibbles);

        spin_lock_irqsave(&a2d->brd->reglock,flags);
        outb(chanNibbles, a2d->brd->addr + 2);
        spin_unlock_irqrestore(&a2d->brd->reglock,flags);

        a2d->waitForA2DSettle(a2d);
        return 0;
}

/**
 * returns: 0: ok,  -EINVAL: channels out of range.
 */
static int selectA2DChannelsMM32XAT(struct DMMAT_A2D* a2d)
{
        unsigned long flags;
        int nchan = a2d->getNumChannels(a2d);

        if (a2d->lowChan < 0) return -EINVAL;
        if (a2d->highChan >= nchan ||
            a2d->highChan < a2d->lowChan) return -EINVAL;
        if (a2d->lowChan > a2d->highChan) return -EINVAL;
          
        spin_lock_irqsave(&a2d->brd->reglock,flags);
        outb(a2d->lowChan, a2d->brd->addr + 2);
        outb(a2d->highChan, a2d->brd->addr + 3);
        spin_unlock_irqrestore(&a2d->brd->reglock,flags);

        a2d->waitForA2DSettle(a2d);
        return 0;
}

static int getConvRateMM16AT(struct DMMAT_A2D* a2d, unsigned char* val)
{
        int totalRate = a2d->scanRate * (a2d->highChan - a2d->lowChan + 1);
        // setting bit 4 enables 5.3 usec conversions, 9.3 usec otherwise.
        // 9.3 usec gives an approx sampling rate of 108 KHz.
        // We'll switch to 5.3 for rates over 80 KHz.
        if (totalRate > 80000) *val |= 0x10;
        else *val &= ~0x10;
        return 0;
}

static int getConvRateMM32XAT(struct DMMAT_A2D* a2d, unsigned char* val)
{
        int totalRate = a2d->scanRate * (a2d->highChan - a2d->lowChan + 1);

        // bits 4&5:
        //	0: 20usec, 50KHz
        //	1: 15usec, 66KHz
        //	2: 10usec, 100KHz
        //	3:  4usec, 250KHz

        *val &= ~0x30;
        if (totalRate > 50000) *val |= 0x30;
        else if (totalRate > 33000) *val |= 0x20;
        else if (totalRate > 25000) *val |= 0x10;
        return 0;
}

/*
 * bipolar gains
 *	gain	range
 *	1	+-10V
 *	2	+-5V
 *	4	+-2.5V
 *	8	+-1.25V
 *	16	+-0.625V
 * unipolar gains
 *	gain	range
 *	1	0-20V	not avail
 *	2	0-10V
 *	4	0-5V
 *	8	0-2.5
 *	16	0-1.25
 */
static int getGainSettingMM16AT(struct DMMAT_A2D* a2d,int gain,
	int bipolar,unsigned char* val)
{
        int i;
        const char* gainStrings[2] = {"2,4,8,16","1,2,4,8,16"};

        unsigned char aconfig[][3] = {
        //bipolar,gain,register
            {0,  2, DMMAT_UNIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_1 },// 0:10V
            {0,  4, DMMAT_UNIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_2 },// 0:5V
            {0,  8, DMMAT_UNIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_4 },// 0:2.5V
            {0, 16, DMMAT_UNIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_8 },// 0:1.25V

            {1,  1, DMMAT_BIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_1 },// -10:10
            {1,  2, DMMAT_BIPOLAR | DMMAT_RANGE_5V | DMMAT_GAIN_1 }, // -5:5
            {1,  4, DMMAT_BIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_4 },// -2.5:2.5
            {1,  8, DMMAT_BIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_8 },// -1.25:1.25
            {1, 16, DMMAT_BIPOLAR | DMMAT_RANGE_5V | DMMAT_GAIN_8 }, // -0.625:.625
        };
        int n = sizeof(aconfig) / sizeof(aconfig[0]);

        for (i = 0; i < n; i++)
            if (aconfig[i][0] == bipolar && aconfig[i][1] == gain) break;
        if (i == n) {
            KLOG_ERR("%s: illegal gain=%d for polarity=%s. Allowed gains=%s\n",
                    a2d->deviceName,gain,(bipolar ? "bipolar" : "unipolar"),
                    gainStrings[bipolar]);
            return -EINVAL;
        }
        *val = aconfig[i][2];
        return 0;
}

static int getGainSettingMM32XAT(struct DMMAT_A2D* a2d, int gain,
	int bipolar, unsigned char* val)
{
        int i;
        const char* gainStrings[2] = {"2,4,8,16,32","1,2,4,8,16"};

        unsigned char aconfig[][3] = {
        //bipolar,gain,register setting
            {0,  2, DMMAT_UNIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_1 },// 0:10V
            {0,  4, DMMAT_UNIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_2 },// 0:5V
            {0,  8, DMMAT_UNIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_4 },// 0:2.5V
            {0, 16, DMMAT_UNIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_8 },// 0:1.25V
            {0, 32, DMMAT_UNIPOLAR | DMMAT_RANGE_5V | DMMAT_GAIN_8 },// 0:0.625V

            {1,  1, DMMAT_BIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_1 },// -10:10
            {1,  2, DMMAT_BIPOLAR | DMMAT_RANGE_5V | DMMAT_GAIN_1 }, // -5:5
            {1,  4, DMMAT_BIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_4 },// -2.5:2.5
            {1,  8, DMMAT_BIPOLAR | DMMAT_RANGE_10V | DMMAT_GAIN_8 },// -1.25:1.25
            {1, 16, DMMAT_BIPOLAR | DMMAT_RANGE_5V | DMMAT_GAIN_8 }, // -0.625:.625
        };

        int n = sizeof(aconfig) / sizeof(aconfig[0]);

        for (i = 0; i < n; i++)
            if (aconfig[i][0] == bipolar && aconfig[i][1] == gain) break;
        if (i == n) {
            KLOG_ERR("%s: illegal gain=%d for polarity=%s. Allowed gains=%s\n",
                    a2d->deviceName,gain,(bipolar ? "bipolar" : "unipolar"),
                    gainStrings[bipolar]);
            return -EINVAL;
        }
        *val = aconfig[i][2];
        return 0;
}

/*
 * Returns:
 * 3: fifo full or overflowed
 * 2: fifo at or above threshold but not full
 * 1: fifo not empty but less than threshold
 * 0: fifo empty
 */
static int getFifoLevelMM16AT(struct DMMAT_A2D* a2d)
{
        /* base + 10 
         * bit 2: 0=no overflow, 1=overflow
         * bit 1: 0=less than half, 1=at least half
         * bit 0: 0=not empty, 1= empty
         *
         * Note: on the MM16AT, the FIFO must be reset (base + 10)
         * to clear an overflow.
         */
        unsigned char fifo = inb(a2d->brd->addr + 10) & 0x3;
        switch (fifo) {
        case 1: return 0;
        case 0: return 1;
        case 2: return 2;
        case 6: return 3;

        /* other settings don't make sense - return overflow */
        default: return 3;
        }
}

/*
 * Returns:
 * 3: fifo full or overflowed
 * 2: fifo at or above threshold but not full
 * 1: fifo not empty but less than threshold
 * 0: fifo empty
 */
static int getFifoLevelMM32XAT(struct DMMAT_A2D* a2d)
{
        /* base + 7 
         * bit 7: 0=not empty, 1= empty
         * bit 6: 0=less than threshold, 1=at least threshold
         * bit 5: 0=not full, 1=full
         * bit 4: 0=not overflowed, 1=overflow
         */
        unsigned char fifo = (inb(a2d->brd->addr + 7) & 0xf0) >> 4;
        switch (fifo) {
        case 8: return 0;
        case 0: return 1;
        case 4: return 2;
        case 3:
        case 2:
        case 1: return 3;

        /* other settings don't make sense - return overflow */
        default: return 3;
        }
}

static void resetFifoMM16AT(struct DMMAT_A2D* a2d)
{
        outb(0x80,a2d->brd->addr + 10);
}

static void resetFifoMM32XAT(struct DMMAT_A2D* a2d)
{
        outb(0x02,a2d->brd->addr + 7);
}

static void waitForA2DSettleMM16AT(struct DMMAT_A2D* a2d)
{
        int ntry = 0;
        do {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(1);
        } while(ntry++ < 50 && inb(a2d->brd->addr + 10) & 0x80);
        KLOG_DEBUG("ntry=%d\n",ntry);
}

static void waitForA2DSettleMM32XAT(struct DMMAT_A2D* a2d)
{
        do {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(1);
        } while(inb(a2d->brd->addr + 11) & 0x80);
}

static void freeA2DFilters(struct DMMAT_A2D *a2d)
{
        int i;
        for (i = 0; i < a2d->nfilters; i++) {
            struct a2d_filter_info* finfo = a2d->filters + i;
            /* cleanup filter */
            if (finfo->filterObj && finfo->fcleanup)
                finfo->fcleanup(finfo->filterObj);
            finfo->filterObj = 0;
            finfo->fcleanup = 0;
            kfree(finfo->channels);
        }
        kfree(a2d->filters);
        a2d->filters = 0;
        a2d->nfilters = 0;
}

/*
 * Initial configuration of an A2D.
 */
static int configA2D(struct DMMAT_A2D* a2d,
        struct nidas_a2d_config* cfg)
{

        if(a2d->running) {
                KLOG_ERR("A2D's running. Can't configure\n");
                return -EBUSY;
        }

        freeA2DFilters(a2d);
        a2d->lowChan = MAX_DMMAT_A2D_CHANNELS;
        a2d->highChan = -1;

        a2d->scanRate = cfg->scanRate;

        if ((USECS_PER_SEC * 10) % a2d->scanRate)
                KLOG_WARNING("%s: max sampling rate=%d is not a factor of 10 MHz\n",
                    a2d->deviceName,a2d->scanRate);

        a2d->scanRate = cfg->scanRate;

        a2d->latencyMsecs = cfg->latencyUsecs / USECS_PER_MSEC;
        if (a2d->latencyMsecs == 0) a2d->latencyMsecs = 500;

        a2d->latencyJiffies = (cfg->latencyUsecs * HZ) / USECS_PER_SEC;
        if (a2d->latencyJiffies == 0) a2d->latencyJiffies = HZ / 10;
        KLOG_DEBUG("%s: latencyJiffies=%ld, HZ=%d\n",
                    a2d->deviceName,a2d->latencyJiffies,HZ);

        a2d->scanDeltaT =  TMSECS_PER_SEC / a2d->scanRate;
        return 0;
}

/*
 * Add a sample to the A2D configuration.  Board should not be busy.
 */
static int addA2DSampleConfig(struct DMMAT_A2D* a2d,struct nidas_a2d_sample_config* cfg)
{

        int result = 0;
        int i;
        int nfilters;
        struct a2d_filter_info* filters;
        struct a2d_filter_info* finfo;
        struct short_filter_methods methods;

        if(a2d->running) {
                KLOG_ERR("A2D's running. Can't configure\n");
                return -EBUSY;
        }
        // grow filter info array by one
        nfilters = a2d->nfilters + 1;
        filters = kmalloc(nfilters * sizeof (struct a2d_filter_info),
                               GFP_KERNEL);
        if (!filters) return -ENOMEM;
        // copy previous filter infos, and free the space
        memcpy(filters,a2d->filters,
            a2d->nfilters * sizeof(struct a2d_filter_info));
        kfree(a2d->filters);

        finfo = filters + a2d->nfilters;
        a2d->filters = filters;
        a2d->nfilters = nfilters;

        memset(finfo, 0, sizeof(struct a2d_filter_info));

        if (!(finfo->channels =
                kmalloc(cfg->nvars * sizeof(int),GFP_KERNEL)))
                return -ENOMEM;

        memcpy(finfo->channels,cfg->channels,cfg->nvars * sizeof(int));
        finfo->nchans = cfg->nvars;

        KLOG_DEBUG("%s: sindex=%d,nfilters=%d\n",
                   a2d->deviceName, cfg->sindex, a2d->nfilters);

        if (cfg->sindex < 0 || cfg->sindex >= a2d->nfilters)
                return -EINVAL;

        KLOG_DEBUG("%s: scanRate=%d,cfg->rate=%d\n",
                   a2d->deviceName, a2d->scanRate, cfg->rate);

       if (a2d->scanRate % cfg->rate) {
                KLOG_ERR
                    ("%s: A2D scanRate=%d is not a multiple of the rate=%d for sample %d\n",
                     a2d->deviceName, a2d->scanRate, cfg->rate,
                     cfg->sindex);
                return -EINVAL;
        }

        finfo->decimate = a2d->scanRate / cfg->rate;
        finfo->filterType = cfg->filterType;
        finfo->index = cfg->sindex;

        KLOG_DEBUG("%s: decimate=%d,filterType=%d,index=%d\n",
                   a2d->deviceName, finfo->decimate, finfo->filterType,
                   finfo->index);

        methods = get_short_filter_methods(cfg->filterType);
        if (!methods.init) {
                KLOG_ERR("%s: filter type %d unsupported\n",
                         a2d->deviceName, cfg->filterType);
                return -EINVAL;
        }
        finfo->finit = methods.init;
        finfo->fconfig = methods.config;
        finfo->filter = methods.filter;
        finfo->fcleanup = methods.cleanup;

        /* Create the filter object */
        finfo->filterObj = finfo->finit();
        if (!finfo->filterObj)
                return -ENOMEM;

        for (i = 0; i < cfg->nvars; i++) {
                int ichan = cfg->channels[i];
                if (ichan < 0 || ichan >= MAX_DMMAT_A2D_CHANNELS) {
                        KLOG_ERR("%s: channel number %d out of range\n",
                                 a2d->deviceName, ichan);
                        return -EINVAL;
                }
                KLOG_DEBUG("%s: configuring channel %d,gain=%d,bipolar=%d\n",
                     a2d->deviceName, ichan,cfg->gain[i],cfg->bipolar[i]);

                if (a2d->highChan < 0) {
                        a2d->gain = cfg->gain[i];
                        a2d->bipolar = cfg->bipolar[i];
                        result = a2d->getGainSetting(a2d,a2d->gain,a2d->bipolar,&a2d->gainConvSetting);
                        if (result != 0) return result;
                }
                if (ichan < a2d->lowChan) a2d->lowChan = ichan;
                if (ichan > a2d->highChan) a2d->highChan = ichan;

                // gains must all be the same
                if (cfg->gain[i] != a2d->gain) return -EINVAL;
                // Must have same polarity.
                if (cfg->bipolar[i] != a2d->bipolar) return -EINVAL;
        }

        // subtract low channel from the filter channel numbers
        // since we don't scan the channels below lowChan
        for (i = 0; i < cfg->nvars; i++)
                finfo->channels[i] -= a2d->lowChan;
        a2d->nchans = a2d->highChan - a2d->lowChan + 1;

        /* Configure the filter */
        result = finfo->fconfig(finfo->filterObj, finfo->index,
                               finfo->nchans,
                               finfo->channels,
                               finfo->decimate,
                               cfg->filterData,
                               cfg->nFilterData);
        if (result) KLOG_ERR("%s: error in filter config\n",a2d->deviceName);

        return result;
}

/*
 * Function to stop the A2D on a MM16AT
 */
static void stopMM16AT_A2D(struct DMMAT_A2D* a2d,int lock)
{
        struct DMMAT* brd = a2d->brd;
        unsigned long flags = 0;
        if (lock) spin_lock_irqsave(&brd->reglock,flags);

        // disable A2D triggering and interrupts
        brd->itr_ctrl_val &= ~0x83;
        outb(brd->itr_ctrl_val, brd->addr + 9);

        // disble fifo, scans
        outb(0,brd->addr + 10);

        // reset fifo
        outb(0x80,brd->addr + 10);

        if (lock) spin_unlock_irqrestore(&brd->reglock,flags);
}

/*
 * Function to stop the A2D on a MM32XAT
 */
static void stopMM32XAT_A2D(struct DMMAT_A2D* a2d,int lock)
{
        struct DMMAT* brd = a2d->brd;
        unsigned long flags = 0;
        if (lock) spin_lock_irqsave(&brd->reglock,flags);

        // set page to 0
        outb(0x00, brd->addr + 8);
        
        // disable A2D interrupts, hardware clock
        brd->itr_ctrl_val &= ~0x83;
        outb(brd->itr_ctrl_val, brd->addr + 9);

        // disable and reset fifo, disable scan mode
        outb(0x2,brd->addr + 7);

        if (lock) spin_unlock_irqrestore(&brd->reglock,flags);
}

/**
 * General function to stop the A2D. Calls the board specific method.
 */
static int stopA2D(struct DMMAT_A2D* a2d,int lock)
{
        int ret = 0;

        if (a2d->stop) a2d->stop(a2d,lock);
#ifdef USE_TASKLET
        // shut down tasklet. Not necessary, you can leave it enabled.
        // tasklet_disable(&a2d->tasklet);
#endif
        a2d->running = 0;

        return ret;
}

/*
 * Function to start the A2D on a MM16AT
 */
static int startMM16AT_A2D(struct DMMAT_A2D* a2d,int lock)
{
        int result = 0;
        unsigned long flags = 0;
        struct DMMAT* brd = a2d->brd;

        if (lock) spin_lock_irqsave(&brd->reglock,flags);

        // reset fifo
        outb(0x80,brd->addr + 10);

        /*
         * base+10, Counter/timer and FIFO control
         * 
         * bit 7: fifo reset
         * bit 6: page number for registers at base+12 to base+15:
         *        0=82C54 counter/timer, 1=calibration
         * bit 5: fifo enable
         * bit 4: scan enable
         * bit 3: 0=input to counter/timer 1 is 10MHz, 1=1MHz
         * bit 2: 1=IN0- (IO pin 29) is a gate for A/D sample control
         *        when extern A/D clock is enabled.
         *        0=IN0- is not a gate for A/D sample control
         * bit 1: 1=input to counter 0 is a 100kHz reference, gated by IN0-.
         *        0=input to counter 0 is IN0-.
         * bit 0: 1=counters 1&2 are gated by DIN0 (IO pin 48).
         *        0=counters 1&2 run freely
         */
        outb(0x30,brd->addr + 10);

        a2d->fifoThreshold = 256;

        initializeA2DClock(a2d);

         /*
          * base+9, Control register
          *
          * bit 7: enable A/D interrupts
          * bit 3: enable timer 0 interrupts
          *     (can't set both 3 and 7 according to manual)
          * bit 1: 1=enable hardware A/D hardware clock
                  0=A/D software triggered by write to base+0
          * bit 0: 1=internal hardware trigger, counter/timer 1&2
                0=external trigger: DIN0, IO pin 48
          */
        brd->itr_ctrl_val |= 0x83;
        outb(brd->itr_ctrl_val,brd->addr + 9);

        if (lock) spin_unlock_irqrestore(&brd->reglock,flags);

        return result;
}

/*
 * Function to start the A2D on a MM32XAT
 */
static int startMM32XAT_A2D(struct DMMAT_A2D* a2d,int lock)
{
        int result = 0;
        unsigned long flags = 0;
        struct DMMAT* brd = a2d->brd;
        int nscans;
        int nsamps;

        if (lock) spin_lock_irqsave(&brd->reglock,flags);

        outb(0x04,brd->addr + 8);	// set page 4
        outb(0x02,brd->addr + 14);	// abort any currently running autocal
        outb(0x10,brd->addr + 14);	// disable auto-cal
        outb(0x00,brd->addr + 8);	// set page 0

        // compute fifo threshold
        // number of scans in latencyMsecs
        nscans = (a2d->latencyMsecs * a2d->scanRate) / MSECS_PER_SEC;
        if (nscans == 0) nscans = 1;

        nsamps = nscans * a2d->nchans;
        // fifo is 1024 samples. Want an interrupt before it is 1/2 full
        if (nsamps > 512) nsamps = (512 / a2d->nchans) * a2d->nchans;
        if ((nsamps % 2)) nsamps += a2d->nchans;		// must be even

        if (nsamps == 2) nsamps = 4;
        a2d->fifoThreshold = nsamps;

        if (a2d->fifoThreshold > a2d->maxFifoThreshold) {
                KLOG_ERR("%s: Programming error: fifoThreshold=%d is bigger than max=%d\n",
                
                    a2d->deviceName,a2d->fifoThreshold, a2d->maxFifoThreshold);
                return -EINVAL;
        }

        KLOG_INFO("%s: fifoThreshold=%d,latency=%ld,nchans=%d\n",
            a2d->deviceName, a2d->fifoThreshold,a2d->latencyMsecs,
                a2d->nchans);
        // register value is 1/2 the threshold
        nsamps /= 2;

        if (nsamps > 255) {
            outb(0x02,brd->addr + 8);	// set page 2
            outb(0x01,brd->addr + 12);	// set high bit for fifo threshold
            outb(0x00,brd->addr + 8);	// set page 0
        }
        else {
            outb(0x02,brd->addr + 8);	// set page 2
            outb(0x00,brd->addr + 12);	// turn off high bit for fifo threshold
            outb(0x00,brd->addr + 8);	// set page 0
        }
        outb(nsamps & 0xff, brd->addr + 6);

        /*
         * base+7, Counter/timer and FIFO control
         * 
         * bit 3: fifo enable
         * bit 2: scan enable
         * bit 1: fifo reset
         */
        outb(0x0e,brd->addr + 7);

        /*
         * base+10, Counter/timer and DIO control
         * 
         * bit 7: 1=input to counter1&2 is 100 KHz, 0=10MHz
         * bit 6: 1=input to counter0 is 10 KHz, 0=10MHz
         * bit 5: 1=counter 1&2 output on J3,pin 42, 0=42 is digital out
         * bit 4: 1=counter 0 output on J3,pin 44, 0=44 is digital out
         * bit 2: 1=J3 pin 47 is gate for counter0, 0=counter0 free running
         * bit 1: 1=counter0 input is clock, set by bit6. 0=input is J3 pin 48
         * bit 0: 1=J3 pin 46 is gate for counter1&2, 0=counter1&2 free running
         */
        outb(0x0,brd->addr + 10);

        initializeA2DClock(a2d);

         /*
          * base+9, Control register
          *
          * bit 7: enable A/D interrupts
          * bit 6: enable dio interrupts
          * bit 5: enable timer 0 interrupts
          * bit 1: 1=enable hardware A/D hardware clock
                  0=A/D software triggered by write to base+0
          * bit 0: 1=internal hardware trigger, counter/timer 1&2
                0=external trigger: DIN0, IO pin 48
          */
        brd->itr_ctrl_val |= 0x83;
        outb(brd->itr_ctrl_val,brd->addr + 9);

        if (lock) spin_unlock_irqrestore(&brd->reglock,flags);
        return result;
}

/**
 * General function to start an A2D. Calls the board specific method.
 */
static int startA2D(struct DMMAT_A2D* a2d,int lock)
{
        int result;
        unsigned long flags = 0;
        struct DMMAT* brd = a2d->brd;

        if (a2d->running) stopA2D(a2d,lock);

        a2d->running = 1;	// Set the running flag

        a2d->status.irqsReceived = 0;
        a2d->sampBytesLeft = 0;
        a2d->sampPtr = 0;
        a2d->lastWakeup = jiffies;

        if ((result = a2d->selectChannels(a2d))) return result;

        if (lock) spin_lock_irqsave(&brd->reglock,flags);

        // Just in case the irq handler or bottom half is running,
        // lock the board reglock before resetting the circular
        // buffer head
        a2d->fifo_samples.head = a2d->fifo_samples.tail = 0;
        a2d->samples.head = a2d->samples.tail = 0;

        // needs scanRate and high/low channels defined
        result = a2d->getConvRateSetting(a2d,&a2d->gainConvSetting);
        if (result != 0) return result;

        // same addr on MM16AT and MM32XAT
        outb(a2d->gainConvSetting,a2d->brd->addr + 11);
        if (lock) spin_unlock_irqrestore(&brd->reglock,flags);

        a2d->waitForA2DSettle(a2d);

        a2d->bh_data.saveSample.length = 0;

#ifdef USE_TASKLET
        // Not necessary to enable the tasklet, we don't disable it.
        // tasklet_enable(&a2d->tasklet);
#endif

        a2d->start(a2d,lock);
        return 0;
}

static int startMM32XAT_AutoCal(struct DMMAT_A2D* a2d)
{
        unsigned long flags = 0;
        struct DMMAT* brd = a2d->brd;
        int ntry = 1000;
        int a2dRunning = a2d->running;
        int result = 0;

        spin_lock_irqsave(&brd->reglock,flags);

        if (a2dRunning) stopA2D(a2d,0);

        outb(0x04,brd->addr + 8);	// set page 4
        outb(0x01,brd->addr + 14);	// start cal
        do {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(1);
        } while(inb(a2d->brd->addr + 14) & 0x02 && ntry--);
        KLOG_INFO("auto calibration nloop=%d\n",1000-ntry);
        outb(0x00,brd->addr + 8);	// set page 0
        if (ntry == 0) {
            KLOG_ERR("auto calibration timeout\n");
            result = -ETIMEDOUT;
        }
        spin_unlock_irqrestore(&brd->reglock,flags);
        if (a2dRunning) startA2D(a2d,0);
        return result;
}

/*
 * Function to stop the counter on a MM32AT
 */
static void stopMM16AT_CNTR(struct DMMAT_CNTR* cntr)
{
        struct DMMAT* brd = cntr->brd;

         /*
          * base+9, Control register
          *
          * bit 3: enable timer 0 interrupts
          *     (can't set both 3 and 7 according to manual)
          */
        brd->itr_ctrl_val &= ~0x08;
        outb(brd->itr_ctrl_val,brd->addr + 9);
}

/*
 * Function to stop the counter on a MM32AT
 */
static void stopMM32AT_CNTR(struct DMMAT_CNTR* cntr)
{
        struct DMMAT* brd = cntr->brd;
         /*
          * base+9, Control register
          *
          * bit 5: enable timer 0 interrupts
          */
        brd->itr_ctrl_val &= ~0x20;
        outb(brd->itr_ctrl_val,brd->addr + 9);
}

/**
 * General function to stop a counter. Calls the board specific method.
 */
static int stopCNTR(struct DMMAT_CNTR* cntr)
{
        cntr->shutdownTimer = 1;
        del_timer_sync(&cntr->timer);

        // call the board-specific stop function
        cntr->stop(cntr);

        return 0;
}

/*
 * Function to start the counter on a MM32AT
 */
static int startMM16AT_CNTR(struct DMMAT_CNTR* cntr)
{
        unsigned long flags;
        struct DMMAT* brd = cntr->brd;
        int result = 0;

        spin_lock_irqsave(&brd->reglock,flags);

         /*
          * base+9, Control register
          *
          * bit 7: enable A/D interrupts
          * bit 3: enable timer 0 interrupts
          *     (can't set both 3 and 7 according to manual)
          */
        if (brd->itr_ctrl_val & 0x80) result = -EINVAL;
        else {
            brd->itr_ctrl_val |= 0x08;
            outb(brd->itr_ctrl_val,brd->addr + 9);
        }

        // Use mode 2 on the 82C54 for pulse counting -
        // it repeats indefinitely, generating interrupts
        // when it rolls over.
        setTimerClock(cntr->brd,0,2,0);

        spin_unlock_irqrestore(&brd->reglock,flags);

        return result;
}

/*
 * Function to start the counter on a MM32AT
 */
static int startMM32AT_CNTR(struct DMMAT_CNTR* cntr)
{
        unsigned long flags;
        struct DMMAT* brd = cntr->brd;
        spin_lock_irqsave(&brd->reglock,flags);
        /*
         * base+9, Control register:
         *
         * bit 7: enable A/D interrupts
         * bit 6: enable dio interrupts
         * bit 5: enable timer 0 interrupts
         * bit 1: 1=enable hardware A/D hardware clock
                 0=A/D software triggered by write to base+0
         * bit 0: 1=internal hardware trigger, counter/timer 1&2
               0=external trigger: DIN0, IO pin 48
         */
        brd->itr_ctrl_val |= 0x20;
        outb(brd->itr_ctrl_val,brd->addr + 9);

        // Use mode 2 on the 82C54 for pulse counting -
        // it repeats indefinitely, generating interrupts
        // when it rolls over.
        setTimerClock(cntr->brd,0,2,0);

        spin_unlock_irqrestore(&brd->reglock,flags);
        return 0;
}

/**
 * General function to start a counter. Calls the board specific method.
 */
static int startCNTR(struct DMMAT_CNTR* cntr,struct DMMAT_CNTR_Config* cfg)
{
        int result;
        unsigned long flags;
        struct DMMAT* brd = cntr->brd;
        unsigned long jnext;

        if (cntr->busy) stopCNTR(cntr);

        cntr->busy = 1;	// Set the busy flag

        memset(&cntr->status,0,sizeof(cntr->status));

        spin_lock_irqsave(&brd->reglock,flags);

        cntr->samples.head = cntr->samples.tail = 0;

        spin_unlock_irqrestore(&brd->reglock,flags);

        cntr->jiffiePeriod = (cfg->msecPeriod * HZ) / MSECS_PER_SEC;

        /* start the timer, give it more than one period at first */
        jnext = jiffies;
        jnext += cntr->jiffiePeriod - (jnext % cntr->jiffiePeriod);
        jnext += cntr->jiffiePeriod;

        cntr->timer.expires = jnext;
        cntr->shutdownTimer = 0;
        cntr->firstTime = 1;
        cntr->lastVal = 0;
        add_timer(&cntr->timer);

        // call the board-specific start function
        result = cntr->start(cntr);

        return result;
}

/**
 * Invoke filters.
 */
static void do_filters(struct DMMAT_A2D* a2d,dsm_sample_time_t tt,
    const short* dp)
{
        int i;

// #define DO_FILTER_DEBUG
#if defined(DEBUG) & defined(DO_FILTER_DEBUG)
        static size_t nfilt = 0;
        static int maxAvail = 0;
        static int minAvail = 99999;

        i = CIRC_SPACE(a2d->samples.head,a2d->samples.tail,DMMAT_A2D_SAMPLE_QUEUE_SIZE);
        if (i < minAvail) minAvail = i;
        if (i > maxAvail) maxAvail = i;
        if (!(nfilt++ % 1000)) {
                KLOG_DEBUG("minAvail=%d,maxAvail=%d\n",minAvail,maxAvail);
                maxAvail = 0;
                minAvail = 99999;
                nfilt = 1;
        }
#endif
        for (i = 0; i < a2d->nfilters; i++) {
                short_sample_t* osamp = (short_sample_t*)
                    GET_HEAD(a2d->samples,DMMAT_A2D_SAMPLE_QUEUE_SIZE);
                if (!osamp) {
                        // no output sample available
                        // still execute filter so its state is up-to-date.
                        struct a2d_sample toss;
                        if (!(a2d->status.missedSamples++ % 1000))
                            KLOG_WARNING("%s: missedSamples=%d\n",
                                a2d->deviceName,a2d->status.missedSamples);
                        a2d->filters[i].filter(
                            a2d->filters[i].filterObj,tt,dp,1,
                            (short_sample_t*)&toss);
                }
                else if (a2d->filters[i].filter(
                            a2d->filters[i].filterObj,tt,dp,1,osamp)) {
#ifdef __BIG_ENDIAN
                        // convert to little endian
                        int j;
                        osamp->id = cpu_to_le16(osamp->id);
                        for (j = 0; j < osamp->length / sizeof (short) - 1; j++)
                                osamp->data[j] = cpu_to_le16(osamp->data[j]);
#elif defined __LITTLE_ENDIAN
#else
#error "UNSUPPORTED ENDIAN-NESS"
#endif
                        INCREMENT_HEAD(a2d->samples,DMMAT_A2D_SAMPLE_QUEUE_SIZE);
                }
        }
}

/**
 * Tasklet that invokes filters on the data in a fifo sample.
 * The contents of the fifo sample is not necessarily a multiple
 * of the number of channels scanned.
 */
#ifdef USE_TASKLET
static void dmmat_a2d_bottom_half(unsigned long dev)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void dmmat_a2d_bottom_half(struct work_struct* work)
#else
static void dmmat_a2d_bottom_half(void* work)
#endif
{

#ifdef USE_TASKLET
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) dev;
#else
        struct DMMAT_A2D* a2d = container_of(work,struct DMMAT_A2D,worker);
#endif

        struct a2d_bh_data* tld = &a2d->bh_data;

        dsm_sample_time_t tt0;

        int saveChan = tld->saveSample.length / sizeof(short);

        // the consumer of fifo_samples.
        while (a2d->fifo_samples.head != a2d->fifo_samples.tail) {
                struct dsm_sample* insamp =
                    a2d->fifo_samples.buf[a2d->fifo_samples.tail];

                int nval = insamp->length / sizeof(short);
                // # of deltaT to back up for timetag of first sample
                int ndt = (nval - 1) / a2d->nchans;
                long dt = ndt * a2d->scanDeltaT;    // 1/10ths of msecs
                short *dp = (short *)insamp->data;
                short *ep = dp + nval;

                if (saveChan > 0) {     // leftover data
                    int n = min(ep-dp,a2d->nchans - saveChan);
                    memcpy(tld->saveSample.data+saveChan,dp,
                        n * sizeof(short));
                    dp += n;
                    saveChan += n;
                    if (saveChan == a2d->nchans) {
                        do_filters(a2d, tld->saveSample.timetag,
                                    tld->saveSample.data);
                        saveChan = tld->saveSample.length = 0;
                    }
                }

                // compute time of first scan in this fifo sample.
                // The fifoThreshold of MM16AT is fixed at 256 samples,
                // which may not be an integral number of scans.
                // This fifo sample may not contain an integral
                // number of scans and the first data points may be
                // the last channels of the previous scan

                // tt0 is conversion time of first compete scan in fifo
                tt0 = insamp->timetag - dt;  // fifo interrupt time

                for (; dp < ep; ) {
                    int n = (ep - dp);
                    if(n >= a2d->nchans) {
                        do_filters(a2d, tt0,dp);
                        dp += a2d->nchans;
                    }
                    else {
                        tld->saveSample.timetag = tt0;
                        memcpy(tld->saveSample.data,dp,n*sizeof(short));
                        tld->saveSample.length = n * sizeof(short);
                        saveChan = n;
                        dp += n;
                    }
                    tt0 += a2d->scanDeltaT;
                }
                INCREMENT_TAIL(a2d->fifo_samples,DMMAT_FIFO_SAMPLE_QUEUE_SIZE);
                // see fast bottom half for a discussion of this.
                if (((long)jiffies - (long)a2d->lastWakeup) > a2d->latencyJiffies ||
                        CIRC_SPACE(a2d->samples.head,a2d->samples.tail,
                        DMMAT_A2D_SAMPLE_QUEUE_SIZE) < DMMAT_A2D_SAMPLE_QUEUE_SIZE/2) {
                        wake_up_interruptible(&a2d->read_queue);
                        a2d->lastWakeup = jiffies;
                }
        }
        tld->saveSample.length = saveChan * sizeof(short);
}

/**
 * Tasklet that invokes filters on the data in a fifo sample.
 * The contents of the fifo sample must be a multiple
 * of the number of channels scanned.
 */
#ifdef USE_TASKLET
static void dmmat_a2d_bottom_half_fast(unsigned long dev)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void dmmat_a2d_bottom_half_fast(struct work_struct* work)
#else
static void dmmat_a2d_bottom_half_fast(void* work)
#endif
{

#ifdef USE_TASKLET
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) dev;
#else
        struct DMMAT_A2D* a2d = container_of(work,struct DMMAT_A2D,worker);
#endif

        dsm_sample_time_t tt0;

        KLOG_DEBUG("%s: tasklet entry, fifo head=%d,tail=%d\n",
            a2d->deviceName,a2d->fifo_samples.head,a2d->fifo_samples.tail);

        while (a2d->fifo_samples.head != a2d->fifo_samples.tail) {
                struct dsm_sample* insamp =
                    a2d->fifo_samples.buf[a2d->fifo_samples.tail];

                int nval = insamp->length / sizeof(short);
                short *dp = (short *)insamp->data;
                short *ep = dp + nval;

                // # of deltaTs to backup for first timetag
                int ndt = (nval - 1) / a2d->nchans;
                long dt = ndt * a2d->scanDeltaT;

                BUG_ON((nval % a2d->nchans) != 0);

                // tt0 is conversion time of first compete scan in fifo
                tt0 = insamp->timetag - dt;  // fifo interrupt time

                for (; dp < ep; ) {
                    do_filters(a2d, tt0,dp);
                    dp += a2d->nchans;
                    tt0 += a2d->scanDeltaT;
                }
                INCREMENT_TAIL(a2d->fifo_samples,DMMAT_FIFO_SAMPLE_QUEUE_SIZE);
                // We wake up the read_queue here so that the filters
                // don't have to know about it.  How often the
                // queue is woken depends on the requested latency.
                // 
                // If the user is sampling slowly, then the fifo threshold
                // will be set to a small number so that interrupts
                // happen at about the requested latency period.
                // The read_queue will be woken here if latencyJiffies have elapsed.
                // If sampling fast, then interrupts will happen
                // much faster than the latency period.  Since the
                // sample queue may fill up before latencyJiffies have elapsed,
                // we also wake the read_queue if the output sample queue is half full.
                if (((long)jiffies - (long)a2d->lastWakeup) > a2d->latencyJiffies ||
                        CIRC_SPACE(a2d->samples.head,a2d->samples.tail,
                        DMMAT_A2D_SAMPLE_QUEUE_SIZE) < DMMAT_A2D_SAMPLE_QUEUE_SIZE/2) {
                        wake_up_interruptible(&a2d->read_queue);
                        a2d->lastWakeup = jiffies;
                }
        }
}

/*
 * Handler for A2D interrupts. Called from board interrupt handler.
 */
static irqreturn_t dmmat_a2d_handler(struct DMMAT_A2D* a2d)
{
        // brd->reglock is locked before entering this function

        struct DMMAT* brd = a2d->brd;
        int flevel = a2d->getFifoLevel(a2d);
        int i;
        struct dsm_sample* samp;

        if (!(a2d->status.irqsReceived++ % 100)) KLOG_DEBUG("%s: %d irqs received\n",
            a2d->deviceName,a2d->status.irqsReceived);
        switch (flevel) {
        default:
        case 3:         // full or overflowed, we're falling behind
            if (!(a2d->status.fifoOverflows++ % 10))
                    KLOG_WARNING("%s: fifoOverflows=%d, restarting A2D\n",
                            a2d->deviceName,a2d->status.fifoOverflows);
            a2d->stop(a2d,0);
            a2d->start(a2d,0);
            return IRQ_HANDLED;
        case 2: break;	// at or above threshold, but not full (expected value)

        case 1:         // less than threshold, shouldn't happen
            if (!(a2d->status.fifoUnderflows++ % 1000))
                    KLOG_WARNING("%s: fifoUnderflows=%d,irqs=%d\n",
                            a2d->deviceName,
                            a2d->status.fifoUnderflows,
                            a2d->status.irqsReceived);
            // return IRQ_HANDLED;
            break;

        case 0:         // empty, shouldn't happen
            if (!(a2d->status.fifoEmpty++ % 100))
                    KLOG_WARNING("%s: fifoEmpty=%d\n",
                            a2d->deviceName,a2d->status.fifoUnderflows);
            return IRQ_HANDLED;
        }

        samp = GET_HEAD(a2d->fifo_samples,DMMAT_FIFO_SAMPLE_QUEUE_SIZE);
        if (!samp) {                // no output sample available
            a2d->status.missedSamples += (a2d->fifoThreshold / a2d->nchans);
            KLOG_WARNING("%s: missedSamples=%d\n",
                a2d->deviceName,a2d->status.missedSamples);
            for (i = 0; i < a2d->fifoThreshold; i++) inw(brd->addr);
            return IRQ_HANDLED;
        }

        samp->timetag = getSystemTimeTMsecs();

        // Finally!!!! the actual read from the hardware fifo.
        // All this overhead just to do this...
        insw(brd->addr,(short*)samp->data,a2d->fifoThreshold);
        samp->length = a2d->fifoThreshold * sizeof(short);

        /* On the MM16AT the fifo empty bit isn't set after reading
         * threshold number of values.  Perhaps it's a board bug?
         * On the MM32XAT the fifo empty bit is set at this point.
         */

#ifdef DEBUG
        flevel = a2d->getFifoLevel(a2d);
        if (flevel != 0) {
            if (!(a2d->status.fifoNotEmpty++ % 1000))
                    KLOG_DEBUG("fifo level=%d, base+7=0x%x,base+8=0x%x, base+10=0x%x\n",
                    flevel,inb(brd->addr + 7),inb(brd->addr + 8),inb(brd->addr + 10));
        }
#endif

        // KLOG_DEBUG("irq: timetag=%ld, data0=%d,dataN=%d\n", samp->timetag,
          //   ((short *)samp->data)[0],
            // ((short *)samp->data)[a2d->fifoThreshold-1]);

        /* increment head, this sample is ready for consumption */
        INCREMENT_HEAD(a2d->fifo_samples,DMMAT_FIFO_SAMPLE_QUEUE_SIZE);

#ifdef USE_TASKLET
        tasklet_schedule(&a2d->tasklet);
#elif defined(USE_MY_WORK_QUEUE)
        queue_work(work_queue,&a2d->worker);
#else
        schedule_work(&a2d->worker);
#endif
        return IRQ_HANDLED;
}

/*
 * Handler for counter 0 interrupts. Called from board interrupt handler.
 */
static irqreturn_t dmmat_cntr_handler(struct DMMAT_CNTR* cntr)
{

        cntr->rolloverSum += 65536;
        KLOG_DEBUG("%s: rolloverSum=%ld\n",
            cntr->deviceName,cntr->rolloverSum);
        cntr->status.irqsReceived++;
        return IRQ_HANDLED;
}

/*
 * General IRQ handler for the board.  Calls the A2D handler or
 * the pulse counter handler depending on who interrupted.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
static irqreturn_t dmmat_irq_handler(int irq, void* dev_id)
#else
static irqreturn_t dmmat_irq_handler(int irq, void* dev_id, struct pt_regs *regs)
#endif
{
        irqreturn_t result = IRQ_NONE;
        struct DMMAT* brd = (struct DMMAT*) dev_id;
        unsigned char status;

        spin_lock(&brd->reglock);
        status = inb(brd->itr_status_reg);

        if (!status) {      // not my interrupt
                spin_unlock(&brd->reglock);
                return result;
        }

        if (status & brd->cntr_itr_mask)
                result = dmmat_cntr_handler(brd->cntr);
        if (status & brd->ad_itr_mask)
                result = dmmat_a2d_handler(brd->a2d);

        // acknowledge interrupt
        outb(brd->itr_ack_val, brd->itr_ack_reg);

        spin_unlock(&brd->reglock);
        return result;
}

/************ D2A Utilities ****************/
/*
 * Set one or more analog output voltages on a MM16AT board.
 */
static int setD2A_MM16AT(struct DMMAT_D2A* d2a,
    struct DMMAT_D2A_Outputs* outputs,int iout)
{
        int i;
        unsigned long flags;
        struct DMMAT* brd = d2a->brd;

        spin_lock_irqsave(&brd->reglock,flags);

        for (i = 0; i < DMMAT_D2A_OUTPUTS_PER_BRD; i++) {
                if (outputs->active[i+iout]) {
                        char lsb;
                        char msb;
                        if (outputs->counts[i+iout] < d2a->cmin)
                                outputs->counts[i+iout] = d2a->cmin;
                        if (outputs->counts[i+iout] > d2a->cmax)
                                outputs->counts[i+iout] = d2a->cmax;
                        lsb = outputs->counts[i+iout] % 256;
                        msb = outputs->counts[i+iout] / 256;
                        outb(lsb,brd->addr + 1);
                        outb(msb,brd->addr + 4+i);

                        d2a->outputs.active[i] = 1;
                        d2a->outputs.counts[i] = outputs->counts[i+iout];
                }
        }
        inb(brd->addr + 4);           // causes all outputs to be updated
        spin_unlock_irqrestore(&brd->reglock,flags);
        return 0;
}

/*
 * Set one or more analog output voltages on a MM32AT.
 * This supports setting outputs on more than one board.
 */
static int setD2A_MM32AT(struct DMMAT_D2A* d2a,
    struct DMMAT_D2A_Outputs* outputs,int iout)
{
        int i;
        unsigned long flags;
        struct DMMAT* brd = d2a->brd;
        int nwait;
        int nout = 0;
        int nset = 0;

        // Check if setting more than one output
        for (i = 0; i < DMMAT_D2A_OUTPUTS_PER_BRD; i++)
                if (outputs->active[i+iout]) nout++;
        KLOG_DEBUG("nout=%d\n",nout);

        spin_lock_irqsave(&brd->reglock,flags);

        for (i = 0; i < DMMAT_D2A_OUTPUTS_PER_BRD; i++) {
                KLOG_DEBUG("active[%d]=%d\n",i+iout,outputs->active[i+iout]);
                if (outputs->active[i+iout]) {
                        char lsb;
                        char msb;
                        KLOG_DEBUG("counts[%d]=%d\n",i+iout,outputs->counts[i+iout]);
                        if (outputs->counts[i+iout] < d2a->cmin)
                                outputs->counts[i+iout] = d2a->cmin;
                        if (outputs->counts[i+iout] > d2a->cmax)
                                outputs->counts[i+iout] = d2a->cmax;
                        lsb = outputs->counts[i+iout] % 256;
                        msb = outputs->counts[i+iout] / 256;
                        msb += i << 6;
                        nset++;
                        if (nset < nout) msb |= 0x20;
                        KLOG_DEBUG("lsb=%d,msb=%d\n",(int)lsb,(int)msb);

                        if (nset > 1) {
                                // Check DAC busy if we have already set an output
                                // Took nwait=3 on 400MHz viper without udelay
                                nwait = 0;
                                // according to manual DACBUSY=1 for 10usec
                                while(inb(brd->addr + 4) & 0x80 && nwait++ < 5)
                                    udelay(5);
                                KLOG_DEBUG("nwait=%d\n",nwait);
                        }

                        outb(lsb,brd->addr + 4);
                        outb(msb,brd->addr + 5);

                        d2a->outputs.active[i] = 1;
                        d2a->outputs.counts[i] = outputs->counts[i+iout];
                }
        }
        // This check for nset>0 is unnecessary since this function shouldn't be
        // called if no outputs were to be changed, but might as well check it.
        if (nset > 0) {
            nwait = 0;
            while(inb(brd->addr + 4) & 0x80 && nwait++ < 5) udelay(5);
            KLOG_DEBUG("nwait=%d\n",nwait);
        }

        spin_unlock_irqrestore(&brd->reglock,flags);
        return 0;
}

/* 
 * Set the D2A outputs on 1 or more boards.
 */
static int setD2A_mult(struct DMMAT_D2A* d2a,struct DMMAT_D2A_Outputs* outputs)
{
        int i;
        struct DMMAT* brd = d2a->brd;
        int res = 0;
        // how many boards to affect
        int nbrds = (outputs->nout + DMMAT_D2A_OUTPUTS_PER_BRD - 1) /
            DMMAT_D2A_OUTPUTS_PER_BRD;
        nbrds = min(nbrds,numboards-brd->num);

        for (i = 0; i < nbrds; i++) {
                d2a = brd->d2a;
                res = d2a->setD2A(d2a,outputs,i * DMMAT_D2A_OUTPUTS_PER_BRD);
                if (res) return res;
                brd++;
        }
        return res;
}

/* 
 * Get the D2A outputs on 1 or more boards.
 */
static void getD2A_mult(struct DMMAT_D2A* d2a,struct DMMAT_D2A_Outputs* outputs)
{
        int i,j,iout = 0;
        struct DMMAT* brd = d2a->brd;
        // how many boards to check
        int nbrds = numboards-brd->num;

        for (i = 0; i < nbrds; i++) {
                d2a = brd->d2a;
                for (j = 0; j < DMMAT_D2A_OUTPUTS_PER_BRD; j++) {
                        outputs->counts[iout] = d2a->outputs.counts[j];
                        outputs->active[iout++] = d2a->outputs.active[j];
                }
                brd++;
        }
        outputs->nout = nbrds * DMMAT_D2A_OUTPUTS_PER_BRD;
}

/* 
 * Get the D2A conversion factors for 1 or more boards
 */
static void getD2A_conv(struct DMMAT_D2A* d2a,struct DMMAT_D2A_Conversion* conv)
{
        int i,j,iout = 0;
        struct DMMAT* brd = d2a->brd;
        // how many boards to check
        int nbrds = numboards-brd->num;

        for (i = 0; i < nbrds; i++) {
                d2a = brd->d2a;
                for (j = 0; j < DMMAT_D2A_OUTPUTS_PER_BRD; j++) {
                        conv->vmin[iout] = d2a->vmin;
                        conv->vmax[iout] = d2a->vmax;
                        conv->cmin[iout] = d2a->cmin;
                        conv->cmax[iout++] = d2a->cmax;
                }
                brd++;
        }
}
/*
 * Both the A2D and the pulse counter device use interrupts.
 * If an interrupt handler has already been set up for this
 * board, then do nothing.
 */
static int dmd_mmat_add_irq_user(struct DMMAT* brd,int user_type)
{
        int result;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        if ((result = mutex_lock_interruptible(&brd->irqreq_mutex)))
                return result;
#else
        if ((result = down_interruptible(&brd->irqreq_mutex)))
                return result;
#endif
        if (brd->irq == 0) {
                int irq;
                BUG_ON(brd->irq_users[0] + brd->irq_users[1] != 0);

                /* We don't use SA_INTERRUPT flag here.  We don't
                 * need to block other interrupts while we're running.
                 * Note: request_irq can wait, so spin_lock not advised.
                 */
#ifdef GET_SYSTEM_ISA_IRQ
                irq = GET_SYSTEM_ISA_IRQ(irqs[brd->num]);
#else
                irq = irqs[brd->num];
#endif
                KLOG_INFO("board %d: requesting irq: %d,%d\n",brd->num,irqs[brd->num],irq);
                result = request_irq(irq,dmmat_irq_handler,IRQF_SHARED,"dmd_mmat",brd);
                if (result) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
                        mutex_unlock(&brd->irqreq_mutex);
#else
                        up(&brd->irqreq_mutex);
#endif
                        return result;
                }
                brd->irq = irq;
        }
        else {
                BUG_ON(brd->irq_users[0] + brd->irq_users[1] <= 0);
                /* DMM16AT cannot do A2D interrupts and counter
                 * interrupts simultaneously.  So, effectively
                 * this card can be used for A2D or counting,
                 * not both.
                 */
                if (types[brd->num] == DMM16AT_BOARD &&
                        brd->irq_users[!user_type] > 0) {
                        KLOG_ERR("board %d is a DMM16AT and cannot do interrupt driven A2D and pulse counting simultaneously",brd->num);
                        result = -EINVAL;
                }
        }
        brd->irq_users[user_type]++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        mutex_unlock(&brd->irqreq_mutex);
#else
        up(&brd->irqreq_mutex);
#endif
        return result;
}

static int dmd_mmat_remove_irq_user(struct DMMAT* brd,int user_type)
{
        int result;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        if ((result = mutex_lock_interruptible(&brd->irqreq_mutex)))
                return result;
#else
        if ((result = down_interruptible(&brd->irqreq_mutex)))
                return result;
#endif
        brd->irq_users[user_type]--;
        if (brd->irq_users[0] + brd->irq_users[1] == 0) {
                KLOG_NOTICE("freeing irq %d\n",brd->irq);
                free_irq(brd->irq,brd);
                brd->irq = 0;
        }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        mutex_unlock(&brd->irqreq_mutex);
#else
        up(&brd->irqreq_mutex);
#endif
        return result;
}

/************ A2D File Operations ****************/

/* Most likely an A2D has only been opened by one thread.
 * A second reader would cause problems, but a second
 * thread could do GET_STATUS ioctls without causing problems.
 */
static int dmmat_open_a2d(struct inode *inode, struct file *filp)
{
        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int ia2d = i % DMMAT_DEVICES_PER_BOARD;

        struct DMMAT* brd;
        struct DMMAT_A2D* a2d;
        int result = 0;

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        KLOG_DEBUG("open_a2d, iminor=%d,ibrd=%d,ia2d=%d,numboards=%d\n",
            i,ibrd,ia2d,numboards);

        if (ibrd >= numboards) return -ENXIO;
        if (ia2d != 0) return -ENXIO;

        brd = board + ibrd;
        a2d = brd->a2d;

        filp->private_data = a2d;

        memset(&a2d->status,0,sizeof(a2d->status));

        if (atomic_inc_return(&a2d->num_opened) == 1) 
            result = dmd_mmat_add_irq_user(brd,0);
        KLOG_DEBUG("open_a2d, num_opened=%d\n",
            atomic_read(&a2d->num_opened));

        return result;
}

static int dmmat_release_a2d(struct inode *inode, struct file *filp)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) filp->private_data;

        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int ia2d = i % DMMAT_DEVICES_PER_BOARD;

        struct DMMAT* brd;
        int result = 0;

        KLOG_DEBUG("release_a2d, iminor=%d,ibrd=%d,ia2d=%d,numboards=%d\n",
            i,ibrd,ia2d,numboards);

        if (ibrd >= numboards) return -ENXIO;
        if (ia2d != 0) return -ENXIO;

        brd = board + ibrd;
        BUG_ON(a2d != brd->a2d);

        if (atomic_dec_and_test(&a2d->num_opened)) {
            if (a2d->running) stopA2D(a2d,1);
            result = dmd_mmat_remove_irq_user(brd,0);
            /* cleanup filters */
            for (i = 0; i < a2d->nfilters; i++) {
                struct a2d_filter_info* fcfg;
                fcfg = &a2d->filters[i];
                if (fcfg->filterObj && fcfg->fcleanup) 
                    fcfg->fcleanup(fcfg->filterObj);
                fcfg->filterObj = 0;
                fcfg->fcleanup = 0;
            }
        }
        KLOG_DEBUG("release_a2d, num_opened=%d\n",
            atomic_read(&a2d->num_opened));

        return result;
}

static ssize_t dmmat_read_a2d(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) filp->private_data;
        size_t countreq = count;
        int n;
        struct dsm_sample* insamp;

        size_t bytesLeft;
        char* sampPtr = a2d->sampPtr;

// #define OUT_DEBUG
#if defined(DEBUG) & defined(OUT_DEBUG)
        static int nreads = 0;
        static size_t maxOcount = 0;
        static size_t minOcount = 9999999;
#endif

        KLOG_DEBUG("head=%d,tail=%d\n",
            a2d->samples.head,a2d->samples.tail);

        if(!sampPtr && a2d->samples.head == a2d->samples.tail) {
                if (filp->f_flags & O_NONBLOCK) return -EAGAIN;
                if (wait_event_interruptible(a2d->read_queue,
                      a2d->samples.head != a2d->samples.tail)) return -ERESTARTSYS;
        }

        if (!sampPtr) {
                insamp = a2d->samples.buf[a2d->samples.tail];
                sampPtr = (char*)insamp;
                bytesLeft = insamp->length + SIZEOF_DSM_SAMPLE_HEADER;
        }
        else bytesLeft = a2d->sampBytesLeft;

        for ( ; count; ) {

            KLOG_DEBUG("count=%d,copied=%d,sampBytesLeft=%d\n",
                count,countreq-count,bytesLeft);
            if ((n = min(bytesLeft,count)) > 0) {
                    if (copy_to_user(buf,sampPtr,n)) return -EFAULT;
                    count -= n;
                    buf += n;
                    sampPtr += n;
                    bytesLeft -= n;
            }
            if (bytesLeft == 0) {
                    // finished with sample
                    INCREMENT_TAIL(a2d->samples,DMMAT_A2D_SAMPLE_QUEUE_SIZE);
                    if (a2d->samples.head == a2d->samples.tail) {
                            KLOG_DEBUG("no more samples,copied=%d\n",countreq-count);
                            a2d->sampPtr = 0;
#if defined(DEBUG) & defined(OUT_DEBUG)
                            if (countreq - count > maxOcount) maxOcount = countreq - count;
                            if (countreq - count < minOcount) minOcount = countreq - count;
                            if (!(nreads++ % 100))  {
                                KLOG_DEBUG("minOcount=%u, maxOcount=%u\n",
                                    minOcount,maxOcount);
                                maxOcount = 0;
                                minOcount = 9999999;
                                nreads = 1;
                            }
#endif
                            return countreq - count;
                    }
                    insamp = a2d->samples.buf[a2d->samples.tail];
                    sampPtr = (char*)insamp;
                    bytesLeft = insamp->length + SIZEOF_DSM_SAMPLE_HEADER;
            }
        }
#if defined(DEBUG) & defined(OUT_DEBUG)
        if (countreq - count > maxOcount) maxOcount = countreq - count;
        if (countreq - count < minOcount) minOcount = countreq - count;
        if (!(nreads++ % 100))  {
            KLOG_DEBUG("minOcount=%u, maxOcount=%u\n",
                minOcount,maxOcount);
            maxOcount = 0;
            minOcount = 9999999;
            nreads = 1;
        }
#endif
        a2d->sampPtr = sampPtr;
        a2d->sampBytesLeft = bytesLeft;
        return countreq - count;
}

static int dmmat_ioctl_a2d(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) filp->private_data;
        struct DMMAT* brd;
        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        // int ia2d = i % DMMAT_DEVICES_PER_BOARD;
        int result = -EINVAL,err = 0;
        void __user *userptr = (void __user *) arg;
        int len;

        KLOG_DEBUG("ioctl_a2d, iminor=%d,ibrd=%d,numboards=%d\n",
            i,ibrd,numboards);

         /* don't even decode wrong cmds: better returning
          * ENOTTY than EFAULT */
        if (_IOC_TYPE(cmd) != DMMAT_IOC_MAGIC &&
            _IOC_TYPE(cmd) != NIDAS_A2D_IOC_MAGIC) return -ENOTTY;
        if (_IOC_NR(cmd) > DMMAT_IOC_MAXNR) return -ENOTTY;

        /*
         * the type is a bitmask, and VERIFY_WRITE catches R/W
         * transfers. Note that the type is user-oriented, while
         * verify_area is kernel-oriented, so the concept of "read" and
         * "write" is reversed
         */
        if (_IOC_DIR(cmd) & _IOC_READ)
                err = !access_ok(VERIFY_WRITE, userptr,_IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                err =  !access_ok(VERIFY_READ, userptr, _IOC_SIZE(cmd));
        if (err) return -EFAULT;

        if (ibrd >= numboards) return -ENXIO;

        brd = board + ibrd;

        BUG_ON(brd != a2d->brd);

        switch (cmd) 
        {

        case NIDAS_A2D_GET_NCHAN:
            {
                u32 nchan = a2d->getNumChannels(a2d);
                if (copy_to_user(userptr,&nchan,sizeof(nchan)))
                    return -EFAULT;
                result = 0;
                break;
            }
        case NIDAS_A2D_SET_CONFIG:      /* user set */
            {
                struct nidas_a2d_config cfg;
                if (MAX_A2D_CHANNELS < MAX_DMMAT_A2D_CHANNELS) {
                        KLOG_ERR("programming error: MAX_A2D_CHANNELS=%d should be at least %d\n",
                        MAX_A2D_CHANNELS,MAX_DMMAT_A2D_CHANNELS);
                        return -EINVAL;
                }
                if (copy_from_user(&cfg,userptr,
                        sizeof(struct nidas_a2d_config))) return -EFAULT;
                result = configA2D(a2d,&cfg);
            }
	    break;
        case NIDAS_A2D_CONFIG_SAMPLE:      /* user set */
            {
                /*
                 * copy structure without the contents
                 * of cfg.filterData, which has variable length
                 * depending on the filter. Then allocate another
                 * struct with an additional cfg.nFilterData bytes
                 * and copy into it.
                 */
                struct nidas_a2d_sample_config cfg;
                struct nidas_a2d_sample_config* cfgp;
                len = _IOC_SIZE(cmd);
                if (copy_from_user(&cfg,userptr,len) != 0)
                            return -EFAULT;

                // kmalloc enough structure for additional filter data
                len = sizeof(struct nidas_a2d_sample_config) +
                            cfg.nFilterData;
                cfgp = kmalloc(len,GFP_KERNEL);
                if (!cfgp) {
                        result = -ENOMEM;
                        break;
                }
                if (copy_from_user(cfgp, userptr, len) != 0) {
                        kfree(cfgp);
                        result = -EFAULT;
                        break;
                }
                result = addA2DSampleConfig(a2d,cfgp);
                kfree(cfgp);
            }
	    break;
        case DMMAT_A2D_GET_STATUS:	/* user get of status struct */
                if (copy_to_user(userptr,&a2d->status,
                    sizeof(struct DMMAT_A2D_Status))) return -EFAULT;
                result = 0;
                break;
        case DMMAT_A2D_START:
                result = startA2D(a2d,1);
                break;
        case DMMAT_A2D_STOP:
                result = stopA2D(a2d,1);
                break;
        case DMMAT_A2D_DO_AUTOCAL:
                if (types[brd->num] != DMM32XAT_BOARD) {
                        KLOG_ERR("board %d is not a DMM32AT and does not support auto-calibration\n",brd->num);
                        result = -EINVAL;
                }
                else result = startMM32XAT_AutoCal(a2d);
                break;
        default:
                result = -ENOTTY;
                break;
        }
        return result;
}

/*
 * Implementation of poll fops.
 */
static unsigned int dmmat_poll_a2d(struct file *filp, poll_table *wait)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) filp->private_data;
        unsigned int mask = 0;
        poll_wait(filp, &a2d->read_queue, wait);
        if (a2d->sampPtr || a2d->samples.head != a2d->samples.tail) 
            mask |= POLLIN | POLLRDNORM;    /* readable */
        if (mask) {
            KLOG_DEBUG("mask=%x\n",mask);
        }
        return mask;
}

/************ Pulse Counter File Operations ****************/
static int dmmat_open_cntr(struct inode *inode, struct file *filp)
{
        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int icntr = i % DMMAT_DEVICES_PER_BOARD;

        struct DMMAT* brd;
        struct DMMAT_CNTR* cntr;
        int result;

        KLOG_DEBUG("open_cntr, i=%d,ibrd=%d,icntr=%d,numboards=%d\n",
            i,ibrd,icntr,numboards);

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        if (ibrd >= numboards) return -ENXIO;
        if (icntr != 1) return -ENXIO;

        brd = board + ibrd;
        cntr = brd->cntr;

        result = dmd_mmat_add_irq_user(brd,1);

        filp->private_data = cntr;

        return result;
}

static int dmmat_release_cntr(struct inode *inode, struct file *filp)
{
        struct DMMAT_CNTR* cntr = (struct DMMAT_CNTR*) filp->private_data;

        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int icntr = i % DMMAT_DEVICES_PER_BOARD;

        struct DMMAT* brd;
        int result;

        if (ibrd >= numboards) return -ENXIO;
        if (icntr != 1) return -ENXIO;

        brd = board + ibrd;
        BUG_ON(cntr != brd->cntr);

        result = stopCNTR(cntr);

        result = dmd_mmat_remove_irq_user(brd,1);

        return 0;
}

static ssize_t dmmat_read_cntr(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct DMMAT_CNTR* cntr = (struct DMMAT_CNTR*) filp->private_data;
        size_t countreq = count;
        int n = SIZEOF_DSM_SAMPLE_HEADER + sizeof(long);
        struct dsm_sample* insamp;

        KLOG_DEBUG("head=%d,tail=%d\n",
            cntr->samples.head,cntr->samples.tail);

        if(cntr->samples.head == cntr->samples.tail) {
                if (filp->f_flags & O_NONBLOCK) return -EAGAIN;
                if (wait_event_interruptible(cntr->read_queue,
                      cntr->samples.head != cntr->samples.tail)) return -ERESTARTSYS;
        }

        for ( ;cntr->samples.head != cntr->samples.tail &&
            count > SIZEOF_DSM_SAMPLE_HEADER + sizeof(long); ) {

                insamp = cntr->samples.buf[cntr->samples.tail];
                if (copy_to_user(buf,insamp,n)) return -EFAULT;
                count -= n;
                buf += n;
                INCREMENT_TAIL(cntr->samples,DMMAT_CNTR_QUEUE_SIZE);
        }
        KLOG_DEBUG("copied=%d\n",countreq - count);
        return countreq - count;
}

static int dmmat_ioctl_cntr(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
        struct DMMAT_CNTR* cntr = (struct DMMAT_CNTR*) filp->private_data;
        struct DMMAT* brd;
        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        // int icntr = i % DMMAT_DEVICES_PER_BOARD;
        int result = -EINVAL,err = 0;
        void __user *userptr = (void __user *) arg;

         /* don't even decode wrong cmds: better returning
          * ENOTTY than EFAULT */
        if (_IOC_TYPE(cmd) != DMMAT_IOC_MAGIC) return -ENOTTY;
        if (_IOC_NR(cmd) > DMMAT_IOC_MAXNR) return -ENOTTY;

        /*
         * the type is a bitmask, and VERIFY_WRITE catches R/W
         * transfers. Note that the type is user-oriented, while
         * verify_area is kernel-oriented, so the concept of "read" and
         * "write" is reversed
         */
        if (_IOC_DIR(cmd) & _IOC_READ)
                err = !access_ok(VERIFY_WRITE, userptr,
                    _IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                err =  !access_ok(VERIFY_READ, userptr,
                    _IOC_SIZE(cmd));
        if (err) return -EFAULT;

        if (ibrd >= numboards) return -ENXIO;

        brd = board + ibrd;

        BUG_ON(brd != cntr->brd);

        switch (cmd) 
        {
        case DMMAT_CNTR_START:
                {
                struct DMMAT_CNTR_Config cfg;
                if (copy_from_user(&cfg,userptr,
                        sizeof(struct DMMAT_CNTR_Config))) return -EFAULT;
                result = startCNTR(cntr,&cfg);
                }
                break;
        case DMMAT_CNTR_STOP:      /* user set */
                result = stopCNTR(cntr);
                break;
        case DMMAT_CNTR_GET_STATUS:	/* user get of status struct */
                if (copy_to_user(userptr,&cntr->status,
                    sizeof(struct DMMAT_CNTR_Status))) return -EFAULT;
                result = 0;
                break;
        default:
                result = -ENOTTY;
                break;
        }
        return result;
}

/*
 * Implementation of poll fops.
 */
unsigned int dmmat_poll_cntr(struct file *filp, poll_table *wait)
{
        struct DMMAT_CNTR* cntr = (struct DMMAT_CNTR*) filp->private_data;
        unsigned int mask = 0;
        poll_wait(filp, &cntr->read_queue, wait);
        if (cntr->samples.head != cntr->samples.tail) 
            mask |= POLLIN | POLLRDNORM;    /* readable */
        if (mask) {
            KLOG_DEBUG("mask=%x\n",mask);
        }
        return mask;
}

/************ D2A File Operations ****************/
/*
 * Opening a d2a provides access to the D2A outputs on that
 * board and the D2A outputs on successive boards
 * in the system.  By opening the first DMMAT D2A device
 * the user can control all outputs.
 */
static int dmmat_open_d2a(struct inode *inode, struct file *filp)
{
        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int id2a = i % DMMAT_DEVICES_PER_BOARD;

        struct DMMAT* brd;
        struct DMMAT_D2A* d2a;

        KLOG_DEBUG("open_d2a, i=%d,ibrd=%d,id2a=%d,numboards=%d\n",
            i,ibrd,id2a,numboards);

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        if (ibrd >= numboards) return -ENXIO;
        if (id2a != 2) return -ENXIO;

        brd = board + ibrd;
        d2a = brd->d2a;

        filp->private_data = d2a;

        return 0;
}

/* release currently does nothing, other than check arguments */
static int dmmat_release_d2a(struct inode *inode, struct file *filp)
{
        struct DMMAT_D2A* d2a = (struct DMMAT_D2A*) filp->private_data;

        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int id2a = i % DMMAT_DEVICES_PER_BOARD;

        struct DMMAT* brd;
        // int result;

        if (ibrd >= numboards) return -ENXIO;
        if (id2a != 2) return -ENXIO;

        brd = board + ibrd;
        BUG_ON(d2a != brd->d2a);

        // return voltage to default?

        return 0;
}

static int dmmat_ioctl_d2a(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
        struct DMMAT_D2A* d2a = (struct DMMAT_D2A*) filp->private_data;
        struct DMMAT* brd;
        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int id2a = i % DMMAT_DEVICES_PER_BOARD;
        int result = -EINVAL,err = 0;
        void __user *userptr = (void __user *) arg;

        if (ibrd >= numboards) return -ENXIO;
        if (id2a != 2) return -ENXIO;

         /* don't even decode wrong cmds: better returning
          * ENOTTY than EFAULT */
        if (_IOC_TYPE(cmd) != DMMAT_IOC_MAGIC) return -ENOTTY;
        if (_IOC_NR(cmd) > DMMAT_IOC_MAXNR) return -ENOTTY;

        /*
         * the type is a bitmask, and VERIFY_WRITE catches R/W
         * transfers. Note that the type is user-oriented, while
         * verify_area is kernel-oriented, so the concept of "read" and
         * "write" is reversed
         */
        if (_IOC_DIR(cmd) & _IOC_READ)
                err = !access_ok(VERIFY_WRITE, userptr,
                    _IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                err =  !access_ok(VERIFY_READ, userptr,
                    _IOC_SIZE(cmd));
        if (err) return -EFAULT;

        brd = board + ibrd;

        BUG_ON(d2a != brd->d2a);

        switch (cmd) 
        {
        case DMMAT_D2A_GET_NOUTPUTS:
                result = (numboards-brd->num) *
                    DMMAT_D2A_OUTPUTS_PER_BRD;
                break;
        case DMMAT_D2A_GET_CONVERSION:	/* user get of conversion struct */
                {
                struct DMMAT_D2A_Conversion conv;
                getD2A_conv(d2a,&conv);
                if (copy_to_user(userptr,&conv,
                    sizeof(struct DMMAT_D2A_Conversion))) return -EFAULT;
                result = 0;
                }
                break;
        case DMMAT_D2A_SET:      /* user set */
                {
                struct DMMAT_D2A_Outputs outputs;
                if (copy_from_user(&outputs,userptr,
                        sizeof(struct DMMAT_D2A_Outputs))) return -EFAULT;
                result = setD2A_mult(d2a,&outputs);
                }
                break;
        case DMMAT_D2A_GET:      /* user get */
                {
                struct DMMAT_D2A_Outputs outputs;
                getD2A_mult(d2a,&outputs);
                if (copy_to_user(userptr,&outputs,
                    sizeof(struct DMMAT_D2A_Outputs))) return -EFAULT;
                result = 0;
                }
                break;
        default:
                result = -ENOTTY;
                break;
        }
        return result;
}


static struct file_operations a2d_fops = {
        .owner   = THIS_MODULE,
        .read    = dmmat_read_a2d,
        .poll    = dmmat_poll_a2d,
        .open    = dmmat_open_a2d,
        .ioctl   = dmmat_ioctl_a2d,
        .release = dmmat_release_a2d,
        .llseek  = no_llseek,
};

static struct file_operations cntr_fops = {
        .owner   = THIS_MODULE,
        .read    = dmmat_read_cntr,
        .poll    = dmmat_poll_cntr,
        .open    = dmmat_open_cntr,
        .ioctl   = dmmat_ioctl_cntr,
        .release = dmmat_release_cntr,
        .llseek  = no_llseek,
};

static struct file_operations d2a_fops = {
        .owner   = THIS_MODULE,
        .open    = dmmat_open_d2a,
        .ioctl   = dmmat_ioctl_d2a,
        .release = dmmat_release_d2a,
        .llseek  = no_llseek,
};

#ifdef OTHER_FUNCTIONALITY
// TODO: implement
static struct file_operations dio_fops = {
        .owner   = THIS_MODULE,
        .open    = dmmat_open_dio,
        .ioctl   = dmmat_ioctl_dio,
        .release = dmmat_release_dio,
        .llseek  = no_llseek,
};

#endif

static int init_a2d(struct DMMAT* brd,int type)
{
        int result;
        struct DMMAT_A2D* a2d;
        dev_t devno;

        result = -ENOMEM;
        brd->a2d = a2d = kmalloc(sizeof(struct DMMAT_A2D),GFP_KERNEL);
        if (!a2d) return result;
        memset(a2d,0, sizeof(struct DMMAT_A2D));

        a2d->brd = brd;

        // for informational messages only at this point
        sprintf(a2d->deviceName,"/dev/dmmat_a2d%d",brd->num);

        // a2d device
        cdev_init(&a2d->cdev,&a2d_fops);
        a2d->cdev.owner = THIS_MODULE;
        devno = MKDEV(MAJOR(dmmat_device),brd->num*DMMAT_DEVICES_PER_BOARD);
        KLOG_DEBUG("%s: MKDEV, major=%d minor=%d\n",
                a2d->deviceName,MAJOR(devno),MINOR(devno));

        atomic_set(&a2d->num_opened,0);

        switch (type) {
        case DMM16AT_BOARD:
                /* board registers */
                brd->itr_status_reg = brd->addr + 8;
                brd->ad_itr_mask = 0x10;

                brd->itr_ack_reg = brd->addr + 8;
                brd->itr_ack_val = 0x00;        // any value

                a2d->start = startMM16AT_A2D;
                a2d->stop = stopMM16AT_A2D;
                a2d->getNumChannels = getNumA2DChannelsMM16AT;
                a2d->selectChannels = selectA2DChannelsMM16AT;
                a2d->getConvRateSetting = getConvRateMM16AT;
                a2d->getGainSetting = getGainSettingMM16AT;
                a2d->getFifoLevel = getFifoLevelMM16AT;
                a2d->resetFifo = resetFifoMM16AT;
                a2d->waitForA2DSettle = waitForA2DSettleMM16AT;
                a2d->maxFifoThreshold = 256;
#ifdef USE_TASKLET
                tasklet_init(&a2d->tasklet,dmmat_a2d_bottom_half,
                    (unsigned long)a2d);
#else

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
                INIT_WORK(&a2d->worker,dmmat_a2d_bottom_half);
#else
                INIT_WORK(&a2d->worker,dmmat_a2d_bottom_half,&a2d->worker);
#endif
#endif
            break;
        case DMM32XAT_BOARD:
                brd->itr_status_reg = brd->addr + 9;
                brd->ad_itr_mask = 0x80;

                brd->itr_ack_reg = brd->addr + 8;
                brd->itr_ack_val = 0x08;

                a2d->start = startMM32XAT_A2D;
                a2d->stop = stopMM32XAT_A2D;
                a2d->getNumChannels = getNumA2DChannelsMM32XAT;
                a2d->selectChannels = selectA2DChannelsMM32XAT;
                a2d->getConvRateSetting = getConvRateMM32XAT;
                a2d->getGainSetting = getGainSettingMM32XAT;
                a2d->getFifoLevel = getFifoLevelMM32XAT;
                a2d->resetFifo = resetFifoMM32XAT;
                a2d->waitForA2DSettle = waitForA2DSettleMM32XAT;
                a2d->maxFifoThreshold = 512;
#ifdef USE_TASKLET
                tasklet_init(&a2d->tasklet,dmmat_a2d_bottom_half_fast,
                    (unsigned long)a2d);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
                INIT_WORK(&a2d->worker,dmmat_a2d_bottom_half_fast);
#else
                INIT_WORK(&a2d->worker,dmmat_a2d_bottom_half_fast,&a2d->worker);
#endif
#endif
                // full reset
                outb(0x20, brd->addr + 8);

                // enable enhanced features on this board
                outb(0x03,brd->addr + 8);	// set page 3
                outb(0xa6,brd->addr + 15);	// enable enhanced features
                outb(0x00, brd->addr + 8);      // back to page 0
                break;
        }

        /*
         * Samples from the FIFO. Data portion must be
         * as big as largest FIFO threshold.
         */
        result = alloc_dsm_circ_buf(&a2d->fifo_samples,
            a2d->maxFifoThreshold * sizeof(short),
            DMMAT_FIFO_SAMPLE_QUEUE_SIZE);
        if (result) return result;

        /*
         * Output samples. Data portion just needs to be
         * as big as the number of channels on the board
         * plus one for the sample id.
         */
        result = alloc_dsm_circ_buf(&a2d->samples,
            (MAX_DMMAT_A2D_CHANNELS + 1) * sizeof(short),
            DMMAT_A2D_SAMPLE_QUEUE_SIZE);
        if (result) return result;

        a2d->gainConvSetting = 0;	// default value

        init_waitqueue_head(&a2d->read_queue);

        /* After calling cdev_add the device is "live"
         * and ready for user operation.
         */
        result = cdev_add(&a2d->cdev, devno, 1);
        return result;
}
static void cleanup_a2d(struct DMMAT* brd)
{
        struct DMMAT_A2D* a2d = brd->a2d;
        int i;

        if (!a2d) return;

        stopA2D(a2d,1);

        cdev_del(&a2d->cdev);

        free_dsm_circ_buf(&a2d->samples);

        free_dsm_circ_buf(&a2d->fifo_samples);

        if (a2d->filters) {
                for (i = 0; i < a2d->nfilters; i++) {
                        if (a2d->filters[i].channels)
                                kfree(a2d->filters[i].channels);
                        a2d->filters[i].channels = 0;
                }
                kfree(a2d->filters);
                a2d->filters = 0;
        }

#ifdef USE_TASKLET
        tasklet_kill(&a2d->tasklet);
#elif defined(USE_MY_WORK_QUEUE)
#endif
        kfree(a2d);
        brd->a2d = 0;
}

/**
 * Timer function which fetches the current counter value,
 * makes a timetagged sample out of it, puts it in
 * the circular buffer for the read method and wakens
 * the read queue.  This timer function is called at the
 * rate requested by the user.
 */
static void cntr_timer_fn(unsigned long arg)
{
        struct DMMAT_CNTR* cntr = (struct DMMAT_CNTR*) arg;
        struct DMMAT* brd = cntr->brd;

        unsigned long jnext = jiffies;
        unsigned long cval;
        unsigned long total;
        unsigned long flags;

        /*
         * cntr->rolloverSum is incremented by 65535 in the
         * interrupt function every time there is a 2^16 rollover.
         * So the total count for the sampling period is the
         * sum of the rollovers, plus the current counter value,
         * minus the counter value at the end of the previous sampling
         * period.  Note that we don't reset the hardware counter.
         */
        spin_lock_irqsave(&brd->reglock,flags);

        // timer counts down from 65536
        cval = 65536 - readLatchedTimer(brd,0);

        total = cntr->rolloverSum + cval - cntr->lastVal;

        KLOG_DEBUG("%s: lastVal=%d,cval=%ld,rollover=%ld,total=%ld\n",
            cntr->deviceName,cntr->lastVal,cval,cntr->rolloverSum,total);

        cntr->rolloverSum = 0;
        spin_unlock_irqrestore(&brd->reglock,flags);

        cntr->lastVal = cval;

        // The first counting period is the wrong length, so ignore the data.
        if (!cntr->firstTime) {
                struct cntr_sample* osamp;
                osamp = (struct cntr_sample*) GET_HEAD(cntr->samples,
                    DMMAT_CNTR_QUEUE_SIZE);
                if (!osamp && !(cntr->status.lostSamples++ % 100))
                        KLOG_WARNING("%s: lostSamples=%d\n",
                            cntr->deviceName,cntr->status.lostSamples);
                else {
                        osamp->timetag = getSystemTimeTMsecs();

                        // We'll standardize to little endian sensor output.
                        osamp->data = cpu_to_le32(total);
                        osamp->length = sizeof(osamp->data);
                        INCREMENT_HEAD(cntr->samples,DMMAT_CNTR_QUEUE_SIZE);
                        // TODO: implement buffering and latency
                        wake_up_interruptible(&cntr->read_queue);
                }
        }
        else cntr->firstTime = 0;

        jnext += cntr->jiffiePeriod - (jnext % cntr->jiffiePeriod);
        if (!cntr->shutdownTimer) 
            mod_timer(&cntr->timer,jnext);  // re-schedule
}

static int init_cntr(struct DMMAT* brd,int type)
{
        int result = -ENOMEM;
        struct DMMAT_CNTR* cntr;
        dev_t devno;

        brd->cntr = cntr = kmalloc(sizeof(struct DMMAT_CNTR),GFP_KERNEL);
        if (!cntr) return result;
        memset(cntr,0, sizeof(struct DMMAT_CNTR));

        cntr->brd = brd;

        // for informational messages only at this point
        sprintf(cntr->deviceName,"/dev/dmmat_cntr%d",brd->num);

        // cntr device
        cdev_init(&cntr->cdev,&cntr_fops);
        cntr->cdev.owner = THIS_MODULE;

        devno = MKDEV(MAJOR(dmmat_device),brd->num*DMMAT_DEVICES_PER_BOARD+1);
        KLOG_DEBUG("%s: MKDEV, major=%d minor=%d\n",
                cntr->deviceName,MAJOR(devno),MINOR(devno));

        switch (type) {
        case DMM16AT_BOARD:
                brd->cntr_itr_mask = 0x40;
                cntr->start = startMM16AT_CNTR;
                cntr->stop = stopMM16AT_CNTR;
                break;
        case DMM32XAT_BOARD:
                brd->cntr_itr_mask = 0x20;
                cntr->start = startMM32AT_CNTR;
                cntr->stop = stopMM32AT_CNTR;
                break;
        }
            
        /*
         * Allocate counter samples in circular buffer
         */
        result = alloc_dsm_circ_buf(&cntr->samples,
            sizeof(long),DMMAT_CNTR_QUEUE_SIZE);
        if (result) return result;

        init_waitqueue_head(&cntr->read_queue);

        init_timer(&cntr->timer);
        cntr->timer.function = cntr_timer_fn;
        cntr->timer.data = (unsigned long)cntr;

        /* After calling cdev_all the device is "live"
         * and ready for user operation.
         */
        result = cdev_add (&cntr->cdev, devno,1);
        return result;
}

static void cleanup_cntr(struct DMMAT* brd)
{
        struct DMMAT_CNTR* cntr = brd->cntr;
        if (!cntr) return;
        cdev_del(&cntr->cdev);

        free_dsm_circ_buf(&cntr->samples);

        kfree(cntr);
        brd->cntr = 0;
}

static int init_d2a(struct DMMAT* brd,int type)
{
        int result = -ENOMEM;
        struct DMMAT_D2A* d2a;
        dev_t devno;

        brd->d2a = d2a = kmalloc(sizeof(struct DMMAT_D2A),GFP_KERNEL);
        if (!d2a) return result;
        memset(d2a,0, sizeof(struct DMMAT_D2A));

        d2a->brd = brd;

        // for informational messages only at this point
        sprintf(d2a->deviceName,"/dev/dmmat_d2a%d",brd->num);

        // d2a device
        cdev_init(&d2a->cdev,&d2a_fops);
        d2a->cdev.owner = THIS_MODULE;

        devno = MKDEV(MAJOR(dmmat_device),brd->num*DMMAT_DEVICES_PER_BOARD+2);
        KLOG_DEBUG("%s: MKDEV, major=%d minor=%d\n",
                d2a->deviceName,MAJOR(devno),MINOR(devno));

        switch (type) {
        case DMM16AT_BOARD:
                d2a->setD2A = setD2A_MM16AT;
                if (d2aconfig[brd->num] == DMMAT_D2A_UNI_10 ||
                        d2aconfig[brd->num] == DMMAT_D2A_BI_10) {
                        KLOG_ERR("%s: is a DMM16AT and does not support 10 V D2A\n",
                                d2a->deviceName);
                        return -EINVAL;
                }
                break;
        case DMM32XAT_BOARD:
                d2a->setD2A = setD2A_MM32AT;
                break;
        }
            
        // calculate conversion relation based on presumed
        // correct value for d2aconfig runstring parameter
        d2a->cmin = 0;
        d2a->cmax = 4095;;

        switch(d2aconfig[brd->num]) {
        case DMMAT_D2A_UNI_5:
                d2a->vmin = 0;
                d2a->vmax = 5;
                break;
        case DMMAT_D2A_UNI_10:
                d2a->vmin = 0;
                d2a->vmax = 10;
                break;
        case DMMAT_D2A_BI_5:
                d2a->vmin = -5;
                d2a->vmax = 5;
                break;
        case DMMAT_D2A_BI_10:
                d2a->vmin = -10;
                d2a->vmax = 10;
                break;
        default:
                return -EINVAL;
        }

        /* After calling cdev_all the device is "live"
         * and ready for user operation.
         */
        result = cdev_add (&d2a->cdev, devno,1);
        return result;
}

static void cleanup_d2a(struct DMMAT* brd)
{
        struct DMMAT_D2A* d2a = brd->d2a;
        if (!d2a) return;
        cdev_del(&d2a->cdev);

        kfree(d2a);
        brd->d2a = 0;
}

/*-----------------------Module ------------------------------*/

static void __exit dmd_mmat_cleanup(void)
{

    int ib;

    if (board) {

        for (ib = 0; ib < numboards; ib++) {
            struct DMMAT* brd = board + ib;

            cleanup_a2d(brd);
            cleanup_cntr(brd);
            cleanup_d2a(brd);

            // TODO: implement
            // cleanup_dio(brd);

            if (brd->irq) {
                    KLOG_NOTICE("freeing irq %d\n",brd->irq);
                    free_irq(brd->irq,brd);
            }

            if (brd->addr)
                release_region(brd->addr, DMMAT_IOPORT_WIDTH);
        }
        kfree(board);
        board = 0;
    }

    if (MAJOR(dmmat_device) != 0)
        unregister_chrdev_region(dmmat_device, numboards * DMMAT_DEVICES_PER_BOARD);

    if (work_queue) destroy_workqueue(work_queue);

    KLOG_DEBUG("complete\n");

    return;
}

static int __init dmd_mmat_init(void)
{	
        int result = -EINVAL;
        int ib;

        board = 0;

#ifdef USE_MY_WORK_QUEUE
        work_queue = create_singlethread_workqueue("dmd_mmat");
#endif

        // DSM_VERSION_STRING is found in dsm_version.h
        KLOG_NOTICE("version: %s, HZ=%d\n",DSM_VERSION_STRING,HZ);

        /* count non-zero ioport addresses, gives us the number of boards */
        for (ib = 0; ib < MAX_DMMAT_BOARDS; ib++)
            if (ioports[ib] == 0) break;
        numboards = ib;
        if (numboards == 0) {
            KLOG_ERR("No boards configured, all ioports[]==0\n");
            goto err;
        }

        /*
         * Minor number:
         *  (0,4,8,...)     A2D
         *  (1,5,9,...)    Pulse counter
         *  (2,6,10,...)     Analog out
         *  (3,7,11,...)    Digital out
         */
  
        result = alloc_chrdev_region(&dmmat_device, 0,
            numboards * DMMAT_DEVICES_PER_BOARD,"dmd_mmat");
        if (result < 0) goto err;
        KLOG_DEBUG("alloc_chrdev_region done, major=%d minor=%d\n",
                MAJOR(dmmat_device),MINOR(dmmat_device));

        result = -ENOMEM;
        board = kmalloc( numboards * sizeof(struct DMMAT),GFP_KERNEL);
        if (!board) goto err;
        memset(board,0,numboards * sizeof(struct DMMAT));

        for (ib = 0; ib < numboards; ib++) {
                struct DMMAT* brd = board + ib;
                unsigned long addr =  ioports[ib] + SYSTEM_ISA_IOPORT_BASE;
                KLOG_DEBUG("isa base=%x\n",SYSTEM_ISA_IOPORT_BASE);

                brd->num = ib;
                spin_lock_init(&brd->reglock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
                mutex_init(&brd->irqreq_mutex);
#else
                init_MUTEX(&brd->irqreq_mutex);
#endif
                result = -EBUSY;
                // Get the mapped board address
                if (!request_region(addr, DMMAT_IOPORT_WIDTH, "dmd_mmat")) {
                    KLOG_ERR("ioport at 0x%lx already in use\n", addr);
                    goto err;
                }
                brd->addr = addr;

                result = -EINVAL;
                // irqs are requested at open time.
                if (irqs[ib] <= 0) {
                    KLOG_ERR("missing irq value for board #%d at addr 0x%lx\n",
                        ib,ioports[ib]);
                    goto err;
                }

                // setup A2D
                result = init_a2d(brd,types[ib]);
                if (result) goto err;
                brd->a2d->stop(brd->a2d,1);

                // setup CNTR
                result = init_cntr(brd,types[ib]);
                if (result) goto err;
                brd->cntr->stop(brd->cntr);

                // setup D2A
                result = init_d2a(brd,types[ib]);
                if (result) goto err;
        }

        KLOG_DEBUG("complete.\n");

        return 0;
err:
        dmd_mmat_cleanup();
        return result;
}

module_init(dmd_mmat_init);
module_exit(dmd_mmat_cleanup);

