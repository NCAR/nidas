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
#define NCAR_VENDOR_ID         0x2D2D
#define USB2D_64_PRODUCT_ID    0x2D00
#define USB2D_32_PRODUCT_ID    0x2D01

/* These are the default Cyprus EZ FX & FX2 ID's */
//#define NCAR_VENDOR_ID         0x0547
//#define USB2D_64_PRODUCT_ID  0x1003
//#define USB2D_32_PRODUCT_ID  0x1002

/* table of devices that work with this driver */
static struct usb_device_id twod_table[] = {
        {USB_DEVICE(NCAR_VENDOR_ID, USB2D_64_PRODUCT_ID)},
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

static int throttleRate = 0;
static int throttleJiffies = 0;

module_param(throttleRate, int, 0);

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
                dev->consecTimeouts = 0;
                break;
        case -ENOENT:
                // result of usb_kill_urb
		KLOG_WARNING("%s: urb->status=-ENOENT\n",dev->dev_name);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
                break;
        case -ECONNRESET:
                // urb has been unlinked (usb_unlink_urb) out from under us.
		KLOG_WARNING("%s: urb->status=-ECONNRESET\n", dev->dev_name);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
                break;
        case -ESHUTDOWN:
                // Severe error in host controller, or the urb was submitted
                // after the device was disconnected
		KLOG_WARNING("%s: urb->status=-ESHUTDOWN\n", dev->dev_name);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
                break;
        case -ETIMEDOUT:
                KLOG_WARNING("%s: urb->status=-ETIMEDOUT\n", dev->dev_name);
                dev->stats.urbTimeouts++;
                if (dev->consecTimeouts++ >= 10)
                    dev->errorStatus = urb->status;
		break;
        default:
		KLOG_WARNING("%s: urb->status=%d\n",dev->dev_name, urb->status);
                dev->stats.urbErrors++;
		dev->errorStatus = urb->status;
		return;
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
                KLOG_ERR("%s: out of memory for TAS output buf\n", dev->dev_name);
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
 * the bulk write end-point. Therefore it can be called 
 * in interrupt or non-interrupt mode.
 */
static int write_tas(struct usb_twod *dev, int kmalloc_flags)
{
        int retval = 0;
        if (dev->tas_urb_q.tail != dev->tas_urb_q.head) {
                struct urb *urb = dev->tas_urb_q.buf[dev->tas_urb_q.tail];
		dev->tasValue.cntr %= 10;
		dev->tasValue.cntr++;
		
                memcpy(urb->transfer_buffer, &dev->tasValue,
                       TWOD_TAS_BUFF_SIZE);
                INCREMENT_TAIL(dev->tas_urb_q, TAS_URB_QUEUE_SIZE);

                read_lock(&dev->usb_iface_lock);
                if (dev->interface)         /* check if disconnect() was called */
                        retval = usb_submit_urb(urb, GFP_ATOMIC);
                else
                        retval = -ENODEV;
                read_unlock(&dev->usb_iface_lock);
                if (retval < 0) {
                        dev->stats.urbErrors++;
                        KLOG_ERR("%s: retval=%d, stats.urbErrors=%d\n",
                                dev->dev_name,retval,dev->stats.urbErrors);
			dev->errorStatus = retval;
                }
        } else {
                if (!(dev->stats.lostTASs++ % 100))
                        KLOG_WARNING("%s: no urbs available for TAS write, lostTASs=%d\n",
                             dev->dev_name, dev->stats.lostTASs);
        }
        return retval;
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
		KLOG_INFO("%s: SOR rate=%d,jiffies=%d\n",dev->dev_name,rate,dev->sendTASJiffies);
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
static int usb_twod_submit_img_urb(struct usb_twod *dev, struct urb *urb,
                                    int mem_flags)
{
        int retval;
        if (throttleRate > 0) {
                if (CIRC_SPACE(dev->img_urb_q.head, dev->img_urb_q.tail,
                               IMG_URB_QUEUE_SIZE) == 0)
                        KLOG_ERR("%s: programming error: no space in queue for resubmitting urbs\n", dev->dev_name);
                else {
                        dev->img_urb_q.buf[dev->img_urb_q.head] = urb;
                        INCREMENT_HEAD(dev->img_urb_q, IMG_URB_QUEUE_SIZE);
                }
                return 0;
        }

        read_lock(&dev->usb_iface_lock);
        if (dev->interface)         /* disconnect() was called */
                retval = usb_submit_urb(urb, GFP_ATOMIC);
        else retval = -ENODEV;
        read_unlock(&dev->usb_iface_lock);
        if (retval < 0) {
                dev->stats.urbErrors++;
                KLOG_ERR("%s: retval=%d, stats.urbErrors=%d\n",
                    dev->dev_name,retval,dev->stats.urbErrors);
        }
        return retval;
}


/* -------------------------------------------------------------------- */
static int usb_twod_submit_sor_urb(struct usb_twod *dev, struct urb *urb,
                                    int mem_flags)
{
        int retval;
        read_lock(&dev->usb_iface_lock);
        if (dev->interface)         /* disconnect() was called */
                retval = usb_submit_urb(urb, GFP_ATOMIC);
        else retval = -ENODEV;
        read_unlock(&dev->usb_iface_lock);
        if (retval < 0) {
                dev->stats.urbErrors++;
                KLOG_ERR("%s: retval=%d, stats.urbErrors=%d\n",
                    dev->dev_name,retval,dev->stats.urbErrors);
        }
        return retval;
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
                        KLOG_DEBUG("%s: queue cnt=%d,jiffies=%ld\n",
                                   	dev->dev_name, 
					CIRC_CNT(dev->img_urb_q.head,
                                        dev->img_urb_q.tail,
                                        IMG_URB_QUEUE_SIZE), jiffies);
#endif

                // This is a timer function, running in software
                // interrupt context, so use GFP_ATOMIC.
                read_lock(&dev->usb_iface_lock);
                if (dev->interface)         /* check if disconnect() was called */
                        retval = usb_submit_urb(urb, GFP_ATOMIC);
                else
                        retval = -ENODEV;
                read_unlock(&dev->usb_iface_lock);
                if (retval < 0) {
                        dev->stats.urbErrors++;
                        KLOG_ERR("%s: retval=%d, stats.urbErrors=%d\n",
                                dev->dev_name,retval,dev->stats.urbErrors);
                        dev->errorStatus = retval;
                }
                INCREMENT_TAIL(dev->img_urb_q, IMG_URB_QUEUE_SIZE);
        } else {
                if (!(dev->stats.lostImages++ % 100))
                        KLOG_WARNING
                            ("%s: no image urbs available for throttle function, lostImages=%d\n",
                             dev->dev_name, dev->stats.lostImages);
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

	/*
	 * One should do one of the following here:
	 * 1. urb OK: check if an empty sample is available at head of dev->sampleq
	 *	a. sample available, fill it in, add to tail of dev->sampleq
	 *	b. no sample available, resubmit urb
         * 2. urb status bad, and situation probably not repairable: set dev->errorStatus = urb->status, return
	 *	In this case the user select() or read() will return an error, and the user can
	 *      try to re-open().
         * 3. urb bad, but if the situation might possibly improve on its own: resubmit urb
         */
        switch (urb->status) {
        case 0:
                dev->consecTimeouts = 0;
                break;
        case -ENOENT:
                // result of usb_kill_urb, don't resubmit
		KLOG_WARNING("%s: urb->status=-ENOENT\n",dev->dev_name);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
                return;
        case -ECONNRESET:
                // urb has been unlinked (usb_unlink_urb) out from under us.
		KLOG_WARNING("%s: urb->status=-ECONNRESET\n", dev->dev_name);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
                return;
        case -ESHUTDOWN:
                // Severe error in host controller, or the urb was submitted
                // after the device was disconnected
		KLOG_WARNING("%s: urb->status=-ESHUTDOWN\n", dev->dev_name);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
                return;
        case -ETIMEDOUT:
                KLOG_WARNING("%s: urb->status=-ETIMEDOUT\n", dev->dev_name);
                dev->stats.urbTimeouts++;
                if (dev->consecTimeouts++ >= 10) {
                    dev->errorStatus = urb->status;
		    return;
		}
		else goto resubmit;
        default:
		KLOG_WARNING("%s: urb->status=%d\n", dev->dev_name, urb->status);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
                return;
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
                        KLOG_WARNING("%s: sample queue full: lost images=%d\n",dev->dev_name,
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
                KLOG_ERR("%s: out of memory for read buf\n",
                        dev->dev_name);
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

	/*
	 * One should do one of the following here:
	 * 1. urb OK: check if an empty sample is available at head of dev->sampleq
	 *	a. sample available, fill it in, add to tail of dev->sampleq
	 *	b. no sample available, resubmit urb
         * 2. urb status bad, and situation probably not repairable: set dev->errorStatus = urb->status, return
	 *	In this case the user select() or read() will return an error, and the user can
	 *      try to re-open().
         * 3. urb bad, but if the situation might possibly improve on its own: resubmit urb
         */
        switch (urb->status) {
        case 0:
                dev->consecTimeouts = 0;
                break;
        case -ENOENT:
                // result of usb_kill_urb, don't resubmit
		KLOG_WARNING("%s: urb->status=-ENOENT\n",dev->dev_name);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
                return;
        case -ECONNRESET:
                // urb has been unlinked (usb_unlink_urb) out from under us.
		KLOG_WARNING("%s: urb->status=-ECONNRESET\n", dev->dev_name);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
                return;
        case -ESHUTDOWN:
                // Severe error in host controller, or the urb was submitted
                // after the device was disconnected
		KLOG_WARNING("%s: urb->status=-ESHUTDOWN\n", dev->dev_name);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
                return;
        case -ETIMEDOUT:
                dev->stats.urbTimeouts++;
                KLOG_WARNING("%s: urb->status=-ETIMEDOUT\n", dev->dev_name);
                if (dev->consecTimeouts++ >= 10) {
                    dev->errorStatus = urb->status;
		    return;
		}
		else goto resubmit;
        default:
		KLOG_WARNING("%s: urb->status=%d\n",dev->dev_name, urb->status);
                dev->stats.urbErrors++;
                dev->errorStatus = urb->status;
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
                        KLOG_WARNING("%s: overflow: lost SOR urbs = %d\n",
                                dev->dev_name,dev->stats.lostSORs);
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
                KLOG_ERR("%s: out of memory for read buf\n",dev->dev_name);
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

        nonseekable_open(inode, file);
        subminor = iminor(inode);

        TWOD_MUTEX_LOCK(&twod_open_lock);
        interface = usb_find_interface(&twod_driver, subminor);
        if (!interface) {
                KLOG_ERR("error, can't find device for minor %d\n",
                     subminor);
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

        dev->sorRate = IRIG_NUM_RATES;

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
        dev->sampleq.head = dev->sampleq.tail = 0;
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
                        goto error;
                }
                memset(dev->img_urb_q.buf, 0,
                       sizeof (struct urb *) * IMG_URB_QUEUE_SIZE);
        }

        /* Allocate the image urbs and submit them */
        for (i = 0; i < IMG_URBS_IN_FLIGHT; ++i) {
                dev->img_urbs[i] = twod_make_img_urb(dev);
                if (!dev->img_urbs[i]) {
                        retval = -ENOMEM;
                        goto error;
                }
                if ((retval = usb_twod_submit_img_urb(dev, dev->img_urbs[i], GFP_KERNEL)) < 0)
                    goto error;
        }

        /* Allocate the shadow OR urbs and submit them */
        for (i = 0; i < SOR_URBS_IN_FLIGHT; ++i) {
            /* Only submit sor in urbs if we have an SOR endpoint */
            if (dev->sor_in_endpointAddr) {
                    dev->sor_urbs[i] = twod_make_sor_urb(dev);
                    if (!dev->sor_urbs[i]) {
                            retval = -ENOMEM;
                            goto error;
                    }
                    if ((retval = usb_twod_submit_sor_urb(dev, dev->sor_urbs[i], GFP_KERNEL)) < 0)
                            goto error;
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
                goto error;
        }
        memset(dev->tas_urb_q.buf, 0,
               sizeof (struct urb *) * TAS_URB_QUEUE_SIZE);

        /* Allocate urbs for queue */
        for (i = 0; i < TAS_URB_QUEUE_SIZE; ++i) {
                dev->tas_urb_q.buf[i] = twod_make_tas_urb(dev);
                if (!dev->tas_urb_q.buf[i]) {
                        retval = -ENOMEM;
                        goto error;
                }
        }
        for (i = 0; i < TAS_URB_QUEUE_SIZE-1; ++i)
                INCREMENT_HEAD(dev->tas_urb_q, TAS_URB_QUEUE_SIZE);

#ifndef DO_IRIG_TIMING
        init_timer(&dev->sendTASTimer);
#endif

        /* save our object in the file's private structure */
        file->private_data = dev;

        KLOG_INFO("%s: now opened, throttleRate=%d\n", dev->dev_name,
             throttleRate);

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
        KLOG_INFO("%s: urb timeouts = %d\n",dev->dev_name, dev->stats.urbTimeouts);

        del_timer_sync(&dev->urbThrottle);

        twod_set_sor_rate(dev, 0);

        read_lock(&dev->usb_iface_lock);
        for (i = 0; i < IMG_URBS_IN_FLIGHT; ++i) {
                struct urb *urb = dev->img_urbs[i];
                if (urb) {
                        usb_kill_urb(urb);
                        usb_buffer_free(dev->udev, urb->transfer_buffer_length,
                                        urb->transfer_buffer, urb->transfer_dma);
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
        read_unlock(&dev->usb_iface_lock);

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
        if (dev->readstate.bytesLeft > 0
            || dev->sampleq.head != dev->sampleq.tail)
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
            && dev->sampleq.tail == dev->sampleq.head) {
                if (file->f_flags & O_NONBLOCK) return -EAGAIN;
                if (wait_event_interruptible(dev->read_wait,
                                                  dev->sampleq.tail !=
                                                  dev->sampleq.head)) {
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
                                retval = usb_twod_submit_img_urb(dev,
                                        sample->urb,GFP_KERNEL);
                                break;
                        case TWOD_SOR_TYPE:
                                retval = usb_twod_submit_sor_urb(dev,
                                        sample->urb,GFP_KERNEL);
                                break;
                        }
                        INCREMENT_TAIL(dev->sampleq, SAMPLE_QUEUE_SIZE);
                        if (retval) return retval;
                }
                /* Finished writing previous sample, check for next. 
                 * bytesLeft will be 0 here.
                 * If no more samples, then we're done
                 */
                if (dev->sampleq.tail == dev->sampleq.head) break;
                sample = dev->sampleq.buf[dev->sampleq.tail];

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
			KLOG_DEBUG("%s: SET_SOR_RATE, rate=%d\n",dev->dev_name, sor_rate);
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
        .ioctl = twod_ioctl,
        .open = twod_open,
        .release = twod_release,
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

        kref_init(&dev->kref);
        rwlock_init(&dev->usb_iface_lock);

        dev->udev = usb_get_dev(interface_to_usbdev(interface));
        dev->interface = interface;

        init_waitqueue_head(&dev->read_wait);
        spin_lock_init(&dev->sampqlock);

        dev->sorRate = IRIG_NUM_RATES;

        /* set up the endpoint information */
        KLOG_INFO("idVendor: %x idProduct: %x, speed: %s, #alt_ifaces=%d\n",
                  id->idVendor, id->idProduct,
		((dev->udev->speed == USB_SPEED_LOW) ? "low (1.1 mbps)" :
		((dev->udev->speed == USB_SPEED_FULL) ? "full (12 mbps)" :
		((dev->udev->speed == USB_SPEED_HIGH) ? "high (480 mbps)" : "unknown"))),
                  interface->num_altsetting);
	
        /* use the first sor-in and tas-out endpoints */
        /* use the second ing_in endpoint */
        iface_desc = interface->cur_altsetting;
        dev->ptype = TWOD_64;
        if (iface_desc->desc.bNumEndpoints == 2 && dev->udev->speed == USB_SPEED_FULL) {
        	dev->ptype = TWOD_32;
        }
      
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

			
                if (!dev->img_in_endpointAddr && dir == USB_DIR_IN &&
                           type == USB_ENDPOINT_XFER_BULK) {
			switch(dev->ptype) {
			case TWOD_64:
				/* we found a big bulk in endpoint on the USB2D_N0 64bit, use it for images */
				if (psize >= 512)
					dev->img_in_endpointAddr = endpoint->bEndpointAddress;
				break;
			case TWOD_32:
				dev->img_in_endpointAddr = endpoint->bEndpointAddress;
				break;
			}
                } else if (!dev->sor_in_endpointAddr &&
			dir == USB_DIR_IN &&
			type == USB_ENDPOINT_XFER_BULK &&
			psize < 512 && psize >= TWOD_SOR_BUFF_SIZE) {
                        /* we found a small bulk in endpoint, use it for the SOR */
                        dev->sor_in_endpointAddr =
                            endpoint->bEndpointAddress;
                } else if (!dev->tas_out_endpointAddr &&
                           dir == USB_DIR_OUT &&
                           type == USB_ENDPOINT_XFER_BULK) {
                        /* we found a bulk out endpoint for true air speed */
                        dev->tas_out_endpointAddr =
                            endpoint->bEndpointAddress;
                }

        }

        if (!dev->img_in_endpointAddr || !dev->tas_out_endpointAddr) {
                KLOG_ERR("Could not find img-in or tas-out endpoints\n");
                retval = -ENOENT;
                goto error;
        }

        if (dev->ptype == TWOD_64 && !dev->sor_in_endpointAddr) {
                KLOG_ERR("Could not find sor-in endpoint for 64 bit probe\n");
                retval = -ENOENT;
                goto error;
        }

        /* save our data pointer in this interface device */
        usb_set_intfdata(interface, dev);

        /* We can register the device now, as it is ready.
         * Then create device name for log messages.
         */
        switch(dev->ptype) {
       	  	case TWOD_64:
            		retval = usb_register_dev(interface, &usbtwod_64);
                        sprintf(dev->dev_name, "/dev/usbtwod_64_%d (%x/%x)",
                                interface->minor - USB_TWOD_64_MINOR_BASE,
                                id->idVendor, id->idProduct);
 	    		break;
          	case TWOD_32:
 	    		retval = usb_register_dev(interface, &usbtwod_32);
                        sprintf(dev->dev_name, "/dev/usbtwod_32_%d (%x/%x)",
                                interface->minor - USB_TWOD_32_MINOR_BASE,
                                id->idVendor, id->idProduct);
        }

        if (retval) {
                /* something prevented us from registering this driver */
                KLOG_ERR("Not able to get a minor for this device.\n");
                usb_set_intfdata(interface, NULL);
                goto error;
        }
        KLOG_INFO("%s: connected\n", dev->dev_name);
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

        KLOG_INFO("%s: disconnected\n", dev->dev_name);
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
                KLOG_ERR("usbtwod_register failed. Error number %d\n", result);

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
