
#ifndef _nidas_usbtwod_h_
#define _nidas_usbtwod_h_

#ifndef __KERNEL__
/* User programs need this for the _IO macros, but kernel
 * modules get their's elsewhere.
 */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>
#endif

#include <nidas/core/dsm_sample.h>

struct usb_twod_stats
{
        size_t total_img_callbacks;
        size_t valid_img_callbacks;
        size_t lostImages;
        size_t total_sor_callbacks;
        size_t valid_sor_callbacks;
        size_t lostSORs;
        size_t lostTASs;
        size_t enoents;
        size_t econnresets;
        size_t eshutdowns;
        size_t eothers;
};

/*
 * Struct to adjust probe slice rate for true airspeed
 */
typedef struct _Tap2D
{
        unsigned char ntap;     /* which tap in the variable resistor (0-255) */
        unsigned char vdiv;     /* boolean toggle for voltage divide by 10 */
        unsigned char nmsec;    /* unused */
        unsigned char dummy;
} Tap2D;

#define SIZEOF_TAP2D 4
/* Pick a character as the magic number of your driver.
 * It isn't strictly necessary that it be distinct between
 * all modules on the system, but is a good idea. With
 * distinct magic numbers one can catch a user sending
 * an ioctl to the wrong device.
 */
#define USB2D_IOC_MAGIC	0x2d

#define USB2D_SET_TAS		_IOW(USB2D_IOC_MAGIC,0,Tap2D)
#define USB2D_SET_SOR_RATE	_IOW(USB2D_IOC_MAGIC,1,int)
#define USB2D_GET_STATUS	_IOR(USB2D_IOC_MAGIC,2,struct usb_twod_stats)

#ifndef __KERNEL__
/*
 * Build the struct above from the true airspeed (in m/s)
 * @param t2d the Tap2D to be filled
 * @param tas the true airspeed in m/s
 * @param resolution the resolution or diode size, in meters.
 */
inline int TASToTap2D(Tap2D * t2d, float tas, float resolution)
{
        double freq = tas / resolution;
        unsigned int ntap = (unsigned int) ((1 - (1.0e6 / freq)) * 255);

        t2d->vdiv = 0;          /* currently unused */
        t2d->nmsec = 0;         /* unused for USB probe */
        t2d->ntap = 0;

        if (ntap > 255)
                return -EINVAL;

        t2d->ntap = (unsigned char) ntap;
        return 0;               /* Return success */
}
#endif                          // ifndef __KERNEL__

#define TWOD_IMG_DATA	0
#define TWOD_SOR_DATA   1

#ifdef __KERNEL__
#include <linux/module.h>

#include <nidas/linux/irigclock.h>

/* Get a minor range for your devices from the usb maintainer */
#define USB_TWOD_MINOR_BASE     192

/* our private defines. if this grows any larger, use your own .h file */
#define MAX_TRANSFER            ( PAGE_SIZE - 512 )

#define TWOD_IMG_BUFF_SIZE          4096
#define TWOD_SOR_BUFF_SIZE          4
#define TWOD_TAS_BUFF_SIZE          3

/* SAMPLE_QUEUE_SIZE must be a power of 2 (required by circ_buf)
 * In order for a circ_buf to tell the difference between empty
 * and full, the maximum number of elements in a circ_buf is
 * (size-1).
 * In addition, SAMPLE_QUEUE_SIZE should be at least 
 * IMG_URBS_IN_FLIGHT + SOR_URBS_IN_FLIGHT + 1 
 */
#define SAMPLE_QUEUE_SIZE   8
#define IMG_URB_QUEUE_SIZE  4   /* also must be a power of two */
#define IMG_URBS_IN_FLIGHT   (IMG_URB_QUEUE_SIZE-1)
#define SOR_URBS_IN_FLIGHT   1

#define TAS_URB_QUEUE_SIZE   2

#if defined(CONFIG_ARCH_VIPER) || defined(CONFIG_MACH_ARCOM_MERCURY)
#define DO_IRIG_TIMING
#endif

struct twod_urb_sample
{
        dsm_sample_time_t timetag;      /* timetag of sample */
        dsm_sample_length_t length;     /* number of bytes in data */
        unsigned long id;       /* Sample ID, we may have multiple things */
        unsigned long data;     /* True Airspeed for image sample, shadowOR for sor sample */
        struct urb *urb;
};

struct sample_circ_buf
{
        struct twod_urb_sample **buf;
        volatile int head;
        volatile int tail;
};

struct urb_circ_buf
{
        struct urb **buf;
        volatile int head;
        volatile int tail;
};


/* reading status keeper  */
struct read_state
{
        char *dataPtr;          /* pointer into data of what is left to copy to user */
        size_t bytesLeft;       /* How much is left to copy to user buffer of sample_ptr */
        struct twod_urb_sample *pendingSample;  /* sample partly copied to user space */
};

/* Structure to hold all of our device specific stuff */
struct usb_twod
{
        struct usb_device *udev;        /* the usb device for this device */
        struct usb_interface *interface;        /* the interface for this device */
        struct semaphore sem;   /* lock this structure */

        int is_open;            /* don't allow multiple opens. */

        Tap2D tasValue;         /* TAS value to send to probe (from user ioctl) */
        enum irigClockRates sorRate;
        struct urb *img_urbs[IMG_URBS_IN_FLIGHT];       /* All data read urbs */
        struct urb_circ_buf img_urb_q;

        struct urb *sor_urbs[SOR_URBS_IN_FLIGHT];       /* All sor read urbs */

        struct sample_circ_buf sampleq; /* samples that are ready for reading from user side */
        spinlock_t sampqlock;           /* control sample producer threads */

        struct urb_circ_buf tas_urb_q;  /* queue of TAS samples for writing */

        wait_queue_head_t read_wait;    /* Zzzzz ... */

        size_t img_bulk_in_size;        /* the buffer to receive size */
        size_t sor_bulk_in_size;
        __u8 img_in_endpointAddr;       /* the address of the image in endpoint */
        __u8 tas_out_endpointAddr;      /* the address of the tas out endpoint */
        __u8 sor_in_endpointAddr;       /* the address of the shadow word in endpoint */
        struct usb_twod_stats stats;

        struct read_state readstate;
        size_t debug_cntr;

        struct timer_list urbThrottle;

#ifndef DO_IRIG_TIMING
        struct timer_list sendTASTimer;
        int sendTASJiffies;
#endif
        size_t urbSubmitError;
};
#endif                          // ifdef __KERNEL__
#endif
