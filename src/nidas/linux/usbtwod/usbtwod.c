
/*
 * USB PMS-2D driver - 2.0
 *
 * Copyright (C) 2007 University Corporation for Atmospheric Research
 *
 *
 * TODO: set READ_QUEUE_SIZE to something like 4, and
 *	 make READS_IN_FLIGHT a variable, and kmalloc/kfree the
 *       read_urbs array.
 *	 Set buffer size on user side to 4 samples.
 *       Then if throttleRate == 0 (no throttle) then
 *       set READS_IN_FLIGHT to READ_QUEUE_SIZE-1.
 *       If throttleRate > 0, then set READS_IN_FLIGHT to 1.
 */

// #define DEBUG

#include <linux/kernel.h>
#include <linux/module.h>
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

/* Define these values to match your devices */
#define USB_VENDOR_ID	0x2D2D
#define USB_PRODUCT_ID	0x2D00

/* These are the default Cyprus EZ FX & FX2 ID's */
//#define USB_VENDOR_ID	0x0547
//#define USB_PRODUCT_ID	0x1002

/* table of devices that work with this driver */
static struct usb_device_id twod_table [] = {
	{ USB_DEVICE(USB_VENDOR_ID, USB_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, twod_table);

static int throttleRate = 0;
static int throttleJiffies = 0;

#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param(throttleRate,int,0);
#else
module_param(throttleRate,int,0);
#endif

/* Structure to hold all of our device specific stuff */
struct usb_twod
{
  struct usb_device	*udev;			/* the usb device for this device */
  struct usb_interface	*interface;		/* the interface for this device */
  struct semaphore	sem;			/* lock this structure */
  struct semaphore	limit_sem;		/* limiting the number of writes in progress */

  int			is_open;		/* don't allow multiple opens. */

  long			latestDAQ_TAS;		/* Latest tas from data system. */
  long			latestProbeTAS;		/* Latest tas from the probe. */

  struct urb		*read_urbs[READS_IN_FLIGHT];	/* All read urbs */
  struct urb_sample_circ_buf readq;

  struct urb_circ_buf urbq;

  wait_queue_head_t	read_wait;		/* Zzzzz ... */

  size_t 		bulk_in_size;		/* the buffer to receive size */

  __u8			bulk_in_endpointAddr;	/* the address of the bulk in endpoint */
  __u8			bulk_out_endpointAddr;	/* the address of the bulk out endpoint */
  struct usb_twod_stats stats;

  struct urb_sample	*out_sample;		/* sample which hasn't been completely read by user */
  char 			*out_urb_ptr;	 	/* pointer into urb if what is left to copy */
  size_t		left_to_copy;		/* how much is left to copy to user buffer of sample_ptr */
  size_t		debug_cntr;

  struct timer_list urbThrottle;
  size_t urbSubmitError;
};

static struct usb_driver twod_driver;


static DECLARE_MUTEX (disconnect_sem);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
static void twod_rx_bulk_callback(struct urb * urb);
#else
static void twod_rx_bulk_callback(struct urb * urb, struct pt_regs * regs);
#endif
static ssize_t write_data(struct file *file, const char *user_buffer, size_t count);


/*
 * int mem_flags:
 *  GFP_ATOMIC:
 *      1. when called from urb callback handler,
 *          interrupt, bottom half, tasklet or a timer callback.
 *      2. If you're holding a spinlock or rwlock (but not a semaphore).
 *      3. Current state is not TASK_RUNNING (i.e. current is non-null
 *         and driver has not changed the state).
 *  GFP_NOIO: block drivers.
 *  GFP_KERNEL: all other circumstances.
 */
static void usb_twod_submit_urb(struct usb_twod* dev,struct urb* urb,int mem_flags)
{
	if (throttleRate > 0) {
		if (CIRC_SPACE(dev->urbq.head,dev->urbq.tail,READ_QUEUE_SIZE) == 0)
		      err("programming error: no space in queue for resubmitting urbs");
		else {
		    dev->urbq.buf[dev->urbq.head] = urb;
		    INCREMENT_HEAD(dev->urbq, READ_QUEUE_SIZE);
		}
	}
	else {
		int retval;
		retval = usb_submit_urb(urb, mem_flags);
		if (retval < 0 && !(dev->urbSubmitError++ % 100))
			err("%s urbSubmitErrors=%d",
				    __FUNCTION__,dev->urbSubmitError);
	}
}

static void urb_throttle_func(unsigned long arg)
{
        struct usb_twod* dev = (struct usb_twod*) arg;
        int retval;
        static int debugcntr = 0;

	while (dev->urbq.tail != dev->urbq.head) {
		struct urb* urb = dev->urbq.buf[dev->urbq.tail];
                if (!(debugcntr++ % 100))
                    KLOG_DEBUG("urb_throttle_func, queue cnt=%d,jiffies=%ld\n",
                        CIRC_CNT(dev->urbq.head,dev->urbq.tail,READ_QUEUE_SIZE),
                        jiffies);
                retval = usb_submit_urb(urb, GFP_ATOMIC);
                if (retval < 0 && !(dev->urbSubmitError++ % 100))
                        err("%s urbSubmitErrors=%d",
                                    __FUNCTION__,dev->urbSubmitError);
                INCREMENT_TAIL(dev->urbq, READ_QUEUE_SIZE);
        }
        dev->urbThrottle.expires += throttleJiffies;
        add_timer(&dev->urbThrottle);      // reschedule myself
}

/* -------------------------------------------------------------------- */
static struct urb * twod_make_urb(struct usb_twod * dev)
{
  struct urb * urb;
  int retval = 0;

  urb = usb_alloc_urb(0, GFP_KERNEL);
  if (!urb) {
    retval = -ENOMEM;
    goto exit;
  }

  if (!(urb->transfer_buffer = usb_buffer_alloc(dev->udev, TWOD_BUFF_SIZE,
                                GFP_KERNEL, &urb->transfer_dma))) {
    err("%s - out of memory for read buf", __FUNCTION__);
    usb_free_urb(urb);
    urb = NULL;
    retval = -ENOMEM;
    goto exit;
  }

  urb->transfer_flags = URB_NO_TRANSFER_DMA_MAP;

  usb_fill_bulk_urb(urb, dev->udev,
	usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
	urb->transfer_buffer,
	TWOD_BUFF_SIZE,
	twod_rx_bulk_callback, dev);

exit:
  return urb;
}

static void twod_delete(struct usb_twod *dev)
{
  usb_put_dev(dev->udev);
  kfree(dev);
}

static int twod_release(struct inode *inode, struct file *file)
{
  struct usb_twod *dev = (struct usb_twod *)file->private_data;
  int i, retval = 0;

  if (dev == NULL) {
    return -ENODEV;
    goto exit;
  }

  info("Total read urbs received = %d", dev->stats.total_urb_callbacks);
  info("Valid read urbs received = %d", dev->stats.valid_urb_callbacks);
  info("Read overflow count (lost records) = %d", dev->stats.total_read_overflow);

  del_timer_sync(&dev->urbThrottle);

  wake_up_interruptible(&dev->read_wait);

  for (i = 0; i < READS_IN_FLIGHT; ++i) {
    struct urb * urb = dev->read_urbs[i];

    usb_kill_urb(urb);
    usb_buffer_free(dev->udev, urb->transfer_buffer_length,
		urb->transfer_buffer, urb->transfer_dma);
    usb_free_urb(urb);
  }
  for (i = 0; i < READ_QUEUE_SIZE; ++i) {
    kfree(dev->readq.buf[i]);
  }

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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
static void twod_rx_bulk_callback(struct urb * urb)
#else
static void twod_rx_bulk_callback(struct urb * urb, struct pt_regs * regs)
#endif
{
  struct usb_twod * dev = (struct usb_twod *)urb->context;
  struct urb_sample * osamp;

  ++dev->stats.total_urb_callbacks;

  /* sync/async unlink faults aren't errors */
  if (urb->status) {
    switch (urb->status) {
    case -ENOENT:
	// result of usb_kill_urb, don't resubmit
	if (!(dev->stats.enoents++ % 1000))
	    err("%s - urb->status=-ENOENT %d times", __FUNCTION__, dev->stats.enoents);
        return;
    case -ECONNRESET:
	if (!(dev->stats.econnresets++ % 1000))
	    err("%s - urb->status=-ECONNRESET %d times",
			__FUNCTION__, dev->stats.econnresets);
        break;
    case -ESHUTDOWN:
	if (!(dev->stats.eshutdowns++ % 1000))
	    err("%s - urb->status=-ESHUTDOWN %d times",
			__FUNCTION__, dev->stats.eshutdowns);
        break;
     default:
	if (!(dev->stats.eothers++ % 1000))
	    err("%s - urb->status=%d, %d times",
			__FUNCTION__,urb->status, dev->stats.eothers);
        break;
    }
    goto resubmit; /* maybe we can recover */
  }

  ++dev->stats.valid_urb_callbacks;

  osamp = (struct urb_sample *) GET_HEAD(dev->readq, READ_QUEUE_SIZE);
  if (!osamp) {
    // overflow, no sample available for output.
    if (!(dev->stats.total_read_overflow++ % 1000))
        err("%s - overflow: lost urbs = %d, read urbs= %d, total=%d\n",
            __FUNCTION__, dev->stats.total_read_overflow,
            dev->stats.total_urb_callbacks-dev->stats.total_read_overflow,
            dev->stats.total_urb_callbacks);

    // resubmit the urb (current data is lost)
    goto resubmit;
  }
  else {
    osamp->timetag = getSystemTimeMsecs();
    osamp->length = urb->actual_length + sizeof(osamp->tas) + sizeof(osamp->id);
    osamp->tas = dev->latestProbeTAS;
    osamp->id = TWOD_DATA;
    osamp->urb = urb;
    INCREMENT_HEAD(dev->readq, READ_QUEUE_SIZE);
    wake_up_interruptible(&dev->read_wait);
  }

  return;

resubmit:
  // called from a urb completion handler, so use GFP_ATOMIC
  usb_twod_submit_urb(dev,urb,GFP_ATOMIC);
}

static int twod_open(struct inode *inode, struct file *file)
{
  struct usb_twod * dev;
  struct usb_interface * interface;
  struct urb_sample * samp;
  int subminor;
  int i, retval = 0;

  nonseekable_open(inode, file);
  subminor = iminor(inode);

  down(&disconnect_sem);

  interface = usb_find_interface(&twod_driver, subminor);
  if (!interface) {
    err ("%s - error, can't find device for minor %d",
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

  memset(&dev->stats,0,sizeof(dev->stats));
  dev->out_sample = 0;
  dev->out_urb_ptr = 0;
  dev->left_to_copy = 0;
  dev->debug_cntr = 0;
  dev->urbSubmitError = 0;

  /* save our object in the file's private structure */
  file->private_data = dev;

  dev->readq.head = dev->readq.tail = 0;

  /* create urbs and readq. */
  for (i = 0; i < READ_QUEUE_SIZE; ++i) {
    /* Readq for urbs. */
    samp = kmalloc(sizeof(struct urb_sample), GFP_KERNEL);
    memset(samp, 0, sizeof(struct urb_sample));
    dev->readq.buf[i] = samp;
  }

  /* Create urbs and put them in the submit queue */
  dev->urbq.head = dev->urbq.tail = 0;
  for (i = 0; i < READS_IN_FLIGHT; ++i) {
    dev->read_urbs[i] = twod_make_urb(dev);
    if (!dev->read_urbs[i]) return -ENOMEM;
    // called from _init module, use GFP_KERNEL
    usb_twod_submit_urb(dev,dev->read_urbs[i],GFP_KERNEL);
  }

  init_timer(&dev->urbThrottle);

  if (throttleRate > 0) {
	  throttleJiffies = HZ / throttleRate;
	  if (throttleJiffies < 0) throttleJiffies = 1;
	  dev->urbThrottle.function = urb_throttle_func;
	  dev->urbThrottle.expires = jiffies + throttleJiffies;
	  dev->urbThrottle.data = (unsigned long)dev;
	  add_timer(&dev->urbThrottle);
  }

  info("USB PMS-2D device now opened, throttleRate=%d\n",
	throttleRate);

exit:
  up(&dev->sem);

unlock_disconnect_exit:
  up (&disconnect_sem);
  return retval;
}

static unsigned int twod_poll(struct file *file, poll_table *wait)
{
        struct usb_twod * dev = file->private_data;
        unsigned int mask = POLLOUT | POLLWRNORM;

        poll_wait(file, &dev->read_wait, wait);
        if (dev->readq.head != dev->readq.tail)
          mask |= POLLIN | POLLRDNORM;

        return mask;
}

static ssize_t twod_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
	struct usb_twod *dev;
	ssize_t retval = 0;
	ssize_t countreq = count;	// original request
	size_t n;
	struct urb_sample* sample;

	struct urb* submitUrbs[READ_QUEUE_SIZE];
        int nsubmit = 0;
        int i;

	if (count == 0) return countreq;

	dev = (struct usb_twod *)file->private_data;

	/* lock this object */
	if (down_interruptible(&dev->sem)) return -ERESTARTSYS;

	/* verify that the device wasn't unplugged */
	if (dev->interface == NULL) {
		retval = -ENODEV;
		err("No device or device unplugged %d", retval);
		goto unlock_exit;
	}
	if (dev->readq.tail == dev->readq.head) {
	  if (file->f_flags & O_NONBLOCK) {
	    retval = -EAGAIN;
	    goto unlock_exit;
	  }
	  retval = wait_event_interruptible(dev->read_wait, dev->readq.head != dev->readq.tail);
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
			info("head=%d,tail=%d,urbs=%d,count=%d,left_to_copy=%d",
                            dev->readq.head,dev->readq.tail,
                            CIRC_CNT(dev->readq.head,dev->readq.tail,READ_QUEUE_SIZE),
                            count,dev->left_to_copy);
#endif
		// Check if there is data from previous urb to copy
		if (dev->left_to_copy > 0) {
			n = min(count,dev->left_to_copy);
			if (copy_to_user(buffer, dev->out_urb_ptr, n)) {
			        err("%s - copy_to_user failed - %d", __FUNCTION__, retval);
			        retval = -EFAULT;
				break;
			}
			count -= n;
			dev->left_to_copy -= n;
			buffer += n;
			dev->out_urb_ptr += n;
			if (count == 0) {
				// if count is 0, we're done.
				retval = countreq;
				break;
			}

			// otherwise we're finished with this urb
			// left_to_copy will be 0 here
			INCREMENT_TAIL(dev->readq, READ_QUEUE_SIZE);
                        // defer submitting them until read is done.
                        submitUrbs[nsubmit++] = dev->out_sample->urb;
		}

		// length of header (timetag + length) + tas + id
		n = sizeof(dsm_sample_time_t) + sizeof(dsm_sample_length_t)
			+ sizeof(sample->tas) + sizeof(sample->id);

		// if no more samples or not enough room to copy header, then we're done
		if (dev->readq.tail == dev->readq.head || count < n) {
			retval = countreq - count;
			break;
		}
		  
		sample = dev->readq.buf[dev->readq.tail];

		/* copy header to user space. We know there is room. */
		if (copy_to_user(buffer, sample, n)) {
		  err("%s - copy_to_user failed - %d", __FUNCTION__, retval);
		  retval = -EFAULT;
		  break;
		}
		count -= n;
		buffer += n;

		dev->left_to_copy = sample->length - sizeof(sample->tas) - sizeof(sample->id);
		dev->out_sample = sample;
		dev->out_urb_ptr = sample->urb->transfer_buffer;
#ifdef DEBUG
		if (!(dev->debug_cntr % 100))
			KLOG_DEBUG("bottom count=%d,left_to_copy=%d",count,dev->left_to_copy);
#endif
	}

unlock_exit:
	up(&dev->sem);

        for (i = 0; i < nsubmit; i++) 
            // called from driver read method, use GFP_KERNEL
            usb_twod_submit_urb(dev,submitUrbs[i],GFP_KERNEL);

#ifdef DEBUG
	if (!(dev->debug_cntr++ % 100)) KLOG_DEBUG("retval=%d\n",retval);
#endif
#undef DEBUG
	return retval;
}

/* -------------------------------------------------------------------- */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
static void twod_write_bulk_callback(struct urb *urb)
#else
static void twod_write_bulk_callback(struct urb *urb, struct pt_regs * regs)
#endif
{
  struct usb_twod * dev = (struct usb_twod *)urb->context;
dbg("write_callback");
  /* sync/async unlink faults aren't errors */
  if (urb->status && 
      !(urb->status == -ENOENT || urb->status == -ECONNRESET || urb->status == -ESHUTDOWN))
  {
    dbg("%s - nonzero write bulk status received: %d", __FUNCTION__, urb->status);
  }

  /* free up our allocated buffer */
  usb_buffer_free(urb->dev, urb->transfer_buffer_length, 
		urb->transfer_buffer, urb->transfer_dma);
  up(&dev->limit_sem);
}

static ssize_t twod_write(struct file *file, const char *user_buffer, size_t count, loff_t *ppos)
{
dbg("write");
  return write_data(file, user_buffer, count);
}

/* Used by both twod_write & twod_ioctl to send data via the bulk write end-point
 */
static ssize_t write_data(struct file *file, const char *user_buffer, size_t count)
{
  struct usb_twod *dev;
  int retval = 0;
  struct urb *urb = NULL;
  char *buf = NULL;
  size_t writesize = min(count, (size_t)MAX_TRANSFER);

  dev = (struct usb_twod *)file->private_data;

  /* verify that we actually have some data to write */
  if (count == 0)
    goto exit;

  /* limit the number of URBs in flight to stop a user from using up all RAM */
  if (down_interruptible(&dev->limit_sem))
  {
    retval = -ERESTARTSYS;
    goto exit;
  }

  /* create a urb, and a buffer for it, and copy the data to the urb */
  urb = usb_alloc_urb(0, GFP_KERNEL);
  if (!urb) {
    retval = -ENOMEM;
    goto error;
  }

  buf = usb_buffer_alloc(dev->udev, writesize, GFP_KERNEL, &urb->transfer_dma);
  if (!buf) {
    retval = -ENOMEM;
    goto error;
  }
  if (copy_from_user(buf, user_buffer, writesize)) {
    retval = -EFAULT;
    goto error;
  }

  /* initialize the urb properly */
  usb_fill_bulk_urb(urb, dev->udev,
		  usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
		  buf, writesize, twod_write_bulk_callback, dev);
  urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

  /* send the data out the bulk port */
  retval = usb_submit_urb(urb, GFP_KERNEL);
  if (retval) {
    err("%s - failed submitting write urb, error %d", __FUNCTION__, retval);
    goto error;
  }

  /* release our reference to this urb, the USB core will eventually free it entirely */
  usb_free_urb(urb);

exit:
  return writesize;

error:
  usb_buffer_free(dev->udev, writesize, buf, urb->transfer_dma);
  usb_free_urb(urb);
  kfree(buf);
  return retval;
}

static int twod_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
  struct usb_twod *dev = (struct usb_twod *)file->private_data;
  int retval = -EINVAL;

  if (_IOC_TYPE(cmd) != USB2D_IOC_MAGIC)
    return -ENOTTY;

  switch (cmd)
  {
    case USB2D_SET_TAS:
      retval = write_data(file,(const char*)arg, 3);
      if (retval == 3)
        retval = 0;
      copy_from_user((char *)&dev->latestDAQ_TAS, (const char *)arg, 3);
      copy_from_user((char *)&dev->latestProbeTAS, (const char *)arg, 3);
      break;
  }

  return retval;
}

/* -------------------------------------------------------------------- */
static struct file_operations twod_fops = {
	.owner =	THIS_MODULE,
	.read =		twod_read,
	.write =	twod_write,
	.poll =		twod_poll,
	.ioctl =	twod_ioctl,
	.open =		twod_open,
	.release =	twod_release,
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

static int twod_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
  struct usb_twod *dev = NULL;
  struct usb_host_interface *iface_desc;
  struct usb_endpoint_descriptor *endpoint;
  int i;
  int retval = -ENOMEM;

  /* allocate memory for our device state and initialize it */
  dev = kmalloc(sizeof(*dev), GFP_KERNEL);
  if (dev == NULL) {
     err("Out of memory");
     goto error;
  }
  memset(dev, 0x00, sizeof(*dev));

  dev->interface = interface;
  init_waitqueue_head(&dev->read_wait);
  init_MUTEX(&dev->sem);
  sema_init(&dev->limit_sem, WRITES_IN_FLIGHT);

  dev->udev = usb_get_dev(interface_to_usbdev(interface));

  /* set up the endpoint information */
  /* use only the first bulk-in and bulk-out endpoints */
  iface_desc = interface->cur_altsetting;
  for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
    endpoint = &iface_desc->endpoint[i].desc;

    if (!dev->bulk_in_endpointAddr &&
	((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN) &&
	((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)) {
      /* we found a bulk in endpoint */
      dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
      dev->bulk_in_size = endpoint->wMaxPacketSize;
    }

    if (!dev->bulk_out_endpointAddr &&
	((endpoint->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT) &&
	((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK)) {
      /* we found a bulk out endpoint */
      dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
    }
  }

  if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
    err("%s - Could not find both bulk-in and bulk-out endpoints", __FUNCTION__);
    goto error;
  }


  /* save our data pointer in this interface device */
  usb_set_intfdata(interface, dev);

  /* we can register the device now, as it is ready */
  retval = usb_register_dev(interface, &twod_class);
  if (retval) {
    /* something prevented us from registering this driver */
    err("%s - Not able to get a minor for this device.", __FUNCTION__);
    usb_set_intfdata(interface, NULL);
    goto error;
  }
  /* let the user know what node this device is now attached to */
  KLOG_INFO("USB PMS-2D device now attached to USBtwod-%d", interface->minor);
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

  dev = usb_get_intfdata (interface);
  usb_set_intfdata(interface, NULL);

  down(&dev->sem);

  /* give back our minor */
  usb_deregister_dev(interface, &twod_class);

  up(&dev->sem);

  wake_up_interruptible(&dev->read_wait);

  twod_delete(dev);

  up(&disconnect_sem);

  info("USB PMS-2D #%d now disconnected", minor);
}

/* -------------------------------------------------------------------- */
static struct usb_driver twod_driver = {
	.name		= "usbtwod",
	.id_table	= twod_table,
	.probe		= twod_probe,
	.disconnect	= twod_disconnect,
};

static int __init usb_twod_init(void)
{
  int result;

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
