#define DEBUG

/* Set BLOCKING_READ in .h file if you want urb-less reads.
 */
//#define BLOCKING_READ

/*
 * USB PMS-2D driver - 2.0
 *
 * Copyright (C) 2007 University Corporation for Atmospheric Research
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/usb.h>
#include <linux/poll.h>

#include <nidas/linux/usbtwod/usbtwod.h>

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


/* Structure to hold all of our device specific stuff */
struct usb_twod
{
  struct usb_device	*udev;			/* the usb device for this device */
  struct usb_interface	*interface;		/* the interface for this device */
  struct semaphore	sem;			/* lock this structure */
  struct semaphore	limit_sem;		/* limiting the number of writes in progress */
  spinlock_t		read_lock;

  int			is_open;		/* don't allow multiple opens. */

#ifdef BLOCKING_READ
  unsigned char		*bulk_in_buffer;	/* the buffer to receive data */
#endif

  struct urb		*read_urbs[READS_IN_FLIGHT];	/* All read urbs */
  struct urb_sample_circ_buf readq;

  wait_queue_head_t	read_wait;		/* Zzzzz ... */

  size_t 		bulk_in_size;		/* the buffer to receive size */

  __u8			bulk_in_endpointAddr;	/* the address of the bulk in endpoint */
  __u8			bulk_out_endpointAddr;	/* the address of the bulk out endpoint */
};

static struct usb_driver twod_driver;

static struct usb_twod_stats stats;


static DECLARE_MUTEX (disconnect_sem);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
static void twod_rx_bulk_callback(struct urb * urb);
#else
static void twod_rx_bulk_callback(struct urb * urb, struct pt_regs * regs);
#endif
static ssize_t write_data(struct file *file, const char *user_buffer, size_t count);


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
#ifdef BLOCKING_READ
  kfree(dev->bulk_in_buffer);
#endif
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

  info("Total read urbs received = %d", stats.total_urb_callbacks);
  info("Valid read urbs received = %d", stats.valid_urb_callbacks);
  info("Read overflow count (lost records) = %d", stats.total_read_overflow);

  wake_up_interruptible(&dev->read_wait);

#ifndef BLOCKING_READ
  for (i = 0; i < READS_IN_FLIGHT-1; ++i) {
    struct urb * urb = dev->read_urbs[i];

    usb_kill_urb(urb);
    usb_buffer_free(dev->udev, urb->transfer_buffer_length,
		urb->transfer_buffer, urb->transfer_dma);
    usb_free_urb(urb);
  }
  for (i = 0; i < READS_IN_FLIGHT; ++i) {
    kfree(dev->readq.buf[i]);
  }
#endif

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
#ifndef BLOCKING_READ
  int retval;
  struct usb_twod * dev = (struct usb_twod *)urb->context;
  struct urb_sample * osamp;

  ++stats.total_urb_callbacks;

//((int *)urb->transfer_buffer)[0] = stats.total_urb_callbacks;

  /* sync/async unlink faults aren't errors */
  if (urb->status) {
    if (urb->status == -ENOENT || urb->status == -ECONNRESET || urb->status == -ESHUTDOWN) {
      ;
    } else {
      dbg("%s - nonzero read bulk status received: %d, rx# = %d",
		__FUNCTION__, urb->status, stats.total_urb_callbacks);
    }
    goto resubmit; /* maybe we can recover */
  }

  ++stats.valid_urb_callbacks;

  osamp = (struct urb_sample *) GET_HEAD(dev->readq, READS_IN_FLIGHT);
  if (!osamp) {
    // overflow, no sample available for output.
    ++stats.total_read_overflow;
    err("%s - overflow, user is not keeping up reading, lost read count = %d/%d.",
	__FUNCTION__, stats.total_read_overflow, stats.total_urb_callbacks);

    // resubmit the urb (current data is lost)
    goto resubmit;
  }
  else {
    INCREMENT_HEAD(dev->readq, READS_IN_FLIGHT);
    osamp->tag.timetag = getSystemTimeMsecs();
    osamp->urb = urb;
    wake_up_interruptible(&dev->read_wait);
  }

  return;

resubmit:
  retval = usb_submit_urb(urb, GFP_ATOMIC);
  if (retval) {
    err("%s - failed re-submitting read urb, error %d", __FUNCTION__, retval);
  }
#endif
}

static int twod_open(struct inode *inode, struct file *file)
{
  struct usb_twod * dev;
  struct usb_interface * interface;
  struct urb_sample * samp;
  int subminor;
  int i, retval = 0;
  int submit = READS_IN_FLIGHT - 1;

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

  stats.total_urb_callbacks = 0;
  stats.valid_urb_callbacks = 0;
  stats.total_read_overflow = 0;

  /* save our object in the file's private structure */
  file->private_data = dev;

#ifndef BLOCKING_READ
  dev->readq.head = dev->readq.tail = 0;

  /* create urbs and readq. */
  for (i = 0; i < READS_IN_FLIGHT; ++i) {

    /* Readq for urbs. */
    samp = kmalloc(sizeof(struct urb_sample), GFP_KERNEL);
    memset(samp, 0, sizeof(struct urb_sample));
    dev->readq.buf[i] = samp;
  }

  /* Submit all the urbs. */
  for (i = 0; i < submit; ++i) {
    dev->read_urbs[i] = twod_make_urb(dev);
    if (dev->read_urbs[i]) {
      retval = usb_submit_urb(dev->read_urbs[i], GFP_ATOMIC);
      if (retval) {
        err("%s - failed submitting read urb %d, error %d", __FUNCTION__, i, retval);
      }
    }
  }
#endif

  info("USB PMS-2D device now opened");

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
#ifndef BLOCKING_READ
  if (dev->readq.head != dev->readq.tail)
#endif
    mask |= POLLIN | POLLRDNORM;

  return mask;
}

static ssize_t twod_read(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
  struct usb_twod *dev;
  int retval = 0;
  size_t bytes_read;
  int tagsize = sizeof(dsm_sample_t);
  struct urb_sample* sample;

  if (count == 0)
    goto exit;

  dev = (struct usb_twod *)file->private_data;

#ifdef BLOCKING_READ
  retval = usb_bulk_msg(dev->udev,
		usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
		dev->bulk_in_buffer,
		min(TWOD_BUFF_SIZE, count),
		&bytes_read, 0);

  if (copy_to_user(buffer, dev->bulk_in_buffer, bytes_read)) {
    return -EFAULT;
  }

  return bytes_read;

#else

  /* lock this object */
  if (down_interruptible(&dev->sem)) {
    retval = -ERESTARTSYS;
    goto exit;
  }

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


  // If they ask for less than actual_length, then the rest of the data in this
  // buffer is lost.
  sample = dev->readq.buf[dev->readq.tail];
  bytes_read = min(count, sample->urb->actual_length + tagsize);

  sample->tag.length = bytes_read;

  /* if we have a complete 2D buffer, then copy to user space. */
  if (copy_to_user(buffer, &sample->tag, tagsize)) {
    err("%s - copy_to_user failed - %d", __FUNCTION__, retval);
    retval = -EFAULT;
    goto unlock_exit;
  }

  if (copy_to_user(buffer + tagsize, sample->urb->transfer_buffer, 
		   bytes_read - tagsize)) {
    err("%s - copy_to_user failed - %d", __FUNCTION__, retval);
    retval = -EFAULT;
    goto unlock_exit;
  }

  memset(sample->urb->transfer_buffer, 0, TWOD_BUFF_SIZE);

  retval = usb_submit_urb(sample->urb, GFP_ATOMIC);
  if (retval) {
    err("%s - failed submitting read urb, error %d", __FUNCTION__, retval);
    goto unlock_exit;
  }

  retval = bytes_read;

  INCREMENT_TAIL(dev->readq, READS_IN_FLIGHT);

unlock_exit:
  up(&dev->sem);
#endif

exit:
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
  int retval = -EINVAL;
  Tap2D encoded_tas;

  if (_IOC_TYPE(cmd) != USB2D_IOC_MAGIC)
    return -ENOTTY;

  switch (cmd)
  {
    case USB2D_SET_TAS:
      if (copy_from_user(&encoded_tas, (void __user *)arg, 3))
        return -EFAULT;

      retval = write_data(file, (const char *)&encoded_tas, 3);
      if (retval == 3)
        retval = 0;
      break;
  }

  return retval;
}

/* -------------------------------------------------------------------- */
static const struct file_operations twod_fops = {
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
//  sema_init(&dev->sem, 1);
  init_MUTEX(&dev->sem);
  sema_init(&dev->limit_sem, WRITES_IN_FLIGHT);
  spin_lock_init(&dev->read_lock);

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

#ifdef BLOCKING_READ
  if ((dev->bulk_in_buffer = kmalloc(TWOD_BUFF_SIZE * READS_IN_FLIGHT, GFP_KERNEL)) == NULL) {
    err("%s - out of memory for read buf", __FUNCTION__);
    goto error;
  }
#endif

  /* let the user know what node this device is now attached to */
  info("USB PMS-2D device now attached to USBtwod-%d", interface->minor);
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
