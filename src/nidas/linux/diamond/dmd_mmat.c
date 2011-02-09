/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*-
 * vim: set shiftwidth=8 softtabstop=8 expandtab: */

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
#include <linux/sched.h>
#include <linux/slab.h>		/* kmalloc, kfree */
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
#define mutex_init(x)               init_MUTEX(x)
#define mutex_lock_interruptible(x) ( down_interruptible(x) ? -ERESTARTSYS : 0)
#define mutex_unlock(x)             up(x)
#endif

/* ioport addresses of installed boards, 0=no board installed */
static unsigned int ioports[MAX_DMMAT_BOARDS] = { 0x380, 0, 0, 0 };
/* number of DMMAT boards in system (number of non-zero ioport values) */
static int numboards = 0;

/* ISA irqs, required for each board. Can be shared. */
static int irqs[MAX_DMMAT_BOARDS] = { 3, 0, 0, 0 };
static int numirqs = 0;

/* board types: 0=DMM16AT, 1=DMM32XAT, 2=DMM32DXAT
 * See #defines for DMM_XXXXX_BOARD in header.
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
module_param_array(ioports,int,&numboards,0);
module_param_array(irqs,int,&numirqs,0);
module_param_array(types,int,&numtypes,0);
module_param_array(d2aconfig,int,&numd2aconfig,0);
#else
module_param_array(ioports,int,numboards,0);
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

static struct workqueue_struct* work_queue = 0;

/*********** Board Utility Functions *******************/
/**
 * Set counter value in a 82C54 clock.
 * Works on both MM16AT and MM32XAT, assuming both have set
 * page bit(s) to 0:
 *	MM16AT: bit 6 in base+10 is the page bit
 *	MM32XAT: bits 0-2 in base+8 are page bits
 * Does not hold a spin_lock.
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
static unsigned int readLatchedTimer(struct DMMAT* brd,int clock)
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

/*
 * Return x/y as a whole number, and a base 10 fractional
 * part, *fp,  with prec number of decimal places.
 * Useful for simulating %f in kernel printk's
 */
static int div_10(unsigned int x, unsigned int y,int prec,int* fp)
{
        int n = x / y;
        int f = 0;
        int rem = x % y;
        int i;
        // x%y may be large, and the simple expression
        // (x%y * 10^prec)/y may overflow, so we compute
        // the decimal fraction part one digit at a time.
        for (i = 0; i < prec; i++) {
            f *= 10;
            rem *= 10;
            f += rem / y;
            rem %= y;
        }
        *fp = f;
        return n;
}

static int setClock1InputRate_MM16AT(struct DMMAT* brd, int rate)
{
        unsigned char regval;
        unsigned long flags;

        spin_lock_irqsave(&brd->reglock,flags);
        /*
         * Set counter/timer 1 input rate, in base + 10.
         */
        regval = inb(brd->addr + 10) & 0x78;
        regval |= 0x30;

        switch (rate)
        {
        case 10 * 1000 * 1000:  /* 10 MHz, usual value */
                regval &= ~0x08;
                break;
        case 1000 * 1000:       /* 1 MHz */
                regval |= 0x08;
                break;
        default:
                KLOG_ERR("board %d: Unsupported counter 1&2 input frequency=%d\n",
                                brd->num,rate);
                spin_unlock_irqrestore(&brd->reglock,flags);
                return -EINVAL;
        }
        outb(regval,brd->addr + 10);
        spin_unlock_irqrestore(&brd->reglock,flags);
        return 0;
}

static int setClock1InputRate_MM32AT(struct DMMAT* brd,int rate)
{
        unsigned char regval;
        unsigned long flags;

        spin_lock_irqsave(&brd->reglock,flags);
        /*
         * Set counter/timer 1 input rate, in base + 10.
         */
        regval = inb(brd->addr + 10);

        switch (rate)
        {
        case 10 * 1000 * 1000:  /* 10 MHz, usual value */
                regval &= ~0x80;
                break;
        case 100 * 1000:       /* 100 KHz */
                regval |= 0x80;
                break;
        default:
                KLOG_ERR("board %d: Unsupported counter 1&2 input frequency=%d\n",
                                brd->num,rate);
                spin_unlock_irqrestore(&brd->reglock,flags);
                return -EINVAL;
        }
        outb(regval,brd->addr + 10);
        spin_unlock_irqrestore(&brd->reglock,flags);
        return 0;
}

/*
 * Clock 1/2 is a pair of 16 bit clock dividers for dividing down
 * an input oscillator. The output of clock 1/2 is then used to time
 * A2D conversion scans and output D2A waveforms.  It can be used
 * simultaneously for A2D and D2A, but then, of course, they
 * must agree on the rates.  This is called from user
 * and interrupt context, so no mutexes.
 * spin_lock of brd->reglock should be held prior
 * to calling this function.
 */
static int setupClock12(struct DMMAT* brd,int inputRate, int outputRate)
{
        int result = 0;
        unsigned int c1;
        unsigned short c2;
        unsigned long flags;
        struct counter12* c12p = &brd->clock12;

        /* a few prime numbers for factoring the number of clock ticks. */
        static int primes[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97};
        int nprime = sizeof(primes)/sizeof(primes[0]);

        if (outputRate <= 0) {
            KLOG_ERR("board %d: invalid output clock rate=%d Hz\n", brd->num,outputRate);
            return -EINVAL;
        }

        if (outputRate >= inputRate){
                KLOG_ERR("board %d: Cannot set output clock rate (%d Hz) greater than input oscillator rate (%d Hz)!\n",
                        brd->num,outputRate,inputRate);
                return -EINVAL;
        }

        spin_lock_irqsave(&c12p->lock,flags);

        if (inputRate != c12p->inputRate && c12p->inputRate != 0) {
                KLOG_ERR("board %d: counter/timer 1/2 is already in use with an input rate of %d KHz\n",
                            brd->num,c12p->inputRate/1000);
                spin_unlock_irqrestore(&c12p->lock,flags);
                return -EBUSY;
        }

        if (outputRate != c12p->outputRate && c12p->outputRate != 0) {
                KLOG_ERR("board %d: counter/timer 1/2 is already in use with an output rate of %d Hz\n",
                        brd->num,c12p->outputRate);
                spin_unlock_irqrestore(&c12p->lock,flags);
                return -EBUSY;
        }

        atomic_inc(&c12p->userCount);

        if(c12p->inputRate == inputRate &&
                c12p->outputRate == outputRate) {
                spin_unlock_irqrestore(&c12p->lock,flags);
                /* already setup */
                return 0;
        }

        if ((result = brd->setClock1InputRate(brd,inputRate)) != 0) {
                atomic_dec(&c12p->userCount);
                spin_unlock_irqrestore(&c12p->lock,flags);
                return result;
        }

        c1 = (inputRate + outputRate/2) / outputRate;

        /* Try to find clock divider: input rate/output rate */
        KLOG_DEBUG("clock c1=%ld,outputRate=%d\n",c1,outputRate);
        c2 = 1;

        // The minimum workable counter value for a 82C54 clock chip
        // on a MMAT in mode 2 or 3 is 2.  1 doesn't seem to work.
        // 0 is actually equivalent to 2^16
        while (c1 > 65535 || c2 == 1) {
            int i;
            for (i = 0; i < nprime; i++) {
                if (!(c1 % primes[i])) break;
            }
            if (i < nprime) {
                    c2 *= primes[i];
                    c1 /= primes[i];
            }
            else if (c1 * c2 <= inputRate) c1++; // fudge it
            else c1--;
        }

        if (c1 * c2 * outputRate != inputRate) {
            int f;
            int n = div_10(inputRate,c2*c1,5,&f);
            KLOG_WARNING("board %d: output rate=%d Hz does not divide evenly into %u Hz clock. Actual output rate will be %d.%05d Hz\n",
                    brd->num,outputRate,inputRate,n,f);
        }

        KLOG_DEBUG("c1=%d,c2=%d\n",c1,c2);

        setTimerClock(brd,1,2,c1);
        setTimerClock(brd,2,2,c2);

        c12p->inputRate = inputRate;
        c12p->outputRate = outputRate;

        spin_unlock_irqrestore(&c12p->lock,flags);

        return result;
}

static void releaseClock12(struct DMMAT* brd)
{
        unsigned long flags;
        struct counter12* c12p = &brd->clock12;

        spin_lock_irqsave(&c12p->lock,flags);

        if (atomic_read(&c12p->userCount) > 0 && atomic_dec_and_test(&c12p->userCount)) {
                c12p->inputRate = 0;
                c12p->outputRate = 0;
        }
        spin_unlock_irqrestore(&c12p->lock,flags);
}

/*
 * Return pointer to the current A2D device name, which
 * is /dev/dmmmat_a2dN or /dev/dmmat_d2dN, depending
 * on whether we are in normal or waveform mode.
 */
const char *getA2DDeviceName(struct DMMAT_A2D* a2d)
{
        return a2d->deviceName[a2d->mode];
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
        int nchan = a2d->getNumA2DChannels(a2d);
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

        /* does a scheduler sleep, so don't hold a spin_lock */
        a2d->waitForA2DSettle(a2d);
        return 0;
}

/**
 * returns: 0: ok,  -EINVAL: channels out of range.
 */
static int selectA2DChannelsMM32XAT(struct DMMAT_A2D* a2d)
{
        unsigned long flags;
        int nchan = a2d->getNumA2DChannels(a2d);

        if (a2d->lowChan < 0) return -EINVAL;
        if (a2d->highChan >= nchan ||
            a2d->highChan < a2d->lowChan) return -EINVAL;
        if (a2d->lowChan > a2d->highChan) return -EINVAL;
          
        spin_lock_irqsave(&a2d->brd->reglock,flags);
        outb(a2d->lowChan, a2d->brd->addr + 2);
        outb(a2d->highChan, a2d->brd->addr + 3);
        spin_unlock_irqrestore(&a2d->brd->reglock,flags);

        /* does a scheduler sleep, so don't hold a spin_lock */
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
                                getA2DDeviceName(a2d),gain,(bipolar ? "bipolar" : "unipolar"),
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
                                getA2DDeviceName(a2d),gain,(bipolar ? "bipolar" : "unipolar"),
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


/* wait for the A2D to settle after setting the gain or input channel.
 * Does not hold the register spin_lock */
static void waitForA2DSettleMM16AT(struct DMMAT_A2D* a2d)
{
        int ntry = 0;
        do {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(1);
        } while(ntry++ < 50 && inb(a2d->brd->addr + 10) & 0x80);
        KLOG_DEBUG("ntry=%d\n",ntry);
}

/* wait for the A2D to settle after setting the gain or input channel.
 * Does not hold the register spin_lock */
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

/**
 * Reset state of data processing
 */
static void resetA2D_processing(struct DMMAT_A2D *a2d)
{
        flush_workqueue(work_queue);

        freeA2DFilters(a2d);

        a2d->nwaveformChannels = 0;
        a2d->totalOutputRate = 0;

        free_dsm_disc_circ_buf(&a2d->fifo_samples);
        free_dsm_disc_circ_buf(&a2d->samples);

        a2d->lowChan = MAX_DMMAT_A2D_CHANNELS;
        a2d->highChan = -1;

        memset(&a2d->status,0,sizeof(a2d->status));
}

/*
 * Initial configuration of an A2D.
 */
static int configA2D(struct DMMAT_A2D* a2d,
        struct nidas_a2d_config* cfg)
{
        if(atomic_read(&a2d->running)) {
                KLOG_ERR("A2D's running. Can't configure\n");
                return -EBUSY;
        }

        resetA2D_processing(a2d);

        a2d->scanRate = cfg->scanRate;

        a2d->latencyMsecs = cfg->latencyUsecs / USECS_PER_MSEC;
        if (a2d->latencyMsecs == 0) a2d->latencyMsecs = 500;

        a2d->latencyJiffies = (cfg->latencyUsecs * HZ) / USECS_PER_SEC;
        if (a2d->latencyJiffies == 0) a2d->latencyJiffies = HZ / 10;
        KLOG_DEBUG("%s: latencyJiffies=%ld, HZ=%d\n",
                    getA2DDeviceName(a2d),a2d->latencyJiffies,HZ);

        a2d->scanDeltaT = TMSECS_PER_SEC / a2d->scanRate;
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
        struct a2d_filter_info* filters = 0;
        struct a2d_filter_info* finfo = 0;
        struct short_filter_methods methods;

        if(atomic_read(&a2d->running)) {
                KLOG_ERR("A2D's running. Can't configure\n");
                return -EBUSY;
        }

        if (a2d->mode == A2D_NORMAL) {
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
                           getA2DDeviceName(a2d), cfg->sindex, a2d->nfilters);

                if (cfg->sindex < 0 || cfg->sindex >= a2d->nfilters)
                        return -EINVAL;

                KLOG_DEBUG("%s: scanRate=%d,cfg->rate=%d\n",
                           getA2DDeviceName(a2d), a2d->scanRate, cfg->rate);

               if (a2d->scanRate % cfg->rate) {
                        KLOG_ERR
                            ("%s: A2D scanRate=%d is not a multiple of the rate=%d for sample %d\n",
                             getA2DDeviceName(a2d), a2d->scanRate, cfg->rate,
                             cfg->sindex);
                        return -EINVAL;
                }

                finfo->decimate = a2d->scanRate / cfg->rate;
                finfo->filterType = cfg->filterType;
                finfo->index = cfg->sindex;

                KLOG_DEBUG("%s: decimate=%d,filterType=%d,index=%d\n",
                           getA2DDeviceName(a2d), finfo->decimate, finfo->filterType,
                           finfo->index);

                methods = get_short_filter_methods(cfg->filterType);
                if (!methods.init) {
                        KLOG_ERR("%s: filter type %d unsupported\n",
                                 getA2DDeviceName(a2d), cfg->filterType);
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
        }

        /* keep track of the total number of output samples/sec */
        a2d->totalOutputRate += cfg->rate;

        for (i = 0; i < cfg->nvars; i++) {
                int ichan = cfg->channels[i];
                if (ichan < 0 || ichan >= MAX_DMMAT_A2D_CHANNELS) {
                        KLOG_ERR("%s: channel number %d out of range\n",
                                 getA2DDeviceName(a2d), ichan);
                        return -EINVAL;
                }
                KLOG_DEBUG("%s: configuring channel %d,gain=%d,bipolar=%d\n",
                     getA2DDeviceName(a2d), ichan,cfg->gain[i],cfg->bipolar[i]);

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

                if (a2d->mode == A2D_WAVEFORM) {
                        // waveform input channel numbers, 0-(max-1) in the order they were requested.
                        a2d->waveformChannels[a2d->nwaveformChannels++] = cfg->channels[i];
                }
        }

        a2d->nchanScanned = a2d->highChan - a2d->lowChan + 1;

        /* Configure the filter */
        if (a2d->mode == A2D_NORMAL) {
                // subtract low channel from the filter channel numbers
                // since we don't scan the channels below lowChan
                for (i = 0; i < cfg->nvars; i++)
                        finfo->channels[i] -= a2d->lowChan;

                result = finfo->fconfig(finfo->filterObj, finfo->index,
                                       finfo->nchans,
                                       finfo->channels,
                                       finfo->decimate,
                                       cfg->filterData,
                                       cfg->nFilterData);
                if (result) KLOG_ERR("%s: error in filter config\n",getA2DDeviceName(a2d));
        }
        return result;
}

/*
 * Handler for A2D interrupts. Called from board interrupt handler.
 * The brd->reglock spinlock is locked prior to calling this function.
 */
static irqreturn_t dmmat_a2d_handler(struct DMMAT_A2D* a2d)
{
        // brd->reglock is locked before entering this function

        struct DMMAT* brd = a2d->brd;
        int flevel = a2d->getFifoLevel(a2d);
        int i;
        struct dsm_sample* samp;

        switch (flevel) {
        default:
        case 3: 
                /* full or overflowed, we're falling behind */
#ifdef REPORT_UNDER_OVERFLOWS
                if (!(a2d->status.fifoOverflows++ % 10))
                        KLOG_WARNING("%s: fifoOverflows=%d, restarting A2D\n",
                                        getA2DDeviceName(a2d),a2d->status.fifoOverflows);
#else
                a2d->status.fifoOverflows++;
#endif
                /* resets the fifo */
                a2d->stop(a2d);
                if (a2d->mode == A2D_WAVEFORM) {
                        /* if in waveform mode, lower DOUT, reset waveforms , raise DOUT */
                        a2d->brd->d2d->stop(a2d->brd->d2d);
                        a2d->brd->d2a->stopWaveforms(a2d->brd->d2a);
                        /* Discard any existing samples in fifo_samples (set tail=head)
                         * and notify the waveform bottom half to reset its sample counters,
                         * so that the D2A and A2D buffering can get back in sync.
                         */
                        a2d->overflow = 1;
                        a2d->fifo_samples.head = ACCESS_ONCE(a2d->fifo_samples.tail);
                }
                a2d->start(a2d);
                if (a2d->mode == A2D_WAVEFORM) {
                        a2d->brd->d2a->startWaveforms(a2d->brd->d2a);
                        a2d->brd->d2d->start(a2d->brd->d2d);
                }
                return IRQ_HANDLED;
        case 2:
                /* at or above threshold, but not full (expected value) */
                break;
        case 1:
                /* less than threshold. These seem to occur in clusters on the viper.
                 * They are basically like the spurious interrupts that we see
                 * from time to time from other cards, except here the irq status register
                 * indicates that the A2D has a pending interrupt.  If we just
                 * ignore the interrupt things seem to proceed with no ill effects.
                 */
#ifdef REPORT_UNDER_OVERFLOWS
                if (!(a2d->status.fifoUnderflows++ % 1000))
                        KLOG_WARNING("%s: fifoUnderflows=%d\n",
                                        getA2DDeviceName(a2d),
                                        a2d->status.fifoUnderflows);
#else
                a2d->status.fifoUnderflows++;
#endif
                return IRQ_NONE;
        case 0:
                /* fifo empty. Shouldn't happen, but treat like a less-than-threshold issue */
                if (!(a2d->status.fifoEmpty++ % 100))
                        KLOG_WARNING("%s: fifoEmpty=%d\n",
                                getA2DDeviceName(a2d),a2d->status.fifoEmpty);
                return IRQ_NONE;
        }

        samp = GET_HEAD(a2d->fifo_samples,a2d->fifo_samples.size);
        if (!samp) {                // no output sample available
                a2d->status.missedSamples += (a2d->fifoThreshold / a2d->nchanScanned);
                KLOG_WARNING("%s: missedSamples=%d\n",
                                getA2DDeviceName(a2d),a2d->status.missedSamples);
                for (i = 0; i < a2d->fifoThreshold; i++) inw(brd->addr);
                return IRQ_HANDLED;
        }

        samp->timetag = getSystemTimeTMsecs();

        // Finally!!!! the actual read from the hardware fifo.
        // All this overhead just to do this...
        insw(brd->addr,(short*)samp->data,a2d->fifoThreshold);
        samp->length = a2d->fifoThreshold * sizeof(short);

        /* increment head. This sample is ready for processing
         * by bottom-half workers */
        INCREMENT_HEAD(a2d->fifo_samples,a2d->fifo_samples.size);

        switch (a2d->mode) {
        case A2D_NORMAL:
                queue_work(work_queue,&a2d->worker);
                break;
        case A2D_WAVEFORM:
                queue_work(work_queue,&a2d->waveform_worker);
                break;
        }
        return IRQ_HANDLED;
}

/*
 * Handler for counter 0 interrupts. Called from board interrupt handler.
 * The brd->reglock spinlock is locked prior to calling this function.
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
 * General IRQ handler for the board.  Calls the A2D handler and/or
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

        // acknowledge interrupt now.
        // If an A2D or counter interrupt happens between the
        // time of the read of itr_status_reg and this acknowledgement
        // then we could miss an interrupt, but there doesn't seem
        // to be any way of solving that in software.
        outb(brd->itr_ack_val, brd->itr_ack_reg);

        if (status & brd->cntr_itr_mask)
                result = dmmat_cntr_handler(brd->cntr);
        if (status & brd->ad_itr_mask)
                result = dmmat_a2d_handler(brd->a2d);

        spin_unlock(&brd->reglock);
        return result;
}

/*
 * Both the A2D and the pulse counter device use interrupts.
 * If an interrupt handler has already been set up for this
 * board, then do nothing.
 * user_type: 0=a2d, 1=cntr
 */
static int dmd_mmat_add_irq_user(struct DMMAT* brd,int user_type)
{
        int result;
        if ((result = mutex_lock_interruptible(&brd->irqreq_mutex)))
                return result;
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
                        mutex_unlock(&brd->irqreq_mutex);
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
        mutex_unlock(&brd->irqreq_mutex);
        return result;
}

static int dmd_mmat_remove_irq_user(struct DMMAT* brd,int user_type)
{
        int result;
        if ((result = mutex_lock_interruptible(&brd->irqreq_mutex)))
                return result;
        if (brd->irq_users[user_type] > 0) brd->irq_users[user_type]--;
        if (brd->irq != 0 && brd->irq_users[0] + brd->irq_users[1] == 0) {
                KLOG_NOTICE("freeing irq %d\n",brd->irq);
                free_irq(brd->irq,brd);
                brd->irq = 0;
        }
        mutex_unlock(&brd->irqreq_mutex);
        return result;
}

/*
 * Function to stop the A2D on a MM16AT.
 * spin_lock of brd->reglock should be held prior
 * to calling this function.
 */
static void stopA2D_MM16AT(struct DMMAT_A2D* a2d)
{
        struct DMMAT* brd = a2d->brd;

        // disable A2D triggering and interrupts
        brd->itr_ctrl_val &= ~0x83;
        outb(brd->itr_ctrl_val, brd->addr + 9);

        // disble fifo, scans
        outb(0,brd->addr + 10);

        // reset fifo
        outb(0x80,brd->addr + 10);

}

/*
 * Function to stop the A2D on a MM32XAT.
 * spin_lock of brd->reglock should be held prior
 * to calling this function.
 */
static void stopA2D_MM32XAT(struct DMMAT_A2D* a2d)
{
        struct DMMAT* brd = a2d->brd;

        // set page to 0
        outb(0x00, brd->addr + 8);
        
        // disable A2D interrupts, hardware clock
        brd->itr_ctrl_val &= ~0x83;
        outb(brd->itr_ctrl_val, brd->addr + 9);

        // disable and reset fifo, disable scan mode
        outb(0x2,brd->addr + 7);

}

/**
 * General function to stop the A2D. Calls the board specific method.
 * a2d->mutex should be locked, so that a2d->running
 * reflects the current state of the A2D.
 */
static void stopA2D(struct DMMAT_A2D* a2d)
{
        struct DMMAT* brd = a2d->brd;
        unsigned long flags = 0;

        if (atomic_read(&a2d->running) == 0) return;

        spin_lock_irqsave(&brd->reglock,flags);
        a2d->stop(a2d);
        spin_unlock_irqrestore(&brd->reglock,flags);

        releaseClock12(brd);

        dmd_mmat_remove_irq_user(brd,0);

        KLOG_INFO("%s: stopping A2D, missedSamples=%d, fifoOverFlows=%d, fifoUnderflows=%d\n",
                getA2DDeviceName(a2d),a2d->status.missedSamples,
                a2d->status.fifoOverflows,a2d->status.fifoUnderflows),

        resetA2D_processing(a2d);

        atomic_set(&a2d->running,0);
}

static int getA2DThreshold_MM16AT(struct DMMAT_A2D* a2d)
{
        return 256;
}

/*
 * Compute interrupt theshold level of A2D hardware fifo on the MM32XAT,
 * based on the values of the A2D scan rate, number of sampled channels
 * and the requested latency.
 */
static int getA2DThreshold_MM32XAT(struct DMMAT_A2D* a2d)
{
        int nscans,nsamps;

        if (a2d->scanRate == 0) {
                KLOG_ERR("%s: scanRate is not defined.",getA2DDeviceName(a2d));
                return -EINVAL;
        }
        if (a2d->nchanScanned == 0) {
                KLOG_ERR("%s: number of scanned channels is 0",getA2DDeviceName(a2d));
                return -EINVAL;
        }

        /* figure out a fifo threshold, so that we get an interrupt
         * about every latencyMsecs.
         */
        /* number of scans in latencyMsecs */
        nscans = (a2d->latencyMsecs * a2d->scanRate) / MSECS_PER_SEC;
        if (nscans < 1) nscans = 1;

        /* number of word samples in latencyMsecs */
        nsamps = nscans * a2d->nchanScanned;
        /* threshold must be a multiple of nchanScanned, and even */
        if (nsamps > a2d->maxFifoThreshold) nsamps =
                (a2d->maxFifoThreshold / a2d->nchanScanned) * a2d->nchanScanned;
        if ((nsamps % 2)) nsamps += a2d->nchanScanned;
        if (nsamps == 2) nsamps = 4;
        a2d->fifoThreshold = nsamps;
        return nsamps;
}

/*
 * Function to start the A2D on a MM16AT.
 * spin_lock of brd->reglock should be held prior
 * to calling this function.
 * This function should be callable from interrupt context.
 */
static void startA2D_MM16AT(struct DMMAT_A2D* a2d)
{
        struct DMMAT* brd = a2d->brd;
        unsigned char regval;

        if (a2d->mode != A2D_NORMAL) return;

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

        /* fifo enable, scan enable */
        regval = inb(brd->addr + 10);
        regval |= 0x30;

        outb(regval,brd->addr + 10);

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
}

/*
 * Function to start the A2D on a MM32XAT.
 * spin_lock of brd->reglock should be held prior
 * to calling this function.
 * This function should be callable from interrupt context.
 */
static void startA2D_MM32XAT(struct DMMAT_A2D* a2d)
{
        struct DMMAT* brd = a2d->brd;
        int nsamps;
#ifdef OUTPUT_CLOCK12_DOUT2
        unsigned char regval;
#endif

        outb(0x04,brd->addr + 8);	// set page 4
        outb(0x02,brd->addr + 14);	// abort any currently running autocal
        outb(0x10,brd->addr + 14);	// disable auto-cal
        outb(0x00,brd->addr + 8);	// set page 0

        // register value is 1/2 the threshold
        nsamps = a2d->fifoThreshold / 2;

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

        /* ---------
         * Base + 10
         * ---------
         * Bit 7: Input frequency (reference clock) for counter 1/2
         *        1 = 100KHz
         *        0 = 10 MHz
         * 
         * Bit 6: Input frequency (reference clock) for counter 0 
         *        1 = 10 KHz
         *        0 = 10 MHz
         *
         * Bit 5: Counter 1/2 output enable
         *        1 = Output appears on I/O header J3 pin 42, OUT2/DOUT2
         *        0 = OUT2/DOUT2 pin is set by bit DOUT2 at base + 1
         * 
         * Bit 4: Counter 0 output enable
         *        1 = Output appears on I/O header J3 pin 44, OUT0/DOUT0
         *        0 = OUT0/DOUT0 pin is set by bit DOUT0 at base + 1
         * 
         * Bit 3: Reserved for future boards
         * 
         * Bit 2: Counter 0 gate enable
         *        1 = Gate 0/DIN 1, J3 pin 47, acts as an active high gate for
         *            counter/timer 0. This pin is connected to a 10K pull-up
         *            resistor.
         *        0 = No gating
         *
         * Bit 1: Counter 0 input source
         *        1 = Input determined by bit 6
         *        0 = Input to counter 0 is J3 pin 48 (CLK0/DIN0). The falling edge
         *            is active. This pin is connected to a 10K pull-up resistor.
         *
         * Bit 0: Counter 1/2 external trigger gate enable.
         *        1 = When J3 pin 46 (EXTGATE / DIN 2) is low prior to the start of
         *            A/D conversions, A/D conversions will not begin until it is 
         *            brought high (trigger mode).
         *
         *            If the pin is brought low while conversions are occurring, 
         *            conversions will pause until it is brought high (gate mode).
         *
         *            J3 pin 46 is connected to a 10KΩ pull-up resistor.
         *
         *        0 = The interrupt operation begins immediately, no external
         *            trigger or gating.
         *
         */
  

#ifdef OUTPUT_CLOCK12_DOUT2
        regval = inb(brd->addr + 10);
        /* Output the 1&2 clock to J3 pin 42, OUT2/DOUT2 so that we can see
         * what is going on from an oscilloscope.
         */
        regval |= 0x20;
        outb(regval, brd->addr + 10);
#endif

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
}

/*
 * General function to start an A2D. Calls the board specific method.
 * Does alloc_dsm_disc_circ_buf, which does kmalloc(,GFP_KERNEL) and
 * calls a2d->waitForA2DSettle(a2d) which does a scheduler sleep,
 * so spin_locks should NOT be held before calling this function.
 * a2d->mutex should be locked, so that a2d->running
 * reflects the current state of the A2D.
 */
static int startA2D(struct DMMAT_A2D* a2d)
{
        int result;
        unsigned long flags = 0;
        struct DMMAT* brd = a2d->brd;
        int nsamps;

        /* Create sample circular buffers to hold this much
         * data to get over momentary lapses. Then the bottom-half
         * worker can get this far behind the interrupt handler,
         * and likewise, user reads can get this far behind the
         * bottom-half worker without losing data.
         */
        int maxBufferSecs = 2;

        memset(&a2d->read_state,0,sizeof(a2d->read_state));
        a2d->lastWakeup = jiffies;

        result = a2d->getA2DThreshold(a2d);
        if (result < 0) return result;
        a2d->fifoThreshold = result;

        /*
         * Allocate circular buffer space for samples from the FIFO.
         * Figure out how many fifo samples to expect per second.
         * Then allow a buffering time of maxBufferSecs.
         *   shorts/sec = (scanRate * nchanScanned)
         *   shorts/fifo = fifoThreshold
         *   fifo/sec = scanRate * nchanScanned / fifoThreshold
         */
        nsamps = maxBufferSecs * a2d->scanRate * a2d->nchanScanned / a2d->fifoThreshold;
        /* next higher power of 2. fls()=find-last-set bit, numbered from 1 */
        nsamps = 1 << fls(nsamps);
        if (nsamps < 4) nsamps = 4;

        KLOG_INFO("%s: scan rate=%d Hz, latency=%d msecs, fifo size=%d, nchans=%d, circbuf size=%d\n",
            getA2DDeviceName(a2d),a2d->scanRate,a2d->latencyMsecs,
            a2d->fifoThreshold,a2d->nchanScanned,nsamps);

        free_dsm_disc_circ_buf(&a2d->fifo_samples);

        /* Data portion must be as big as the FIFO threshold. */
        result = alloc_dsm_disc_circ_buf(&a2d->fifo_samples,
            a2d->fifoThreshold * sizeof(short), nsamps);
        if (result) return result;

        free_dsm_disc_circ_buf(&a2d->samples);

        a2d->bh_data.saveSample.length = 0;
        memset(&a2d->waveform_bh_data,0,sizeof(a2d->waveform_bh_data));
        a2d->overflow = 0;

        /* number of output samples in maxBuffSecs */
        nsamps = maxBufferSecs * a2d->totalOutputRate;
        /* next higher power of 2. fls()=find-last-set bit, numbered from 1 */
        nsamps = 1 << fls(nsamps);
        if (nsamps < 4) nsamps = 4;

        KLOG_INFO("%s: totalOutputRate=%d Hz, circbuf size=%d\n",
            getA2DDeviceName(a2d),a2d->totalOutputRate,nsamps);

        if (a2d->mode == A2D_NORMAL) {
                /*
                 * Output samples. Data portion just needs to be
                 * as big as the number of channels on the board
                 * plus one for the sample id.
                 */
                result = alloc_dsm_disc_circ_buf(&a2d->samples,
                    (MAX_DMMAT_A2D_CHANNELS + 1) * sizeof(short), nsamps);
                if (result) return result;
        }
        else {
                /*
                 * Allocate output samples. Data portion just needs to be
                 * as big as the wavesize plus one for the sample id.
                 */
                result = alloc_dsm_disc_circ_buf(&a2d->samples,
                    (a2d->wavesize + 1) * sizeof(short), nsamps);
                if (result) return result;
                /* set counter into wave sample so that new samples are allocated
                 * on first run of waveform bottom half. */
                a2d->waveform_bh_data.waveSampCntr = a2d->wavesize;
        }

        /* selectA2DChannels does its own spin_lock */
        if ((result = a2d->selectA2DChannels(a2d))) return result;

        // needs scanRate and high/low channels defined
        result = a2d->getConvRateSetting(a2d,&a2d->gainConvSetting);
        if (result != 0) return result;

        spin_lock_irqsave(&brd->reglock,flags);
        // same addr on MM16AT and MM32XAT
        outb(a2d->gainConvSetting,a2d->brd->addr + 11);
        spin_unlock_irqrestore(&brd->reglock,flags);

        /* wait for A2D to settle after setting gain or channel.
         * Does a scheduler sleep, don't hold a spin_lock.
         */
        a2d->waitForA2DSettle(a2d);

        /* this holds a mutex */
        result = dmd_mmat_add_irq_user(brd,0);
        if (result) return result;

        spin_lock_irqsave(&brd->reglock,flags);

        if (!(result = setupClock12(brd,DMMAT_CNTR1_INPUT_RATE,a2d->scanRate))) {
                /* board specific method */
                a2d->start(a2d);
        }
        spin_unlock_irqrestore(&brd->reglock,flags);

        if (result) {
                result = dmd_mmat_remove_irq_user(brd,0);
                return result;
        }

        atomic_set(&a2d->running,1);	// Set the running flag

        return result;
}

static int startAutoCal_MM32XAT(struct DMMAT_A2D* a2d)
{
        unsigned long flags = 0;
        struct DMMAT* brd = a2d->brd;
        int ntry = 1000;
        int result = 0;

        spin_lock_irqsave(&brd->reglock,flags);

        outb(0x04,brd->addr + 8);	// set page 4
        outb(0x01,brd->addr + 14);	// start cal
        do {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(1);
        } while(inb(a2d->brd->addr + 14) & 0x02 && ntry--);
        KLOG_DEBUG("auto calibration nloop=%d\n",1000-ntry);
        outb(0x00,brd->addr + 8);	// set page 0
        if (ntry == 0) {
            KLOG_ERR("auto calibration timeout\n");
            result = -ETIMEDOUT;
        }
        spin_unlock_irqrestore(&brd->reglock,flags);
        return result;
}

/*
 * Function to stop the counter on a MM32AT
 */
static void stopCntr_MM16AT(struct DMMAT_CNTR* cntr)
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
static void stopCntr_MM32AT(struct DMMAT_CNTR* cntr)
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
        int result;
        struct DMMAT* brd = cntr->brd;

        if (!atomic_read(&cntr->running)) return 0;

        atomic_set(&cntr->shutdownTimer,1);
        del_timer_sync(&cntr->timer);

        // call the board-specific stop function
        cntr->stop(cntr);

        result = dmd_mmat_remove_irq_user(brd,1);
        atomic_set(&cntr->running,0);

        return result;
}

/*
 * Function to start the counter on a MM32AT
 */
static int startCntr_MM16AT(struct DMMAT_CNTR* cntr)
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
static int startCntr_MM32AT(struct DMMAT_CNTR* cntr)
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

        if (atomic_read(&cntr->running)) stopCNTR(cntr);

        atomic_set(&cntr->running,1);

        memset(&cntr->status,0,sizeof(cntr->status));

        spin_lock_irqsave(&brd->reglock,flags);

        EMPTY_CIRC_BUF(cntr->samples);

        spin_unlock_irqrestore(&brd->reglock,flags);

        cntr->jiffiePeriod = (cfg->msecPeriod * HZ) / MSECS_PER_SEC;

        /* start the timer, give it more than one period at first */
        jnext = jiffies;
        jnext += cntr->jiffiePeriod - (jnext % cntr->jiffiePeriod);
        jnext += cntr->jiffiePeriod;

        cntr->timer.expires = jnext;
        atomic_set(&cntr->shutdownTimer,0);
        cntr->firstTime = 1;
        cntr->lastVal = 0;
        add_timer(&cntr->timer);

        result = dmd_mmat_add_irq_user(brd,1);
        if (result) return result;

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

        i = CIRC_SPACE(a2d->samples.head,a2d->samples.tail,a2d->samples.size);
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
                    GET_HEAD(a2d->samples,a2d->samples.size);
                if (!osamp) {
                        // no output sample available
                        // still execute filter so its state is up-to-date.
                        struct a2d_sample toss;
                        if (!(a2d->status.missedSamples++ % 1000))
                            KLOG_WARNING("%s: missedSamples=%d\n",
                                getA2DDeviceName(a2d),a2d->status.missedSamples);
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
                        INCREMENT_HEAD(a2d->samples,a2d->samples.size);
                }
        }
}

/**
 * Worker function that invokes filters on the data in a fifo sample.
 * The contents of the fifo sample is not necessarily a multiple
 * of the number of channels scanned.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void dmmat_a2d_bottom_half(struct work_struct* work)
#else
static void dmmat_a2d_bottom_half(void* work)
#endif
{

        struct DMMAT_A2D* a2d = container_of(work,struct DMMAT_A2D,worker);

        struct a2d_bh_data* tld = &a2d->bh_data;

        dsm_sample_time_t tt0;

        int saveChan = tld->saveSample.length / sizeof(short);

        // the consumer of fifo_samples.
        struct dsm_sample* insamp;
        while ((insamp = GET_TAIL(a2d->fifo_samples,a2d->fifo_samples.size))) {
                int nval = insamp->length / sizeof(short);
                // # of deltaT to back up for timetag of first sample
                int ndt = (nval - 1) / a2d->nchanScanned;
                int dt = ndt * a2d->scanDeltaT;    // 1/10ths of msecs
                short *dp = (short *)insamp->data;
                short *ep = dp + nval;

                if (saveChan > 0) {     // leftover data
                    int n = min(nval,a2d->nchanScanned - saveChan);
                    memcpy(tld->saveSample.data+saveChan,dp,
                        n * sizeof(short));
                    dp += n;
                    saveChan += n;
                    if (saveChan == a2d->nchanScanned) {
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
                    if(n >= a2d->nchanScanned) {
                        do_filters(a2d, tt0,dp);
                        dp += a2d->nchanScanned;
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
                INCREMENT_TAIL(a2d->fifo_samples,a2d->fifo_samples.size);
                // see fast bottom half for a discussion of this.
                if (((long)jiffies - (long)a2d->lastWakeup) > a2d->latencyJiffies ||
                        CIRC_SPACE(a2d->samples.head,a2d->samples.tail,
                        a2d->samples.size) < a2d->samples.size/2) {
                        wake_up_interruptible(&a2d->read_queue);
                        a2d->lastWakeup = jiffies;
                }
        }
        tld->saveSample.length = saveChan * sizeof(short);
}

/**
 * Worker function that invokes filters on the data in a fifo sample.
 * The contents of the fifo sample must be a multiple
 * of the number of channels scanned.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void dmmat_a2d_bottom_half_fast(struct work_struct* work)
#else
static void dmmat_a2d_bottom_half_fast(void* work)
#endif
{
        struct DMMAT_A2D* a2d = container_of(work,struct DMMAT_A2D,worker);
        struct dsm_sample* insamp;

        dsm_sample_time_t tt0;

        KLOG_DEBUG("%s: worker entry, fifo head=%d,tail=%d\n",
            getA2DDeviceName(a2d),a2d->fifo_samples.head,a2d->fifo_samples.tail);

        while ((insamp = GET_TAIL(a2d->fifo_samples,a2d->fifo_samples.size))) {

                int nval = insamp->length / sizeof(short);
                short *dp = (short *)insamp->data;
                short *ep = dp + nval;

                // # of deltaTs to backup for first timetag
                int ndt = (nval - 1) / a2d->nchanScanned;
                int dt = ndt * a2d->scanDeltaT;

                BUG_ON((nval % a2d->nchanScanned) != 0);

                // tt0 is conversion time of first compete scan in fifo
                tt0 = insamp->timetag - dt;  // fifo interrupt time

                for (; dp < ep; ) {
                    do_filters(a2d, tt0,dp);
                    dp += a2d->nchanScanned;
                    tt0 += a2d->scanDeltaT;
                }
                INCREMENT_TAIL(a2d->fifo_samples,a2d->fifo_samples.size);
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
                        a2d->samples.size) < a2d->samples.size/2) {
                        wake_up_interruptible(&a2d->read_queue);
                        a2d->lastWakeup = jiffies;
                }
        }
}

/**
 * Worker function for forming samples when operating in waveform mode.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void dmmat_a2d_waveform_bh(struct work_struct* work)
#else
static void dmmat_a2d_waveform_bh(void* work)
#endif
{
        struct DMMAT_A2D* a2d = container_of(work,struct DMMAT_A2D,waveform_worker);
        struct dsm_sample* insamp;
        int i;
        struct waveform_bh_data* bhd = &a2d->waveform_bh_data;
        int wavecntr = bhd->waveSampCntr;
        unsigned long flags;
        struct DMMAT* brd = a2d->brd;

        dsm_sample_time_t tt0;

        KLOG_DEBUG("%s: worker entry, fifo head=%d,tail=%d\n",
            getA2DDeviceName(a2d),a2d->fifo_samples.head,a2d->fifo_samples.tail);


        for (;;) {
                int nval;
                short *dp;
                short *ep;
                /*
                 * If the hardware fifo overflows, we need to know, so that
                 * we can get back in sync.
                 */
                spin_lock_irqsave(&brd->reglock,flags);
                if (a2d->overflow) {
                        wavecntr = bhd->waveSampCntr = a2d->wavesize;
                        a2d->overflow = 0;
                }
                insamp = GET_TAIL(a2d->fifo_samples,a2d->fifo_samples.size);
                spin_unlock_irqrestore(&brd->reglock,flags);

                if (!insamp) break;

                nval = insamp->length / sizeof(short);
                dp = (short *)insamp->data;
                ep = dp + nval;

                BUG_ON((nval % a2d->nchanScanned) != 0);

                for (; dp < ep; ) {

                        if (wavecntr == a2d->wavesize) {
                                int ndt;
                                int dt;

                                /* If nwaveformChannels number of output samples are not available, we have to
                                 * go through the motions, but discarding A2D values until the beginning
                                 * of the next waveform, so things don't get out-of-whack.
                                 */
                                if (CIRC_SPACE(a2d->samples.head,ACCESS_ONCE(a2d->samples.tail),a2d->samples.size) < a2d->nwaveformChannels) {
                                        a2d->status.missedSamples += a2d->nwaveformChannels;
                                        KLOG_WARNING("%s: missedSamples=%d\n",
                                                getA2DDeviceName(a2d),a2d->status.missedSamples);
                                        /* the owsamp pointers should be NULL */
                                        for (i = 0; i < a2d->nwaveformChannels; i++) BUG_ON(bhd->owsamp[i]);
                                }
                                else {
                                        int head = a2d->samples.head;
                                        // # of deltaTs to backup for first timetag
                                        ndt = (dp - (short*)insamp->data) / a2d->nchanScanned;
                                        dt = ndt * a2d->scanDeltaT;

                                        // tt0 is conversion time of first compete scan in fifo
                                        tt0 = insamp->timetag - dt;  // fifo interrupt time

                                        for (i = 0; i < a2d->nwaveformChannels; i++) {
                                                bhd->owsamp[i] = (short_sample_t*) a2d->samples.buf[head];
                                                head = (head + 1) & (a2d->samples.size - 1);
                                                bhd->owsamp[i]->timetag = tt0;
                                                bhd->owsamp[i]->length = (a2d->wavesize + 1) * sizeof(short);
                                                bhd->owsamp[i]->id = cpu_to_le16(i);
                                        }
                                }
                                wavecntr = 0;
                        }
                        if (bhd->owsamp[0]) {
                                for (i = 0; i < a2d->nwaveformChannels; i++) {
                                        bhd->owsamp[i]->data[wavecntr] = cpu_to_le16(dp[a2d->waveformChannels[i]]);
                                }
                        }
                        dp += a2d->nchanScanned;
                        wavecntr++;
                        if (wavecntr == a2d->wavesize) {
                                for (i = 0; i < a2d->nwaveformChannels; i++) {
                                        if (bhd->owsamp[i]) INCREMENT_HEAD(a2d->samples,a2d->samples.size);
                                        bhd->owsamp[i] = 0;
                                }
                                // We wake up the read_queue.  How often the
                                // queue is woken depends on the requested latency.
                                // 
                                // Since the sample queue may fill up before latencyJiffies have elapsed,
                                // we also wake the read_queue if the output sample queue is half full.
                                if (((long)jiffies - (long)a2d->lastWakeup) > a2d->latencyJiffies ||
                                        CIRC_SPACE(a2d->samples.head,a2d->samples.tail,
                                        a2d->samples.size) < a2d->samples.size/2) {
                                        wake_up_interruptible(&a2d->read_queue);
                                        a2d->lastWakeup = jiffies;
                                }
                        }
                }
                spin_lock_irqsave(&brd->reglock,flags);
                if (!a2d->overflow)
                        INCREMENT_TAIL(a2d->fifo_samples,a2d->fifo_samples.size);
                spin_unlock_irqrestore(&brd->reglock,flags);
        }
        bhd->waveSampCntr = wavecntr;
}

/************ D2A Utilities ****************/

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

                        // Set the DASIM bit on all channels except the last. When it is
                        // set to 0 on the last channel, then all outputs will update.
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
 * Set one or more analog output voltages on a MM32DXAT.
 * This supports setting outputs on more than one board.
 * The 32DXAT version of the board has a 16 bit D2A. See According to the manual
 */
static int setD2A_MM32DXAT(struct DMMAT_D2A* d2a,
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
                        char chn;
                        KLOG_DEBUG("counts[%d]=%d\n",i+iout,outputs->counts[i+iout]);
                        if (outputs->counts[i+iout] < d2a->cmin)
                                outputs->counts[i+iout] = d2a->cmin;
                        if (outputs->counts[i+iout] > d2a->cmax)
                                outputs->counts[i+iout] = d2a->cmax;
                        lsb = outputs->counts[i+iout] % 256;
                        msb = outputs->counts[i+iout] / 256;
                        chn = i << 6;
                        nset++;

#ifdef SET_DASIM_ON_DMM32DXAT
                        // Set the DASIM bit on all channels except the last.
                        // With the 16 bit D2A on the the 32DXAT it doesn't appear
                        // that DASIM is used, instead all the channels are updated
                        // when you read register 5. So it is #ifdef'd out until
                        // it appears to be needed.
                        if (nset < nout) chn |= 0x20;
#endif
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

                        outb(0x07,brd->addr + 8);	// set page 7
                        outb(lsb,brd->addr + 12);
                        outb(msb,brd->addr + 13);

                        outb(0x00,brd->addr + 8);	// set page 0
                        outb(chn,brd->addr + 5);        // channel number

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

        // trigger the update
        inb(brd->addr + 5);

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

static int addWaveform_MM16AT(struct DMMAT_D2A* d2a, struct D2A_Waveform* wave)
{
        KLOG_ERR("%s: output D2A waveforms not supported on this card\n",
                d2a->deviceName);
        return -EINVAL;
}

static int addWaveform_MM32XAT(struct DMMAT_D2A* d2a, struct D2A_Waveform* wave)
{
        if (d2a->nWaveforms == DMMAT_D2A_OUTPUTS_PER_BRD) {
                KLOG_NOTICE("%s: Cannot configure more than %d D2A waveforms.\n", 
                                d2a->deviceName,DMMAT_D2A_OUTPUTS_PER_BRD);
                return -EINVAL;
        }

        if (wave->channel < 0 || wave->channel >= DMMAT_D2A_OUTPUTS_PER_BRD) {
                KLOG_NOTICE("%s: Invalid channel number: %d. Max is %d.\n", 
                                d2a->deviceName,wave->channel,DMMAT_D2A_OUTPUTS_PER_BRD);
                return -EINVAL;
        }

        if (wave->size % 64) {
                KLOG_ERR("%s: waveform size (%d) must be a multiple of 64.\n",
                                        d2a->deviceName,wave->size);
                return -EINVAL;
        }

        if (d2a->wavesize > 0 && wave->size != d2a->wavesize) {
                KLOG_ERR("%s: waveform size (%d) for channel %d is different from other channels (%d)",
                        d2a->deviceName,wave->size,wave->channel,d2a->wavesize);
                return -EINVAL;
        }
        d2a->wavesize = wave->size;
        d2a->waveforms[d2a->nWaveforms++] = wave;
        return 0;
}

static int loadWaveforms_MM16AT(struct DMMAT_D2A* d2a)
{
        KLOG_ERR("%s: output D2A waveforms not supported on this card\n",
                d2a->deviceName);
        return -EINVAL;
}

/*
 * Load the D2A waveform.  Holds spin locks.
 */
static int loadWaveforms_MM32XAT(struct DMMAT_D2A* d2a)
{
        struct DMMAT* brd = d2a->brd; 

        int ipt,ichan;
        unsigned char lsb, msb;
        unsigned long flags;
        int bufaddr;
        int depth30;
        int i;

        if (atomic_read(&d2a->waveform_running) == 1){
                KLOG_NOTICE("D2A is already running\n");
                return -EINVAL;
        } 

        if (d2a->nWaveforms == 0) {
                KLOG_ERR("%s: No waveforms have been added\n",d2a->deviceName);
                return -EINVAL;
        }

        /* we must generate a dummy channel output if the user has asked for 3
         * channels, since the board supports waveform outputs to 1,2 or 4 channels.
         * In this case waveforms[3] will be NULL, and the last channel will
         * be specified twice in the output.
         */
        if (d2a->nWaveforms == 3) {
                BUG_ON(d2a->waveforms[3] != NULL);
                BUG_ON(d2a->waveforms[2] == NULL);
                d2a->nWaveforms = DMMAT_D2A_OUTPUTS_PER_BRD;
        }

        if (d2a->wavesize * d2a->nWaveforms > DMMAT_D2A_WAVEFORM_SIZE){
                KLOG_ERR("%s: waveform size (%d) times #channels (%d) exceeds capacity on board (%d)\n",
                        d2a->deviceName,d2a->wavesize,d2a->nWaveforms,DMMAT_D2A_WAVEFORM_SIZE);
                return -EINVAL;
        }

        spin_lock_irqsave(&brd->reglock,flags);

        // Set to page 5
        outb(0x05, brd->addr + 8);

        // Reset D/A waveform pointer.
        outb(0x04, brd->addr + 15);

        for (i = 0; inb(brd->addr + 4) & 0x80; i++);
        KLOG_DEBUG("%s: i=%d\n",d2a->deviceName,i);

        bufaddr = 0;
        for (ipt = 0; ipt < d2a->wavesize; ipt++) {
                for (ichan = 0; ichan < d2a->nWaveforms; ichan++) {
                        struct D2A_Waveform* wave = d2a->waveforms[ichan];

                        // User must have asked for 3 output channels, repeat last one
                        if (!wave) wave = d2a->waveforms[2];

                        /* 12 bit D2A value */
                        lsb = (wave->point[ipt]) & 0xFF;
                        /*
                         * channel number and upper 4 bits of D2A value
                         * 0x10 = DAGEN bit, so that D2A value is latched to internal memory
                         * and not to the DAC chip.
                         */
                        msb = (wave->channel << 6) + 0x10 + ((wave->point[ipt] >> 8) & 0x0F);

                        // Monitor DACBUSY Bit. Wait till bit shifting into register completes.
                        // Cannot do a process sleep since we hold a spin_lock.
                        for (i = 0; inb(brd->addr + 4) & 0x80; i++);

                        // Write LSB and MSB
                        outb(lsb, brd->addr + 4);
                        outb(msb, brd->addr + 5); 

                        //Store D2A value into the buffer.
                        outb(bufaddr & 0xFF, brd->addr + 12);
                        outb((bufaddr >> 8) & 0x3, brd->addr + 13);
                        bufaddr++;
                }
        }

        /* -------------------
         * Page 5 at base + 14
         * -------------------
         * Bits 7-4: How many values in buffer. Equation is
         *                Depth = [(DEPTH3-0)+1]*64
         *           EX: 512 values means Depth = 8, so DEPTH3-0 = 7
         *
         * Bits 3-2: How many codes per frame output
         *           00 = 1
         *           01 = 2
         *           10 = 4
         *           11 = 4
         *
         * Bits 1-0: What trigger?
         *           00 = Manual (Using WGINC)
         *           01 = Counter 0 output
         *         * 10 = Counter 1/2 output
         *           11 = External Trigger (J3 pin 45)
         */
        depth30 = (d2a->wavesize * d2a->nWaveforms) / 64 - 1;  
        outb( (depth30 << 4) + ((d2a->nWaveforms - 1) << 2) + 2, brd->addr + 14);        

        outb(0x00, brd->addr + 8);      /* page 0 */
        spin_unlock_irqrestore(&brd->reglock,flags);

        /*
         * free the kmalloc'd waveforms
         */
        for (ichan = 0; ichan < DMMAT_D2A_OUTPUTS_PER_BRD; ichan++) {
                struct D2A_Waveform* wave = d2a->waveforms[ichan];
                kfree(wave);
                d2a->waveforms[ichan] = 0;
        }
        return 0;
}

static void stopWaveforms_MM16AT(struct DMMAT_D2A* d2a)
{
}

/*
 * Board specific method to stop the D2A waveforms.
 * spin_lock of brd->reglock should be held.
 */
static void stopWaveforms_MM32XAT(struct DMMAT_D2A* d2a)
{
        struct DMMAT* brd = d2a->brd;
        outb(0x05, brd->addr + 8);      /* page 5 */
        outb(2, brd->addr + 15);        /* stop the waveform generator */
        outb(0x00, brd->addr + 8);      /* page 0 */
}

/**
 * General function to stop the D2A waveform.
 * Calls the board specific method.
 * d2da>waveform_mutex should be locked, so that d2a->waveform_running
 * reflects the current state of the A2D.
 */
static void stopWaveforms(struct DMMAT_D2A* d2a)
{
        struct DMMAT* brd = d2a->brd;
        unsigned long flags;
        int i;

        /* free waveforms */
        d2a->wavesize = 0;
        d2a->nWaveforms = 0;
        d2a->waveformRate = 0;
        for (i = 0; i < DMMAT_D2A_OUTPUTS_PER_BRD; i++) {
                struct D2A_Waveform* wave = d2a->waveforms[i];
                kfree(wave);
                d2a->waveforms[i] = 0;
        }

        if (atomic_read(&d2a->waveform_running) == 0) return;

        spin_lock_irqsave(&brd->reglock,flags);
        d2a->stopWaveforms(d2a);
        spin_unlock_irqrestore(&brd->reglock,flags);

        releaseClock12(brd);

        atomic_set(&d2a->waveform_running, 0);
}

static void startWaveforms_MM16AT(struct DMMAT_D2A* d2a)
{
        return;
}

/*
 * Board specific method to start the D2A waveforms.
 * spin_lock of brd->reglock should be held.
 */
static void startWaveforms_MM32XAT(struct DMMAT_D2A* d2a)
{
        struct DMMAT* brd = d2a->brd;

// #define OUTPUT_CLOCK12_DOUT2
#ifdef OUTPUT_CLOCK12_DOUT2
        unsigned char regval;
#endif

        // Start D2A
        /* -------------------
         * Page 5 at base + 15
         * -------------------
         * Bit 3   (WGINC): Force waveform generator to increment by one 
         *                  frame 
         * 
         * Bit 2   (WGRST): Reset the waveform generator output form the  
         *                  beginning of the D/A code buffer.
         *
         * Bit 1    (WGPS): Pause/stop the waveform generator. The current
         *                  position in memory is saved for the next
         *                  begin/resume, or can be reset using WGRST.
         *
         * Bit 0 (WGSTART): Begin or resume the waveform generator.
         * 
         * NOTE: Only one bit can be set to 1 at once. Commands are 
         *       processed MSB to LSB, so the first 1 encountered determines 
         *       the command that is executed.
         */
        outb(0x05, brd->addr + 8);      /* Set to page 5 */
        outb(0x04, brd->addr + 15);     /* reset the waveform pointer */
        outb(0x01, brd->addr + 15);     /* start the waveform generator */
        outb(0x00, brd->addr + 8);      /* back to page 0 */

#ifdef OUTPUT_CLOCK12_DOUT2
        regval = inb(brd->addr + 10);
        /* Output the 1&2 clock to J3 pin 42, OUT2/DOUT2 so that we can see
         * what is going on from an oscilloscope.
         * TODO: control this from a driver parameter: debug12=0|1
         */
        regval |= 0x20;
        outb(regval, brd->addr + 10);
#endif
}

/*
 * General function to start the D2A waveform. Calls the board specific method.
 * d2da>waveform_mutex should be locked, so that d2a->waveform_running
 * reflects the current state of the A2D.
 */
static int startWaveforms(struct DMMAT_D2A* d2a)
{
        struct DMMAT* brd = d2a->brd;
        int result;
        unsigned long flags;

        if (atomic_read(&d2a->waveform_running)) {
                KLOG_ERR("%s: already running",d2a->deviceName);
                return -EBUSY;
        }

        /*
         * Setup the D2A Settings and loads waveform.
         * Sets d2a->wavesize and nWaveforms. Holds the spin_lock.
         */
        if ( (result = d2a->loadWaveforms(d2a)) != 0){
                KLOG_ERR("%s: Failed to load D2A waveform",d2a->deviceName);
                return result;
        }

        if (d2a->waveformRate == 0) {
                KLOG_ERR("%s: unknown waveform rate",d2a->deviceName);
                return -EINVAL;
        }

        if (d2a->wavesize == 0) {
                KLOG_ERR("%s: unknown waveform size",d2a->deviceName);
                return -EINVAL;
        }


        spin_lock_irqsave(&brd->reglock,flags);

        if (!(result = setupClock12(brd,DMMAT_CNTR1_INPUT_RATE,
                                        d2a->waveformRate * d2a->wavesize))) {
                /* board specific method */
                d2a->startWaveforms(d2a);
        }
        spin_unlock_irqrestore(&brd->reglock,flags);

        atomic_set(&d2a->waveform_running, 1);
        return result;
}

static void startD2D_MM16AT(struct DMMAT_D2D* d2d)
{
}

static void startD2D_MM32XAT(struct DMMAT_D2D* d2d)
{
        struct DMMAT* brd = d2d->brd;
        /* Set DOUT1 high */
        outb(0x2, brd->addr + 1);
}

static int startD2D(struct DMMAT_D2D* d2d)
{
        int result;
        unsigned long flags;
        struct DMMAT* brd = d2d->brd;
        struct DMMAT_D2A* d2a = brd->d2a;
        struct DMMAT_A2D* a2d = brd->a2d;
        unsigned char regval;

        spin_lock_irqsave(&brd->reglock,flags);

        d2d->stop(d2d);

        /*
         * Setup a gate for counter/timer 1/2. With this gate we can wait
         * until after the A2D and D2A are setup, and start them
         * simultaneously.
         * The gate for counter/timer 1/2 is EXTGATE/DIN 2, J3 pin 46.
         * If EXTGATE/DIN 2 is connected to DOUT 1, J3 pin 43, then we can control
         * the start of the D2A/A2D via software.
         * get value of register 10, set GT12EN bit */
        regval = inb(brd->addr + 10);
        regval |= 0x1;
        outb(regval, brd->addr + 10);
        spin_unlock_irqrestore(&brd->reglock,flags);

        /* start D2A waveform */
        if ((result = mutex_lock_interruptible(&d2a->waveform_mutex)))
                return result;
        result = startWaveforms(d2a);
        mutex_unlock(&d2a->waveform_mutex);
        if (result) return result;

        /*
         * Now that we know the waveform size, we can update the A2D scanrate.
         * and start the A2D
         */
        if ((result = mutex_lock_interruptible(&a2d->mutex)))
                return result;
        if (atomic_read(&a2d->running)) {
                KLOG_ERR("%s: already running",getA2DDeviceName(a2d));
                mutex_unlock(&a2d->mutex);
                return -EBUSY;
        }

        /* In waveform mode, the user requested scan rate is the
         * number of waveform/sec.  The actual rate is
         * waveform/sec * wavesize.
         */
        a2d->wavesize = d2a->wavesize;
        a2d->scanRate = a2d->scanRate * a2d->wavesize;
        a2d->scanDeltaT = TMSECS_PER_SEC / a2d->scanRate;

        result = startA2D(a2d);
        mutex_unlock(&a2d->mutex);
        if (result) return result;

        spin_lock_irqsave(&brd->reglock,flags);
        d2d->start(d2d);
        spin_unlock_irqrestore(&brd->reglock,flags);
        KLOG_DEBUG("D2A waveform generator has started output\n");
        return 0;
}

static void stopD2D_MM16AT(struct DMMAT_D2D* d2d)
{
}

static void stopD2D_MM32XAT(struct DMMAT_D2D* d2d)
{
        struct DMMAT* brd = d2d->brd;
        /* Set DOUT1 low */
        outb(0x0, brd->addr + 1);
}

/*
 * Stop the D2D.
 */
static int stopD2D(struct DMMAT_D2D* d2d)
{
        int result;
        struct DMMAT* brd = d2d->brd;
        struct DMMAT_D2A* d2a = brd->d2a;
        struct DMMAT_A2D* a2d = brd->a2d;
        unsigned long flags;
        unsigned char regval;

        spin_lock_irqsave(&brd->reglock,flags);
        d2d->stop(d2d);

        /* turn off gate-ing of counter 1&2 */
        regval = inb(brd->addr + 10);
        regval &= ~0x1;
        outb(regval, brd->addr + 10);

        spin_unlock_irqrestore(&brd->reglock,flags);

        /* stop D2A waveform */
        if ((result = mutex_lock_interruptible(&d2a->waveform_mutex)))
                return result;
        stopWaveforms(d2a);
        mutex_unlock(&d2a->waveform_mutex);

        /* stop A2D */
        if ((result = mutex_lock_interruptible(&a2d->mutex)))
                return result;
        stopA2D(a2d);
        mutex_unlock(&a2d->mutex);

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
        if (ia2d != DMMAT_DEVICES_A2D_MINOR) return -ENXIO;

        brd = board + ibrd;
        a2d = brd->a2d;
        a2d->mode = A2D_NORMAL;

        filp->private_data = a2d;

        atomic_inc(&a2d->num_opened);
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
        if (ia2d != DMMAT_DEVICES_A2D_MINOR) return -ENXIO;

        brd = board + ibrd;
        BUG_ON(a2d != brd->a2d);

        if ((result = mutex_lock_interruptible(&a2d->mutex)))
                return result;

        /* decrements and tests. If value is 0, returns true. */
        if (atomic_dec_and_test(&a2d->num_opened)) {
                stopA2D(a2d);
        }
        mutex_unlock(&a2d->mutex);
        KLOG_DEBUG("release_a2d, num_opened=%d\n",
            atomic_read(&a2d->num_opened));

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

        if (sample_remains(&a2d->read_state) ||
                GET_TAIL(a2d->samples,a2d->samples.size))
                mask |= POLLIN | POLLRDNORM;    /* readable */

        // if (mask) KLOG_DEBUG("mask=%x\n",mask);
        return mask;
}

static ssize_t dmmat_read_a2d(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) filp->private_data;
        return nidas_circbuf_read(filp,buf,count,&a2d->samples,&a2d->read_state,
            &a2d->read_queue);
}

static int dmmat_ioctl_a2d(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
        struct DMMAT_A2D* a2d = (struct DMMAT_A2D*) filp->private_data;
        struct DMMAT* brd;
        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int ia2d = i % DMMAT_DEVICES_PER_BOARD;
        int result = -EINVAL,err = 0;
        void __user *userptr = (void __user *) arg;
        int len;
        
        if (ibrd >= numboards) return -ENXIO;
        if (ia2d != DMMAT_DEVICES_A2D_MINOR) return -ENXIO;

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
                        u32 nchan = a2d->getNumA2DChannels(a2d);
                        if (copy_to_user(userptr,&nchan,sizeof(nchan)))
                                return -EFAULT;
                        result = 0;
                        break;
                }
        case NIDAS_A2D_SET_CONFIG:      /* user set */
                if ((result = mutex_lock_interruptible(&a2d->mutex)))
                        return result;
                if (atomic_read(&a2d->running)) {
                        KLOG_ERR("%s: already running",getA2DDeviceName(a2d));
                        result = -EBUSY;
                }
                else {
                        struct nidas_a2d_config cfg;
                        if (copy_from_user(&cfg,userptr,
                                sizeof(struct nidas_a2d_config))) result = -EFAULT;
                        else result = configA2D(a2d,&cfg);
                }
                mutex_unlock(&a2d->mutex);
                break;
        case NIDAS_A2D_CONFIG_SAMPLE:      /* user set */
                if (MAX_A2D_CHANNELS < MAX_DMMAT_A2D_CHANNELS) {
                        KLOG_ERR("programming error: MAX_A2D_CHANNELS=%d should be at least %d\n",
                                        MAX_A2D_CHANNELS,MAX_DMMAT_A2D_CHANNELS);
                        return -EINVAL;
                }
                if ((result = mutex_lock_interruptible(&a2d->mutex)))
                        return result;
                if (atomic_read(&a2d->running)) {
                        KLOG_ERR("%s: already running",getA2DDeviceName(a2d));
                        result = -EBUSY;
                }
                else {
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
                                result = -EFAULT;
                        else {
                                // kmalloc enough structure for additional filter data
                                len = sizeof(struct nidas_a2d_sample_config) +
                                        cfg.nFilterData;
                                cfgp = kmalloc(len,GFP_KERNEL);
                                if (!cfgp) {
                                        result = -ENOMEM;
                                }
                                else if (copy_from_user(cfgp, userptr, len) != 0) {
                                        kfree(cfgp);
                                        result = -EFAULT;
                                }
                                else result = addA2DSampleConfig(a2d,cfgp);
                                kfree(cfgp);
                        }
                }
                mutex_unlock(&a2d->mutex);
                break;
        case DMMAT_A2D_GET_STATUS:	/* user get of status struct */
                if (copy_to_user(userptr,&a2d->status,
                                        sizeof(struct DMMAT_A2D_Status))) return -EFAULT;
                result = 0;
                break;
        case DMMAT_START:
                if ((result = mutex_lock_interruptible(&a2d->mutex)))
                        return result;
                if (atomic_read(&a2d->running)) {
                        KLOG_ERR("%s: already running",getA2DDeviceName(a2d));
                        result = -EBUSY;
                }
                else {
                        result = startA2D(a2d);
                }
                mutex_unlock(&a2d->mutex);
                break;
        case DMMAT_STOP:
                if ((result = mutex_lock_interruptible(&a2d->mutex)))
                        return result;
                stopA2D(a2d);
                mutex_unlock(&a2d->mutex);
                break;
        case DMMAT_A2D_DO_AUTOCAL:
                if (types[brd->num] != DMM32XAT_BOARD && types[brd->num] != DMM32DXAT_BOARD) {
                        KLOG_ERR("board %d is not a DMM32AT/DMM32DXAT and does not support auto-calibration\n",brd->num);
                        result = -EINVAL;
                        break;
                }
                if ((result = mutex_lock_interruptible(&a2d->mutex)))
                        return result;
                {
                        int a2dRunning = atomic_read(&a2d->running);
                        stopA2D(a2d);
                        result = startAutoCal_MM32XAT(a2d);
                        if (a2dRunning) startA2D(a2d);
                }
                mutex_unlock(&a2d->mutex);
                break;
        default:
                result = -ENOTTY;
                break;
        }
        return result;
}
/************ Pulse Counter File Operations ****************/
static int dmmat_open_cntr(struct inode *inode, struct file *filp)
{
        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int icntr = i % DMMAT_DEVICES_PER_BOARD;

        struct DMMAT* brd;
        struct DMMAT_CNTR* cntr;

        KLOG_DEBUG("open_cntr, i=%d,ibrd=%d,icntr=%d,numboards=%d\n",
            i,ibrd,icntr,numboards);

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        if (ibrd >= numboards) return -ENXIO;
        if (icntr != DMMAT_DEVICES_CNTR_MINOR) return -ENXIO;

        brd = board + ibrd;
        cntr = brd->cntr;

        memset(&cntr->read_state,0,
                        sizeof(struct sample_read_state));

        atomic_inc(&cntr->num_opened);

        filp->private_data = cntr;

        return 0;
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
        if (icntr != DMMAT_DEVICES_CNTR_MINOR) return -ENXIO;

        brd = board + ibrd;
        BUG_ON(cntr != brd->cntr);

        if ((result = mutex_lock_interruptible(&cntr->mutex)))
                return result;
        /* decrements and tests. If value is 0, returns true. */
        if (atomic_dec_and_test(&cntr->num_opened)) {
                stopCNTR(cntr);
        }
        mutex_unlock(&cntr->mutex);

        return 0;
}

static ssize_t dmmat_read_cntr(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct DMMAT_CNTR* cntr = (struct DMMAT_CNTR*) filp->private_data;

        return nidas_circbuf_read(filp,buf,count,&cntr->samples,&cntr->read_state,
            &cntr->read_queue);
}

static int dmmat_ioctl_cntr(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
        struct DMMAT_CNTR* cntr = (struct DMMAT_CNTR*) filp->private_data;
        struct DMMAT* brd;
        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int icntr = i % DMMAT_DEVICES_PER_BOARD;

        // int icntr = i % DMMAT_DEVICES_PER_BOARD;
        int result = -EINVAL,err = 0;
        void __user *userptr = (void __user *) arg;

        if (ibrd >= numboards) return -ENXIO;
        if (icntr != DMMAT_DEVICES_CNTR_MINOR) return -ENXIO;

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
                        if ((result = mutex_lock_interruptible(&cntr->mutex)))
                                return result;
                        if (!atomic_read(&cntr->running)) result = startCNTR(cntr,&cfg);
                        mutex_unlock(&cntr->mutex);
                }
                break;
        case DMMAT_CNTR_STOP:      /* user set */
                if ((result = mutex_lock_interruptible(&cntr->mutex)))
                        return result;
                stopCNTR(cntr);
                mutex_unlock(&cntr->mutex);
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

        if (sample_remains(&cntr->read_state) ||
                GET_TAIL(cntr->samples,cntr->samples.size))
                mask |= POLLIN | POLLRDNORM;    /* readable */

        if (mask) KLOG_DEBUG("mask=%x\n",mask);
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
        if (id2a != DMMAT_DEVICES_D2A_MINOR) return -ENXIO;

        brd = board + ibrd;
        d2a = brd->d2a;

        atomic_inc(&d2a->num_opened);

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
        int result;

        struct DMMAT* brd;
        // int result;

        if (ibrd >= numboards) return -ENXIO;
        if (id2a != DMMAT_DEVICES_D2A_MINOR) return -ENXIO;

        brd = board + ibrd;
        BUG_ON(d2a != brd->d2a);

        if ((result = mutex_lock_interruptible(&d2a->waveform_mutex)))
                return result;
        /* decrements and tests. If value is 0, returns true. */
        if (atomic_dec_and_test(&d2a->num_opened)) {
                stopWaveforms(d2a);
        }
        mutex_unlock(&d2a->waveform_mutex);

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
        unsigned long len;
        void __user *userptr = (void __user *) arg;

        KLOG_ERR("%s: ibrd=%d\n",d2a->deviceName,ibrd);

        if (ibrd >= numboards) return -ENXIO;

        if (id2a != DMMAT_DEVICES_D2A_MINOR) return -ENXIO;

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
        case DMMAT_D2A_SET_CONFIG:      /* user set of */
                {
                        /* D2D configuration, currently only D2A output waveform rate. */
                        struct D2A_Config cfg;
                        if (copy_from_user(&cfg,userptr,
                                                sizeof(struct D2A_Config))) return -EFAULT;
                        d2a->waveformRate = cfg.waveformRate;
                        result = 0;
                }
                break;
        case DMMAT_ADD_WAVEFORM:
                {
                        /*
                         * Copy initial portion structure without the wave values.
                         * Once we know how many wave values there are, allocate a
                         * struct with room for the wave values.
                         */
                        struct D2A_Waveform wave;
                        struct D2A_Waveform* wavep;
                        len = _IOC_SIZE(cmd);

                        if (atomic_read(&d2a->waveform_running) == 1){
                                KLOG_ERR("%s: D2A already running\n",d2a->deviceName);
                                return -EINPROGRESS;
                        }

                        if (copy_from_user(&wave,userptr,len) != 0)
                                return -EFAULT;

                        // kmalloc full waveform structure
                        len = sizeof(struct D2A_Waveform) + wave.size * sizeof(int);
                        wavep = kmalloc(len,GFP_KERNEL);
                        if (!wavep) {
                                result = -ENOMEM;
                                KLOG_ERR("%s: D2A_Waveform struct could not be allocated.\n",d2a->deviceName);
                                break;
                        }
                        if (copy_from_user(wavep, userptr, len) != 0) {
                                kfree(wavep);
                                result = -EFAULT;
                                break;
                        }
                        if ((result = mutex_lock_interruptible(&d2a->waveform_mutex)))
                                return result;
                        result = d2a->addWaveform(d2a,wavep);
                        mutex_unlock(&d2a->waveform_mutex);
                }
                break;
        case DMMAT_START:
                if ((result = mutex_lock_interruptible(&d2a->waveform_mutex)))
                        return result;
                result = startWaveforms(d2a);
                mutex_unlock(&d2a->waveform_mutex);
                break;
        case DMMAT_STOP:
                if ((result = mutex_lock_interruptible(&d2a->waveform_mutex)))
                        return result;
                stopWaveforms(d2a);
                mutex_unlock(&d2a->waveform_mutex);
                break;
        default:
                result = -ENOTTY;
                break;
        }
        return result;
}

/*********** D2D File Operations *****************/
/* 
 * This device serves a dual role, generating one or more
 * waveforms on a D2A, while simultaneously scanning
 * one or more A2D channels, synchronized with the
 * generation of each waveform point.
 */

static int dmmat_open_d2d(struct inode *inode, struct file *filp)
{
        
        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int id2d = i % DMMAT_DEVICES_PER_BOARD;

        struct DMMAT* brd;
        struct DMMAT_D2D* d2d;
        struct DMMAT_D2A* d2a;
        struct DMMAT_A2D* a2d;

        KLOG_DEBUG("open_d2d, i=%d,ibrd=%d,id2d=%d,numboards=%d\n",
            i,ibrd,id2d,numboards);

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        if (ibrd >= numboards) return -ENXIO;
        // minor number of D2D devices is (numboard*DMMAT_DEVICES_PER_BOARD)+
        //                                 DMMAT_DEVICES_D2D_MINOR
        if (id2d != DMMAT_DEVICES_D2D_MINOR) return -ENXIO;   

        brd = board + ibrd;
        d2d = brd->d2d;
        d2a = brd->d2a;
        a2d = brd->a2d;

        a2d->mode = A2D_WAVEFORM;
        memset(&a2d->status,0,sizeof(a2d->status));

        atomic_inc(&d2d->num_opened);
        atomic_inc(&d2a->num_opened);

        filp->private_data = d2d;

        return 0;
}

static int dmmat_release_d2d(struct inode *inode, struct file *filp)
{
        struct DMMAT_D2D* d2d = (struct DMMAT_D2D*) filp->private_data;
        struct DMMAT* brd;
        struct DMMAT_A2D* a2d;
        int result;

        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int id2d = i % DMMAT_DEVICES_PER_BOARD;

        if (ibrd >= numboards) return -ENXIO;
        if (id2d != DMMAT_DEVICES_D2D_MINOR) return -ENXIO;

        brd = board + ibrd;
        BUG_ON(d2d != brd->d2d);
        a2d = brd->a2d;

        if ((result = mutex_lock_interruptible(&d2d->mutex)))
                return result;

        /* decrements and tests. If value is 0, returns true. */
        if (atomic_dec_and_test(&d2d->num_opened))
                stopD2D(d2d);
        mutex_unlock(&d2d->mutex);
        return 0;
}

/*
 * Implementation of poll fops.
 */
static unsigned int dmmat_poll_d2d(struct file *filp, poll_table *wait)
{
        struct DMMAT_D2A* d2d = (struct DMMAT_D2A*) filp->private_data;
        struct DMMAT* brd = d2d->brd;
        struct DMMAT_A2D* a2d = brd->a2d;

        unsigned int mask = 0;
        poll_wait(filp, &a2d->read_queue, wait);

        if (sample_remains(&a2d->read_state) ||
                GET_TAIL(a2d->samples,a2d->samples.size))
                mask |= POLLIN | POLLRDNORM;    /* readable */

        // if (mask) KLOG_DEBUG("mask=%x\n",mask);
        return mask;
}

static ssize_t dmmat_read_d2d(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct DMMAT_D2A* d2d = (struct DMMAT_D2A*) filp->private_data;
        struct DMMAT* brd = d2d->brd;
        struct DMMAT_A2D* a2d = brd->a2d;
        return nidas_circbuf_read(filp,buf,count,&a2d->samples,&a2d->read_state,
            &a2d->read_queue);
}

static int dmmat_ioctl_d2d(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
        struct DMMAT_D2D* d2d = (struct DMMAT_D2D*) filp->private_data;
        struct DMMAT* brd = d2d->brd;
        struct DMMAT_D2A* d2a = brd->d2a;
        struct DMMAT_A2D* a2d = brd->a2d;

        int i = iminor(inode);
        int ibrd = i / DMMAT_DEVICES_PER_BOARD;
        int id2d = i % DMMAT_DEVICES_PER_BOARD;
        int result = -EINVAL,err = 0;
        void __user *userptr = (void __user *) arg;
        unsigned long len;

        if (ibrd >= numboards) return -ENXIO;
        if (id2d != DMMAT_DEVICES_D2D_MINOR) return -ENXIO;

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
                err = !access_ok(VERIFY_WRITE, userptr,
                                _IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                err =  !access_ok(VERIFY_READ, userptr,
                                _IOC_SIZE(cmd));
        if (err) return -EFAULT;

        brd = board + ibrd;

        BUG_ON(d2d != brd->d2d);

        switch (cmd) 
        {
        case NIDAS_A2D_GET_NCHAN:
                {
                        u32 nchan = a2d->getNumA2DChannels(a2d);
                        if (copy_to_user(userptr,&nchan,sizeof(nchan)))
                                return -EFAULT;
                        result = 0;
                        break;
                }
        case NIDAS_A2D_SET_CONFIG:      /* user set */
                if ((result = mutex_lock_interruptible(&a2d->mutex)))
                        return result;
                if (atomic_read(&a2d->running)) {
                        KLOG_ERR("%s: already running",getA2DDeviceName(a2d));
                        result = -EBUSY;
                }
                else {
                        struct nidas_a2d_config cfg;
                        if (copy_from_user(&cfg,userptr,
                                sizeof(struct nidas_a2d_config))) result = -EFAULT;
                        else result = configA2D(a2d,&cfg);
                }
                mutex_unlock(&a2d->mutex);
                break;
        case NIDAS_A2D_CONFIG_SAMPLE:      /* user set */
                {
                        struct nidas_a2d_sample_config cfg;
                        if (MAX_A2D_CHANNELS < MAX_DMMAT_A2D_CHANNELS) {
                                KLOG_ERR("programming error: MAX_A2D_CHANNELS=%d should be at least %d\n",
                                                MAX_A2D_CHANNELS,MAX_DMMAT_A2D_CHANNELS);
                                return -EINVAL;
                        }
                        /*
                         * There is no filtering of D2D data,
                         * so we don't need to copy the filter information from this structure.
                         */
                        len = _IOC_SIZE(cmd);
                        if (copy_from_user(&cfg,userptr,len) != 0)
                                return -EFAULT;
                        result = addA2DSampleConfig(a2d,&cfg);
                }
                break;
        case DMMAT_A2D_GET_STATUS:	/* user get of status struct */
                if (copy_to_user(userptr,&a2d->status,
                                        sizeof(struct DMMAT_A2D_Status))) return -EFAULT;
                result = 0;
                break;
        case DMMAT_D2A_SET_CONFIG:      /* user set of */
                {
                        /* D2D configuration, currently only D2A output waveform rate. */
                        struct D2A_Config cfg;
                        if (copy_from_user(&cfg,userptr,
                                                sizeof(struct D2A_Config))) return -EFAULT;
                        d2a->waveformRate = cfg.waveformRate;
                        result = 0;
                }
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
        case DMMAT_ADD_WAVEFORM:
                {
                        /*
                         * Copy initial portion structure without the wave values.
                         * Once we know how many wave values there are, allocate a
                         * struct with room for the wave values.
                         */
                        struct D2A_Waveform wave;
                        struct D2A_Waveform* wavep;
                        len = _IOC_SIZE(cmd);

                        if (atomic_read(&d2a->waveform_running) == 1){
                                KLOG_ERR("D2A waveform already running");
                                return -EINPROGRESS;
                        }

                        if (copy_from_user(&wave,userptr,len) != 0)
                                return -EFAULT;

                        // kmalloc full waveform structure
                        len = sizeof(struct D2A_Waveform) + wave.size * sizeof(int);
                        wavep = kmalloc(len,GFP_KERNEL);
                        if (!wavep) {
                                result = -ENOMEM;
                                KLOG_ERR("Larger waveform struct could not be "
                                                "allocated.\n");
                                break;
                        }
                        if (copy_from_user(wavep, userptr, len) != 0) {
                                kfree(wavep);
                                result = -EFAULT;
                                break;
                        }
                        if ((result = mutex_lock_interruptible(&d2a->waveform_mutex)))
                                return result;
                        result = d2a->addWaveform(d2a, wavep);
                        mutex_unlock(&d2a->waveform_mutex);
                }
                break;

        case DMMAT_START:
                {
                        if ((result = mutex_lock_interruptible(&d2d->mutex)))
                                return result;
                        result = startD2D(d2d);
                        mutex_unlock(&d2d->mutex);
                }
                break;
        case DMMAT_STOP:
                {
                        if ((result = mutex_lock_interruptible(&d2d->mutex)))
                                return result;
                        result = stopD2D(d2d);
                        mutex_unlock(&d2d->mutex);
                        break;
                }
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

static struct file_operations d2d_fops = {
        .owner   = THIS_MODULE,
        .read    = dmmat_read_d2d,
        .poll    = dmmat_poll_d2d,
        .open    = dmmat_open_d2d,
        .ioctl   = dmmat_ioctl_d2d,
        .release = dmmat_release_d2d,
        .llseek  = no_llseek,
};

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
        sprintf(a2d->deviceName[A2D_NORMAL],"/dev/dmmat_a2d%d",brd->num);
        sprintf(a2d->deviceName[A2D_WAVEFORM],"/dev/dmmat_d2d%d",brd->num);

        // a2d device
        cdev_init(&a2d->cdev,&a2d_fops);
        a2d->cdev.owner = THIS_MODULE;
        devno = MKDEV(MAJOR(dmmat_device),brd->num*DMMAT_DEVICES_PER_BOARD
                                               + DMMAT_DEVICES_A2D_MINOR);
        KLOG_DEBUG("%s: MKDEV, major=%d minor=%d\n",
                getA2DDeviceName(a2d),MAJOR(devno),MINOR(devno));

        mutex_init(&a2d->mutex);
        atomic_set(&a2d->num_opened,0);
        atomic_set(&a2d->running,0);

        switch (type) {
        case DMM16AT_BOARD:
                /* board registers */
                brd->itr_status_reg = brd->addr + 8;
                brd->ad_itr_mask = 0x10;

                brd->itr_ack_reg = brd->addr + 8;
                brd->itr_ack_val = 0x00;        // any value

                a2d->start = startA2D_MM16AT;
                a2d->stop = stopA2D_MM16AT;
                a2d->getNumA2DChannels = getNumA2DChannelsMM16AT;
                a2d->selectA2DChannels = selectA2DChannelsMM16AT;
                a2d->getConvRateSetting = getConvRateMM16AT;
                a2d->getGainSetting = getGainSettingMM16AT;
                a2d->getA2DThreshold = getA2DThreshold_MM16AT;
                a2d->getFifoLevel = getFifoLevelMM16AT;
                a2d->resetFifo = resetFifoMM16AT;
                a2d->waitForA2DSettle = waitForA2DSettleMM16AT;
                a2d->maxFifoThreshold = 256;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
                INIT_WORK(&a2d->worker,dmmat_a2d_bottom_half);
                INIT_WORK(&a2d->waveform_worker,dmmat_a2d_waveform_bh);
#else
                INIT_WORK(&a2d->worker,dmmat_a2d_bottom_half,&a2d->worker);
                INIT_WORK(&a2d->waveform_worker,dmmat_a2d_waveform_bh,&a2d->waveform_worker);
#endif
            break;
        case DMM32XAT_BOARD:
        case DMM32DXAT_BOARD:
                brd->itr_status_reg = brd->addr + 9;
                brd->ad_itr_mask = 0x80;

                brd->itr_ack_reg = brd->addr + 8;
                brd->itr_ack_val = 0x08;

                a2d->start = startA2D_MM32XAT;
                a2d->stop = stopA2D_MM32XAT;
                a2d->getNumA2DChannels = getNumA2DChannelsMM32XAT;
                a2d->selectA2DChannels = selectA2DChannelsMM32XAT;
                a2d->getConvRateSetting = getConvRateMM32XAT;
                a2d->getGainSetting = getGainSettingMM32XAT;
                a2d->getA2DThreshold = getA2DThreshold_MM32XAT;
                a2d->getFifoLevel = getFifoLevelMM32XAT;
                a2d->resetFifo = resetFifoMM32XAT;
                a2d->waitForA2DSettle = waitForA2DSettleMM32XAT;
                /* fifo on DMM32 is 1024 samples. For good data recovery
                 * we want an interrupt when it is about 1/2 full */
                a2d->maxFifoThreshold = 512;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
                INIT_WORK(&a2d->worker,dmmat_a2d_bottom_half_fast);
                INIT_WORK(&a2d->waveform_worker,dmmat_a2d_waveform_bh);
#else
                INIT_WORK(&a2d->worker,dmmat_a2d_bottom_half_fast,&a2d->worker);
                INIT_WORK(&a2d->waveform_worker,dmmat_a2d_waveform_bh,&a2d->waveform_worker);
#endif
                // full reset
                outb(0x20, brd->addr + 8);

                // enable enhanced features on this board
                outb(0x03,brd->addr + 8);	// set page 3
                outb(0xa6,brd->addr + 15);	// enable enhanced features
                outb(0x00, brd->addr + 8);      // back to page 0
                break;
        }

        a2d->gainConvSetting = 0;	// default value

        init_waitqueue_head(&a2d->read_queue);

        /* After calling cdev_add the device is "live"
         * and ready for user operation.
         */
        result = cdev_add(&a2d->cdev, devno, 1);
        return result;
}

/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
static void cleanup_a2d(struct DMMAT* brd)
{
        struct DMMAT_A2D* a2d = brd->a2d;
        int i;

        if (!a2d) return;

        cdev_del(&a2d->cdev);

        free_dsm_disc_circ_buf(&a2d->samples);

        free_dsm_disc_circ_buf(&a2d->fifo_samples);

        if (a2d->filters) {
                for (i = 0; i < a2d->nfilters; i++) {
                        if (a2d->filters[i].channels)
                                kfree(a2d->filters[i].channels);
                        a2d->filters[i].channels = 0;
                }
                kfree(a2d->filters);
                a2d->filters = 0;
        }

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

        unsigned long flags;
        unsigned long jnext = jiffies;
        unsigned int cval;
        unsigned int total;

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
        if (!atomic_read(&cntr->shutdownTimer)) 
            mod_timer(&cntr->timer,jnext);  // re-schedule
}

static int __init init_cntr(struct DMMAT* brd,int type)
{
        int result = -ENOMEM;
        struct DMMAT_CNTR* cntr;
        dev_t devno;

        brd->cntr = cntr = kmalloc(sizeof(struct DMMAT_CNTR),GFP_KERNEL);
        if (!cntr) return result;
        memset(cntr,0, sizeof(struct DMMAT_CNTR));

        cntr->brd = brd;

        atomic_set(&cntr->shutdownTimer,0);
        atomic_set(&cntr->running,0);
        atomic_set(&cntr->num_opened,0);

        mutex_init(&cntr->mutex);

        // for informational messages only at this point
        sprintf(cntr->deviceName,"/dev/dmmat_cntr%d",brd->num);

        // cntr device
        cdev_init(&cntr->cdev,&cntr_fops);
        cntr->cdev.owner = THIS_MODULE;

        devno = MKDEV(MAJOR(dmmat_device),brd->num*DMMAT_DEVICES_PER_BOARD
                                               + DMMAT_DEVICES_CNTR_MINOR);
        KLOG_DEBUG("%s: MKDEV, major=%d minor=%d\n",
                cntr->deviceName,MAJOR(devno),MINOR(devno));

        switch (type) {
        case DMM16AT_BOARD:
                brd->cntr_itr_mask = 0x40;
                cntr->start = startCntr_MM16AT;
                cntr->stop = stopCntr_MM16AT;
                break;
        case DMM32XAT_BOARD:
        case DMM32DXAT_BOARD:
                brd->cntr_itr_mask = 0x20;
                cntr->start = startCntr_MM32AT;
                cntr->stop = stopCntr_MM32AT;
                break;
        }
            
        /*
         * Allocate counter samples in circular buffer
         */
        result = alloc_dsm_circ_buf(&cntr->samples,
            sizeof(int),DMMAT_CNTR_QUEUE_SIZE);
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

/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
static void cleanup_cntr(struct DMMAT* brd)
{
        struct DMMAT_CNTR* cntr = brd->cntr;
        if (!cntr) return;
        cdev_del(&cntr->cdev);

        free_dsm_circ_buf(&cntr->samples);

        kfree(cntr);
        brd->cntr = 0;
}

static int __init init_d2a(struct DMMAT* brd,int type)
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

        mutex_init(&d2a->waveform_mutex);
        atomic_set(&d2a->waveform_running, 0);
        atomic_set(&d2a->num_opened,0);

        devno = MKDEV(MAJOR(dmmat_device),brd->num*DMMAT_DEVICES_PER_BOARD
                                              + DMMAT_DEVICES_D2A_MINOR);
        KLOG_DEBUG("%s: MKDEV, major=%d minor=%d\n",
                d2a->deviceName,MAJOR(devno),MINOR(devno));

        // calculate conversion relation based on presumed
        // correct value for d2aconfig runstring parameter
        d2a->cmin = 0;
        d2a->cmax = 4095;

        switch (type) {
        case DMM16AT_BOARD:
                d2a->setD2A = setD2A_MM16AT;
                d2a->addWaveform = addWaveform_MM16AT;
                d2a->loadWaveforms = loadWaveforms_MM16AT;
                d2a->startWaveforms = startWaveforms_MM16AT;
                d2a->stopWaveforms = stopWaveforms_MM16AT;
                if (d2aconfig[brd->num] == DMMAT_D2A_UNI_10 ||
                        d2aconfig[brd->num] == DMMAT_D2A_BI_10) {
                        KLOG_ERR("%s: is a DMM16AT and does not support 10 V D2A\n",
                                d2a->deviceName);
                        return -EINVAL;
                }
                break;
        case DMM32XAT_BOARD:
                d2a->setD2A = setD2A_MM32AT;
                d2a->addWaveform = addWaveform_MM32XAT;
                d2a->loadWaveforms = loadWaveforms_MM32XAT;
                d2a->startWaveforms = startWaveforms_MM32XAT;
                d2a->stopWaveforms = stopWaveforms_MM32XAT;
                break;
        case DMM32DXAT_BOARD:
                d2a->setD2A = setD2A_MM32DXAT;
                d2a->addWaveform = addWaveform_MM32XAT;
                d2a->loadWaveforms = loadWaveforms_MM32XAT;
                d2a->startWaveforms = startWaveforms_MM32XAT;
                d2a->stopWaveforms = stopWaveforms_MM32XAT;
                d2a->cmax = 65535;
                break;
        }
            
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


/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
static void cleanup_d2a(struct DMMAT* brd)
{
        struct DMMAT_D2A* d2a = brd->d2a;
        if (!d2a) return;

        cdev_del(&d2a->cdev);

        kfree(d2a);
        brd->d2a = 0;
}

static int __init init_d2d(struct DMMAT* brd, int type)
{
        int result = -ENOMEM;
        struct DMMAT_D2D* d2d;
        dev_t devno;

        brd->d2d = d2d = kmalloc(sizeof(struct DMMAT_D2D),GFP_KERNEL);
        if (!d2d) return result;
        memset(d2d,0, sizeof(struct DMMAT_D2D));

        d2d->brd = brd;

        // for informational messages only at this point
        sprintf(d2d->deviceName,"/dev/dmmat_d2d%d",brd->num);

        mutex_init(&d2d->mutex);
        atomic_set(&d2d->num_opened, 0);

        // d2d device
        cdev_init(&d2d->cdev,&d2d_fops);
        d2d->cdev.owner = THIS_MODULE;

        devno = MKDEV(MAJOR(dmmat_device),brd->num*DMMAT_DEVICES_PER_BOARD
                        + DMMAT_DEVICES_D2D_MINOR);
        KLOG_DEBUG("%s: MKDEV, major=%d minor=%d\n",
                        d2d->deviceName,MAJOR(devno),MINOR(devno));

        switch (type){
        case DMM16AT_BOARD:
                d2d->start = startD2D_MM16AT;
                d2d->stop = stopD2D_MM16AT;
                break;
        case DMM32XAT_BOARD:
        case DMM32DXAT_BOARD:
                d2d->start = startD2D_MM32XAT;
                d2d->stop = stopD2D_MM32XAT;
                break;
        }

        /* After calling cdev_all the device is "live"
         * and ready for user operation.
         */
        result = cdev_add (&d2d->cdev, devno,1);

        return result;
}

static void cleanup_d2d(struct DMMAT* brd)
{
        struct DMMAT_D2D* d2d = brd->d2d;
        if (!d2d) return;

        cdev_del(&d2d->cdev);

        kfree(d2d);
        brd->d2d = 0;
}

/*-----------------------Module ------------------------------*/

/* Don't add __exit macro to the declaration of dmd_mmat_cleanup,
 * since it is also called at init time, if init fails. */
static void dmd_mmat_cleanup(void)
{

    int ib;

    if (board) {

        for (ib = 0; ib < numboards; ib++) {
            struct DMMAT* brd = board + ib;

            cleanup_a2d(brd);
            cleanup_cntr(brd);
            cleanup_d2a(brd);
            cleanup_d2d(brd);

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

        work_queue = create_singlethread_workqueue("dmd_mmat");

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
         *  (0,5,10,...)     A2D
         *  (1,6,11,...)    Pulse counter
         *  (2,7,12,...)     Analog out
         *  (3,8,13,...)    Digital out
         *  (4,9,14,...)    D2D
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
                unsigned long addr =  (unsigned long)ioports[ib] + SYSTEM_ISA_IOPORT_BASE;
                KLOG_DEBUG("isa base=%x\n",SYSTEM_ISA_IOPORT_BASE);

                brd->num = ib;
                spin_lock_init(&brd->reglock);
                mutex_init(&brd->irqreq_mutex);
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
                    KLOG_ERR("missing irq value for board #%d at addr 0x%x\n",
                        ib,ioports[ib]);
                    goto err;
                }

                /* counter 1&2 */
                switch (types[ib]) {
                case DMM16AT_BOARD:
                        brd->setClock1InputRate = setClock1InputRate_MM16AT;
                        break;
                case DMM32XAT_BOARD:
                case DMM32DXAT_BOARD:
                        brd->setClock1InputRate = setClock1InputRate_MM32AT;
                        break;
                }

                spin_lock_init(&brd->clock12.lock);
                atomic_set(&brd->clock12.userCount,0);

                // setup A2D
                result = init_a2d(brd,types[ib]);
                if (result) goto err;
                brd->a2d->stop(brd->a2d);

                // setup CNTR
                result = init_cntr(brd,types[ib]);
                if (result) goto err;
                brd->cntr->stop(brd->cntr);

                // setup D2A
                result = init_d2a(brd,types[ib]);
                if (result) goto err;

                // setup D2D
                result = init_d2d(brd, types[ib]);
                if (result) goto err;
                brd->d2d->stop(brd->d2d);
        }

        KLOG_DEBUG("complete.\n");

        return 0;
err:
        dmd_mmat_cleanup();
        return result;
}

module_init(dmd_mmat_init);
module_exit(dmd_mmat_cleanup);
