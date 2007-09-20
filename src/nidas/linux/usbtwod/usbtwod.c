/*
 * USB PMS-2D driver - 2.0
 *
 * Copyright (C) 2007 University Corporation for Atmospheric Research
 */

// #define DEBUG

#include <linux/kernel.h>
#include <linux/version.h>

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/usb.h>
#include <linux/poll.h>
#include <linux/timer.h>
#include <linux/moduleparam.h>

#include <nidas/linux/usbtwod/usbtwod.h>
#include <nidas/linux/klog.h>
#include <nidas/linux/irigclock.h>

/* Define these values to match your devices */
//#define USB_VENDOR_ID	0x2D2D
//#define USB_PRODUCT_ID	0x2D00


/* These are the default Cyprus EZ FX & FX2 ID's */
#define USB_VENDOR_ID         0x0547
#define USB_PRODUCT_ID        0x1002


/* table of devices that work with this driver */
static struct usb_device_id twod_table[] = {
        {USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID)},
        {}                      /* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, twod_table);

static int throttleRate = 0;
static int throttleJiffies = 0;

module_param(throttleRate, int, 0);

static struct usb_driver twod_driver;
static DECLARE_MUTEX(disconnect_sem);

/* -------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
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

        // there must be space, since TAIL was incremented before
        // when this urb was submitted.
        BUG_ON(CIRC_SPACE(dev->tas_urb_q.head, dev->tas_urb_q.tail,
                          TAS_URB_QUEUE_SIZE) == 0);
        INCREMENT_HEAD(dev->tas_urb_q, TAS_URB_QUEUE_SIZE);

        switch (urb->status) {
        case 0:
                break;
        case -ENOENT:
                // result of usb_kill_urb
		// KLOG_WARNING("urb->status=-ENOENT\n");
                dev->stats.urbErrors++;
                break;
        case -ECONNRESET:
                // urb has been unlinked (usb_unlink_urb) out from under us.
		KLOG_WARNING("urb->status=-ECONNRESET\n");
                dev->stats.urbErrors++;
                break;
        case -ESHUTDOWN:
                // Severe error in host controller, or the urb was submitted
                // after the device was disconnected
		KLOG_WARNING("urb->status=-ESHUTDOWN\n");
                dev->stats.urbErrors++;
                break;
        default:
		KLOG_WARNING("urb->status=%d\n",urb->status);
                dev->stats.urbErrors++;
                break;
        }
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
                KLOG_ERR("out of memory for TAS output buf");
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

/* Used by both irig callback or timer function to send the tas value via
 * the bulk write end-point
 */
static void write_tas(struct usb_twod *dev, int kmalloc_flags)
{
        if (dev->tas_urb_q.tail != dev->tas_urb_q.head) {
                struct urb *urb = dev->tas_urb_q.buf[dev->tas_urb_q.tail];
                memcpy(urb->transfer_buffer, &dev->tasValue,
                       TWOD_TAS_BUFF_SIZE);
                INCREMENT_TAIL(dev->tas_urb_q, TAS_URB_QUEUE_SIZE);

                if (usb_submit_urb(urb, kmalloc_flags) < 0 &&
                    !(dev->stats.urbErrors++ % 100))
                        KLOG_ERR("stats.urbErrors=%d",
                                 dev->stats.urbErrors);
        } else {
                if (!(dev->stats.lostTASs++ % 100))
                        KLOG_WARNING("no urbs available for TAS write, lostTASs=%d\n",
                             dev->stats.lostTASs);
        }
}

#ifdef DO_IRIG_TIMING
static void send_tas_callback(void *ptr)
{
        // This is an irig callback, which called from a work queue.
        struct usb_twod *dev = (struct usb_twod *) ptr;
        write_tas(dev, GFP_KERNEL);
}
#else
static void send_tas_timer_func(unsigned long arg)
{
        // Note that this runs in software interrupt context.
        struct usb_twod *dev = (struct usb_twod *) arg;
        write_tas(dev, GFP_ATOMIC);
        dev->sendTASTimer.expires += dev->sendTASJiffies;
        add_timer(&dev->sendTASTimer);  // reschedule
}
#endif

static int twod_set_sor_rate(struct usb_twod *dev, int rate)
{

#ifdef DO_IRIG_TIMING
        // If rate enumeration is IRIG_NUM_RATES, then rate is 0.
        enum irigClockRates irigRate = IRIG_NUM_RATES;
        if (rate > 0) {
                irigRate = irigClockRateToEnum(rate);
                if (irigRate == IRIG_NUM_RATES)
                        return -EINVAL;
        }

        if (dev->sorRate != IRIG_NUM_RATES && irigRate != dev->sorRate)
                unregister_irig_callback(send_tas_callback, dev->sorRate,
                                         dev);

        if (irigRate != IRIG_NUM_RATES && irigRate != dev->sorRate) {
                dev->sorRate = irigRate;
                register_irig_callback(send_tas_callback, irigRate, dev);
        }
#else
        if (rate > 0) {
                dev->sendTASJiffies = HZ / rate;
                if (dev->sendTASJiffies <= 0)
                        dev->sendTASJiffies = 1;
		KLOG_INFO("SOR rate=%d,jiffies=%d\n",rate,dev->sendTASJiffies);
                dev->sendTASTimer.function = send_tas_timer_func;
                dev->sendTASTimer.expires = jiffies + dev->sendTASJiffies;
                dev->sendTASTimer.data = (unsigned long) dev;
                add_timer(&dev->sendTASTimer);
        } else {
                if (dev->sendTASJiffies > 0)
                        del_timer(&dev->sendTASTimer);
                dev->sendTASJiffies = 0;
        }
#endif

        return 0;
}

/*
 * int mem_flags:
 *  GFP_ATOMIC:
 *      1. when called from urb callback handler,
 *          interrupt service routine, tasklet or a kernel timer callback.
 *      2. If you're holding a spinlock or rwlock (but not a semaphore).
 *      3. Current state is not TASK_RUNNING (i.e. current is non-null
 *         and driver has not changed the state).
 *  GFP_NOIO: block drivers.
 *  GFP_KERNEL: all other circumstances.
 */


/* -------------------------------------------------------------------- */
static void usb_twod_submit_img_urb(struct usb_twod *dev, struct urb *urb,
                                    int mem_flags)
{
        if (throttleRate > 0) {
                if (CIRC_SPACE(dev->img_urb_q.head, dev->img_urb_q.tail,
                               IMG_URB_QUEUE_SIZE) == 0)
                        err("programming error: no space in queue for resubmitting urbs");
                else {
                        dev->img_urb_q.buf[dev->img_urb_q.head] = urb;
                        INCREMENT_HEAD(dev->img_urb_q, IMG_URB_QUEUE_SIZE);
                }
        } else {
                int retval;
                retval = usb_submit_urb(urb, mem_flags);
                if (retval < 0 && !(dev->stats.urbErrors++ % 100))
                        err("%s stats.urbErrors=%d",
                            __FUNCTION__, dev->stats.urbErrors);
        }
}


/* -------------------------------------------------------------------- */
static void usb_twod_submit_sor_urb(struct usb_twod *dev, struct urb *urb,
                                    int mem_flags)
{
        int retval = usb_submit_urb(urb, mem_flags);
        if (retval < 0 && !(dev->stats.urbErrors++ % 100))
                err("%s stats.urbErrors=%d",
                    __FUNCTION__, dev->stats.urbErrors);
}


/* -------------------------------------------------------------------- */
static void urb_throttle_func(unsigned long arg)
{
        struct usb_twod *dev = (struct usb_twod *) arg;
        int retval;
#ifdef DEBUG
        static int debugcntr = 0;
#endif

        if (dev->img_urb_q.tail != dev->img_urb_q.head) {
                struct urb *urb = dev->img_urb_q.buf[dev->img_urb_q.tail];

#ifdef DEBUG
                if (!(debugcntr++ % 100))
                        KLOG_DEBUG("queue cnt=%d,jiffies=%ld\n",
                                   CIRC_CNT(dev->img_urb_q.head,
                                            dev->img_urb_q.tail,
                                            IMG_URB_QUEUE_SIZE), jiffies);
#endif

                // This is a timer function, running in software
                // interrupt context, so use GFP_ATOMIC.
                retval = usb_submit_urb(urb, GFP_ATOMIC);
                if (retval < 0 && !(dev->stats.urbErrors++ % 100))
                        err("%s stats.urbErrors=%d",
                            __FUNCTION__, dev->stats.urbErrors);
                INCREMENT_TAIL(dev->img_urb_q, IMG_URB_QUEUE_SIZE);
        } else {
                if (!(dev->stats.lostImages++ % 100))
                        KLOG_WARNING
                            ("no image urbs available for throttle function, lostImages=%d\n",
                             dev->stats.lostImages);
        }
        dev->urbThrottle.expires += throttleJiffies;
        add_timer(&dev->urbThrottle);   // reschedule myself
}

/* -------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
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

        switch (urb->status) {
        case 0:
                break;
        case -ENOENT:
                // result of usb_kill_urb, don't resubmit
		// KLOG_WARNING("urb->status=-ENOENT\n");
                dev->stats.urbErrors++;
                return;
        case -ECONNRESET:
                // urb has been unlinked (usb_unlink_urb) out from under us.
		KLOG_WARNING("urb->status=-ECONNRESET\n");
                dev->stats.urbErrors++;
                return;
        case -ESHUTDOWN:
                // Severe error in host controller, or the urb was submitted
                // after the device was disconnected
		KLOG_WARNING("urb->status=-ESHUTDOWN\n");
                dev->stats.urbErrors++;
                return;
        default:
		KLOG_WARNING("urb->status=%d\n",urb->status);
                dev->stats.urbErrors++;
                goto resubmit;  /* maybe we can recover */
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
                        KLOG_WARNING("sample queue full: lost images=%d\n",
                             dev->stats.lostImages);
                // resubmit the urb (current data is lost)
                goto resubmit;
        } else {
                osamp->timetag = getSystemTimeMsecs();
                osamp->length = sizeof(osamp->stype) +
				sizeof(osamp->data) +
				urb->actual_length;
                osamp->stype = cpu_to_be32(TWOD_IMG_TYPE);
                // stuff the current TAS value in the data.
                memcpy(&osamp->data, &dev->tasValue, sizeof(Tap2D));
                osamp->pre_urb_len = sizeof(osamp->timetag) +
			sizeof(osamp->length) +
			sizeof(osamp->stype) +
			sizeof(osamp->data);
                osamp->urb = urb;
                INCREMENT_HEAD(dev->sampleq, SAMPLE_QUEUE_SIZE);
                spin_unlock(&dev->sampqlock);

		if (((long)jiffies - (long)dev->lastWakeup) > dev->latencyJiffies ||
                        CIRC_CNT(dev->sampleq.head,dev->sampleq.tail,SAMPLE_QUEUE_SIZE) >=
				SOR_URBS_IN_FLIGHT) {
                        wake_up_interruptible(&dev->read_wait);
                        dev->lastWakeup = jiffies;
                }
        }
        return;
resubmit:
        // called from a urb completion handler, so use GFP_ATOMIC
        usb_twod_submit_img_urb(dev, urb, GFP_ATOMIC);
}

/* -------------------------------------------------------------------- */
static struct urb *twod_make_img_urb(struct usb_twod *dev)
{
        struct urb *urb;

        // We're doing GFP_KERNEL memory allocations, so it is a
        // bug if this is running from interrupt context.
        BUG_ON(in_interrupt());

        urb = usb_alloc_urb(0, GFP_KERNEL);
        if (!urb)
                return urb;

        if (!
            (urb->transfer_buffer =
             usb_buffer_alloc(dev->udev, TWOD_IMG_BUFF_SIZE, GFP_KERNEL,
                              &urb->transfer_dma))) {
                err("%s - out of memory for read buf", __FUNCTION__);
                usb_free_urb(urb);
                urb = NULL;
                return urb;
        }

        urb->transfer_flags = URB_NO_TRANSFER_DMA_MAP;

        usb_fill_bulk_urb(urb, dev->udev,
                          usb_rcvbulkpipe(dev->udev,
                                          dev->img_in_endpointAddr),
                          urb->transfer_buffer, TWOD_IMG_BUFF_SIZE,
                          twod_img_rx_bulk_callback, dev);
        return urb;
}

/* -------------------------------------------------------------------- */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
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

        switch (urb->status) {
        case 0:
                break;
        case -ENOENT:
                // result of usb_kill_urb, don't resubmit
		// KLOG_WARNING("urb->status=-ENOENT\n");
                dev->stats.urbErrors++;
                return;
        case -ECONNRESET:
                // urb has been unlinked (usb_unlink_urb) out from under us.
		KLOG_WARNING("urb->status=-ECONNRESET\n");
                dev->stats.urbErrors++;
                return;
        case -ESHUTDOWN:
                // Severe error in host controller, or the urb was submitted
                // after the device was disconnected
		twod_set_sor_rate(dev, 0);
		KLOG_WARNING("urb->status=-ESHUTDOWN\n");
                dev->stats.urbErrors++;
                return;
        default:
		KLOG_WARNING("urb->status=%d\n",urb->status);
                dev->stats.urbErrors++;
                return;
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
                        KLOG_WARNING("overflow: lost SOR urbs = %d\n",
				dev->stats.lostSORs);
                // resubmit the urb (current data is lost)
                goto resubmit;
        } else {
                osamp->timetag = getSystemTimeMsecs();
                osamp->length = sizeof(osamp->stype) +
				urb->actual_length;
                osamp->stype = cpu_to_be32(TWOD_SOR_TYPE);
                osamp->pre_urb_len = sizeof(osamp->timetag) +
			sizeof(osamp->length) +
			sizeof(osamp->stype);
                osamp->urb = urb;
                INCREMENT_HEAD(dev->sampleq, SAMPLE_QUEUE_SIZE);
                spin_unlock(&dev->sampqlock);
		if (((long)jiffies - (long)dev->lastWakeup) > dev->latencyJiffies ||
                        CIRC_CNT(dev->sampleq.head,dev->sampleq.tail,SAMPLE_QUEUE_SIZE) >=
				SOR_URBS_IN_FLIGHT) {
                        wake_up_interruptible(&dev->read_wait);
                        dev->lastWakeup = jiffies;
                }
        }
        return;
resubmit:
	// called from a urb completion handler, so use GFP_ATOMIC
	usb_twod_submit_sor_urb(dev, urb, GFP_ATOMIC);
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
                err("%s - out of memory for read buf", __FUNCTION__);
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
static void twod_delete(struct usb_twod *dev)
{
        usb_put_dev(dev->udev);
        kfree(dev);
}

/* -------------------------------------------------------------------- */
static int twod_open(struct inode *inode, struct file *file)
{
        struct usb_twod *dev;
        struct usb_interface *interface;
        struct twod_urb_sample *samp;
        int subminor;
        int i, retval = 0;

        nonseekable_open(inode, file);
        subminor = iminor(inode);

        down(&disconnect_sem);

        interface = usb_find_interface(&twod_driver, subminor);
        if (!interface) {
                err("%s - error, can't find device for minor %d",
                    __FUNCTION__, subminor);
                retval = -ENODEV;
                goto unlock_disconnect_exit;
        }

        dev = usb_get_intfdata(interface);
        if (!dev) {
                retval = -ENODEV;
                goto unlock_disconnect_exit;
        }

        /* lock this device */
        if (down_interruptible(&dev->sem)) {
                retval = -ERESTARTSYS;
                goto unlock_disconnect_exit;
        }

        if (dev->is_open) {
                retval = -EBUSY;
                goto exit;
        }
        dev->is_open = 1;

        memset(&dev->stats, 0, sizeof (dev->stats));
        memset(&dev->readstate, 0, sizeof (dev->readstate));
        dev->debug_cntr = 0;

	dev->latencyJiffies = 250 * HZ / MSECS_PER_SEC;
	dev->lastWakeup = jiffies;

        /* save our object in the file's private structure */
        file->private_data = dev;

        /* allocate the sample circular buffer */
        dev->sampleq.head = dev->sampleq.tail = 0;
        dev->sampleq.buf =
            kmalloc(sizeof (struct twod_urb_sample *) * SAMPLE_QUEUE_SIZE,
                    GFP_KERNEL);
        if (!dev->sampleq.buf) {
                retval = -ENOMEM;
                goto exit;
        }
        memset(dev->sampleq.buf, 0,
               sizeof (struct twod_urb_sample *) * SAMPLE_QUEUE_SIZE);
        samp =
            kmalloc(sizeof (struct twod_urb_sample) * SAMPLE_QUEUE_SIZE,
                    GFP_KERNEL);
        if (!samp) {
                retval = -ENOMEM;
                goto exit;
        }
        /* initialize the pointers to the samples */
        for (i = 0; i < SAMPLE_QUEUE_SIZE; ++i)
                dev->sampleq.buf[i] = samp++;

        /* In order to support throttling of the image urbs, we create
         * a circular buffer of the image urbs.
         */
        dev->img_urb_q.buf = 0;
        if (throttleRate > 0) {
                dev->img_urb_q.head = dev->img_urb_q.tail = 0;
                dev->img_urb_q.buf =
                    kmalloc(sizeof (struct urb *) * IMG_URB_QUEUE_SIZE,
                            GFP_KERNEL);
                if (!dev->img_urb_q.buf) {
                        retval = -ENOMEM;
                        goto exit;
                }
                memset(dev->img_urb_q.buf, 0,
                       sizeof (struct urb *) * IMG_URB_QUEUE_SIZE);
        }

        /* Allocate the image urbs and submit them */
        for (i = 0; i < IMG_URBS_IN_FLIGHT; ++i) {
                dev->img_urbs[i] = twod_make_img_urb(dev);
                if (!dev->img_urbs[i])
                        return -ENOMEM;
                usb_twod_submit_img_urb(dev, dev->img_urbs[i], GFP_KERNEL);
        }

        /* Allocate the shadow OR urbs and submit them */
        for (i = 0; i < SOR_URBS_IN_FLIGHT; ++i) {
            /* Only submit sor in urbs if we have an SOR endpoint */
            if (dev->sor_in_endpointAddr) {
                    dev->sor_urbs[i] = twod_make_sor_urb(dev);
                    if (!dev->sor_urbs[i])
                            return -ENOMEM;
                    usb_twod_submit_sor_urb(dev, dev->sor_urbs[i], GFP_KERNEL);
            }
            else dev->sor_urbs[i] = 0;
        }

        init_timer(&dev->urbThrottle);

        if (throttleRate > 0) {
                throttleJiffies = HZ / throttleRate;
                if (throttleJiffies < 0)
                        throttleJiffies = 1;
                dev->urbThrottle.function = urb_throttle_func;
                dev->urbThrottle.expires = jiffies + throttleJiffies;
                dev->urbThrottle.data = (unsigned long) dev;
                add_timer(&dev->urbThrottle);
        }

        /* Create a circular buffer of the true airspeed urbs for
         * periodic writing.
         */
        dev->tas_urb_q.head = dev->tas_urb_q.tail = 0;
        dev->tas_urb_q.buf =
            kmalloc(sizeof (struct urb *) * TAS_URB_QUEUE_SIZE,
                    GFP_KERNEL);
        if (!dev->tas_urb_q.buf) {
                retval = -ENOMEM;
                goto exit;
        }
        memset(dev->tas_urb_q.buf, 0,
               sizeof (struct urb *) * TAS_URB_QUEUE_SIZE);

        /* Allocate urbs for queue */
        for (i = 0; i < TAS_URB_QUEUE_SIZE; ++i) {
                dev->tas_urb_q.buf[i] = twod_make_tas_urb(dev);
                if (!dev->tas_urb_q.buf[i])
                        return -ENOMEM;
        }
        for (i = 0; i < TAS_URB_QUEUE_SIZE-1; ++i)
                INCREMENT_HEAD(dev->tas_urb_q, TAS_URB_QUEUE_SIZE);

#ifndef DO_IRIG_TIMING
        init_timer(&dev->sendTASTimer);
#endif

        KLOG_INFO("USB PMS-2D device now opened, throttleRate=%d\n",
             throttleRate);

exit:
        up(&dev->sem);

unlock_disconnect_exit:
        up(&disconnect_sem);
        return retval;
}

/* -------------------------------------------------------------------- */
static int twod_release(struct inode *inode, struct file *file)
{
        struct usb_twod *dev = (struct usb_twod *) file->private_data;
        int i, retval = 0;

        if (dev == NULL) {
                return -ENODEV;
                goto exit;
        }

        KLOG_INFO("Total images = %d\n",dev->stats.numImages);
        KLOG_INFO("Lost images = %d\n",dev->stats.lostImages);
        KLOG_INFO("Total SORs = %d\n",dev->stats.numSORs);
        KLOG_INFO("Lost SORs = %d\n",dev->stats.lostSORs);
        KLOG_INFO("Lost TASs = %d\n",dev->stats.lostTASs);
        KLOG_INFO("urb errors = %d\n",dev->stats.urbErrors);

        del_timer_sync(&dev->urbThrottle);

        wake_up_interruptible(&dev->read_wait);

        twod_set_sor_rate(dev, 0);

        for (i = 0; i < IMG_URBS_IN_FLIGHT; ++i) {
                struct urb *urb = dev->img_urbs[i];
                if (urb) {
                        usb_kill_urb(urb);
                        usb_buffer_free(dev->udev, urb->transfer_buffer_length,
                                        urb->transfer_buffer, urb->transfer_dma);
                        usb_free_urb(urb);
                }
        }

        if (dev->img_urb_q.buf) {
                kfree(dev->img_urb_q.buf[0]);
                kfree(dev->img_urb_q.buf);
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
                kfree(dev->tas_urb_q.buf);
                dev->tas_urb_q.buf = 0;
        }

        if (dev->sampleq.buf)
                kfree(dev->sampleq.buf[0]);
        kfree(dev->sampleq.buf);
        dev->sampleq.buf = 0;

        if (down_interruptible(&dev->sem)) {
                retval = -ERESTARTSYS;
                goto exit;
        }

        if (dev->is_open == 0) {
                retval = -ENODEV;
                goto unlock_exit;
        }

        dev->is_open = 0;

unlock_exit:
        up(&dev->sem);

exit:
        return retval;
}

/* -------------------------------------------------------------------- */
static unsigned int twod_poll(struct file *file, poll_table * wait)
{
        struct usb_twod *dev = file->private_data;
        unsigned int mask = 0;
        poll_wait(file, &dev->read_wait, wait);
        if (dev->readstate.bytesLeft > 0
            || dev->sampleq.head != dev->sampleq.tail)
                mask |= POLLIN | POLLRDNORM;
        return mask;
}

/* -------------------------------------------------------------------- */
static ssize_t twod_read(struct file *file, char __user * buffer,
                         size_t count, loff_t * ppos)
{
        struct usb_twod *dev;
        ssize_t retval = 0;
        ssize_t countreq = count;       // original request
        size_t n;
        struct twod_urb_sample *sample;

        if (count == 0)
                return count;

        dev = (struct usb_twod *) file->private_data;

        /* lock this object */
        if (down_interruptible(&dev->sem))
                return -ERESTARTSYS;

        /* verify that the device wasn't unplugged */
        if (dev->interface == NULL) {
                retval = -ENODEV;
                err("No device or device unplugged %d", retval);
                goto unlock_exit;
        }
        if (dev->readstate.bytesLeft == 0
            && dev->sampleq.tail == dev->sampleq.head) {
                if (file->f_flags & O_NONBLOCK) {
                        retval = -EAGAIN;
                        goto unlock_exit;
                }
                retval = wait_event_interruptible(dev->read_wait,
                                                  dev->sampleq.tail !=
                                                  dev->sampleq.head);
                if (retval < 0)
                        goto unlock_exit;
        }
        // loop until user buffer is full (or no more samples)
        for (;;) {
                if (count == 0) {
                        retval = countreq;
                        break;
                }
// #define DEBUG

#ifdef DEBUG
                if (!(dev->debug_cntr % 100))
                        KLOG_DEBUG("head=%d,tail=%d,urbs=%d,count=%d,bytesLeft=%d",
                        dev->sampleq.head, dev->sampleq.tail,
                        CIRC_CNT(dev->sampleq.head, dev->sampleq.tail,
                            SAMPLE_QUEUE_SIZE),
                        count, dev->readstate.bytesLeft);
#endif

                // Check if there is data from previous urb to copy
                if (dev->readstate.bytesLeft > 0) {
                        n = min(count, dev->readstate.bytesLeft);
                        if (copy_to_user
                            (buffer, dev->readstate.dataPtr, n)) {
                                err("%s - copy_to_user failed - %d",
                                    __FUNCTION__, retval);
                                retval = -EFAULT;
                                break;
                        }
                        count -= n;
                        dev->readstate.bytesLeft -= n;
                        buffer += n;
                        dev->readstate.dataPtr += n;
			if (dev->readstate.bytesLeft == 0) {
				// we're finished with this sample
				switch (be32_to_cpu(dev->readstate.pendingSample->stype)) {
				case TWOD_IMG_TYPE:
					usb_twod_submit_img_urb(dev,
						dev->readstate.pendingSample->urb,GFP_KERNEL);
					break;
				case TWOD_SOR_TYPE:
					usb_twod_submit_sor_urb(dev,
						dev->readstate.pendingSample->urb,GFP_KERNEL);
					break;
				}
				INCREMENT_TAIL(dev->sampleq, SAMPLE_QUEUE_SIZE);
			}
                        if (count == 0) {
                                // if count is 0, we're done.
                                retval = countreq;
                                break;
                        }
                }
                /* Finished writing previous sample, check for next. 
                 * dev->readstate.bytesLeft will be 0 here.
                 * If no more samples, then we're done
                 */
                if (dev->sampleq.tail == dev->sampleq.head) {
                        retval = countreq - count;
                        break;
                }
                sample = dev->sampleq.buf[dev->sampleq.tail];

                /* length of initial, non-urb portion of
                 * image or SOR samples.
                 */
                n = sample->pre_urb_len;
                /* if not enough room to copy initial portion,
		 * then we're done */
                if (count < n) {
                        retval = countreq - count;
                        break;
                }

                /* copy initial portion to user space. */
                if (copy_to_user(buffer, sample, n)) {
                        err("%s - copy_to_user failed - %d", __FUNCTION__,
                            retval);
                        retval = -EFAULT;
                        break;
                }
                count -= n;
                buffer += n;
                dev->readstate.pendingSample = sample;
                dev->readstate.bytesLeft = sample->urb->actual_length;
                dev->readstate.dataPtr = sample->urb->transfer_buffer;

#ifdef DEBUG
                if (!(dev->debug_cntr % 100))
                        KLOG_DEBUG("bottom count=%d,bytesLeft=%d",
                                   count, dev->readstate.bytesLeft);
#endif
        }

unlock_exit:
        up(&dev->sem);

#ifdef DEBUG
        if (!(dev->debug_cntr++ % 100))
                KLOG_DEBUG("retval=%d\n", retval);
#endif

#undef DEBUG
        return retval;
}

static int twod_ioctl(struct inode *inode, struct file *file,
                      unsigned int cmd, unsigned long arg)
{
        struct usb_twod *dev = (struct usb_twod *) file->private_data;
        int retval = -EINVAL;

        if (_IOC_TYPE(cmd) != USB2D_IOC_MAGIC)
                return -ENOTTY;

        switch (cmd) {
        case USB2D_SET_TAS:
                if (copy_from_user
                    ((char *) &dev->tasValue, (const void __user *) arg,
                     sizeof (dev->tasValue)) != 0) retval = -EFAULT;
                else retval = 0;
                break;
        case USB2D_SET_SOR_RATE:
                {
                        int sor_rate;
                        if (copy_from_user
                            ((char *) &sor_rate, (const void __user *) arg,
                             sizeof (int)) != 0) retval = -EFAULT;
                        else retval = twod_set_sor_rate(dev, sor_rate);
			KLOG_DEBUG("SET_SOR_RATE, rate=%d\n",sor_rate);
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
	KLOG_DEBUG("ioctl, retval=%d\n",retval);
        return retval;
}

/* -------------------------------------------------------------------- */
static struct file_operations twod_fops = {
        .owner = THIS_MODULE,
        .read = twod_read,
        .poll = twod_poll,
        .ioctl = twod_ioctl,
        .open = twod_open,
        .release = twod_release,
};

/* 
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with devfs and the driver core
 */
static struct usb_class_driver twod_class = {
        .name = "usbtwod%d",
        .fops = &twod_fops,
        .minor_base = USB_TWOD_MINOR_BASE,
};

static int twod_probe(struct usb_interface *interface,
                      const struct usb_device_id *id)
{
        struct usb_twod *dev = NULL;
        struct usb_host_interface *iface_desc;
        struct usb_endpoint_descriptor *endpoint;
        int i;
        int retval = -ENOMEM;

        /* allocate memory for our device state and initialize it */
        dev = kmalloc(sizeof (*dev), GFP_KERNEL);
        if (dev == NULL) {
                err("Out of memory");
                goto error;
        }
        memset(dev, 0x00, sizeof (*dev));

        dev->interface = interface;
        init_waitqueue_head(&dev->read_wait);
        init_MUTEX(&dev->sem);
        spin_lock_init(&dev->sampqlock);

        dev->sorRate = IRIG_NUM_RATES;

        dev->udev = usb_get_dev(interface_to_usbdev(interface));

        /* set up the endpoint information */

        KLOG_INFO("number of alternate interfaces: %d\n",
                  interface->num_altsetting);

        /* use the first sor-in and tas-out endpoints */
        /* use the second ing_in endpoint */
        iface_desc = interface->cur_altsetting;
        // KLOG_INFO("%d endpoints in probe\n", iface_desc->desc.bNumEndpoints);

        for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		int psize,dir,type;
                endpoint = &iface_desc->endpoint[i].desc;
		// wMaxPacketSize is little endian
                psize = le16_to_cpu(endpoint->wMaxPacketSize);
		dir = endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK;
		type = endpoint-> bmAttributes & USB_ENDPOINT_XFERTYPE_MASK;
                KLOG_INFO("endpoint %d, dir=%s,type=%s,wMaxPackeSize=%d\n",
                          i, ((dir == USB_DIR_IN) ? "IN" : "OUT"),
                          ((type  == USB_ENDPOINT_XFER_BULK) ? "BULK" : "OTHER"),
				psize);

                if (!dev->sor_in_endpointAddr &&
			dir == USB_DIR_IN &&
			type == USB_ENDPOINT_XFER_BULK &&
			psize < 512 && psize >= TWOD_SOR_BUFF_SIZE) {
                        /* we found a small bulk in endpoint, use it for the SOR */
                        dev->sor_in_endpointAddr =
                            endpoint->bEndpointAddress;
                        dev->sor_bulk_in_size = psize;
                } else if (!dev->img_in_endpointAddr &&
                           dir == USB_DIR_IN &&
                           type == USB_ENDPOINT_XFER_BULK &&
                           psize >= 512) {
                        /* we found a big bulk in endpoint, use it for images */
                        dev->img_in_endpointAddr =
                            endpoint->bEndpointAddress;
                        dev->img_bulk_in_size = psize;
                } else if (!dev->tas_out_endpointAddr &&
                           dir == USB_DIR_OUT &&
                           type == USB_ENDPOINT_XFER_BULK) {
                        /* we found a bulk out endpoint for true air speed */
                        dev->tas_out_endpointAddr =
                            endpoint->bEndpointAddress;
                }

        }

        if (!dev->img_in_endpointAddr || !dev->tas_out_endpointAddr) {
                KLOG_ERR("Could not find both img-in and tas-out endpoints");
                goto error;
        }

        if (!dev->sor_in_endpointAddr)
                KLOG_WARNING("Could not find sor-in endpoint. Will not read SOR samples");

        /* save our data pointer in this interface device */
        usb_set_intfdata(interface, dev);

        /* we can register the device now, as it is ready */
        retval = usb_register_dev(interface, &twod_class);
        if (retval) {
                /* something prevented us from registering this driver */
                err("%s - Not able to get a minor for this device.",
                    __FUNCTION__);
                usb_set_intfdata(interface, NULL);
                goto error;
        }
        /* let the user know what node this device is now attached to */
        KLOG_INFO("USB PMS-2D device now attached to USBtwod-%d\n",
                  interface->minor);
        return 0;

      error:
        if (dev)
                twod_delete(dev);
        return retval;
}

static void twod_disconnect(struct usb_interface *interface)
{
        struct usb_twod *dev;
        int minor = interface->minor;

        down(&disconnect_sem);

        dev = usb_get_intfdata(interface);

	twod_set_sor_rate(dev,0);
	
        usb_set_intfdata(interface, NULL);

        down(&dev->sem);

        /* give back our minor */
        usb_deregister_dev(interface, &twod_class);

        up(&dev->sem);

        wake_up_interruptible(&dev->read_wait);

        twod_delete(dev);

        up(&disconnect_sem);

        KLOG_INFO("USB PMS-2D #%d now disconnected\n", minor);
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
        int i;

        /* Check that circular buffer sizes are a power of 2 */
        for (i = SAMPLE_QUEUE_SIZE; i != 1; i >>= 1)
                if (i % 2) {
                        KLOG_ERR
                            ("SAMPLE_QUEUE_SIZE =%d is not a power of 2\n",
                             SAMPLE_QUEUE_SIZE);
                        return -EINVAL;
                }

        for (i = IMG_URB_QUEUE_SIZE; i != 1; i >>= 1)
                if (i % 2) {
                        KLOG_ERR
                            ("IMG_URB_QUEUE_SIZE =%d is not a power of 2\n",
                             IMG_URB_QUEUE_SIZE);
                        return -EINVAL;
                }

        for (i = TAS_URB_QUEUE_SIZE; i != 1; i >>= 1)
                if (i % 2) {
                        KLOG_ERR
                            ("TAS_URB_QUEUE_SIZE =%d is not a power of 2\n",
                             TAS_URB_QUEUE_SIZE);
                        return -EINVAL;
                }

        /* register this driver with the USB subsystem */
        result = usb_register(&twod_driver);
        if (result)
                err("usbtwod_register failed. Error number %d", result);

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
