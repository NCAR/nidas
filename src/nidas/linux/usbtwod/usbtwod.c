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
 * USB PMS-2D driver - 2.0
 *
 * Copyright (C) 2007 University Corporation for Atmospheric Research
 *
 * This driver uses kernel reference counting (kref_get/put) and a rwlock
 * in order to avoid problems if user code is accessing the device
 * while it is disconnected (hot-unplugged).  These ideas were taken
 * from the 2.6.22.9 version of drivers/usb/usb-skeleton.c.
 */

// #define DEBUG

#include "usbtwod.h"

#include <linux/kernel.h>
#include <linux/version.h>

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/moduleparam.h>

#include <nidas/linux/klog.h>

#include <nidas/linux/Revision.h>    // REPO_REVISION

#ifndef REPO_REVISION
#define REPO_REVISION "unknown"
#endif

/* This driver will be invoked when devices with the
 * NCAR_VENDOR_ID and either of the PRODUCT_IDs are
 * plugged in.  Currently it isn't strictly necessary
 * for 64 and 32 bit probes to have a different product
 * id, since we can differentiate between them by the
 * number of endpoints and the speed:
 * 64 bit probes have 3 endpoints and are high speed
 * 32 bit probes have 2 endpoints and are full speed
 */
/* Define these values to match your devices */
#define NCAR_VENDOR_ID            0x2D2D
#define USB2D_64_PRODUCT_ID       0x2D00
#define USB2D_32_PRODUCT_ID       0x2D01
#define USB2D_64_V3_PRODUCT_ID    0x2D03

/* These are the default Cyprus EZ FX & FX2 ID's */
//#define NCAR_VENDOR_ID         0x0547
//#define USB2D_64_PRODUCT_ID  0x1003
//#define USB2D_32_PRODUCT_ID  0x1002

/* table of devices that work with this driver */
static struct usb_device_id twod_table[] = {
        {USB_DEVICE(NCAR_VENDOR_ID, USB2D_64_PRODUCT_ID)},
        {USB_DEVICE(NCAR_VENDOR_ID, USB2D_64_V3_PRODUCT_ID)},
        {USB_DEVICE(NCAR_VENDOR_ID, USB2D_32_PRODUCT_ID)},
        {}                      /* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, twod_table);

/* use twod_open_lock to prevent a race between open and disconnect */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)

static DEFINE_MUTEX(twod_open_lock);
#define TWOD_MUTEX_LOCK(d) mutex_lock(d);
#define TWOD_MUTEX_UNLOCK(d) mutex_unlock(d);

#else

static DECLARE_MUTEX(twod_open_lock);
#define TWOD_MUTEX_LOCK(d) down(d)
#define TWOD_MUTEX_UNLOCK(d) up(d)

#endif

/*
 * If BUFFER_USER_READS is true, then don't call wake_up_interruptible() every
 * time an URB is received. Instead, only call it if half of the available
 * URBS are available to be read or dev->latencyJiffies has elapsed since the last read.
#define BUFFER_USER_READS
 */

/*
 * For info in whether to set USE_DMA_COHERENT_FOR_IMG_URBS, see
 * https://www.kernel.org/doc/html/latest/driver-api/usb/dma.html.
 * https://www.kernel.org/doc/html/latest/core-api/dma-api-howto.html
 * dma.html says:
 *    If you’re doing lots of small data transfers from the same buffer all the time,
 *    that can really burn up resources on systems which use an IOMMU to manage the
 *    DMA mappings. It can cost MUCH more to set up and tear down the IOMMU mappings
 *    with each request than perform the I/O!
 *
 * To avoid these issues, one can use usb_alloc_coherent() instead of kmalloc()
 * and set URB_NO_TRANSFER_DMA_MAP in transfer flags.
 *
 * However, in our case we're not doing lots of small transfers from the same buffer, and so
 * it seems we should use the default DMA handling supported by kmalloc() and not use
 * URB_NO_TRANSFER_DMA_MAP.  As the doc says;
 *     Most drivers should NOT be using these primitives; they don’t need to use
 *     this type of memory (“dma-coherent”), and memory returned from kmalloc() will work just fine.
 *
 * This is perhaps one area where you'd see differences in performance between ARM and X86.
 * Other web posts indicate that it could depend on the USB controller.
 * Using usb_alloc_coherent and URB_NO_TRANSFER_DMA_MAP was in effect up until 2023, on ARM Vulcans.
 * #define USE_DMA_COHERENT_FOR_IMG_URBS
 */

static unsigned int throttleRate = 0;

MODULE_PARM_DESC(throttleRate,
    "image/sec: 0 for no throttling, or (N X " __stringify(MAX_THROTTLE_FUNC_RATE) \
    "), for N in [1:" __stringify(IMG_URBS_IN_FLIGHT) "]");
module_param(throttleRate, uint, 0);

/*
 * Report apparent USB shutdowns/disconnects.
 */
static void klog_shutdown(const char* devname, const char* func, const char* status)
{
        KLOG_NOTICE("%s: %s: status=%s\n", devname, func, status);
}

/*
 * Report USB URB errors.
 */
static void klog_error(const char* devname, const char* func, const char* status, int err)
{
        KLOG_ERR("%s: %s: status=%s (%d)\n", devname, func, status, err);
}

static int check_urb_status(int urbstatus, struct usb_twod *dev, const char* func)
{
        /*
         * The error handling for all three types of urbs, received images and SORs, and
         * sent TASs, is basically the same. If the urb->status is an error value,
         * then set dev->errorStatus, which will result in an error in the user-side
         * poll or read.  The user can then try to reopen.
         *
         * For status values that indicate that the device was disconnected or shutdown,
         * just report that situation once.
         *
         * Return values: 
         *      0: urb OK
         *      1: urb error
         *      2: urb timeout. For received urbs, resubmit.
         *
         * See https://www.kernel.org/doc/html/latest/driver-api/usb/error-codes.html
         * in section "Error codes returned by in urb->status".
         */

        switch (urbstatus) {
        case 0:
                dev->consecTimeouts = 0;
                return 0;
        case -ENOENT:
                /*
                 * URB was synchronously unlinked by usb_unlink_urb
                 */
		if (!dev->stats.shutdowns)    /* don't report shutdown more than once */
                        klog_shutdown(dev->dev_name, func, "-ENOENT");
                dev->stats.shutdowns++;
                dev->errorStatus = urbstatus;
                return 1;
        case -ESHUTDOWN:
                /*
                 * The device or host controller has been disabled due
                 * to some problem that could not be worked around,
                 * such as a physical disconnect.
                 */
		if (!dev->stats.shutdowns)
                        klog_shutdown(dev->dev_name, func, "-ESHUTDOWN");
                dev->stats.shutdowns++;
                dev->errorStatus = urbstatus;
                return 1;
        case -ECONNRESET:
                /*
                 * URB was asynchronously unlinked by usb_unlink_urb
                 */
                klog_error(dev->dev_name, func, "-ECONNRESET", urbstatus);
                dev->stats.urbErrors++;
                dev->errorStatus = urbstatus;
                return 1;
        case -EOVERFLOW:
                /*
                 * -EOVERFLOW (*)
                 * The amount of data returned by the endpoint was
                 * greater than either the max packet size of the
                 * endpoint or the remaining buffer size.  "Babble".
                 */
                klog_error(dev->dev_name, func, "-EOVERFLOW", urbstatus);
                dev->stats.urbErrors++;
                dev->errorStatus = urbstatus;
                return 1;
        case -EPROTO:
                /*
                 * -EPROTO (*, **)
                 *     a: bitstuff error
                 *     b: no response packet received within the prescribed
                 *        bus turn-around time
                 *     c: unknown USB error 
                 */
                klog_error(dev->dev_name, func, "-EPROTO", urbstatus);
                dev->stats.urbErrors++;
                dev->errorStatus = urbstatus;
                return 1;
        case -ETIMEDOUT:
                /*
                 * Synchronous USB message functions use this code
                 * to indicate timeout expired before the transfer
                 * completed, and no other error was reported by HC.
                 *
                 * We're using asynchronous functions (callbacks) so we probably
                 * shouldn't see ETIMEDOUTs.
                 *
                 * The following is an old comment that perhaps was related to
                 * early use of synchronous USB I/O functions:
		 *      Sometimes we see one urb ETIMEDOUT and things continue working.
		 *      Other times the probe (or usb controller, not sure which)
		 *      never recovers and returns ETIMEDOUT for every returned urb.
		 *      We'll give up resubmitting after 10 in a row.
                 */
                dev->consecTimeouts++;
                KLOG_WARNING("%s: %s: image urbstatus=-ETIMEDOUT, consecutive=%d\n",
			func, dev->dev_name,dev->consecTimeouts);
                dev->stats.urbTimeouts++;
                if (dev->consecTimeouts >= 10) {
                    dev->errorStatus = urbstatus;
		    return 1;
		}
		return 2;
        default:
                klog_error(dev->dev_name, func, "", urbstatus);
                dev->stats.urbErrors++;
                dev->errorStatus = urbstatus;
                return 1;
        }
}

static struct usb_driver twod_driver;

static void twod_dev_free(struct usb_twod *dev)
{
        if (dev->sampleq.buf) {
                if (dev->sampleq.buf[0]) kfree(dev->sampleq.buf[0]);
                kfree(dev->sampleq.buf);
                dev->sampleq.buf = 0;
        }
        if (dev->img_urb_q.buf) kfree(dev->img_urb_q.buf);
        dev->img_urb_q.buf = 0;

        if (dev->tas_urb_q.buf) kfree(dev->tas_urb_q.buf);
        dev->tas_urb_q.buf = 0;
}

static void twod_dev_delete(struct kref *kref)
{
        struct usb_twod *dev = 
            container_of(kref, struct usb_twod, kref);
        usb_put_dev(dev->udev);
        twod_dev_free(dev);
        kfree (dev);
}

/* -------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
static void twod_tas_tx_bulk_callback(struct urb *urb)
#else
static void twod_tas_tx_bulk_callback(struct urb *urb,
                                      struct pt_regs *regs)
#endif
{
	/* Note that these urb callbacks are called in 
         * software interrupt context. 
         */
        struct usb_twod *dev = (struct usb_twod *) urb->context;

        // there must be space, since TAIL was incremented
        // when this urb was submitted.
        BUG_ON(CIRC_SPACE(dev->tas_urb_q.head, READ_ONCE(dev->tas_urb_q.tail),
                          TAS_URB_QUEUE_SIZE) == 0);
        INCREMENT_HEAD(dev->tas_urb_q, TAS_URB_QUEUE_SIZE);

        check_urb_status(urb->status, dev, __func__);
}

/* -------------------------------------------------------------------- */
static struct urb *twod_make_tas_urb(struct usb_twod *dev)
{
        struct urb *urb;
        char *buf;

        // We're doing GFP_KERNEL memory allocations, so it is a
        // bug if this is running from interrupt context.
        BUG_ON(in_interrupt());

        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb)
                return urb;

        buf = kmalloc(TWOD_TAS_BUFF_SIZE, GFP_KERNEL);
        if (!buf) {
                KLOG_ERR("%s: %s: out of memory for TAS output buf\n",
                                dev->dev_name, __func__);
                usb_free_urb(urb);
                urb = NULL;
                return urb;
        }

        urb->transfer_flags = 0;
	/* This sets urb->transfer_buffer for us. */
        usb_fill_bulk_urb(urb, dev->udev,
                          usb_sndbulkpipe(dev->udev,
                                          dev->tas_out_endpointAddr), buf,
                          TWOD_TAS_BUFF_SIZE, twod_tas_tx_bulk_callback,
                          dev);
        return urb;
}

/*
 * Used by timer function send_tas_timer_func to send the true air speed
 * value via the bulk write end-point.  Therefore, it is called from
 * interrupt context.
 */
static int write_tas(struct usb_twod *dev)
{
        int retval = 0;
        struct urb *urb;
        if ((urb = GET_TAIL(dev->tas_urb_q,TAS_URB_QUEUE_SIZE))) {
                spin_lock(&dev->taslock);
                memcpy(urb->transfer_buffer, &dev->tasValue, TWOD_TAS_BUFF_SIZE);
                if (dev->ptype != TWOD_64_V3) {
                        dev->tasValue.cntr++;
                        dev->tasValue.cntr %= 10;
                }
                spin_unlock(&dev->taslock);

                INCREMENT_TAIL(dev->tas_urb_q, TAS_URB_QUEUE_SIZE);

                /*
                 * usb_submit_urb, memory flag argument:
                 *  GFP_ATOMIC:
                 *      1. when called from urb callback handler,
                 *          interrupt service routine, tasklet or a kernel timer
                 *          callback.
                 *      2. If you're holding a spinlock or rwlock
                 *          (but not a semaphore).
                 *      3. Current state is not TASK_RUNNING (i.e. current is
                 *          non-null and driver has not changed the state).
                 *  GFP_NOIO: block drivers.
                 *  GFP_KERNEL: all other circumstances.
                 */

                read_lock(&dev->usb_iface_lock);

                /* must use GFP_ATOMIC since we hold a rwlock */
                if (dev->interface) retval = usb_submit_urb(urb, GFP_ATOMIC);
                else retval = -ENODEV;         /* disconnect() was called, errno 19 */

                read_unlock(&dev->usb_iface_lock);
                if (retval < 0) {
                        if (retval == -ENODEV) {
                                if (!dev->stats.shutdowns)
                                        klog_shutdown(dev->dev_name, __func__, "-ENODEV");
                                dev->stats.shutdowns++;
                        }
                        else {
                                dev->stats.urbErrors++;
                                KLOG_ERR("%s: %s: retval=%d, stats.urbErrors=%d\n",
                                        dev->dev_name, __func__,retval,dev->stats.urbErrors);
                        }
			dev->errorStatus = retval;
                }
        } else {
                if (!(dev->stats.lostTASs++ % 100))
                        KLOG_WARNING("%s: no urbs available for TAS write, lostTASs=%d\n",
                             dev->dev_name, dev->stats.lostTASs);
        }
//        KLOG_INFO("writing tas for %s: tas=%d\n", dev->dev_name, retval);
        return retval;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
static void send_tas_timer_func(unsigned long arg)
{
        struct timer_list* tlist = (struct timer_list*)arg;
#else
static void send_tas_timer_func(struct timer_list* tlist)
{
#endif
        // Note that this runs in software interrupt context.
        struct usb_twod *dev = container_of(tlist, struct usb_twod, sendTASTimer);

        write_tas(dev);
        // reschedule
        mod_timer(&dev->sendTASTimer,
                dev->sendTASTimer.expires + dev->sendTASJiffies);
}

static int twod_set_sor_rate(struct usb_twod *dev, int rate)
{
        if (dev->sendTASJiffies > 0)
                del_timer_sync(&dev->sendTASTimer);
        dev->sendTASJiffies = 0;
        if (rate > 0) {
                /* how many jiffies we are into the second */
//
                unsigned int jif = 
                        (getSystemTimeTMsecs() % TMSECS_PER_SEC) * HZ / TMSECS_PER_SEC;

                dev->sendTASJiffies = HZ / rate;
                if (dev->sendTASJiffies <= 0)
                        dev->sendTASJiffies = 1;
		KLOG_INFO("%s: SOR rate=%d,jiffies=%d\n",dev->dev_name,rate,dev->sendTASJiffies);
                /*
                 * The calls to send_tas_timer_func will be on
                 * an integral sendTASJiffies interval.
                 */
                dev->sendTASTimer.expires = jiffies + 2 * dev->sendTASJiffies -
                        (jif % dev->sendTASJiffies);
                add_timer(&dev->sendTASTimer);
        }
        return 0;
}

static int usb_twod_submit_img_urb(struct usb_twod *dev, struct urb *urb)
{
        int retval;
        if (throttleRate > 0) {
                /* there should always be space in this queue, because
                 * there are IMG_URB_QUEUE_SIZE-1 number of urbs
                 * in flight */
                if (CIRC_SPACE(dev->img_urb_q.head, READ_ONCE(dev->img_urb_q.tail),
                       IMG_URB_QUEUE_SIZE) == 0)
                    KLOG_ERR("%s: %s: programming error: no space in queue for resubmitting urbs\n",
                                    dev->dev_name, __func__);
                else {
                        dev->img_urb_q.buf[dev->img_urb_q.head] = urb;
                        INCREMENT_HEAD(dev->img_urb_q, IMG_URB_QUEUE_SIZE);
                }
                return 0;
        }

        /*
         * usb_submit_urb, memory flag argument:
         *  GFP_ATOMIC:
         *      1. when called from urb callback handler,
         *          interrupt service routine, tasklet or a kernel timer
         *          callback.
         *      2. If you're holding a spinlock or rwlock
         *          (but not a semaphore).
         *      3. Current state is not TASK_RUNNING (i.e. current is
         *          non-null and driver has not changed the state).
         *  GFP_NOIO: block drivers.
         *  GFP_KERNEL: all other circumstances.
         *
         *  Since we're holding a rwlock, we must use GFP_ATOMIC.
         */
        read_lock(&dev->usb_iface_lock);

        /* must use GFP_ATOMIC since we hold a rwlock */
        if (dev->interface) retval = usb_submit_urb(urb, GFP_ATOMIC);
        else retval = -ENODEV;         /* disconnect() was called */

        read_unlock(&dev->usb_iface_lock);
        if (retval < 0) {
                if (retval == -ENODEV) {
                        if (!dev->stats.shutdowns)
                                klog_shutdown(dev->dev_name, __func__, "-ENODEV");
                        dev->stats.shutdowns++;
                }
                else {
                        dev->stats.urbErrors++;
                        KLOG_ERR("%s: %s: retval=%d, stats.urbErrors=%d\n",
                                dev->dev_name, __func__, retval,dev->stats.urbErrors);
                }
                dev->errorStatus = retval;
                wake_up_interruptible(&dev->read_wait);
        }
        return retval;
}

/* -------------------------------------------------------------------- */
static int usb_twod_submit_sor_urb(struct usb_twod *dev, struct urb *urb)
{
        int retval;
        read_lock(&dev->usb_iface_lock);

        /* must use GFP_ATOMIC since we hold a rwlock */
        if (dev->interface) retval = usb_submit_urb(urb, GFP_ATOMIC);
        else retval = -ENODEV;         /* disconnect() was called */

        read_unlock(&dev->usb_iface_lock);
        if (retval < 0) {
                if (retval == -ENODEV) {
                        if (!dev->stats.shutdowns)
                                klog_shutdown(dev->dev_name, __func__, "-ENODEV");
                        dev->stats.shutdowns++;
                }
                else {
                        dev->stats.urbErrors++;
                        KLOG_ERR("%s: %s: retval=%d, stats.urbErrors=%d\n",
                                dev->dev_name, __func__, retval,dev->stats.urbErrors);
                }
                dev->errorStatus = retval;
                wake_up_interruptible(&dev->read_wait);
        }
        return retval;
}


/* -------------------------------------------------------------------- */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
static void urb_throttle_func(unsigned long arg)
{
        struct timer_list *tlist = (struct timer_list*)arg;
#else
static void urb_throttle_func(struct timer_list* tlist)
{
#endif
        struct usb_twod *dev = container_of(tlist, struct usb_twod, urbThrottle);
        int retval,i;
        struct urb *urb;
// #define DEBUG
#ifdef DEBUG
        static int debugcntr = 0;
#endif
        for (i = 0; i < dev->nurbPerTimer; i++) {

                if ((urb = GET_TAIL(dev->img_urb_q,IMG_URB_QUEUE_SIZE))) {
#ifdef DEBUG
                        if (!(debugcntr++ % 100))
                                KLOG_INFO("%s: queue cnt=%d,jiffies=%ld\n",
                                                dev->dev_name, 
                                                CIRC_CNT(dev->img_urb_q.head,
                                                dev->img_urb_q.tail,
                                                IMG_URB_QUEUE_SIZE), jiffies);
#endif
// #undef DEBUG

                        read_lock(&dev->usb_iface_lock);

                        /* This is a timer function, running in software
                         * interrupt context, and we hold a rwlock,
                         * so use GFP_ATOMIC.
                         */
                        if (dev->interface) retval = usb_submit_urb(urb, GFP_ATOMIC);
                        else retval = -ENODEV;         /* disconnect() was called */

                        read_unlock(&dev->usb_iface_lock);
                        if (retval < 0) {
                                if (retval == -ENODEV) {
                                        if (!dev->stats.shutdowns)
                                                klog_shutdown(dev->dev_name, __func__, "-ENODEV");
                                        dev->stats.shutdowns++;
                                }
                                else {
                                        dev->stats.urbErrors++;
                                        KLOG_ERR("%s: %s: retval=%d, stats.urbErrors=%d\n",
                                                dev->dev_name, __func__, retval,dev->stats.urbErrors);
                                }
                                dev->errorStatus = retval;
                                wake_up_interruptible(&dev->read_wait);
                        }
                        INCREMENT_TAIL(dev->img_urb_q, IMG_URB_QUEUE_SIZE);
                }
        }
        // reschedule myself
        mod_timer(&dev->urbThrottle, jiffies + dev->throttleJiffies);
}

/* -------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
static void twod_img_rx_bulk_callback(struct urb *urb)
#else
static void twod_img_rx_bulk_callback(struct urb *urb,
                                      struct pt_regs *regs)
#endif
{
	/* Note that these urb callbacks are called in 
         * software interrupt context. 
         */
        struct usb_twod *dev = (struct usb_twod *) urb->context;
        struct twod_urb_sample *osamp;
        dsm_sample_time_t timetag = getSystemTimeTMsecs();

        switch (check_urb_status(urb->status, dev, __func__)) {
        case 0: /* OK */
                break;  
        case 1: /* error */
                wake_up_interruptible(&dev->read_wait);
                return;
        case 2: /* timeout, try again */
                goto resubmit;
        }

        dev->stats.numImages++;

        /*
         * The sampleq head pointer is incremented in two different callback
         * handers, the image rx and the sor rx handlers. We use a spinlock
         * here to avoid conflicts.
         */
        spin_lock(&dev->sampqlock);
        osamp =
            (struct twod_urb_sample *) GET_HEAD(dev->sampleq,
                                                SAMPLE_QUEUE_SIZE);
        if (!osamp) {
                spin_unlock(&dev->sampqlock);
                // overflow, no sample available for output.
                if (!(dev->stats.lostImages++ % 100))
                        KLOG_WARNING("%s: sample queue full: lost images=%d\n",
                                dev->dev_name, dev->stats.lostImages);
                // resubmit the urb (current data is lost)
                goto resubmit;
        } else {
                osamp->timetag = timetag;
                osamp->length = sizeof(osamp->stype) +
				sizeof(osamp->data) +
				urb->actual_length;
                if (dev->ptype == TWOD_64_V3){
                        osamp->stype = cpu_to_be32(TWOD_IMGv3_TYPE);
                } else if (dev->ptype == TWOD_64){
                        osamp->stype = cpu_to_be32(TWOD_IMGv2_TYPE);
                }

                /*
                 * stuff the current TAS value in the data.
                 * It is little-endian, since it was converted
                 * before being sent to the probe. For 64_v3
                 * tas will be tas*10. Currently all three tas
                 * variants are the same size.
                 */
                spin_lock(&dev->taslock);
                memcpy(&osamp->data, &dev->tasValue, sizeof(Tap2D));
                spin_unlock(&dev->taslock);
                osamp->pre_urb_len = sizeof(osamp->timetag) +
			sizeof(osamp->length) +
			sizeof(osamp->stype) +
			sizeof(osamp->data);
                osamp->urb = urb;
                INCREMENT_HEAD(dev->sampleq, SAMPLE_QUEUE_SIZE);
                spin_unlock(&dev->sampqlock);

#ifdef BUFFER_USER_READS
		if (((long)jiffies - (long)dev->lastWakeup) > dev->latencyJiffies ||
                        CIRC_SPACE(dev->sampleq.head, READ_ONCE(dev->sampleq.tail), SAMPLE_QUEUE_SIZE) <
				(IMG_URBS_IN_FLIGHT + SOR_URBS_IN_FLIGHT)/2) {
                        wake_up_interruptible(&dev->read_wait);
                        dev->lastWakeup = jiffies;
                }
#else
                wake_up_interruptible(&dev->read_wait);
#endif
        }
        return;
resubmit:
        usb_twod_submit_img_urb(dev, urb);
}

/* -------------------------------------------------------------------- */
static struct urb *twod_make_img_urb(struct usb_twod *dev)
{
        struct urb *urb;
        char *buf = NULL;

        // We're doing GFP_KERNEL memory allocations, so it is a
        // bug if this is running from interrupt context.
        BUG_ON(in_interrupt());

        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb)
                return urb;

        if (!
            (buf =
#ifdef USE_DMA_COHERENT_FOR_IMG_URBS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
             usb_alloc_coherent(dev->udev, TWOD_IMG_BUFF_SIZE, GFP_KERNEL,
                              &urb->transfer_dma)
#else
             usb_buffer_alloc(dev->udev, TWOD_IMG_BUFF_SIZE, GFP_KERNEL,
                              &urb->transfer_dma)
#endif
#else
             kmalloc(TWOD_IMG_BUFF_SIZE, GFP_KERNEL)
#endif
                              )) {
                KLOG_ERR("%s: %s: out of memory for read buf\n",
                                dev->dev_name, __func__);
                usb_free_urb(urb);
                urb = NULL;
                return urb;
        }

#ifdef USE_DMA_COHERENT_FOR_IMG_URBS
        urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
#endif

        usb_fill_bulk_urb(urb, dev->udev,
                          usb_rcvbulkpipe(dev->udev,
                                          dev->img_in_endpointAddr),
                          buf, TWOD_IMG_BUFF_SIZE,
                          twod_img_rx_bulk_callback,dev);
        return urb;
}

/* -------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
static void twod_sor_rx_bulk_callback(struct urb *urb)
#else
static void twod_sor_rx_bulk_callback(struct urb *urb,
                                      struct pt_regs *regs)
#endif
{
	/* Note that these urb callbacks are called in 
         * software interrupt context. 
         */
        struct usb_twod *dev = (struct usb_twod *) urb->context;
        struct twod_urb_sample *osamp;
        dsm_sample_time_t timetag = getSystemTimeTMsecs();

        switch (check_urb_status(urb->status, dev, __func__)) {
        case 0: /* OK */
                break;  
        case 1: /* error */
                wake_up_interruptible(&dev->read_wait);
                return;
        case 2: /* timeout, try again */
                goto resubmit;
        }

        dev->stats.numSORs++;

        /*
         * The sampleq head pointer is incremented in two different callback
         * handers, the image rx and the sor rx handlers. We use a spinlock
         * here to avoid conflicts.
         */
        spin_lock(&dev->sampqlock);
        osamp =
            (struct twod_urb_sample *) GET_HEAD(dev->sampleq,
                                                SAMPLE_QUEUE_SIZE);
        if (!osamp) {
                spin_unlock(&dev->sampqlock);
                // overflow, no sample available for output.
                if (!(dev->stats.lostSORs++ % 1000))
                        KLOG_WARNING("%s: overflow: lost SOR urbs = %d\n",
                                dev->dev_name,dev->stats.lostSORs);
                // resubmit the urb (current data is lost)
                goto resubmit;
        } else {

#ifdef SOR_DEBUG
                if (dev->SORdebugmessages++ < 5) {
                        KLOG_INFO("%s: SOR received, transfer_buffer_length: %d, actual length: %d, #packets: %d\n",
                                dev->dev_name, urb->transfer_buffer_length, urb->actual_length,
                                urb->number_of_packets);
                }
#endif
                osamp->timetag = timetag;
                osamp->length = sizeof(osamp->stype) +
				urb->actual_length;
                if (dev->ptype == TWOD_64_V3){
                        osamp->stype = cpu_to_be32(TWOD_SORv3_TYPE);
                } else if (dev->ptype == TWOD_64){
                        osamp->stype = cpu_to_be32(TWOD_SOR_TYPE);
                }
                osamp->pre_urb_len = sizeof(osamp->timetag) +
			sizeof(osamp->length) +
			sizeof(osamp->stype);
                osamp->urb = urb;
                INCREMENT_HEAD(dev->sampleq, SAMPLE_QUEUE_SIZE);
                spin_unlock(&dev->sampqlock);
#ifdef BUFFER_USER_READS
		if (((long)jiffies - (long)dev->lastWakeup) > dev->latencyJiffies ||
                        CIRC_SPACE(dev->sampleq.head, READ_ONCE(dev->sampleq.tail), SAMPLE_QUEUE_SIZE) <
				(IMG_URBS_IN_FLIGHT + SOR_URBS_IN_FLIGHT)/2) {
                        wake_up_interruptible(&dev->read_wait);
                        dev->lastWakeup = jiffies;
                }
#else
                wake_up_interruptible(&dev->read_wait);
#endif
        }
        return;
resubmit:
	usb_twod_submit_sor_urb(dev, urb);
        return;
}

/* -------------------------------------------------------------------- */
static struct urb *twod_make_sor_urb(struct usb_twod *dev)
{
        struct urb *urb;
        char *buf;

        // We're doing GFP_KERNEL memory allocations, so it is a
        // bug if this is running from interrupt context.
        BUG_ON(in_interrupt());

        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb)
                return urb;

        buf = kmalloc(TWOD_SOR_BUFF_SIZE, GFP_KERNEL);
        if (!buf) {
                KLOG_ERR("%s: %s: out of memory for read buf\n",
                                dev->dev_name, __func__);
                usb_free_urb(urb);
                urb = NULL;
                return urb;
        }

        urb->transfer_flags = 0;

        usb_fill_bulk_urb(urb, dev->udev,
                          usb_rcvbulkpipe(dev->udev,
                                          dev->sor_in_endpointAddr), buf,
                          TWOD_SOR_BUFF_SIZE, twod_sor_rx_bulk_callback,
                          dev);
        return urb;
}

/* -------------------------------------------------------------------- */
static int twod_open(struct inode *inode, struct file *file)
{
        struct usb_twod *dev;
        struct usb_interface *interface;
        struct twod_urb_sample *samp;
        int subminor;
        int i, retval = 0;

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,file);

        subminor = iminor(inode);

        TWOD_MUTEX_LOCK(&twod_open_lock);
        interface = usb_find_interface(&twod_driver, subminor);
        if (!interface) {
                KLOG_ERR("%s: error, can't find device for minor %d\n",
                     __func__, subminor);
                TWOD_MUTEX_UNLOCK(&twod_open_lock);
                return -ENODEV;
        }

        dev = usb_get_intfdata(interface);
        if (!dev) {
                TWOD_MUTEX_UNLOCK(&twod_open_lock);
                return -ENODEV;
        }

        /* prevent two opens */
        if (dev->is_open) {
                TWOD_MUTEX_UNLOCK(&twod_open_lock);
                return -EBUSY;
        }
        dev->is_open = 1;
        /* increment our usage count for this device */
        kref_get(&dev->kref);
        /* now we can drop the lock */
        TWOD_MUTEX_UNLOCK(&twod_open_lock);

        memset(&dev->stats, 0, sizeof (dev->stats));
        memset(&dev->readstate, 0, sizeof (dev->readstate));
        dev->errorStatus = 0;
        dev->consecTimeouts = 0;

	dev->latencyJiffies = 250 * HZ / MSECS_PER_SEC;
	dev->lastWakeup = jiffies;

        /*
         * Since we are preventing simultaneous opens,
         * then either this is the first open of this
         * device, or there was a previous close. Check it.
         */
        BUG_ON(dev->sampleq.buf);

        /* allocate the sample circular buffer */
        dev->sampleq.buf =
            kmalloc(sizeof (struct twod_urb_sample *) * SAMPLE_QUEUE_SIZE,
                    GFP_KERNEL);
        if (!dev->sampleq.buf) {
                retval = -ENOMEM;
                goto error;
        }
        memset(dev->sampleq.buf, 0,
               sizeof (struct twod_urb_sample *) * SAMPLE_QUEUE_SIZE);

        samp =
            kmalloc(sizeof (struct twod_urb_sample) * SAMPLE_QUEUE_SIZE,
                    GFP_KERNEL);
        if (!samp) {
                retval = -ENOMEM;
                goto error;
        }
        /* initialize the pointers to the samples */
        for (i = 0; i < SAMPLE_QUEUE_SIZE; ++i)
                dev->sampleq.buf[i] = samp++;
        EMPTY_CIRC_BUF(dev->sampleq);

        /*
         * In order to support throttling of the image urbs, we create
         * a circular buffer of the image urbs. The circular buffer
         * should be large enough to accept all the urbs in flight.
         */
        BUG_ON(IMG_URBS_IN_FLIGHT > IMG_URB_QUEUE_SIZE - 1);

        dev->img_urb_q.buf = 0;
        if (throttleRate > 0) {
                dev->img_urb_q.buf =
                    kmalloc(sizeof (struct urb *) * IMG_URB_QUEUE_SIZE,
                            GFP_KERNEL);
                if (!dev->img_urb_q.buf) {
                        retval = -ENOMEM;
                        goto error;
                }
                memset(dev->img_urb_q.buf, 0,
                       sizeof (struct urb *) * IMG_URB_QUEUE_SIZE);
                EMPTY_CIRC_BUF(dev->img_urb_q);
        }

        /* Allocate the image urbs and submit them */
        for (i = 0; i < IMG_URBS_IN_FLIGHT; ++i) {
                dev->img_urbs[i] = twod_make_img_urb(dev);
                if (!dev->img_urbs[i]) {
                        retval = -ENOMEM;
                        goto error;
                }
                if ((retval = usb_twod_submit_img_urb(dev, dev->img_urbs[i])) < 0)
                    goto error;
        }

        /* Allocate the shadow OR urbs and submit them */
        for (i = 0; i < SOR_URBS_IN_FLIGHT; ++i) {
            /* Only submit SOR in urbs if we have an SOR endpoint */
            if (dev->sor_in_endpointAddr) {
                    dev->sor_urbs[i] = twod_make_sor_urb(dev);
                    if (!dev->sor_urbs[i]) {
                            retval = -ENOMEM;
                            goto error;
                    }
                    if ((retval = usb_twod_submit_sor_urb(dev, dev->sor_urbs[i])) < 0)
                            goto error;
            }
            else dev->sor_urbs[i] = 0;
        }


        if (throttleRate > 0) {
                if (throttleRate > MAX_THROTTLE_RATE) {
                        KLOG_WARNING("%s: the max throttleRate is %d/s at the current values of MAX_THROTTLE_FUNC_RATE \
and IMG_URB_QUEUE_SIZE",
                                dev->dev_name, MAX_THROTTLE_RATE);
                        throttleRate = MAX_THROTTLE_RATE;
                }

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
                init_timer(&dev->urbThrottle);
                dev->urbThrottle.function = urb_throttle_func;
                dev->urbThrottle.data = (unsigned long)&dev->urbThrottle;
#else
                timer_setup(&dev->urbThrottle, urb_throttle_func, 0);
#endif

                /*
                 * Don't call the throttle timer function faster than MAX_THROTTLE_FUNC_RATE.
                 * Units:
                 *      HZ: jiffies/sec.
                 *      MAX_THROTTLE_FUNC_RATE: timer/sec
                 *      throttleJiffies: jiffies/timer
                 *      throttleRate: urb/sec
                 */
                BUG_ON(MAX_THROTTLE_FUNC_RATE > HZ);
                if (throttleRate < MAX_THROTTLE_FUNC_RATE) {
                        dev->throttleJiffies = HZ / throttleRate;
                        dev->nurbPerTimer = 1;
                }
                else {
                        dev->throttleJiffies = HZ / MAX_THROTTLE_FUNC_RATE;
                        dev->nurbPerTimer = min((int)(dev->throttleJiffies * throttleRate / HZ), IMG_URBS_IN_FLIGHT);
                }       

                dev->urbThrottle.expires = jiffies + dev->throttleJiffies;
                add_timer(&dev->urbThrottle);
        }

        /* Create a circular buffer of the true airspeed urbs for
         * periodic writing.
         */
        dev->tas_urb_q.buf =
            kmalloc(sizeof (struct urb *) * TAS_URB_QUEUE_SIZE,
                    GFP_KERNEL);
        if (!dev->tas_urb_q.buf) {
                retval = -ENOMEM;
                goto error;
        }
        memset(dev->tas_urb_q.buf, 0,
               sizeof (struct urb *) * TAS_URB_QUEUE_SIZE);
        EMPTY_CIRC_BUF(dev->tas_urb_q);

        /* Allocate urbs for queue */
        for (i = 0; i < TAS_URB_QUEUE_SIZE; ++i) {
                dev->tas_urb_q.buf[i] = twod_make_tas_urb(dev);
                if (!dev->tas_urb_q.buf[i]) {
                        retval = -ENOMEM;
                        goto error;
                }
        }
        for (i = 0; i < TAS_URB_QUEUE_SIZE - 1; ++i)
                INCREMENT_HEAD(dev->tas_urb_q, TAS_URB_QUEUE_SIZE);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
        init_timer(&dev->sendTASTimer);
        dev->sendTASTimer.function = send_tas_timer_func;
        dev->sendTASTimer.data = (unsigned long)&dev->sendTASTimer;
#else
        timer_setup(&dev->sendTASTimer, send_tas_timer_func, 0);
#endif
        /* save our object in the file's private structure */
        file->private_data = dev;

        KLOG_INFO("%s: open, throttleRate=%d\n", dev->dev_name,
             throttleRate);
        if (throttleRate > 0)
	    KLOG_INFO("%s: HZ=%d, throttleJiffies=%d, nurb=%d\n", dev->dev_name,
		HZ, dev->throttleJiffies,dev->nurbPerTimer);

        return 0;
error:
        /* unsuccessful open, give reference back */
        kref_put(&dev->kref, twod_dev_delete);
        return retval;
}

/* -------------------------------------------------------------------- */
static int twod_release(struct inode *inode, struct file *file)
{
        struct usb_twod *dev = (struct usb_twod *) file->private_data;
        int i;

        if (dev == NULL) return -ENODEV;

        KLOG_INFO("%s: Total images = %d\n",dev->dev_name, dev->stats.numImages);
        KLOG_INFO("%s: Lost images = %d\n",dev->dev_name, dev->stats.lostImages);
        KLOG_INFO("%s: Total SORs = %d\n",dev->dev_name, dev->stats.numSORs);
        KLOG_INFO("%s: Lost SORs = %d\n",dev->dev_name, dev->stats.lostSORs);
        KLOG_INFO("%s: Lost TASs = %d\n",dev->dev_name, dev->stats.lostTASs);
        KLOG_INFO("%s: urb errors = %d\n",dev->dev_name, dev->stats.urbErrors);
        KLOG_INFO("%s: shutdown indications = %d\n",dev->dev_name, dev->stats.shutdowns);
        KLOG_INFO("%s: urb timeouts = %d\n",dev->dev_name, dev->stats.urbTimeouts);

        if (throttleRate > 0) 
                del_timer_sync(&dev->urbThrottle);

        twod_set_sor_rate(dev, 0);

        // read_lock(&dev->usb_iface_lock);
        for (i = 0; i < IMG_URBS_IN_FLIGHT; ++i) {
                struct urb *urb = dev->img_urbs[i];
                if (urb) {
                        usb_kill_urb(urb);
#ifdef USE_DMA_COHERENT_FOR_IMG_URBS
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
                        usb_free_coherent(dev->udev, urb->transfer_buffer_length,
                                        urb->transfer_buffer, urb->transfer_dma);
#else
                        usb_buffer_free(dev->udev, urb->transfer_buffer_length,
                                        urb->transfer_buffer, urb->transfer_dma);
#endif
#else
                        kfree(urb->transfer_buffer);
#endif
                        usb_free_urb(urb);
                }
        }

        for (i = 0; i < SOR_URBS_IN_FLIGHT; ++i) {
                struct urb *urb = dev->sor_urbs[i];
                if (urb) {
                        usb_kill_urb(urb);
                        kfree(urb->transfer_buffer);
                        usb_free_urb(urb);
                }
        }

        if (dev->tas_urb_q.buf) {
                for (i = 0; i < TAS_URB_QUEUE_SIZE; ++i) {
                        struct urb *urb = dev->tas_urb_q.buf[i];
                        if (urb) {
                                usb_kill_urb(urb);
                                kfree(urb->transfer_buffer);
                                usb_free_urb(urb);
                        }
                }
        }
        // read_unlock(&dev->usb_iface_lock);

        twod_dev_free(dev);

        dev->is_open = 0;

        kref_put(&dev->kref,twod_dev_delete);

        return 0;
}

/* -------------------------------------------------------------------- */
static unsigned int twod_poll(struct file *file, poll_table * wait)
{
        struct usb_twod *dev = file->private_data;
        unsigned int mask = 0;
        poll_wait(file, &dev->read_wait, wait);
        if (dev->errorStatus != 0)
                mask |= POLLERR;
        if (dev->readstate.bytesLeft > 0 || GET_TAIL(dev->sampleq,SAMPLE_QUEUE_SIZE))
                mask |= POLLIN | POLLRDNORM;
        return mask;
}

/* -------------------------------------------------------------------- */
static ssize_t twod_read(struct file *file, char __user * buffer,
                         size_t count, loff_t * ppos)
{
        struct usb_twod *dev = (struct usb_twod *) file->private_data;
        ssize_t countreq = count;       // original request
        size_t n;
        struct twod_urb_sample *sample;
        size_t bytesLeft;
        char* dataPtr;
	//KLOG_INFO("%s reading \n",dev->dev_name);

        /*
         * We're not locking anything here to make sure dev still exists.
         * We assume that the system wouldn't allow a read on a closed
         * device, and since we're doing reference counting on
         * dev, it should be OK.
         */

        if (dev->errorStatus != 0) return dev->errorStatus;

        sample = dev->readstate.pendingSample;
        bytesLeft = dev->readstate.bytesLeft;
        dataPtr = dev->readstate.dataPtr;

        if (bytesLeft == 0
            && !GET_TAIL(dev->sampleq,SAMPLE_QUEUE_SIZE)) {
                if (file->f_flags & O_NONBLOCK) return -EAGAIN;
                if (wait_event_interruptible(dev->read_wait,
                    GET_TAIL(dev->sampleq,SAMPLE_QUEUE_SIZE))) {
                        return -ERESTARTSYS;
                }
        }
        // loop until user buffer is full (or no more samples)
        for ( ;; ) {
                // Check if there is data from previous urb to copy
                if ((n = min(count,bytesLeft)) > 0) {
                        int retval = 0;
                        if (copy_to_user(buffer,dataPtr, n)) return -EFAULT;
                        count -= n;
                        bytesLeft -= n;
                        buffer += n;
                        dataPtr += n;
                        /* user buffer filled, count==0 */
			if (bytesLeft > 0) break;

                        // we're finished with this sample, resubmit urb
                        switch (be32_to_cpu(sample->stype)) {
                        case TWOD_IMG_TYPE:
                        case TWOD_IMGv2_TYPE:
                        case TWOD_IMGv3_TYPE:
                                retval = usb_twod_submit_img_urb(dev,
                                        sample->urb);
                                break;
                        case TWOD_SOR_TYPE:
                        case TWOD_SORv3_TYPE:
                                retval = usb_twod_submit_sor_urb(dev,
                                        sample->urb);
                                break;
                        }
                        INCREMENT_TAIL(dev->sampleq,SAMPLE_QUEUE_SIZE);
                        if (retval) return retval;
                }
                /* Finished writing previous sample, check for next. 
                 * bytesLeft will be 0 here.
                 * If no more samples, then we're done
                 */
                sample = GET_TAIL(dev->sampleq,SAMPLE_QUEUE_SIZE);
                if (!sample) break;

                /* length of initial, non-urb portion of
                 * image or SOR samples.
                 */
                n = sample->pre_urb_len;
                /* if not enough room to copy initial portion,
		 * then we're done */
                if (count < n) break;

                /* copy initial portion to user space. */
                if (copy_to_user(buffer, sample, n)) return -EFAULT;
                count -= n;
                buffer += n;
                bytesLeft = sample->urb->actual_length;
                dataPtr = sample->urb->transfer_buffer;
        }
        dev->readstate.pendingSample = sample;
        dev->readstate.bytesLeft = bytesLeft;
        dev->readstate.dataPtr = dataPtr;
        return countreq - count;
}

static long twod_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
        struct usb_twod *dev = (struct usb_twod *) file->private_data;
        int retval = -EINVAL;

        if (_IOC_TYPE(cmd) != USB2D_IOC_MAGIC)
                return -ENOTTY;

        switch (cmd) {
        case USB2D_SET_TAS:
                {
                Tap2D tasValue;
                if (copy_from_user
                      ((char *) &tasValue, (const void __user *) arg,
                      sizeof (tasValue)) != 0)
                    retval = -EFAULT;
                else {
                    retval = 0;
                    spin_lock_bh(&dev->taslock);
                    if (dev->ptype == TWOD_64_V3) {
                        unsigned short *dp = (unsigned short *)&dev->tasValue;
                        unsigned short *sp = (unsigned short *)&tasValue;
                        dp[0] = cpu_to_le16(sp[0]);
                        dp[1] = cpu_to_le16(sp[1]);
                    }
                    else
                    {
                        dev->tasValue.ntap = cpu_to_le16(tasValue.ntap);
                        dev->tasValue.div10 = tasValue.div10;
                        dev->tasValue.cntr = 0;
                    }
                    spin_unlock_bh(&dev->taslock);
                  }
                }
                break;
        case USB2D_SET_SOR_RATE:
                {
                int sor_rate;
                if (copy_from_user
                   ((char *) &sor_rate, (const void __user *) arg,
                        sizeof (int)) != 0) retval = -EFAULT;
                else retval = twod_set_sor_rate(dev, sor_rate);
                }
                break;
       case USB2D_GET_STATUS:      /* user get of status struct */
                if (copy_to_user((void __user *)arg,&dev->stats,
                    sizeof(struct usb_twod_stats))) retval = -EFAULT;
                else retval = 0;
                break;
        default:
                retval = -ENOTTY;       // inappropriate ioctl for device
                break;
        }
	KLOG_DEBUG("%s: ioctl, retval=%d\n",dev->dev_name,retval);
        return retval;
}

/* -------------------------------------------------------------------- */
static struct file_operations twod_fops = {
        .owner = THIS_MODULE,
        .read = twod_read,
        .poll = twod_poll,
        .unlocked_ioctl = twod_ioctl,
        .open = twod_open,
        .release = twod_release,
        .llseek  = no_llseek,
};

/* 
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with devfs and the driver core
 */
static struct usb_class_driver usbtwod_64 = {
        .name = "usbtwod_64_%d",
        .fops = &twod_fops,
        .minor_base = USB_TWOD_64_MINOR_BASE,
};

static struct usb_class_driver usbtwod_32 = {
        .name = "usbtwod_32_%d",
        .fops = &twod_fops,
        .minor_base = USB_TWOD_32_MINOR_BASE,
};

/*
 * This is called in the context of the USB hub kernel thread
 * so it is legal to sleep and do GFP_KERNEL kmallocs.
 */
static int twod_probe(struct usb_interface *interface,
                      const struct usb_device_id *id)
{
        struct usb_twod *dev = NULL;
        struct usb_host_interface *iface_desc;
        struct usb_endpoint_descriptor *endpoint;
        int i;
        int retval = -ENOMEM;

        /* allocate memory for our device state and initialize it */
        /* note: no kzalloc in 2.6.11 */
        dev = (struct usb_twod*) kmalloc(sizeof (*dev), GFP_KERNEL);
        if (dev == NULL) return -ENOMEM;
        memset(dev,0,sizeof(*dev));

        /* fill in with complete info later */
        sprintf(dev->dev_name, "/dev/usbtwod_64");

        kref_init(&dev->kref);
        rwlock_init(&dev->usb_iface_lock);

        dev->udev = usb_get_dev(interface_to_usbdev(interface));
        dev->interface = interface;

        init_waitqueue_head(&dev->read_wait);
        spin_lock_init(&dev->sampqlock);

        spin_lock_init(&dev->taslock);

        /* set up the endpoint information */
        KLOG_INFO("idVendor: %x idProduct: %x, speed: %s, #alt_ifaces=%d\n",
                  id->idVendor, id->idProduct,
		((dev->udev->speed == USB_SPEED_LOW) ? "low (1.1 mbps)" :
		((dev->udev->speed == USB_SPEED_FULL) ? "full (12 mbps)" :
		((dev->udev->speed == USB_SPEED_HIGH) ? "high (480 mbps)" : "unknown"))),
                  interface->num_altsetting);
	
        iface_desc = interface->cur_altsetting;
        dev->ptype = TWOD_64; 

        if (id->idProduct == USB2D_64_V3_PRODUCT_ID)
        {
                dev->ptype = TWOD_64_V3;
        }
        else if (iface_desc->desc.bNumEndpoints == 2 && dev->udev->speed == USB_SPEED_FULL) {
                /*
                 * The 32 bit 2DP not longer exists. It was converted to 64 bit.
                 * So support for TWOD_32 could be removed from this driver
                 */
        	dev->ptype = TWOD_32;
        }

        /* scan endpoints to use for images, SOR, TAS. */
        for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		int psize,dir,type;
                int buflen = 0;
                const char* use = "";
                endpoint = &iface_desc->endpoint[i].desc;
		// wMaxPacketSize is little endian
                psize = le16_to_cpu(endpoint->wMaxPacketSize);
		dir = endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK;
		type = endpoint-> bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
                if (!dev->img_in_endpointAddr && dir == USB_DIR_IN &&
                           type == USB_ENDPOINT_XFER_BULK) {
			switch(dev->ptype) {
                        case TWOD_64_V3:
			case TWOD_64:
				/* we found a big bulk in endpoint on the USB2D_N0 64bit, use 
                                 * t for images */
				if (psize >= 512) {
					dev->img_in_endpointAddr = endpoint->bEndpointAddress;
                                        use = "64 bit images";
                                        buflen = TWOD_IMG_BUFF_SIZE;
                                }
				break;
			case TWOD_32:
				dev->img_in_endpointAddr = endpoint->bEndpointAddress;
                                use = "32 bit images";
                                buflen = TWOD_IMG_BUFF_SIZE;
				break;
			}
                } else if (!dev->sor_in_endpointAddr &&
			dir == USB_DIR_IN &&
			type == USB_ENDPOINT_XFER_BULK &&
			psize < 512 && psize >= 4) {
                        /*TWOD_SOR_BUFF_SIZE changed from 4 to 128 to accomidate houskeeping
                         * packet in v3. psize may have to be greater than the lesser of the two
                         * buffer sizes. Given that the houskeeping packet is of a variable size
                         * and TWOD_SOR_BUFF_SIZE is the max, min should be 4.
                         * We found a small bulk in endpoint, use it for the SOR */
                        dev->sor_in_endpointAddr =
                            endpoint->bEndpointAddress;
                        use = "SOR";
                        buflen = TWOD_SOR_BUFF_SIZE;
                }
                else if (!dev->tas_out_endpointAddr &&
                           dir == USB_DIR_OUT &&
                           type == USB_ENDPOINT_XFER_BULK) {
                        /* we found a bulk out endpoint for true air speed */
                        dev->tas_out_endpointAddr =
                            endpoint->bEndpointAddress;
                        use = "TAS";
                        buflen = TWOD_TAS_BUFF_SIZE;
                }

                if (strlen(use) > 0) {
                        KLOG_INFO("%s: endpoint for %s: dir=%s, type=%s, wMaxPackeSize=%d, buflen=%d\n",
                                dev->dev_name, use, ((dir == USB_DIR_IN) ? "IN" : "OUT"),
                                ((type  == USB_ENDPOINT_XFER_BULK) ? "BULK" : "OTHER"),
                                psize, buflen);
                }
                else {
                        KLOG_INFO("%s: endpoint not used, dir=%s,type=%s,wMaxPackeSize=%d\n",
                                dev->dev_name, ((dir == USB_DIR_IN) ? "IN" : "OUT"),
                                ((type  == USB_ENDPOINT_XFER_BULK) ? "BULK" : "OTHER"),
                                psize);
                }
        }

        if (!dev->img_in_endpointAddr || !dev->tas_out_endpointAddr) {
                KLOG_ERR("Could not find img-in or tas-out endpoints\n");
                retval = -ENOENT;
                goto error;
        }

        if ((dev->ptype == TWOD_64 || dev->ptype == TWOD_64_V3) && !dev->sor_in_endpointAddr) {
                KLOG_ERR("Could not find sor-in endpoint for 64 bit probe\n");
                retval = -ENOENT;
                goto error;
        }

        /* save our data pointer in this interface device */
        usb_set_intfdata(interface, dev);

        /* We can register the device now, as it is ready.
         * Then create device name for log messages.
         *
         * From: https://www.kernel.org/doc/html/v4.14/driver-api/usb/usb.html#c.usb_register_dev
         * If CONFIG_USB_DYNAMIC_MINORS is enabled, the minor number will be dynamically
         * allocated out of the list of available ones. If it is not enabled, the minor
         * number will be based on the next available free minor, starting at the
         * class_driver->minor_base.
         *
         * On Vortex, grep -F CONFIG_USB_DYNAMIC_MIN /boot/config-4.15.18-vortex86dx3  returns y.
         */
        switch(dev->ptype) {
                case TWOD_64_V3:
       	  	case TWOD_64:
            		retval = usb_register_dev(interface, &usbtwod_64);
                        sprintf(dev->dev_name, "/dev/usbtwod_64_%d (%x/%x)",
                                interface->minor, id->idVendor, id->idProduct);
 	    		break;
          	case TWOD_32:
 	    		retval = usb_register_dev(interface, &usbtwod_32);
                        sprintf(dev->dev_name, "/dev/usbtwod_32_%d (%x/%x)",
                                interface->minor, id->idVendor, id->idProduct);
        }

        if (retval) {
                /* something prevented us from registering this driver */
                KLOG_ERR("Not able to get a minor for this device.\n");
                usb_set_intfdata(interface, NULL);
                goto error;
        }
        KLOG_INFO("%s: %s connected\n", dev->dev_name, __func__);
        return 0;

error:
        /* this frees dev */
        kref_put(&dev->kref,twod_dev_delete);
        return retval;
}

static void twod_disconnect(struct usb_interface *interface)
{
        struct usb_twod *dev;

        /* prevent twod_open() from racing twod_disconnect() */
        TWOD_MUTEX_LOCK(&twod_open_lock);

        dev = usb_get_intfdata(interface);
        usb_set_intfdata(interface, NULL);

        /* give back our minor */
	switch(dev->ptype) {
	case TWOD_64_V3:
	case TWOD_64:
		usb_deregister_dev(interface, &usbtwod_64);
		break;
	case TWOD_32:
		usb_deregister_dev(interface, &usbtwod_32);
		break;
        }
        TWOD_MUTEX_UNLOCK(&twod_open_lock);

        dev->errorStatus = -ENOENT;
        wake_up_interruptible(&dev->read_wait);

        /* prevent more I/O from starting */
        write_lock_bh(&dev->usb_iface_lock);
        dev->interface = NULL;
        write_unlock_bh(&dev->usb_iface_lock);

        KLOG_INFO("%s: %s: disconnected\n", dev->dev_name, __func__);
        kref_put(&dev->kref, twod_dev_delete);
}

/* -------------------------------------------------------------------- */
static struct usb_driver twod_driver = {
        .name = "usbtwod",
        .id_table = twod_table,
        .probe = twod_probe,
        .disconnect = twod_disconnect,
};

static int __init usb_twod_init(void)
{
        int result = 0;

        KLOG_NOTICE("version: %s\n", REPO_REVISION);

        /* first bit set should be the same as last bit set for power of two */
        if (SAMPLE_QUEUE_SIZE <= 0 || ffs(SAMPLE_QUEUE_SIZE) != fls(SAMPLE_QUEUE_SIZE)) {
                KLOG_ERR("SAMPLE_QUEUE_SIZE=%d is not a power of 2\n",
                             SAMPLE_QUEUE_SIZE);
                return -EINVAL;
        }
        if (IMG_URB_QUEUE_SIZE <= 0 || ffs(IMG_URB_QUEUE_SIZE) != fls(IMG_URB_QUEUE_SIZE)) {
                KLOG_ERR("IMG_URB_QUEUE_SIZE=%d is not a power of 2\n",
                             IMG_URB_QUEUE_SIZE);
                return -EINVAL;
        }

        if (TAS_URB_QUEUE_SIZE <= 0 || ffs(TAS_URB_QUEUE_SIZE) != fls(TAS_URB_QUEUE_SIZE)) {
                KLOG_ERR("TAS_URB_QUEUE_SIZE=%d is not a power of 2\n",
                             TAS_URB_QUEUE_SIZE);
                return -EINVAL;
        }
        if (SAMPLE_QUEUE_SIZE < IMG_URBS_IN_FLIGHT + SOR_URBS_IN_FLIGHT + 1) {
                KLOG_ERR("SAMPLE_QUEUE_SIZE=%d should be greater than IMG_URBS_IN_FLIGHT(%d) + SOR_URBS_IN_FLIGHT(%d)\n",
                             SAMPLE_QUEUE_SIZE,IMG_URBS_IN_FLIGHT,SOR_URBS_IN_FLIGHT);
                return -EINVAL;
        }

        /* register this driver with the USB subsystem */
        result = usb_register(&twod_driver);
        if (result)
                KLOG_ERR("usb_register failed. Error number %d\n", result);

        KLOG_INFO("%s success", __func__);

        return result;
}

static void __exit usb_twod_exit(void)
{
        /* deregister this driver with the USB subsystem */
        usb_deregister(&twod_driver);
}

module_init(usb_twod_init);
module_exit(usb_twod_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Chris Webster <cjw@ucar.edu>");
MODULE_DESCRIPTION("USB PMS-2D Probe Driver");
MODULE_VERSION(REPO_REVISION);
