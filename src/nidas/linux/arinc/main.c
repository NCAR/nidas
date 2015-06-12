/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*-
 * vim: set shiftwidth=8 softtabstop=8 expandtab: */
/* main.c

   Linux module for interfacing the ISA bus interfaced
   Condor Engineering's CEI-420A-42 ARINC card.

   Original Author: John Wasinger

   Copyright 2008 UCAR, NCAR, All Rights Reserved

   Implementation notes:

      I have tagged areas which need development with a 'TODO' comment.

   Acronyms:

      LPS - Labels  Per Second
      LPB - Labels  Per Buffer
      BPS - Buffers Per Second

   Revisions:

     $LastChangedRevision$
         $LastChangedDate$
           $LastChangedBy$
                 $HeadURL$
*/

// Linux module includes...
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>         // module_init, module_exit
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/slab.h>		// kmalloc, kfree
#include <linux/timer.h>
#include <linux/version.h>

#include <linux/fs.h>           // has to be before <linux/cdev.h>! GRRR! 
//#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <asm/atomic.h>
#include <asm/io.h>             // readb
#include <asm/uaccess.h>        // VERIFY_???

// DSM includes... 
#include "arinc.h"
#include "CEI420A/Include/utildefs.h"

#include <nidas/linux/types.h>
#include <nidas/linux/util.h>
#include <nidas/linux/isa_bus.h>
#include <nidas/linux/klog.h>
#include <nidas/linux/SvnInfo.h>    // SVNREVISION

/*
 * This driver schedules two functions to run periodically.
 *
 * One, arinc_sweep, reads the arinc data from the card at a sufficient
 * rate to keep up, usually something like 20 times/sec.
 *
 * The other, arinc_timesync, runs at 1 Hz, and updates the internal
 * millisecond ARINC clock. This clock value is attached to each ARINC data value.
 *
 * These timer functions can be configured to be called by the
 * IRIG driver, or as kernel timers.
 *
 * Define USE_IRIG_CALLBACK to schedule the functions as IRIG callbacks.
 * Otherwise kernel timers will be used, and an IRIG card and driver do not
 * need to be present.
 */
#define USE_IRIG_CALLBACK

#ifdef USE_IRIG_CALLBACK
#include <nidas/linux/irigclock.h>
#endif

#ifndef SVNREVISION
#define SVNREVISION "unknown"
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("CEI420a ISA driver for Linux");
MODULE_VERSION(SVNREVISION);

#define ARINC_SAMPLE_QUEUE_SIZE 8
#define BOARD_NUM   0


unsigned int iomem = 0xd0000;

/* module prameters (can be passed in via command line) */
module_param(iomem, uint, 0);
MODULE_PARM_DESC(iomem, "ISA memory base (default 0xd0000)");

/* global variables */

struct arinc_board
{
        /* Set the base address of the ARINC card */
        void* phys_membase;

        unsigned long basemem;

#ifdef USE_IRIG_CALLBACK
        enum irigClockRates sync_rate;
        struct irig_callback* timeSyncCallback;
#else
        int sync_jiffies;
        struct timer_list syncer;
#endif

        spinlock_t lock;

        atomic_t numRxChannelsOpen;

        int running;
};

static struct arinc_board board;

/**
 * Device structure used in the file operations of the character device
 * which provides ARINC samples.
 */
struct arinc_dev
{
        // setup info... 
        char deviceName[64];
        struct dsm_sample_circ_buf samples;     // samples for reading
        struct sample_read_state read_state;
        wait_queue_head_t rwaitq;    // wait queue for user reads
        unsigned int skippedSamples;
#ifdef USE_IRIG_CALLBACK
        enum irigClockRates poll;
        struct irig_callback *sweepCallback;
#else
        int sweep_jiffies;
        struct timer_list sweeper;
#endif
        unsigned int speed;
        unsigned int parity;
        int pollDtMsec;         // number of millisecs between polls

        // run-time info... 
        atomic_t used;
        int nSweeps;
	unsigned int lps_cnt_current;

        dsm_arinc_status status;

        unsigned char rate[0400];
        unsigned char arcfgs[0400];
        unsigned int nArcfg;
};
static struct arinc_dev *chn_info = 0;

/**
 * Info for ARINC user devices
 */
#define DEVNAME_ARINC "arinc"
static dev_t arinc_device = MKDEV(0, 0);
static struct cdev arinc_cdev;

// -- UTILITY --------------------------------------------------------- 
static void log_error(short board, short err)
{
        if (!err)
                return;

        // display the error message
        KLOG_ERR("\nError while on board %d\n", board);
        KLOG_ERR("  Error reported:  \'%s\'\n", ar_get_error(err));
        KLOG_ERR("  Additional info: \'%s\'\n", ar_get_error(ARS_LAST_ERROR));
}

// -- UTILITY --------------------------------------------------------- 
static short roundUpRate(short rate)
{
        if (rate == 3)
                return 4;
        if (rate == 6)
                return 7;
        if (rate == 12)
                return 13;
        return rate;
}

/* -- IRIG CALLBACK ---------------------------------------------------
   sync up the i960's internal clock to the current time
   This is called from software interrupt context.
   Be quick, no sleeping, use spinlocks instead of mutexes or semaphores.
*/
#ifdef USE_IRIG_CALLBACK
static void arinc_timesync(void *junk)
#else
static void arinc_timesync(unsigned long junk)
#endif
{
//      KLOG_INFO("%6d, %6d\n", GET_MSEC_CLOCK, ar_get_timercntl(BOARD_NUM));
#ifndef USE_IRIG_CALLBACK
        unsigned int msecs;
#endif
        spin_lock(&board.lock);

#ifdef USE_IRIG_CALLBACK
        ar_set_timercnt(BOARD_NUM, GET_MSEC_CLOCK);
#else
        msecs = getSystemTimeMsecs();
        ar_set_timercnt(BOARD_NUM,msecs);
#endif
        spin_unlock(&board.lock);

#ifndef USE_IRIG_CALLBACK
        /* schedule this function to run at the even second */
        msecs %= MSECS_PER_SEC; /* milliseconds after the second */
        board.syncer.expires = jiffies + board.sync_jiffies - msecs * HZ / MSECS_PER_SEC;
        add_timer(&board.syncer);
#endif
}
/* -- IRIG CALLBACK ---------------------------------------------------
   This is called from software interrupt context.
   Be quick, no sleeping, use spinlocks instead of mutexes or semaphores.
*/
#ifdef USE_IRIG_CALLBACK
static void arinc_sweep(void* arg)
#else
static void arinc_sweep(unsigned long arg)
#endif
{

#ifdef USE_IRIG_CALLBACK
        int chn = (long) arg;
#else
        int chn = arg - 1;
#endif
        short err;
        struct arinc_dev *dev = &chn_info[chn];
        int nData;
        struct dsm_sample *sample;
        tt_data_t *data;

        sample = GET_HEAD(dev->samples, ARINC_SAMPLE_QUEUE_SIZE);
        if (!sample) {           // no output sample available
                dev->skippedSamples++;
                KLOG_WARNING("%s: skippedSamples=%d\n",
                             dev->deviceName, dev->skippedSamples);
                return;
        }               

        data = (tt_data_t*) sample->data;
        KLOG_DEBUG("%d data:   %x\n", chn, (unsigned int) data);
        // Set the sample block's time tag to an estimate of
        // the timetag of the earliest data in the sweep.
        // We'll use the computed time of the previous sweep.
        //
        // Using the earliest sample time as the time tag
        // of the sweep improves the chances that samples
        // will get sorted correctly later with a minimum
        // of buffering.

#ifdef USE_IRIG_CALLBACK
        sample->timetag = GET_MSEC_CLOCK;
#else
        sample->timetag = getSystemTimeMsecs();
#endif
        sample->timetag -= dev->pollDtMsec;
        KLOG_DEBUG("%d sample->timetag: %d\n", chn, sample->timetag);

        // read ARINC channel until it's empty or our buffer is full 
        spin_lock(&board.lock);
        err = ar_getwordst(BOARD_NUM, chn, LPB, &nData, data);
        spin_unlock(&board.lock);
        KLOG_DEBUG("%d nData:           %d\n", chn, nData);

        // Can't sync to card.
        if (err == ARS_NOSYNC) {
                dev->status.nosync++;
                return;
        }
        // note possible buffer underflows 
        if (err == ARS_NODATA) {
                dev->status.underflow++;
		return;
        }

        // note the number of received labels per second 
        dev->lps_cnt_current += nData;
        if (++dev->nSweeps == dev->status.pollRate) {
                dev->status.lps_cnt = dev->lps_cnt_current;
                dev->nSweeps = dev->lps_cnt_current = 0;
        }
        // note possible buffer overflows 
        if (nData == LPB)
                dev->status.overflow++;

        sample->length = nData * sizeof(tt_data_t);
        KLOG_DEBUG("%d sample->length:  %d\n", chn, sample->length);
        INCREMENT_HEAD(dev->samples, ARINC_SAMPLE_QUEUE_SIZE);
        wake_up_interruptible(&dev->rwaitq);

#ifndef USE_IRIG_CALLBACK
        dev->sweeper.expires += dev->sweep_jiffies;
        add_timer(&dev->sweeper);
#endif
}

static int arinc_open(struct inode *inode, struct file *filp)
{
        struct arinc_dev *dev;
        int chn = iminor(inode);
        int txChn = chn - N_ARINC_RX;
        int err;

        if (chn >= N_ARINC_RX+N_ARINC_TX) return -ENXIO;

        if (chn >= N_ARINC_RX) {    
                /* transmit channel */
                KLOG_NOTICE("arinc_open setting up txChn:  %d\n", txChn);

                // set channel speed 
                err =
                    ar_set_config(BOARD_NUM,
                                  ARU_TX_CH01_BIT_RATE + txChn,
                                  AR_HIGH);
                if (err != ARS_NORMAL) {
                        log_error(BOARD_NUM, err);
                        return -EIO;
                }
                if (ar_get_config (BOARD_NUM,
                     ARU_TX_CH01_BIT_RATE + txChn) != AR_HIGH) {
                        KLOG_ERR("un-settable speed!\n");
                        return -EIO;
                }
                // set channel parity 
                err =
                    ar_set_config(BOARD_NUM,
                                  ARU_TX_CH01_PARITY + txChn,
                                  AR_ODD);
                if (err != ARS_NORMAL) {
                        log_error(BOARD_NUM, err);
                        return -EIO;
                }
                if (ar_get_config (BOARD_NUM,
                     ARU_TX_CH01_PARITY + txChn) != AR_ODD) {
                        KLOG_ERR("un-settable parity!\n");
                        return -EIO;
                }

                /* note that the ioctl, poll and read methods depend
                 * on private_data pointing to an arinc_dev structure.
                 * This structure has not been setup for transmit channels,
                 * and so ioctl or read should check for NULL
                 * private_data and return an error like -EBADF.
                 */
                filp->private_data = 0;

                /* Inform kernel that this device is not seekable */
                nonseekable_open(inode,filp);

                return 0;
        }

        /* receiver channel */
        dev = &chn_info[chn];

        // channel can only be opened once
        if (atomic_inc_return (&dev->used) > 1) {
                KLOG_ERR("chn: %d is already open!\n", chn);
                atomic_dec(&dev->used);
                return -EBUSY; /* already open */
        }
        BUG_ON(atomic_inc_return(&board.numRxChannelsOpen) > N_ARINC_RX);

        // un-filter all labels on this channel 
        spin_lock_bh(&board.lock);
        err =
            ar_label_filter(BOARD_NUM, chn, ARU_ALL_LABELS,
                            ARU_FILTER_OFF);
        if (err != ARS_NORMAL) {
                spin_unlock_bh(&board.lock);
                atomic_dec(&dev->used);
                if (err < 0) return err;
                log_error(BOARD_NUM, err);
                return -EIO;
        }
        spin_unlock_bh(&board.lock);

        // set up the circular buffer
        err = realloc_dsm_circ_buf(&dev->samples,
                                   sizeof(tt_data_t) * LPB,
                                   ARINC_SAMPLE_QUEUE_SIZE);
        if (err) {
                atomic_dec(&dev->used);
                return err;
        }
        KLOG_DEBUG("realloc_dsm_circ_buf(%x,%d,%d)\n", (unsigned int) &dev->samples,
                                   sizeof(tt_data_t) * LPB,
                                   ARINC_SAMPLE_QUEUE_SIZE);

        // reset stuff in dev struct.
        memset(&dev->read_state,0,
            sizeof(struct sample_read_state));
	memset(&dev->status,0,sizeof(dsm_arinc_status));
        memset(dev->rate,0,sizeof(dev->rate));
        EMPTY_CIRC_BUF(dev->samples);
        // don't think it is necessary to zero arcfgs
        // memset(dev->arcfgs,0,sizeof(dev->arcfgs));
        dev->nArcfg = 0;
        dev->nSweeps = 0;
        dev->skippedSamples = 0;
        dev->lps_cnt_current = 0;

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        filp->private_data = dev;
        return 0;
}

static long arinc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        struct arinc_dev *dev;
        int ret;
        void __user *userptr = (void __user *) arg;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
        int chn = iminor(file_inode(filp));
#else
        int chn = iminor(filp->f_dentry->d_inode);
#endif

        int err;
        int pollRate;

	// from linux/irigclock, the supported poll rates are
	//  1,2,4,5,10,20,25,50,100
	int pollRates[] = {2,4,10,20,50,100};	// we'll use these
	int nRates = (signed)(sizeof(pollRates)/sizeof(pollRates[0]));

        arcfg_t arcfg;
        archn_t archn;
        short aBIT;
	int i;

        dev = (struct arinc_dev *) filp->private_data;
        if (!dev) return -EBADF;

        // don't decode wrong cmds: better returning
        // ENOTTY than EFAULT
        if (chn >= N_ARINC_RX)               return -ENXIO;
        if (_IOC_TYPE(cmd) != ARINC_MAGIC)  return -ENOTTY;
        if (_IOC_NR(cmd) > ARINC_IOC_MAXNR) return -ENOTTY;

        // Verify read or write access to the user arg, if necessary
        if (_IOC_DIR(cmd) & _IOC_READ)
                ret = !access_ok(VERIFY_WRITE, userptr,_IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                ret =  !access_ok(VERIFY_READ, userptr, _IOC_SIZE(cmd));
        else ret = 0;
        if (ret) return -EFAULT;

        switch (cmd) {

        case ARINC_SET:

                // unfilter a label for this channel 
                if (copy_from_user(&arcfg, userptr, sizeof(arcfg_t))) {
                        KLOG_ERR("copy_from_user error!\n");
                        return -EFAULT;
                }

                // store the rate for this channel's label 
                if (dev->rate[arcfg.label]) {
                        KLOG_ERR("duplicate label: %04o\n", arcfg.label);
                        return -EINVAL;
                }
                dev->rate[arcfg.label] = arcfg.rate;

                // measure the total labels per second for the given channel 
                dev->status.lps += roundUpRate(arcfg.rate);

                spin_lock_bh(&board.lock);

                // stop the board 
                if (board.running) {
                        err = ar_reset(BOARD_NUM);
                        if (err != ARS_NORMAL)
                                goto ar_fail_unlock;
                        board.running = 0;
                }
                dev->arcfgs[dev->nArcfg++] = arcfg.label;

                // un-filter this label on this channel 
                err =
                    ar_label_filter(BOARD_NUM, chn, arcfg.label,
                                    ARU_FILTER_OFF);
                if (err != ARS_NORMAL)
                        goto ar_fail_unlock;

                spin_unlock_bh(&board.lock);

                KLOG_DEBUG("recv: %04o  rate: %2d Hz\n", arcfg.label, roundUpRate(arcfg.rate));
                break;

        case ARINC_OPEN:

                // store the speed and parity for this channel 
                if (copy_from_user(&archn, userptr, sizeof(archn))) {
                        KLOG_ERR("copy_from_user error!\n");
                        return -EFAULT;
                }
                if (archn.speed != AR_HIGH && archn.speed != AR_LOW) {
                        KLOG_ERR("invalid speed!\n");
                        return -EINVAL;
                }
                if (archn.parity != AR_ODD && archn.parity != AR_EVEN) {
                        KLOG_ERR("invalid parity!\n");
                        return -EINVAL;
                }
                dev->speed = archn.speed;
                dev->parity = archn.parity;

                spin_lock_bh(&board.lock);

                // stop the board 
                if (board.running) {
                        err = ar_reset(BOARD_NUM);
                        if (err != ARS_NORMAL) 
                                goto ar_fail_unlock;
                        board.running = 0;
                }
                // set channel speed 
                err =
                    ar_set_config(BOARD_NUM,
                                  ARU_RX_CH01_BIT_RATE + chn,
                                  dev->speed);
                if (err != ARS_NORMAL)
                        goto ar_fail_unlock;
                if (ar_get_config (BOARD_NUM,
                     ARU_RX_CH01_BIT_RATE + chn) != dev->speed) {
                        spin_unlock_bh(&board.lock);
                        KLOG_ERR("un-settable speed!\n");
                        return -EINVAL;
                }
                // set channel parity 
                err =
                    ar_set_config(BOARD_NUM,
                                  ARU_RX_CH01_PARITY + chn,
                                  dev->parity);
                if (err != ARS_NORMAL)
                        goto ar_fail_unlock;
                if (ar_get_config (BOARD_NUM,
                     ARU_RX_CH01_PARITY + chn) != dev->parity) {
                        spin_unlock_bh(&board.lock);
                        KLOG_ERR("un-settable parity!\n");
                        return -EINVAL;
                }
                if (dev->status.lps == 0) {
                        KLOG_ERR("sequence out of order: use ARINC_SET first\n");
                        spin_unlock_bh(&board.lock);
                        return -EINVAL;
                }

                // determine the poll rate, assuming we only want the
		// buffers to get 1/2 full
                pollRate = dev->status.lps / (LPB / 2);
		for (i = 0; i < nRates; i++) {
			if (pollRate < pollRates[i]) {
				pollRate = pollRates[i];
				break;
			}
		}
		if (i == nRates) pollRate = pollRates[i-1];
		
		dev->status.pollRate = pollRate;
                dev->pollDtMsec = MSECS_PER_SEC / pollRate;

#ifdef USE_IRIG_CALLBACK
                if ( (dev->poll = irigClockRateToEnum(pollRate)) == IRIG_NUM_RATES) {
                        spin_unlock_bh(&board.lock);
                        KLOG_ERR("invalid poll rate: %d Hz\n", pollRate);
                        return -EINVAL;
                }

                // register a sweeping routine for this channel
                dev->sweepCallback =
                    register_irig_callback(arinc_sweep,0, dev->poll, (void *)(long)chn, &err);
                if (err) {
                        spin_unlock_bh(&board.lock);
                        KLOG_ERR("%s: Error registering callback\n",
                                 dev->deviceName);
                        return err;
                }
#else
                dev->sweep_jiffies = HZ / pollRate;
                dev->sweeper.function = arinc_sweep;
                dev->sweeper.expires = jiffies + dev->sweep_jiffies;
                dev->sweeper.data = chn + 1;
                add_timer(&dev->sweeper);
#endif

                // launch the board 
                err = ar_go(BOARD_NUM);
                if (err != ARS_NORMAL)
                        goto ar_fail_unlock;

                board.running = 1;
                spin_unlock_bh(&board.lock);
                break;

        case ARINC_BIT:

                if (dev->status.lps) {
                        KLOG_ERR("cannot run buit in test, already configured!\n");
                        return -EALREADY;
                }
                // perform a series of Built In Tests on the card
                if (copy_from_user(&aBIT, userptr, sizeof(aBIT))) {
                        KLOG_ERR("copy_from_user error!\n");
                        return -EFAULT;
                }

                spin_lock_bh(&board.lock);

                err = ar_execute_bit(BOARD_NUM, aBIT);
                if (err != ARS_NORMAL)
                        goto ar_fail_unlock;

                spin_unlock_bh(&board.lock);
                break;

        case ARINC_STAT:
                if (copy_to_user(userptr, &dev->status, sizeof (dsm_arinc_status))) {
                        KLOG_ERR("copy_to_user error!\n");
                        return -EFAULT;
                }
                break;

        default:
                KLOG_ERR("unrecognized ioctl %d (number %d, size %d)\n",
                     cmd, _IOC_NR(cmd), _IOC_SIZE(cmd));
                ret = -EINVAL;
                break;
        }
        return ret;

      ar_fail_unlock:
        spin_unlock_bh(&board.lock);
        if (err < 0) return err;
        log_error(BOARD_NUM, err);
        return -EIO;
}

/*
 * Implementation of poll fops.
 */
static unsigned int arinc_poll(struct file *filp, poll_table * wait)
{
        unsigned int mask = 0;

        struct arinc_dev *dev = (struct arinc_dev *) filp->private_data;
        if (!dev) return -EBADF;

        poll_wait(filp, &dev->rwaitq, wait);

        if (sample_remains(&dev->read_state) ||
            GET_TAIL(dev->samples,dev->samples.size))
                mask |= POLLIN | POLLRDNORM;    /* readable */
        return mask;
}

/*
 * Implementation of read fops.
 */
static ssize_t arinc_read(struct file *filp, char __user * buf,
                          size_t count, loff_t * pos)
{
        struct arinc_dev *dev = (struct arinc_dev *) filp->private_data;
        if (!dev) return -EBADF;

        return nidas_circbuf_read(filp, buf, count,
                                  &dev->samples, &dev->read_state,
                                  &dev->rwaitq);
}

/*
 * Implementation of write fops.
 */
static ssize_t arinc_write(struct file *filp, const char __user * buf,
                          size_t count, loff_t * pos)
{
        int err;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0)
        int txChn = iminor(file_inode(filp)) - N_ARINC_RX;
#else
        int txChn = iminor(filp->f_dentry->d_inode) - N_ARINC_RX;
#endif
        long data = 0;

        if (copy_from_user(&data, buf, count)) {
                KLOG_ERR("copy_from_user error!\n");
                return -EFAULT;
        }
        err = ar_putword(BOARD_NUM, txChn, data);
        if (err != ARS_NORMAL) {
                log_error(BOARD_NUM, err);
                return -EFAULT;
        }
        return 0;
}

/*
 * Implemention of release (close) fops.
 */
static int arinc_release(struct inode *inode, struct file *filp)
{
        struct arinc_dev *dev = (struct arinc_dev *) filp->private_data;
        int err;
        int chn = iminor(inode);

        if (chn >= N_ARINC_RX) return 0;
        if (!dev) return -EBADF;

#ifdef USE_IRIG_CALLBACK
        // unregister sweep routine with the IRIG driver 
        if (dev->sweepCallback &&
                unregister_irig_callback(dev->sweepCallback) == 0)
                        flush_irig_callbacks();
        dev->sweepCallback = 0;
        flush_irig_callbacks();
#else
        if (dev->sweeper.data > 0)
                del_timer_sync(&dev->sweeper);
        dev->sweeper.data = 0;
#endif

        spin_lock_bh(&board.lock);

        // filter out all labels on this channel 
        err =
            ar_label_filter(BOARD_NUM, chn, ARU_ALL_LABELS,
                            ARU_FILTER_ON);
        if (err != ARS_NORMAL) {
                spin_unlock_bh(&board.lock);
                if (err < 0) return err;
                log_error(BOARD_NUM, err);
                return -EIO;
        }

        // stop the board 
        if (atomic_dec_and_test(&board.numRxChannelsOpen) && board.running) {
                err = ar_reset(BOARD_NUM);
                if (err != ARS_NORMAL) {
                        spin_unlock_bh(&board.lock);
                        if (err < 0) return err;
                        log_error(BOARD_NUM, err);
                        return -EIO;
                }
                board.running = 0;
        }
        spin_unlock_bh(&board.lock);

        if (dev->samples.buf)
                free_dsm_circ_buf(&dev->samples);
        dev->samples.buf = 0;

	atomic_dec(&dev->used);
        return 0;
}

static struct file_operations arinc_fops = {
    .owner   = THIS_MODULE,
    .open    = arinc_open,
    .unlocked_ioctl   = arinc_ioctl,
    .poll    = arinc_poll,
    .read    = arinc_read,
    .write   = arinc_write,
    .release = arinc_release,
    .llseek  = no_llseek,
};

// -- MODULE ---------------------------------------------------------- 
/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
static void arinc_cleanup(void)
{
        short err;
        int chn;
        struct arinc_dev *dev;

        // unregister the channel sweeping routine(s)
        for (chn = 0; chn_info && (chn < N_ARINC_RX); chn++) {
                dev = &chn_info[chn]; 
#ifdef USE_IRIG_CALLBACK
                if (dev->sweepCallback &&
                        unregister_irig_callback(dev->sweepCallback) == 0)
                                flush_irig_callbacks();
                dev->sweepCallback = 0;
#else
                if (dev->sweeper.data > 0)
                    del_timer_sync(&dev->sweeper);
                dev->sweeper.data = 0;
#endif
                if (dev->samples.buf)
                        free_dsm_circ_buf(&dev->samples);
                dev->samples.buf = 0;
        }

        // unregister timesync routine 
#ifdef USE_IRIG_CALLBACK
        if (board.timeSyncCallback)
                unregister_irig_callback(board.timeSyncCallback);
#else
        if (board.syncer.data > 0)
                del_timer_sync(&board.syncer);
#endif

        // remove device
        if (MAJOR(arinc_cdev.dev) != 0) cdev_del(&arinc_cdev);
        if (MAJOR(arinc_device) != 0)
                unregister_chrdev_region(arinc_device, N_ARINC_RX+N_ARINC_TX);

        // free up the ISA memory region 
        if (board.basemem)
                release_region(board.basemem, PAGE_SIZE);

        // free up the chn_info
        if (chn_info)
                kfree(chn_info);

        // close the board 
        err = ar_close(BOARD_NUM);
        if (err != ARS_NORMAL)
                log_error(BOARD_NUM, err);

        // unmap the DPRAM address 
        if (board.phys_membase)
                iounmap(board.phys_membase);
}

// -- UTILITY --------------------------------------------------------- 
///
// There are not a lot of tests to perform to see if it is a
// CEI 420 card. The tests performed are:
//   - configuration register 1 : looking at bits 4-7, bit 6 is set
//                                (CEI-420-XXJ) or bit 4 and 5 are
//                                set (CEI-420-70J)
//   - configuration register 2 : value has to be 0x0
//   - configuration register 3 : value of bits 4-7 has to be 0x0
///
static int scan_ceiisa(void)
{
        unsigned char value;
        unsigned int indx;

        char *boardID[] = { "Standard CEI-220",
                "Standard CEI-420",
                "Custom CEI-220 6-Wire",
                "CEI-420-70J",
                "CEI-420-XXJ",
                "Obsolete",
                "CEI-420A-42-A",
                "CEI-420A-XXJ"
        };
        value = readb(board.phys_membase + 0x808);
        value >>= 4;
        if (value != 0x0) {
                // passed register 1 test... 
                value = readb(board.phys_membase + 0x80A);
                value >>= 4;
                if (value == 0x5)
                        KLOG_ERR("Obsolete CEI-420a\n");
                if (value == 0x0 || value == 0x1 || value == 0x2 ||
                    value == 0x3 || value == 0x4 || value == 0x6
                    || value == 0x7) {
                        // passed register 2 test... 
                        indx = value;
                        KLOG_DEBUG("cei220/420 found.  Board = %s\n",
                                   boardID[indx]);
                        value = readb(board.phys_membase + 0x80C);
                        if (value == 0x0) {
                                // passed register 3 test... 
                                value = readb(board.phys_membase + 0x80E);
                                value >>= 4;
                                if (value == 0) {
                                        // passed register 4 test... 
                                        KLOG_DEBUG
                                            ("found CEI-220/420 at 0x%lX\n",
                                             board.basemem);
                                        return 0;
                                }
                        }
                }
        }
        KLOG_ERR("No CEI420 ARINC board found!\n");
        return -ENODEV;
}

// -- MODULE ---------------------------------------------------------- 
static int __init arinc_init(void)
{
        int chn;
        int err;
        char api_version[150];
        struct arinc_dev *dev;
#ifndef USE_IRIG_CALLBACK
        unsigned int msecs;
#endif

        KLOG_NOTICE("version: %s\n", SVNREVISION);

        // When using gcc-4.9 to build against newer linux kernels,
        // the compiler option "-Werror=date-time" is in effect.
        // This option causes a compile error:
        //      macro "__DATE__" might prevent reproducible builds [-Werror=date-time]
        // when it encounters __DATE__ and __TIME__.
        // One can prevent the error by passing "-Wnoerror=date-time",
        // but older compilers cannot parse that option. We could try
        // testing for gcc and/or kernel version, but SVNREVISION
        // should provide enough information, and so we'll comment this:
        // KLOG_NOTICE("compiled on %s at %s\n", __DATE__, __TIME__);

        memset(&board,0,sizeof(board));
        spin_lock_init(&board.lock);
        atomic_set(&board.numRxChannelsOpen,0);

        // map ISA card memory into kernel memory 
        board.basemem = SYSTEM_ISA_IOMEM_BASE + iomem;
        board.phys_membase = ioremap(board.basemem, PAGE_SIZE);
        if (!board.phys_membase) {
                KLOG_ERR("ioremap failed.\n");
                return -EIO;
        }
        // reserve the ISA memory region 
        if (!request_region(board.basemem, PAGE_SIZE, "arinc")) {
                KLOG_ERR("couldn't allocate I/O range %lX - %lX\n",
                           board.basemem, board.basemem + PAGE_SIZE - 1);
                err = -EBUSY;
                goto fail;
        }
        KLOG_DEBUG("basemem:      %X\n", (unsigned int) board.basemem);
        KLOG_DEBUG("phys_membase: %X\n", (unsigned int) board.phys_membase);

        // scan the ISA bus for the device 
        err = scan_ceiisa();
        if (err) goto fail;

        // obtain the API version string
        ar_version(api_version);
        KLOG_DEBUG("API Version %s\n", api_version);

        // load the board (the size and address are not used - must specify zero) 
        err = ar_loadslv(BOARD_NUM,(unsigned long)board.phys_membase, 0, 0);
        if (err != ARS_NORMAL) goto fail;

        // initialize the board 
        err = ar_init_dual_port(BOARD_NUM);
        if (err != ARS_NORMAL) goto fail;

        // Display the board type 
        KLOG_DEBUG("Board %s detected\n", ar_get_boardname(BOARD_NUM, NULL));
        KLOG_DEBUG("Supporting %d transmitters and %d receivers.\n",
                   ar_num_xchans(BOARD_NUM), ar_num_rchans(BOARD_NUM));

        // select buffered mode 
        err = ar_set_storage_mode(BOARD_NUM, ARU_BUFFERED);
        if (err != ARS_NORMAL) goto fail;

        // adjust the i960's internal clock rate 
        err = ar_set_timerrate(BOARD_NUM, 4 * 1000);
        if (err != ARS_NORMAL) goto fail;

        // select high speed transmit 
        err = ar_set_config(BOARD_NUM, ARU_XMIT_RATE, AR_HIGH);
        if (err != ARS_NORMAL) goto fail;

        // disable scheduled transmit mode
        err = ar_msg_control(BOARD_NUM, AR_OFF);
        if (err != ARS_NORMAL) goto fail;

        // disable internal wrap
        err = ar_set_config(BOARD_NUM, ARU_INTERNAL_WRAP, AR_WRAP_OFF);
        if (err != ARS_NORMAL) goto fail;

        // instruct the board to time tag each label 
        err = ar_timetag_control(BOARD_NUM, ARU_ENABLE_TIMETAG);
        if (err != ARS_NORMAL) goto fail;

        // prematurely launch the board (it will be reset and re-launched via ioctls) 
        err = ar_go(BOARD_NUM);
        if (err != ARS_NORMAL) goto fail;

        // sync up the i960's internal clock to the IRIG time 
        // and register a timesync routine 
#ifdef USE_IRIG_CALLBACK
        ar_set_timercnt(BOARD_NUM, GET_MSEC_CLOCK);
        board.sync_rate = IRIG_1_HZ;
        board.timeSyncCallback = register_irig_callback(arinc_timesync,0,board.sync_rate, (void *) 0, &err);
        if (!board.timeSyncCallback) goto fail;
#else
        ar_set_timercnt(BOARD_NUM, getSystemTimeMsecs());
        init_timer(&board.syncer);

        board.sync_jiffies = HZ / 1;    /* 1 HZ callbacks */
        board.syncer.function = arinc_timesync;

        /* schedule arinc_timesync to run at the even second */
        msecs = getSystemTimeMsecs() % MSECS_PER_SEC;   /* milliseconds after the second */

        board.syncer.expires = jiffies + board.sync_jiffies - msecs * HZ / MSECS_PER_SEC;
        board.syncer.data = 1;  /* set data to 1, just to indicate this timer is active */
        add_timer(&board.syncer);
#endif

        // Initialize and add user-visible devices
        err = alloc_chrdev_region(&arinc_device, 0, N_ARINC_RX+N_ARINC_TX, "arinc");
        if (err < 0) goto fail;
        KLOG_DEBUG("major device number %d\n", MAJOR(arinc_device));

        // reserve and clear kernel memory for chn_info
        err = -ENOMEM;
        chn_info =
            kmalloc(N_ARINC_RX * sizeof(struct arinc_dev), GFP_KERNEL);
        if (!chn_info) goto fail;
        memset(chn_info, 0, N_ARINC_RX * sizeof(struct arinc_dev));

        // initialize each channel structure
        for (chn = 0; chn < N_ARINC_RX; chn++) {
                dev = &chn_info[chn];
                atomic_set(&dev->used,0);
                init_waitqueue_head(&dev->rwaitq);
                sprintf(dev->deviceName, "/dev/arinc%d", chn);

#ifndef USE_IRIG_CALLBACK
                init_timer(&dev->sweeper);
#endif

        }
        cdev_init(&arinc_cdev, &arinc_fops);

        // after calling cdev_add the devices are live and ready for user operations.
        err = cdev_add(&arinc_cdev, arinc_device, N_ARINC_RX+N_ARINC_TX);
        if (err < 0) goto fail;

        KLOG_DEBUG("ARINC init_module complete.\n");
        return 0;               // success 

      fail:
        arinc_cleanup();
        if (err < 0)
                return err;

        // ar_???() error codes are positive... 
        log_error(BOARD_NUM, err);
        return -EIO;
}

module_init(arinc_init);
module_exit(arinc_cleanup);
