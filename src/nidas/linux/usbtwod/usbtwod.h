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

#define SOR_DEBUG

/**
 * Statistics gathered by the PMS2D USB driver.  Structure of
 * counters that can be queried with the USB2D_GET_STATUS ioctl.
 */
struct usb_twod_stats
{
        /** Number of 4K buffers transfered from probe */
        unsigned int numImages;
        unsigned int lostImages;
        /** Number of Shadow-ORs transfered from probe */
        unsigned int numSORs;
        unsigned int lostSORs;
        unsigned int lostTASs;
        unsigned int urbErrors;
        unsigned int shutdowns;         /* number of detected indications of USB shutdown */
        unsigned int urbTimeouts;
};

/**
 * Struct to adjust probe slice rate for true airspeed.  True airspeed
 * arrives at the sensor class which then converts it into this struct.
 * It is then passed to the PMS2D USB driver and then on to the probe
 * itself.  This struct is also then recorded with each buffer that
 * comes back from the probe.
 *
 * This covers versions of the probe through June 22nd 2009.  CJW.
 */
typedef struct _Tap2D_v1
{
        /** which tap in the variable resistor (0-255) */
        unsigned char ntap;
        /** boolean toggle for frequency divide by 10 */
        unsigned char div10;
        /** counter from 1 to 10 */
        unsigned char cntr;
        unsigned char dummy;
} Tap2Dv1;

/**
 * This version is for rev2 of Spowarts USB board.  Gives higher TAS
 * resolution.
 */
typedef struct _Tap2D_v2
{
        /** which tap in the variable resistor (0-255) */
        unsigned short ntap;
        /** boolean toggle for frequency divide by 10 */
        unsigned char div10;

        /** counter from 1 to 10 */
        unsigned char cntr;
} Tap2D;

/*
 * Believe this struct is unnecessary
 */
typedef struct _Tap2D_v3
{
        /** sending tas*10, ntap calculation happens on probe*/
        unsigned short tas;
        /** which probe resolution*/
        unsigned short probeResolution;
} Tap2D_v3;

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
#define TWOD_SOR_TYPE   1
#define TWOD_SORv3_TYPE 0x534f522c /* "SOR," (actually ",ROS" on little endian) */
#define TWOD_IMGv2_TYPE	2
#define TWOD_IMGv3_TYPE	3

#ifdef __KERNEL__
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/kref.h>

/* 64 bit probes will have minor numbers starting at 192,
 * 32 bit probes will have minor numbers starting at 196.
 * On the vulcan, running 2.6.11 kernel without a udev daemon,
 * you have to mknod the devices by hand:
 * mknod /dev/usbtwod_64_0 c 180 192
 * mknod /dev/usbtwod_64_1 c 180 193
 * mknod /dev/usbtwod_64_2 c 180 194
 * mknod /dev/usbtwod_64_3 c 180 195
 * mknod /dev/usbtwod_32_0 c 180 196
 * etc...
 * On systems with a udev daemon, these device files are created
 * automatically when the device is connected, using the
 * name member of the struct usb_class_driver, which
 * is passed to usb_register_dev() in the probe method.
 */
#define USB_TWOD_64_V3_MINOR_BASE  192
#define USB_TWOD_64_MINOR_BASE     192
#define USB_TWOD_32_MINOR_BASE     196

#define TWOD_IMG_BUFF_SIZE	4096    /* Optimal that it's PAGE_SIZE */
#define TWOD_SOR_BUFF_SIZE      128     /* PRS doc has 70 as max possible size of housekeeping*/
#define TWOD_TAS_BUFF_SIZE	4

/*
 * SAMPLE_QUEUE_SIZE is the number of image and SOR samples
 * that can be queued for user reads.
 *
 * SAMPLE_QUEUE_SIZE must be a power of 2 (required by circ_buf).
 * In order for a circ_buf to tell the difference between empty
 * and full, a full circ_buf contains (size-1) elements.
 *
 * An entry in the queue is sizeof(struct twod_urb_sample), either
 * 24 or 32 bytes (if 64 bit pointers), so it's not big.
 *
 * To handle the situation where the user reads might get far behind,
 * make it at least the minimum power of two greater than
 * IMG_URBS_IN_FLIGHT + SOR_URBS_IN_FLIGHT, but
 * there isn't an advantage to make it more than that.
 */
/*
 * Throughput tests on both an Intel core laptop PC and an Arcom Vulcan
 * showed no difference in throughput with the following values
 * for the sample queue sizes and number of urbs in flight:
 * SAMPLE_QUEUE_SIZE IMG_URB_QUEUE_SIZE  IMG_URBS_IN_FLIGHT
 * 	16		8			7
 * 	8		4			3
 * 	4		2			1
 */
#define SAMPLE_QUEUE_SIZE   32   /* power of two */

/*
 * Size of queue used by throttle rate function.
 * A power of two that is also greater than IMG_URBS_IN_FLIGHT.
 */
#define IMG_URB_QUEUE_SIZE  32

/*
 * A TWOD_IMG_BUFF_SIZE buffer is pre-allocated for every image
 * URB in flight, so don't make it too big.
 *
 * It should not be greater than (IMG_URB_QUEUE_SIZE-1), but
 * can be less than that, for example IMG_URB_QUEUE_SIZE=32 and
 * IMG_URBS_IN_FLIGHT=20.
 */
#define IMG_URBS_IN_FLIGHT   20

/*
 * Maximum invocation rate of the throttle function.
 * Should divide evenly into HZ.
 * HZ is 250 on Vortex, 100 on old systems, 1000 on new X86.
 *     grep 'CONFIG_HZ=' /boot/config-$(uname -r)
 * For a requested throttle rate, the function will
 * submit a number of urbs (up to IMG_URBS_IN_FLIGHT)
 * to achieve the requested rate.
 */
#define MAX_THROTTLE_FUNC_RATE 10

/*
 * This is the maximun throttle rate supported, which is limited
 * by MAX_THROTTLE_FUNC_RATE * IMG_URBS_IN_FLIGHT.
 * To achieve higher rates you need to increase MAX_THROTTLE_FUNC_RATE
 * (consuming more CPU) or IMG_URBS_QUEUE_SIZE (consuming more memory).
 */
#define MAX_THROTTLE_RATE (MAX_THROTTLE_FUNC_RATE * IMG_URBS_IN_FLIGHT)

#define SOR_URBS_IN_FLIGHT   4

#define TAS_URB_QUEUE_SIZE   4   /* power of two */

struct twod_urb_sample
{
        dsm_sample_time_t timetag;      /* timetag of sample */
        dsm_sample_length_t length;     /* number of bytes in data */
        unsigned int stype;     /* sample type, 0=image, 1=SOR */
        unsigned int data;     /* True Airspeed for image sample */
        int pre_urb_len;  	/* size of sample without urb contents */
        struct urb *urb;
};
struct twod_house_urb_sample
{
        int serialNum;
        dsm_sample_time_t timetag;
        unsigned int stype; //sor
        unsigned int data; //dof or
        int temperature;
        double levelD1;
        double levelD32;
        double levelD64;
};

struct sample_circ_buf
{
        struct twod_urb_sample **buf;
        int head;
        int tail;
};

struct urb_circ_buf
{
        struct urb **buf;
        int head;
        int tail;
};


/* reading status keeper  */
struct read_state
{
        char *dataPtr;          /* pointer into data of what is left to copy to user */
        size_t bytesLeft;       /* How much is left to copy to user buffer of sample_ptr */
        struct twod_urb_sample *pendingSample;  /* sample partly copied to user space */
};

enum twod_probe_type { TWOD_64_V3, TWOD_64, TWOD_32 };

/* Structure to hold all of our device specific stuff */
struct usb_twod
{       
        struct usb_device *udev;        /* the usb device for this device */
        struct usb_interface *interface;        /* the interface for this device */
        struct kref kref;               /* reference counter for this structure */
        rwlock_t usb_iface_lock;        /* for detection of whether disconnect has been called */

        char   dev_name[64];           /* device name for log messages */

        __u8 img_in_endpointAddr;       /* the address of the image in endpoint */
        __u8 tas_out_endpointAddr;      /* the address of the TAS out endpoint */
        __u8 sor_in_endpointAddr;       /* the address of the SOR in endpoint */
        int is_open;                   /* don't allow multiple opens. */

	enum twod_probe_type ptype;

        struct urb *img_urbs[IMG_URBS_IN_FLIGHT];       /* All data read urbs */
        struct urb_circ_buf img_urb_q;

        struct urb *sor_urbs[SOR_URBS_IN_FLIGHT];       /* All sor read urbs */

        struct sample_circ_buf sampleq; /* samples that are ready for reading from user side */
        spinlock_t sampqlock;           /* control sample producer threads */

        struct urb_circ_buf tas_urb_q;  /* queue of TAS samples for writing */

        wait_queue_head_t read_wait;    /* Zzzzz ... */

        struct usb_twod_stats stats;    /* various I/O counter for info */

        struct read_state readstate;    /* leftovers from past read */

        int errorStatus;                /* current error value */

        struct timer_list urbThrottle;
        
        int throttleJiffies;            /* if throttling jiffie wait between
                                            timer events */

        int nurbPerTimer;               /* if throttling, how many urbs to submit on
                                            every timer event */

        int latencyJiffies;             /* maximum time user wants to wait
                                         * between reads */

	unsigned long lastWakeup;       /* time of last read queue wakeup */

        struct timer_list sendTASTimer; /* kernel timer for sending true airspeed */
        int sendTASJiffies;             /* when to send the next TAS */

        spinlock_t taslock;             /* control access to TAS value */

        Tap2D tasValue;                 /* TAS value to send to probe (from user ioctl) */

        int consecTimeouts;
#ifdef SOR_DEBUG
        int SORdebugmessages;
#endif
};
#endif                          // ifdef __KERNEL__
#endif
