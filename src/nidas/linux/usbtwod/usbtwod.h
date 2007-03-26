
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

/* Set BLOCKING_READ file if you want urb-less reads.
#define BLOCKING_READ
 */

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
/*
 * Build the struct above from the true airspeed (in m/s)
 * @param t2d the Tap2D to be filled
 * @param tas the true airspeed in m/s
 * @param resolution the resolution or diode size, in meters.
 */
inline void TASToTap2D(Tap2D* t2d, float tas, float resolution)
{
    double freq = tas / resolution;
    unsigned int ntap = (unsigned int)(1 - (1.0e6 / freq)) * 255;
    if (ntap < 0) ntap = 0;
    if (ntap > 255) ntap = 255;

    t2d->ntap = (unsigned char)ntap;
    t2d->vdiv = 0;      // currently unused
    t2d->nmsec = 0;     // unused for USB probe
}
#endif


/* Get a minor range for your devices from the usb maintainer */
#define USB_TWOD_MINOR_BASE     192

/* our private defines. if this grows any larger, use your own .h file */
#define MAX_TRANSFER            ( PAGE_SIZE - 512 )
#define WRITES_IN_FLIGHT        8

#define TWOD_BUFF_SIZE          4096

#ifdef BLOCKING_READ
#define READS_IN_FLIGHT         1
#else
#define READS_IN_FLIGHT         16
#endif


struct urb_sample
{
    dsm_sample_t tag;
    struct urb * urb;
};

struct urb_sample_circ_buf
{
    struct urb_sample * buf[READS_IN_FLIGHT];	// Must be power of 2.
    volatile int head;
    volatile int tail;
};

struct usb_twod_stats
{
  int total_urb_callbacks;
  int valid_urb_callbacks;
  int total_read_overflow;
};

#define USB2D_SET_TAS		_IOW(USB2D_IOC_MAGIC,0,float)
#define USB2D_GET_A2D_STATUS	_IOR(USB2D_IOC_MAGIC,1,struct usb_twod_stats)

#endif
