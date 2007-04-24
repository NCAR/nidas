
#ifndef _nidas_usbtwod_h_
#define _nidas_usbtwod_h_

#ifndef __KERNEL__
/* User programs need this for the _IO macros, but kernel
 * modules get their's elsewhere.
 */
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

#include <nidas/core/dsm_sample.h>

/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define USB2D_IOC_MAGIC	0x2d


/*
 * Struct to adjust probe slice rate for true airspeed
 */
typedef struct _Tap2D {
    unsigned char ntap;  /* which tap in the variable resistor (0-255) */
    unsigned char vdiv;  /* boolean toggle for voltage divide by 10 */
    unsigned char nmsec; /* unused */
} Tap2D;

#ifndef __KERNEL__ /* TASToTap2D is only available in user space */

#include <errno.h>

/*
 * Build the struct above from the true airspeed (in m/s)
 * @param t2d the Tap2D to be filled
 * @param tas the true airspeed in m/s
 * @param resolution the resolution or diode size, in meters.
 */
inline int TASToTap2D(Tap2D* t2d, float tas, float resolution)
{
    double freq = tas / resolution;
    unsigned int ntap = (unsigned int)((1 - (1.0e6 / freq)) * 255);

    t2d->vdiv = 0;      /* currently unused */
    t2d->nmsec = 0;     /* unused for USB probe */
    t2d->ntap = 0;

    if (ntap > 255)
        return -EINVAL;

    t2d->ntap = (unsigned char)ntap;
    return 0;		/* Return success */
}
#endif

/* Get a minor range for your devices from the usb maintainer */
#define USB_TWOD_MINOR_BASE     192

/* our private defines. if this grows any larger, use your own .h file */
#define MAX_TRANSFER            ( PAGE_SIZE - 512 )
#define WRITES_IN_FLIGHT        8

#define TWOD_BUFF_SIZE          4096

/* READ_QUEUE_SIZE must be a power of 2 (required by circ_buf)
 * In order for a circ_buf to tell the difference between empty
 * and full, the maximum number of elements in a circ_buf is
 * (size-1).  So we set the number of urbs in flight to (size-1).
 */
#define READ_QUEUE_SIZE         8
#define READS_IN_FLIGHT         (READ_QUEUE_SIZE - 1)

#define TWOD_DATA	0
#define TWOD_HSKP	1

struct urb_sample
{
    dsm_sample_time_t timetag;		/* timetag of sample */
    dsm_sample_length_t length;		/* number of bytes in data */
    unsigned long id;			/* Sample ID, we may have multiple things */
    unsigned long tas;			/* True Airspeed */
    struct urb * urb;
};

struct urb_sample_circ_buf
{
    struct urb_sample * buf[READ_QUEUE_SIZE];	// Must be power of 2.
    volatile int head;
    volatile int tail;
};

struct usb_twod_stats
{
  size_t total_urb_callbacks;
  size_t valid_urb_callbacks;
  size_t total_read_overflow;
  size_t enoents;
  size_t econnresets;
  size_t eshutdowns;
  size_t eothers;
};

#define USB2D_SET_TAS		_IOW(USB2D_IOC_MAGIC,0,float)
#define USB2D_GET_A2D_STATUS	_IOR(USB2D_IOC_MAGIC,1,struct usb_twod_stats)

#endif
