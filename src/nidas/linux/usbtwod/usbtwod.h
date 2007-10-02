
#ifndef _nidas_usbtwod_h_
#define _nidas_usbtwod_h_

#include <nidas/linux/types.h>

#ifndef __KERNEL__
/* User programs need this for the _IO macros, but kernel
 * modules get their's elsewhere.
 */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>
#endif

#include <nidas/linux/util.h>

/*
 * Structure of counters that can be queried with the
 * USB2D_GET_STATUS ioctl.
 */
struct usb_twod_stats
{
        unsigned int numImages;
        unsigned int lostImages;
        unsigned int numSORs;
        unsigned int lostSORs;
        unsigned int lostTASs;
        unsigned int urbErrors;
        unsigned int urbTimeouts;
};

/*
 * Struct to adjust probe slice rate for true airspeed
 */
typedef struct _Tap2D
{
        unsigned char ntap;     /* which tap in the variable resistor (0-255) */
        unsigned char vdiv;     /* boolean toggle for voltage divide by 10 */
        unsigned char cntr;     /* counter from 1 to 10 */
        unsigned char dummy;
} Tap2D;

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

/*
 * Both image and shadow-OR samples are read from this device.
 * We place the following 4-byte type values at the beginning of
 * each data sample in big-endian form, so user code can
 * figure out which is which.
 */
#define TWOD_IMG_TYPE	0
#define TWOD_SOR_TYPE	1

#ifdef __KERNEL__
#include <linux/module.h>

#include <nidas/linux/irigclock.h>

/* Get a minor range for your devices from the usb maintainer */
#define USB_TWOD_64_MINOR_BASE     192
#define USB_TWOD_32_MINOR_BASE     196

/* our private defines. if this grows any larger, use your own .h file */
#define MAX_TRANSFER            ( PAGE_SIZE - 512 )

#define TWOD_IMG_BUFF_SIZE          4096
#define TWOD_SOR_BUFF_SIZE          4
#define TWOD_TAS_BUFF_SIZE          3

/* SAMPLE_QUEUE_SIZE must be a power of 2 (required by circ_buf).
 * In order for a circ_buf to tell the difference between empty
 * and full, a full circ_buf contains (size-1) elements.
 * In addition, SAMPLE_QUEUE_SIZE should be at least 
 * IMG_URBS_IN_FLIGHT + SOR_URBS_IN_FLIGHT + 1 
 */
/*
 * Throughput tests on both an Intel core laptop PC and an Arcom Vulcan
 * showed no difference in throughput with the following values
 * for the sample queue sizes and number of urbs in flight:
 * SAMPLE_QUEUE_SIZE IMG_URB_QUEUE_SIZE  IMG_URBS_IN_FLIGHT
 * 	16		8			7
 * 	8		4			3
 * 	4		2			1
 * We'll use the middle values.
 * Having more than one urb in flight means a read()
 * can return more than one image sample.
 */
#define SAMPLE_QUEUE_SIZE   16
#define IMG_URB_QUEUE_SIZE  8   /* power of two */
#define IMG_URBS_IN_FLIGHT   (IMG_URB_QUEUE_SIZE-1)
#define SOR_URBS_IN_FLIGHT   4

#define TAS_URB_QUEUE_SIZE   4   /* power of two */

#if defined(CONFIG_ARCH_VIPER) || defined(CONFIG_MACH_ARCOM_MERCURY)
#define DO_IRIG_TIMING
#endif

struct twod_urb_sample
{
        dsm_sample_time_t timetag;      /* timetag of sample */
        dsm_sample_length_t length;     /* number of bytes in data */
        unsigned long stype;     /* sample type, 0=image, 1=SOR */
        unsigned long data;     /* True Airspeed for image sample */
        int pre_urb_len;  	/* size of sample without urb contents */
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

enum probe_type { TWOD_64, TWOD_32 };

/* Structure to hold all of our device specific stuff */
struct usb_twod
{       
        char   dev_name[40];               /* the device-idProd + (minor-minorbase)*/
        struct usb_device *udev;        /* the usb device for this device */
        struct usb_interface *interface;        /* the interface for this device */
        struct semaphore sem;   /* lock this structure */

        int is_open;            /* don't allow multiple opens. */
	enum probe_type ptype;

        Tap2D tasValue;         /* TAS value to send to probe (from user ioctl) */
        enum irigClockRates sorRate;
        struct urb *img_urbs[IMG_URBS_IN_FLIGHT];       /* All data read urbs */
        struct urb_circ_buf img_urb_q;

        struct urb *sor_urbs[SOR_URBS_IN_FLIGHT];       /* All sor read urbs */

        struct sample_circ_buf sampleq; /* samples that are ready for reading from user side */
        spinlock_t sampqlock;           /* control sample producer threads */

        struct urb_circ_buf tas_urb_q;  /* queue of TAS samples for writing */

        wait_queue_head_t read_wait;    /* Zzzzz ... */

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
        int latencyJiffies;
	unsigned long lastWakeup;
};
#endif                          // ifdef __KERNEL__
#endif
