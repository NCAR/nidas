
/*  a2d_driver.c/

Driver and utility modules for Diamond System MM AT analog IO cards.

Copyright 2005 UCAR, NCAR, All Rights Reserved

Original author:	Gordon Maclean

Revisions:

*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/kernel.h>       /* printk() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/delay.h>        /* udelay */
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/wait.h>

#include <asm/io.h>

#include <nidas/linux/diamond/dmd_mmat.h>
#include <nidas/rtlinux/dsm_version.h>
#define DEBUG
#include <nidas/linux/klog.h>
#include <nidas/linux/isa_bus.h>

/* ioport addresses of installed boards, 0=no board installed */
static unsigned long ioports[MAX_DMMAT_BOARDS] = { 0x380, 0, 0, 0 };
/* number of DMMAT boards in system (number of non-zero ioport values) */
static int numboards = 0;
module_param_array(ioports,ulong,&numboards,0);

/* irqs, required for each board */
static int irqs[MAX_DMMAT_BOARDS] = { 3, 0, 0, 0 };
static int numirqs = 0;
module_param_array(irqs,int,&numirqs,0);

/* board types: 0=DMM16AT, 1=DMM32XAT 
 * See #defines for DMM_XXXXX_BOARD)
 * Doesn't seem to be an easy way to auto-detect the board type,
 * but it's probably do-able.
 */
static int types[MAX_DMMAT_BOARDS] = { DMM32XAT_BOARD, 0, 0, 0 };
static int numtypes = 0;
module_param_array(types,int,&numtypes,0);

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_LICENSE("Dual BSD/GPL");

#define DEVICES_PER_BOARD 4     // a2d, d2d, dio, pctr
static dev_t board_device = MKDEV(0,0);

/*
 * Pointer to first of dynamically allocated structures containing
 * all data pertaining to the configured DMMAT boards on the system.
 */
static struct DMMAT* board = 0;

/********** Board Utility Functions *******************/
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
         * bits 3,2,1: mode, 2=rate generator
         * bit 0: 0=binary 16 bit counter, 1=4 decade BCD
         */
        unsigned char ctrl = 0;
        int caddr = 0;
        switch (clock) {
        case 0:
            ctrl = 0x00;	// select counter 0
            caddr = 12;
            break;
        case 1:
            ctrl = 0x40;	// select counter 1
            caddr = 13;
            break;
        case 2:
            ctrl = 0x80;	// select counter 2
            caddr = 14;
            break;
        }

        ctrl |= 0x30	// ls byte followed by ms byte
            + (mode << 1);

        outb(ctrl,brd->addr + 15);

        lobyte = val & 0xff;
        outb(lobyte,brd->addr + caddr);
        hibyte = val >> 8;
        outb(hibyte,brd->addr + caddr);

        KLOG_DEBUG("ctrl=0x%x, lobyte=%d,hibyte=%d\n",
            (int)ctrl,(int)lobyte,(int)hibyte);
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

        spin_lock_irqsave(&brd->spinlock,flags);
        status = inb(brd->addr + 8);
        spin_unlock_irqrestore(&brd->spinlock,flags);

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
        spin_lock_irqsave(&brd->spinlock,flags);
        status = inb(brd->addr + 8);
        spin_unlock_irqrestore(&brd->spinlock,flags);

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

        spin_lock_irqsave(&a2d->brd->spinlock,flags);
        outb(chanNibbles, a2d->brd->addr + 2);
        spin_unlock_irqrestore(&a2d->brd->spinlock,flags);

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
          
        spin_lock_irqsave(&a2d->brd->spinlock,flags);
        outb(a2d->lowChan, a2d->brd->addr + 2);
        outb(a2d->highChan, a2d->brd->addr + 3);
        spin_unlock_irqrestore(&a2d->brd->spinlock,flags);

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
        if (totalRate > 90000) *val |= 0x30;
        else if (totalRate > 60000) *val |= 0x20;
        else if (totalRate > 40000) *val |= 0x10;
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
         *
         * Note: on the MM16AT, the FIFO must be reset (base + 10)
         * to clear an overflow.
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
            unsigned long j = jiffies + 1;
            while (jiffies < j) schedule();
        } while(ntry++ < 50 && inb(a2d->brd->addr + 10) & 0x80);
        KLOG_DEBUG("ntry=%d\n",ntry);
}

static void waitForA2DSettleMM32XAT(struct DMMAT_A2D* a2d)
{
        do {
            unsigned long j = jiffies + 1;
            while (jiffies < j) schedule();
        } while(inb(a2d->brd->addr + 11) & 0x80);
}

/*
 * Configure A2D board.  Board should not be busy.
 */
static int configA2D(struct DMMAT_A2D* a2d,struct DMMAT_A2D_Config* cfg)
{

        int result = 0;
        int i,j;

        int gain = 0;
        int bipolar = 0;
        int uniqueIds[MAX_DMMAT_A2D_CHANNELS];

        if(a2d->busy) {
                KLOG_ERR("A2D's running. Can't configure\n");
                return -EBUSY;
        }

        if (a2d->sampleInfo) {
                for (j = 0; j < a2d->nsamples; j++) {
                        if (a2d->sampleInfo[j].channels)
                                kfree(a2d->sampleInfo[j].channels);
                        a2d->sampleInfo[j].channels = 0;
                }
                kfree(a2d->sampleInfo);
                a2d->sampleInfo = 0;
        }

        a2d->nsamples = 0;
        a2d->lowChan = 0;
        a2d->highChan = 0;

        a2d->scanRate = cfg->scanRate;

        /* count number of unique sample ids */
        for (i = 0; i < MAX_DMMAT_A2D_CHANNELS; i++) {
                int id = cfg->id[i];
                // non-zero gain means the channel has been requested
                if (cfg->gain[i] == 0) continue;
                for (j = 0; j < a2d->nsamples; j++)
                    if (id == uniqueIds[j]) break;
                if (j == a2d->nsamples) uniqueIds[a2d->nsamples++] = id;
        }

        if (a2d->nsamples == 0) {
                KLOG_ERR("%s: no channels requested, all gain==0\n",
                    a2d->deviceName);
                return -EINVAL;
        }

        a2d->sampleInfo = kmalloc(a2d->nsamples *
            sizeof(struct DMMAT_A2D_Sample_Info),GFP_KERNEL);
        memset(a2d->sampleInfo,0,
                a2d->nsamples * sizeof(struct DMMAT_A2D_Sample_Info));

        /* Count number of channels for each sample */
        for (i = 0; i < MAX_DMMAT_A2D_CHANNELS; i++) {
                int id = cfg->id[i];
                if (cfg->gain[i] == 0) continue;
                for (j = 0; j < a2d->nsamples; j++)
                    if (id == uniqueIds[j]) break;
                a2d->sampleInfo[j].nchans++;
        }

        /* Allocate array of indicies into scanned channel sequence */
        for (j = 0; j < a2d->nsamples; j++) {
                a2d->sampleInfo[j].channels =
                    kmalloc(a2d->sampleInfo[j].nchans * sizeof(int),GFP_KERNEL);
                a2d->sampleInfo[j].nchans = 0;
        }

        for (i = 0; i < MAX_DMMAT_A2D_CHANNELS; i++) {
                int id = cfg->id[i];
                a2d->requested[i] = 0;
                if (cfg->gain[i] == 0) continue;
                for (j = 0; j < a2d->nsamples; j++)
                    if (id == uniqueIds[j]) break;

                a2d->sampleInfo[j].channels[a2d->sampleInfo[j].nchans++] = i;

                a2d->requested[i] = 1;
                if (a2d->scanRate == 0) {
                    a2d->lowChan = i;
                    gain = cfg->gain[i];
                    bipolar = cfg->bipolar[i];
                }
                a2d->highChan = i;

                // gains must all be the same and positive.
                if (cfg->gain[i] != gain) return -EINVAL;

                // Must have same polarity.
                if (cfg->bipolar[i] != bipolar) return -EINVAL;

                // All variables in the same sample must have same filter
                if (cfg->bipolar[i] != bipolar) return -EINVAL;
        }
        // subtract low channel
        for (j = 0; j < a2d->nsamples; j++) {
                for (i = 0; i < a2d->sampleInfo[j].nchans; i++)
                        a2d->sampleInfo[j].channels[i] -= a2d->lowChan;
        }
        a2d->nchans = a2d->highChan - a2d->lowChan + 1;

        if ((USECS_PER_SEC * 10) % a2d->scanRate)
                KLOG_WARNING("%s: max sampling rate=%d is not a factor of 10 MHz\n",
                    a2d->deviceName,a2d->scanRate);

        result = a2d->getGainSetting(a2d,gain,bipolar,&a2d->gainSetting);
        if (result != 0) return result;

        result = a2d->getConvRateSetting(a2d,&a2d->gainSetting);
        if (result != 0) return result;

        a2d->latencyMsecs = cfg->latencyUsecs / USECS_PER_MSEC;
        if (a2d->latencyMsecs == 0) a2d->latencyMsecs = 500;

        a2d->scanDeltaT =  TMSECS_PER_SEC / a2d->scanRate;

        return 0;
}

/*
 * Configure filter for A2D samples.  Board should not be busy.
 */
static int configA2DSample(struct DMMAT_A2D* a2d,
        struct DMMAT_A2D_Sample_Config* cfg)
{
        struct DMMAT_A2D_Sample_Info* sinfo;
        struct short_filter_methods methods;

        if (cfg->id < 0 || cfg->id >= a2d->nsamples) return -EINVAL;

        sinfo = &a2d->sampleInfo[cfg->id];

        if (a2d->scanRate % cfg->rate) {
                KLOG_ERR("%s: A2D scanRate=%d is not a multiple of the rate=%d for sample %d\n",
                    a2d->deviceName,a2d->scanRate,cfg->rate,cfg->id);
                return -EINVAL;
        }

        sinfo->decimate = a2d->scanRate / cfg->rate;
        sinfo->filterType = cfg->filterType;
        sinfo->id = cfg->id;
        
        methods = get_short_filter_methods(cfg->filterType);
        if (!methods.init) {
                KLOG_ERR("%s: filter type %d unsupported\n",
                    a2d->deviceName,cfg->filterType);
                return -EINVAL;
        }
        sinfo->finit = methods.init;
        sinfo->fconfig = methods.config;
        sinfo->filter = methods.filter;
        sinfo->fcleanup = methods.cleanup;

        /* Create the filter object */
        sinfo->filterObj = sinfo->finit();
        if (!sinfo->filterObj) return -ENOMEM;

        /* Configure the filter */
        switch(sinfo->filterType) {
        case NIDAS_FILTER_BOXCAR:
        {
                struct boxcar_filter_config bcfg;
                bcfg.npts = cfg->boxcarNpts;
                sinfo->fconfig(sinfo->filterObj,cfg->id,
                    sinfo->nchans,sinfo->channels,
                    sinfo->decimate,&bcfg);
        }
                break;
        default:
                sinfo->fconfig(sinfo->filterObj,cfg->id,
                    sinfo->nchans,sinfo->channels,
                    sinfo->decimate,0);
                break;
        }

        return 0;
}

/*
 * Function to stop the A2D on a MM16AT
 */
static void stopMM16AT_A2D(struct DMMAT_A2D* a2d)
{
        struct DMMAT* brd = a2d->brd;
        unsigned long flags;
        spin_lock_irqsave(&brd->spinlock,flags);

        // disable triggering and interrupts
        outb(0, brd->addr + 9);

        // disble fifo, scans
        outb(0,brd->addr + 10);

        // reset fifo
        outb(0x80,brd->addr + 10);

        spin_unlock_irqrestore(&brd->spinlock,flags);
}

/*
 * Function to stop the A2D on a MM32XAT
 */
static void stopMM32XAT_A2D(struct DMMAT_A2D* a2d)
{
        struct DMMAT* brd = a2d->brd;
        unsigned long flags;
        spin_lock_irqsave(&brd->spinlock,flags);

        // full reset
        // outb(0x20, brd->addr + 8);

        // set page to 0
        outb(0x00, brd->addr + 8);
        
        // disable interrupts, hardware clock
        outb(0,brd->addr + 9);

        // disable and reset fifo, disable scan mode
        outb(0x2,brd->addr + 9);

        spin_unlock_irqrestore(&brd->spinlock,flags);
}

/**
 * General function to stop the A2D. Calls the board specific method.
 */
static int stopA2D(struct DMMAT_A2D* a2d)
{
        int ret = 0;

        if (a2d->stop) a2d->stop(a2d);
        // shut down tasklets
        tasklet_disable(&a2d->tasklet);
        a2d->busy = 0;	// Reset the busy flag

        return ret;
}

/*
 * Function to start the A2D on a MM16AT
 */
static int startMM16AT_A2D(struct DMMAT_A2D* a2d)
{
        int result = 0;
        unsigned long flags;
        struct DMMAT* brd = a2d->brd;

        spin_lock_irqsave(&brd->spinlock,flags);

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
        outb(0x83,brd->addr + 9);

        spin_unlock_irqrestore(&brd->spinlock,flags);

        return result;
}

/*
 * Function to start the A2D on a MM32XAT
 */
static int startMM32XAT_A2D(struct DMMAT_A2D* a2d)
{
        int result = 0;
        unsigned long flags;
        struct DMMAT* brd = a2d->brd;
        int nscans;
        int nsamps;

        spin_lock_irqsave(&brd->spinlock,flags);

        outb(0x03,brd->addr + 8);	// set page 3
        outb(0xa6,brd->addr + 15);	// enable enhanced features
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

        a2d->fifoThreshold = nsamps;
        KLOG_DEBUG("fifoThreshold=%d,latency=%ld,nchans=%d,nsamps=%d\n",
            a2d->fifoThreshold,a2d->latencyMsecs,a2d->nchans,nsamps);
        // register value is 1/2 the threshold
        nsamps /= 2;

        if (nsamps > 255) {
            outb(0x02,brd->addr + 8);	// set page 2
            outb(0x01,brd->addr + 12);	// set high bit for fifo threshold
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
        outb(0x83,brd->addr + 9);

        spin_unlock_irqrestore(&brd->spinlock,flags);
        return result;
}

/**
 * General function to start an A2D. Calls the board specific method.
 */
static int startA2D(struct DMMAT_A2D* a2d)
{
        int result;
        unsigned long flags;
        struct DMMAT* brd = a2d->brd;

        if (a2d->busy) stopA2D(a2d);

        a2d->busy = 1;	// Set the busy flag

        memset(&a2d->status,0,sizeof(a2d->status));

        a2d->fifo_samples.head = a2d->fifo_samples.tail = 0;
        a2d->samples.head = a2d->samples.tail = 0;

        a2d->status.irqsReceived = 0;
        a2d->sampBytesLeft = 0;
        a2d->sampPtr = 0;

        if ((result = a2d->selectChannels(a2d))) return result;

        spin_lock_irqsave(&brd->spinlock,flags);
        // same addr on MM16AT and MM32XAT
        outb(a2d->gainSetting,a2d->brd->addr + 11);
        spin_unlock_irqrestore(&brd->spinlock,flags);

        a2d->waitForA2DSettle(a2d);

        a2d->tl_data.saveSample.length = 0;

        a2d->start(a2d);
        return 0;
}

/**
 * Invoke filters.
 */
static void do_filters(struct DMMAT_A2D* a2d,dsm_sample_time_t tt,
    const short* dp)
{
        int i;
        for (i = 0; i < a2d->nsamples; i++) {
            short_sample_t* osamp = (short_sample_t*)
                GET_HEAD(a2d->samples,DMMAT_SAMPLE_QUEUE_SIZE);
            if (!osamp)   // no output sample available
                a2d->status.missedSamples++;
            else if (a2d->sampleInfo[i].filter(
                    a2d->sampleInfo[i].filterObj,tt,dp,osamp)) {
                INCREMENT_HEAD(a2d->samples,DMMAT_SAMPLE_QUEUE_SIZE);
                wake_up_interruptible(&a2d->read_queue);
            }
        }
}

/**
 * Tasklet that invokes filters on the data in a fifo sample.
 * The contents of the fifo sample is not necessarily a multiple
 * of the number of channels scanned.
 */
static void dmmat_a2d_do_tasklet(unsigned long dev)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) dev;
        struct a2d_tasklet_data* tld = &a2d->tl_data;

        dsm_sample_time_t tt0;

        int saveChan = tld->saveSample.length / sizeof(short);

        // the consumer of fifo_samples.
        while (a2d->fifo_samples.head != a2d->fifo_samples.tail) {
                struct dsm_sample* insamp =
                    a2d->fifo_samples.buf[a2d->fifo_samples.tail];

                int nval = insamp->length / sizeof(short);
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
                INCREMENT_TAIL(a2d->fifo_samples,DMMAT_SAMPLE_QUEUE_SIZE);
        }
}

/**
 * Tasklet that invokes filters on the data in a fifo sample.
 * The contents of the fifo sample must be a multiple
 * of the number of channels scanned.
 */
static void dmmat_a2d_do_tasklet_fast(unsigned long dev)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) dev;
        struct a2d_tasklet_data* tld = &a2d->tl_data;

        dsm_sample_time_t tt0;
        int lenout = a2d->nchans * sizeof(short);

        while (a2d->fifo_samples.head != a2d->fifo_samples.tail) {
                struct dsm_sample* insamp =
                    a2d->fifo_samples.buf[a2d->fifo_samples.tail];

                int nval = insamp->length / sizeof(short);
                // # of scans in fifo
                int ndt = (nval - 1) / a2d->nchans;
                long dt = ndt * a2d->scanDeltaT;
                short *dp = (short *)insamp->data;
                short *ep = dp + nval;

                BUG_ON((nval % a2d->nchans) != 0);

                // compute time of first whole scan in this fifo dump.
                // The fifoThreshold of MM16AT is fixed at 256 samples,
                // which may not be an integral number of scans.
                // This sample may not contain an integral number of scans.

                // tt0 is conversion time of first compete scan in fifo
                tt0 = insamp->timetag - dt;  // fifo interrupt time

                for (; dp < ep; ) {
                    do_filters(a2d, tt0,dp);
                    dp += a2d->nchans;
                    tt0 += a2d->scanDeltaT;
                }
                INCREMENT_TAIL(a2d->fifo_samples,DMMAT_SAMPLE_QUEUE_SIZE);
        }
}

/*
 * Handler for A2D interrupts. Called from board interrupt handler.
 */
static irqreturn_t dmmat_a2d_handler(struct DMMAT_A2D* a2d)
{
        // brd->spin_lock and a2d->spin_lock are locked before
        // entering this function

        struct DMMAT* brd = a2d->brd;
        int flevel = a2d->getFifoLevel(a2d);
        int i;
        struct dsm_sample* samp;
        char* dp;

        a2d->status.irqsReceived++;
        switch (flevel) {
        default:
        case 3:         // full or overflowed, we're falling behind
            a2d->status.fifoOverflows++;
            a2d->resetFifo(a2d);
            return IRQ_HANDLED;
        case 2: break;	// at or above threshold, but not full (expected value)
        case 1:         // less than threshold, shouldn't happen
            a2d->resetFifo(a2d);
        case 0:         // empty, shouldn't happen
            a2d->status.fifoUnderflows++;
            return IRQ_HANDLED;
        }

        samp = GET_HEAD(a2d->fifo_samples,DMMAT_SAMPLE_QUEUE_SIZE);
        if (!samp) {                // no output sample available
            a2d->status.missedSamples += a2d->fifoThreshold;
            for (i = 0; i < a2d->fifoThreshold; i++) inw(brd->addr);
            return IRQ_HANDLED;
        }

        samp->timetag = getSystemTimeTMsecs();
        dp = samp->data;

        // Finally!!!! the actual read from the hardware fifo.
        // All this overhead just to do this...
        insw(brd->addr,dp,a2d->fifoThreshold);
        samp->length = a2d->fifoThreshold * sizeof(short);

        /* On the MM16AT the fifo empty bit isn't set after reading
         * threshold number of values.  Perhaps it's a board bug?
         * On the MM32XAT the fifo empty bit is set at this point.
         */
        flevel = a2d->getFifoLevel(a2d);
        if (flevel != 0) {
            a2d->status.fifoNotEmpty++;
            KLOG_INFO("fifo level=%d, base+8=0x%x, base+10=0x%x\n",
                    flevel,inb(brd->addr+8),inb(brd->addr+10));
        }

        KLOG_DEBUG("irq: timetag=%ld, data0=%d,dataN=%d\n", samp->timetag,
            ((short *)samp->data)[0],
            ((short *)samp->data)[a2d->fifoThreshold-1]);

        /* increment head, this sample is ready for consumption */
        INCREMENT_HEAD(a2d->fifo_samples,DMMAT_SAMPLE_QUEUE_SIZE);

        tasklet_schedule(&a2d->tasklet);
        return IRQ_HANDLED;
}

/*
 * General IRQ handler for the board.  Calls the A2D handler or
 * (eventually) the pulse counter handler depending on who interrupted.
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

        spin_lock(&brd->spinlock);
        status = inb(brd->int_status_reg);
        if (!status) {
                spin_unlock(&brd->spinlock);
                return result;
        }
        // acknowledge interrupt
        outb(brd->int_ack_val, brd->int_ack_reg);

        if (status & brd->ad_int_mask) {
            // struct DMMAT_A2D* a2d = brd->a2d;
            // spin_lock(&a2d->spinlock);
            result = dmmat_a2d_handler(brd->a2d);
            // spin_unlock(&a2d->spinlock);
        }

        // TODO: implement pctr interrupts
        // if (status & brd->pctr_int_mask)
            // result = dmmat_pctr_handler(brd->pctr);

        spin_unlock(&brd->spinlock);
        return result;
}

/************ File Operations ****************/
static int dmmat_open_a2d(struct inode *inode, struct file *filp)
{
        int i = iminor(inode);
        int ibrd = i / DEVICES_PER_BOARD;
        int ia2d = i % DEVICES_PER_BOARD;

        struct DMMAT* brd;
        struct DMMAT_A2D* a2d;
        unsigned long flags;

        if (ibrd >= numboards) return -ENXIO;

        brd = board + ibrd;
        a2d = brd->a2d;

        spin_lock_irqsave(&a2d->spinlock,flags);
        a2d->fifo_samples.head = a2d->fifo_samples.tail = 0;
        a2d->samples.head = a2d->samples.tail = 0;
        spin_unlock_irqrestore(&a2d->spinlock,flags);

        /* a2d and pulse counter use interrupts */
        if (ia2d == 0 || ia2d == 3) {
            int result;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
            if ((result = mutex_lock_interruptible(&brd->mutex)))
                return result;
#else
            if ((result = down_interruptible(&brd->mutex)))
                return result;
#endif
            if (brd->irq == 0) {
                BUG_ON(brd->irq_users != 0);
                /* We don't use SA_INTERRUPT flag here.  We don't
                 * need to block other interrupts while we're running.
                 * Note: request_irq can wait, so spin_lock not advised.
                 */
                result = request_irq(irqs[ibrd],dmmat_irq_handler,
                    SA_SHIRQ,"dmd_mmat",brd);
                if (result) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
                    mutex_unlock(&brd->mutex);
#else
                    up(&brd->mutex);
#endif
                    return result;
                }
                brd->irq = irqs[ibrd];
                brd->irq_users = 0;
            }
            else BUG_ON(brd->irq_users <= 0);
            brd->irq_users++;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
            mutex_unlock(&brd->mutex);
#else
            up(&brd->mutex);
#endif
        }
        filp->private_data = a2d;

        return 0;
}

static int dmmat_release_a2d(struct inode *inode, struct file *filp)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) filp->private_data;

        int i = iminor(inode);
        int ibrd = i / DEVICES_PER_BOARD;
        int ia2d = i % DEVICES_PER_BOARD;

        struct DMMAT* brd;

        if (ibrd >= numboards) return -ENXIO;

        brd = board + ibrd;
        BUG_ON(a2d != brd->a2d);

        /* a2d and pulse counter use interrupts */
        if (ia2d == 0 || ia2d == 3) {
            int result;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
            if ((result = mutex_lock_interruptible(&brd->mutex)))
                return result;
#else
            if ((result = down_interruptible(&brd->mutex)))
                return result;
#endif
            if (--brd->irq_users == 0) {
                free_irq(brd->irq,brd);
                brd->irq = 0;
            }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
            mutex_unlock(&brd->mutex);
#else
            up(&brd->mutex);
#endif
        }

        return 0;
}

static ssize_t dmmat_read_a2d(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) filp->private_data;
        size_t ocount = 0;
        int n;

        wait_event_interruptible(a2d->read_queue,
                a2d->samples.head == a2d->samples.tail);

        for ( ; count; ) {

            if (a2d->sampBytesLeft == 0) {
                struct dsm_sample* insamp;
                if (a2d->sampPtr)
                    INCREMENT_TAIL(a2d->samples,DMMAT_SAMPLE_QUEUE_SIZE);
                if (a2d->samples.head == a2d->samples.tail) return ocount;
                insamp = a2d->samples.buf[a2d->samples.tail];

                a2d->sampPtr = (char*)insamp;
                a2d->sampBytesLeft = insamp->length +
                    ((char*)insamp->data - (char*)&insamp->timetag);
            }
            n = min(a2d->sampBytesLeft,count);

            if (copy_to_user(buf,a2d->sampPtr,n)) return -EFAULT;
            ocount += n;
            count -= n;
            a2d->sampPtr += n;
            a2d->sampBytesLeft -= n;
        }
        return ocount;
}

static int dmmat_ioctl_a2d(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) filp->private_data;
        struct DMMAT* brd;
        int i = iminor(inode);
        int ibrd = i / DEVICES_PER_BOARD;
        // int ia2d = i % DEVICES_PER_BOARD;
        int result = -EINVAL,err = 0;

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
                err = !access_ok(VERIFY_WRITE, (void __user *)arg,
                    _IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                err =  !access_ok(VERIFY_READ, (void __user *)arg,
                    _IOC_SIZE(cmd));
        if (err) return -EFAULT;

        if (ibrd >= numboards) return -ENXIO;

        brd = board + ibrd;

        BUG_ON(brd != a2d->brd);

        switch (cmd) 
        {

        case DMMAT_GET_A2D_NCHAN:
                result = a2d->getNumChannels(a2d);
                break;
        case DMMAT_GET_A2D_STATUS:	/* user get of status struct */
                if (copy_to_user((void __user *)arg,&a2d->status,
                    sizeof(struct DMMAT_A2D_Status))) return -EFAULT;
                break;
        case DMMAT_SET_A2D_CONFIG:      /* user set */
            {
                struct DMMAT_A2D_Config cfg;
                if (copy_from_user(&cfg,(void __user *)arg,
                        sizeof(struct DMMAT_A2D_Config))) return -EFAULT;
                result = configA2D(a2d,&cfg);
            }
	    break;
        case DMMAT_SET_A2D_SAMPLE:      /* user set */
            {
                struct DMMAT_A2D_Sample_Config cfg;
                if (copy_from_user(&cfg,(void __user *)arg,
                        sizeof(struct DMMAT_A2D_Sample_Config))) return -EFAULT;
                result = configA2DSample(a2d,&cfg);
            }
	    break;
        case DMMAT_A2D_START:
                result = startA2D(a2d);
                break;
        case DMMAT_A2D_STOP:
                result = stopA2D(a2d);
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
unsigned int dmmat_poll_a2d(struct file *filp, poll_table *wait)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) filp->private_data;
        unsigned int mask = 0;
        poll_wait(filp, &a2d->read_queue, wait);
        if (a2d->samples.head != a2d->samples.tail) 
            mask |= POLLIN | POLLRDNORM;    /* readable */
        return mask;
}

static struct file_operations a2d_fops = {
        .owner   = THIS_MODULE,
        .read    = dmmat_read_a2d,
        .poll    = dmmat_poll_a2d,
        .open    = dmmat_open_a2d,
        .ioctl   = dmmat_ioctl_a2d,
        .release = dmmat_release_a2d,
};

// TODO: implement
static struct file_operations d2a_fops = {
        .owner   = THIS_MODULE,
        .open    = dmmat_open_a2d,
        .ioctl   = dmmat_ioctl_a2d,
        .release = dmmat_release_a2d,
};

// TODO: implement
static struct file_operations dio_fops = {
        .owner   = THIS_MODULE,
        .open    = dmmat_open_a2d,
        .ioctl   = dmmat_ioctl_a2d,
        .release = dmmat_release_a2d,
};

// TODO: implement
static struct file_operations pctr_fops = {
        .owner   = THIS_MODULE,
        .read    = dmmat_read_a2d,
        .poll    = dmmat_poll_a2d,
        .open    = dmmat_open_a2d,
        .ioctl   = dmmat_ioctl_a2d,
        .release = dmmat_release_a2d,
};

static int init_a2d(struct DMMAT* brd,int type)
{
        int result;
        struct DMMAT_A2D* a2d;
        dev_t devno;
        int i;

        result = -ENOMEM;
        brd->a2d = a2d = kmalloc(sizeof(struct DMMAT_A2D),GFP_KERNEL);
        if (!a2d) return result;
        memset(a2d,0, sizeof(struct DMMAT_A2D));

        a2d->brd = brd;
        spin_lock_init(&a2d->spinlock);

        // a2d device
        cdev_init(&a2d->cdev,&a2d_fops);
        a2d->cdev.owner = THIS_MODULE;
        // a2d->cdev.ops = &scullc_fops;
        devno = MKDEV(MAJOR(board_device),brd->num*4);

        // for informational messages only at this point
        a2d->deviceName = kmalloc(64,GFP_KERNEL);
        if (!a2d->deviceName) return result;
        sprintf(a2d->deviceName,"/dev/dmmat_a2d%d",brd->num);
        KLOG_DEBUG("deviceName=%s\n",a2d->deviceName);

        switch (type) {
        case DMM16AT_BOARD:
            /* board registers */
            brd->int_status_reg = brd->addr + 8;
            brd->ad_int_mask = 0x10;

            brd->int_ack_reg = brd->addr + 8;
            brd->int_ack_val = 0x00;        // any value

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
            tasklet_init(&a2d->tasklet,dmmat_a2d_do_tasklet,
                (unsigned long)a2d);
            break;

        case DMM32XAT_BOARD:
            brd->int_status_reg = brd->addr + 9;
            brd->ad_int_mask = 0x80;

            brd->int_ack_reg = brd->addr + 8;
            brd->int_ack_val = 0x08;

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
            tasklet_init(&a2d->tasklet,dmmat_a2d_do_tasklet_fast,
                (unsigned long)a2d);
            break;
        }
        init_waitqueue_head(&a2d->read_queue);

        result = -ENOMEM;
        a2d->fifo_samples.buf = kmalloc(DMMAT_SAMPLE_QUEUE_SIZE *
            sizeof(void*),GFP_KERNEL);
        if (!a2d->fifo_samples.buf) return result;
        memset(a2d->fifo_samples.buf,0,
            DMMAT_SAMPLE_QUEUE_SIZE * sizeof(void*));
                
        for (i = 0; i < DMMAT_SAMPLE_QUEUE_SIZE; i++) {
            struct dsm_sample* samp = (struct dsm_sample*)
                kmalloc(SIZEOF_DSM_SAMPLE_HEADER +
                a2d->maxFifoThreshold * sizeof(short),GFP_KERNEL);
            if (!samp) return result;
            memset(samp,0,SIZEOF_DSM_SAMPLE_HEADER +
                a2d->maxFifoThreshold * sizeof(short));
            a2d->fifo_samples.buf[i] = samp;
        }

        a2d->samples.buf = kmalloc(DMMAT_SAMPLE_QUEUE_SIZE *
            sizeof(void*),GFP_KERNEL);
        if (!a2d->samples.buf) return result;
        memset(a2d->samples.buf,0,
            DMMAT_SAMPLE_QUEUE_SIZE * sizeof(void*));
                
        for (i = 0; i < DMMAT_SAMPLE_QUEUE_SIZE; i++) {
            struct dsm_sample* samp = (struct dsm_sample*)
                kmalloc(SIZEOF_DSM_SAMPLE_HEADER +
                a2d->maxFifoThreshold * sizeof(short),GFP_KERNEL);
            if (!samp) return result;
            memset(samp,0,SIZEOF_DSM_SAMPLE_HEADER +
                MAX_DMMAT_A2D_CHANNELS * sizeof(short));
            a2d->samples.buf[i] = samp;
        }

        a2d->gainSetting = 0;	// default value


        /* After calling cdev_all the device is "live"
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

        stopA2D(a2d);

        cdev_del(&a2d->cdev);

        if (a2d->samples.buf) {
                for (i = 0; i < DMMAT_SAMPLE_QUEUE_SIZE; i++)
                    if (a2d->samples.buf[i]) kfree(a2d->samples.buf[i]);
                kfree(a2d->samples.buf);
                a2d->samples.buf = 0;
        }
        kfree(a2d->deviceName);
        kfree(a2d);
        brd->a2d = 0;
}

/*-----------------------Module------------------------------*/

void dmd_mmat_cleanup(void)
{
    int ib;
    if (board) {

        for (ib = 0; ib < numboards; ib++) {
            struct DMMAT* brd = board + ib;

            cleanup_a2d(brd);
            // TODO: implement
            // cleanup_pctr(brd);
            // cleanup_dio(brd);
            // cleanup_d2a(brd);

            if (brd->irq) free_irq(brd->irq,brd);

            if (brd->addr)
                release_region(brd->addr, DMMAT_IOPORT_WIDTH);
        }
        kfree(board);
        board = 0;
    }

    if (MAJOR(board_device) != 0)
        unregister_chrdev_region(board_device, numboards * DEVICES_PER_BOARD);

    KLOG_DEBUG("complete\n");

    return;
}

int dmd_mmat_init(void)
{	
        int result = -EINVAL;
        int ib;
        struct DMMAT_A2D* a2d;

        board = 0;

        // DSM_VERSION_STRING is found in dsm_version.h
        KLOG_NOTICE("version: %s\n",DSM_VERSION_STRING);

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
         *  (1,5,9,...)     Analog out
         *  (2,6,10,...)    Digital out
         *  (3,7,11,...)    Pulse counter
         */
  
        result = alloc_chrdev_region(&board_device, 0,
            numboards * DEVICES_PER_BOARD,"dmd_mmat");
        if (result < 0) goto err;
        KLOG_DEBUG("alloc_chrdev_region done, major=%d minor=%d\n",
                MAJOR(board_device),MINOR(board_device));

        result = -ENOMEM;
        board = kmalloc( numboards * sizeof(struct DMMAT),GFP_KERNEL);
        if (!board) goto err;
        memset(board,0,numboards * sizeof(struct DMMAT));

        for (ib = 0; ib < numboards; ib++) {
                struct DMMAT* brd = board + ib;
                unsigned long addr =  ioports[ib] + SYSTEM_ISA_IOPORT_BASE;
                dev_t devno;
                KLOG_DEBUG("isa base=%x\n",SYSTEM_ISA_IOPORT_BASE);

                brd->num = ib;
                spin_lock_init(&brd->spinlock);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
                mutex_init(&brd->mutex);
#else
                init_MUTEX(&brd->mutex);
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
                brd->a2d->stop(brd->a2d);

#ifdef IMPLEMENT_THIS
                // d2a device
                cdev_init(&brd->d2a_cdev,&d2a_fops);
                brd->d2a_cdev.owner = THIS_MODULE;
                // brd->cdev.ops = &scullc_fops;
                devno = MKDEV(MAJOR(board_device),ib*4+1);
                /* After calling cdev_all the device is "live"
                 * and ready for user operation.
                 */
                result = cdev_add (&brd->d2a_cdev, devno, 1);
                // 
                if (result < 0) goto err;

                // digital io device
                cdev_init(&brd->dio_cdev,&dio_fops);
                brd->dio_cdev.owner = THIS_MODULE;
                // brd->cdev.ops = &scullc_fops;
                devno = MKDEV(MAJOR(board_device),ib*4+2);
                /* After calling cdev_all the device is "live"
                 * and ready for user operation.
                 */
                result = cdev_add (&brd->dio_cdev, devno, 1);
                // 
                if (result < 0) goto err;

                // pulse counter device
                cdev_init(&brd->pctr_cdev,&pctr_fops);
                brd->pctr_cdev.owner = THIS_MODULE;
                // brd->cdev.ops = &scullc_fops;
                devno = MKDEV(MAJOR(board_device),ib*4+3);
                /* After calling cdev_all the device is "live"
                 * and ready for user operation.
                 */
                result = cdev_add (&brd->pctr_cdev, devno, 1);
                // 
                if (result < 0) goto err;
#endif

        }

        KLOG_DEBUG("complete.\n");

        return 0;
err:
        dmd_mmat_cleanup();
        return result;
}

module_init(dmd_mmat_init);
module_exit(dmd_mmat_cleanup);

