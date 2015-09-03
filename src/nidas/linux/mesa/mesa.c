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
/* mesa.c

   Linux module for interfacing the Mesa Electronics
   4I34(M) Anything I/O FPGA card.

   Original Author: Mike Spowart

   Implementation notes:

*/

// Linux module includes...
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/module.h>
#include <linux/version.h>
#include <asm/io.h>
#include <linux/poll.h>
#include <linux/ioport.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/slab.h>		/* kmalloc, kfree */

#include <nidas/linux/mesa.h>
#include <nidas/linux/irigclock.h>
#include <nidas/linux/klog.h>
#include <nidas/linux/util.h>
#include <nidas/linux/isa_bus.h>
#include <nidas/linux/SvnInfo.h>    // SVNREVISION

#ifndef SVNREVISION
#define SVNREVISION "unknown"
#endif

MODULE_AUTHOR("Mike Spowart <spowart@ucar.edu>");
MODULE_DESCRIPTION("Mesa ISA driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(SVNREVISION);

static struct MESA_Board *boards = 0;

/* number of Mesa boards in system (number of non-zero ioport values) */
static int numboards = 0;
/* Set the base addresses of any Mesa 4I34 card */
static unsigned int ioport[MESA_4I34_MAX_NR_DEVS] = { 0x220, 0, 0, 0 };

#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(ioport, int, &numboards, S_IRUGO);
#else
module_param_array(ioport, int, numboards, S_IRUGO);
#endif
MODULE_PARM_DESC(ioport, "ISA memory base of each board (default 0x220)");

#define DEVNAME_MESA "mesa"
static dev_t mesa_device = MKDEV(0, 0);
static struct cdev mesa_cdev;

#define MESA_CNTR_SAMPLE_QUEUE_SIZE 128
#define MESA_RADAR_SAMPLE_QUEUE_SIZE 32
#define MESA_P260X_SAMPLE_QUEUE_SIZE 32

/* -- IRIG CALLBACK --------------------------------------------------- */
static void read_counter(void *ptr)
{
        struct MESA_Board* brd = (struct MESA_Board*) ptr;
        struct dsm_sample* samp;
        unsigned short *dp;
        int i,read_address_offset;

        samp = GET_HEAD(brd->cntr_samples,MESA_CNTR_SAMPLE_QUEUE_SIZE);
        if (!samp) {                // no output sample available
                brd->status.missedSamples++;
                KLOG_WARNING("%s: missedSamples=%d\n",
                        brd->devName,brd->status.missedSamples);
                // read and discard data
                read_address_offset = COUNT0_READ_OFFSET;
                for (i = 0; i < brd->nCounters; i++) {
                        // read from the counter channel
                        inw_16o(brd->addr + read_address_offset + ISA_16BIT_ADDR_OFFSET);
                        read_address_offset = COUNT1_READ_OFFSET;
                }
                return;
        }

        samp->timetag = GET_MSEC_CLOCK;
        samp->length = (1 + brd->nCounters) * sizeof(short);
        dp = (unsigned short *)samp->data;
        *dp++ = cpu_to_le16(ID_COUNTERS);

        read_address_offset = COUNT0_READ_OFFSET;
        for (i = 0; i < brd->nCounters; i++) {
                // read from the counter channel
                *dp = cpu_to_le16(inw_16o(brd->addr + read_address_offset));
//KLOG_INFO("chn: %d  sample.data: %d\n", i, *dp);
                dp++;
                read_address_offset = COUNT1_READ_OFFSET;
        }

        /* increment head, this sample is ready for consumption */
        INCREMENT_HEAD(brd->cntr_samples,MESA_CNTR_SAMPLE_QUEUE_SIZE);
	if (((long)jiffies - (long)brd->lastWakeup) > brd->latencyJiffies ||
		CIRC_SPACE(brd->cntr_samples.head,brd->cntr_samples.tail,
		MESA_CNTR_SAMPLE_QUEUE_SIZE) < MESA_CNTR_SAMPLE_QUEUE_SIZE/2) {
		wake_up_interruptible(&brd->rwaitq);
		brd->lastWakeup = jiffies;
	}
}

/* -- IRIG CALLBACK --------------------------------------------------- */
static void read_radar(void *ptr)
{
        struct MESA_Board* brd = (struct MESA_Board*) ptr;
        struct dsm_sample* samp;
        unsigned short *dp;
        struct radar_state *rstate = &brd->rstate;
        unsigned short rdata;

        switch(rstate->ngood) {
        case 0:
                rstate->timetag = GET_MSEC_CLOCK;
                rstate->prevData = inw_16o(brd->addr + RADAR_READ_OFFSET);
                rstate->ngood++;
                break;
        case 1:
                rdata = inw_16o(brd->addr + RADAR_READ_OFFSET);
                if (rdata == rstate->prevData) rstate->ngood++;
                else {
                        rstate->timetag = GET_MSEC_CLOCK;
                        rstate->prevData = rdata;       // leave ngood as 1
                }
                break;
        }

        if (++rstate->npoll == rstate->NPOLL) {
                samp = GET_HEAD(brd->radar_samples,MESA_RADAR_SAMPLE_QUEUE_SIZE);
                if (!samp) {                // no output sample available
                        brd->status.missedSamples++;
                        KLOG_WARNING("%s: missedSamples=%d\n",
                                brd->devName,brd->status.missedSamples);
                        rstate->ngood = 0;
                        rstate->npoll = 0;
                        return;
                }
                samp->timetag = rstate->timetag;
                samp->length = 2 * sizeof(short);
                dp = (unsigned short *)samp->data;
                *dp++ = cpu_to_le16(ID_RADAR);
                /*
                 * ngood cannot be 0 here
                 * if ngood is 1 then we haven't read two matching values
                 *      but we output a sample with a data value of 0 anyway.
                 *      0's in the output data means we're not
                 *      getting matching reads from the register.
                 * if ngood is 2 the data is good, send it out.
                 */
                if (rstate->ngood == 2)
                        *dp++ = cpu_to_le16(rstate->prevData);
                else *dp++ = 0;
                /* increment head, this sample is ready for consumption */
                INCREMENT_HEAD(brd->radar_samples,
                    MESA_RADAR_SAMPLE_QUEUE_SIZE);
                if (((long)jiffies - (long)brd->lastWakeup) > brd->latencyJiffies ||
                        CIRC_SPACE(brd->radar_samples.head,brd->radar_samples.tail,
                        MESA_RADAR_SAMPLE_QUEUE_SIZE) < MESA_RADAR_SAMPLE_QUEUE_SIZE/2) {
                        wake_up_interruptible(&brd->rwaitq);
                        brd->lastWakeup = jiffies;
                }

                rstate->ngood = 0;
                rstate->npoll = 0;
        }
}

/* -- IRIG CALLBACK --------------------------------------------------- */
static void read_260x(void *ptr)
{
        struct MESA_Board* brd = (struct MESA_Board*) ptr;
        struct dsm_sample* samp;
        unsigned short *dp;
#ifdef HOUSE_260X
        unsigned short *dphouse;
#endif
        int i;

        /* a short output value for id,strobes,resets and the bins */
        int nshort = 3 + TWO_SIXTY_BINS;
#ifdef HOUSE_260X
        nshort += 8;
#endif

        samp = GET_HEAD(brd->p260x_samples,MESA_P260X_SAMPLE_QUEUE_SIZE);
        if (!samp) {                // no output sample available
                brd->status.missedSamples++;
                KLOG_WARNING("%s: missedSamples=%d\n",
                        brd->devName,brd->status.missedSamples);
                inw_16o(brd->addr + STROBES_OFFSET);
                inw_16o(brd->addr + TWOSIXTY_RESETS_OFFSET);
                for (i = 0; i < TWO_SIXTY_BINS; ++i)
                        inw_16o(brd->addr + HISTOGRAM_READ_OFFSET);
                inb(brd->addr + HISTOGRAM_CLEAR_OFFSET);
#ifdef HOUSE_260X
                for (i = 0; i < 8; ++i) {
                        inw_16o(brd->addr + HOUSE_READ_OFFSET);
                        inb(brd->addr + HOUSE_ADVANCE_OFFSET);
                }
                inb(brd->addr + HOUSE_RESET_OFFSET);
#endif
                return;
        }

        samp->timetag = GET_MSEC_CLOCK;
        samp->length = nshort * sizeof(short);
        dp = (unsigned short *)samp->data;
        *dp++ = cpu_to_le16(ID_260X);
        *dp++ = cpu_to_le16(inw_16o(brd->addr + STROBES_OFFSET));
        *dp++ = cpu_to_le16(inw_16o(brd->addr + TWOSIXTY_RESETS_OFFSET));
#ifdef HOUSE_260X
        dphouse = dp;
        dp += 8;
#endif
        // read 260X histogram data
        for (i = 0; i < TWO_SIXTY_BINS; ++i)
                *dp++ = cpu_to_le16(inw_16o(brd->addr + HISTOGRAM_READ_OFFSET));
        inb(brd->addr + HISTOGRAM_CLEAR_OFFSET);

        // read 260X housekeeping data
#ifdef HOUSE_260X
        for (i = 0; i < 8; ++i) {
                *dphouse++ = cpu_to_le16(inw_16o(brd->addr + HOUSE_READ_OFFSET));
                inb(brd->addr + HOUSE_ADVANCE_OFFSET);
        }

        inb(brd->addr + HOUSE_RESET_OFFSET);
#endif
        /* increment head, this sample is ready for consumption */
        INCREMENT_HEAD(brd->p260x_samples,MESA_P260X_SAMPLE_QUEUE_SIZE);
	if (((long)jiffies - (long)brd->lastWakeup) > brd->latencyJiffies ||
		CIRC_SPACE(brd->p260x_samples.head,brd->p260x_samples.tail,
		MESA_P260X_SAMPLE_QUEUE_SIZE) < MESA_P260X_SAMPLE_QUEUE_SIZE/2) {
		wake_up_interruptible(&brd->rwaitq);
		brd->lastWakeup = jiffies;
	}
}

/* -- UTILITY --------------------------------------------------------- */

static void outportbwswap(struct MESA_Board *brd, unsigned char thebyte)
{
        static unsigned char swaptab[256] = {
                0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90,
                    0x50, 0xD0,
                0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8,
                    0x68, 0xE8,
                0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84,
                    0x44, 0xC4,
                0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4,
                    0x74, 0xF4,
                0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C,
                    0x5C, 0xDC,
                0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2,
                    0x62, 0xE2,
                0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A,
                    0x4A, 0xCA,
                0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA,
                    0x7A, 0xFA,
                0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96,
                    0x56, 0xD6,
                0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE,
                    0x6E, 0xEE,
                0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81,
                    0x41, 0xC1,
                0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1,
                    0x71, 0xF1,
                0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99,
                    0x59, 0xD9,
                0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5,
                    0x65, 0xE5,
                0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D,
                    0x4D, 0xCD,
                0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD,
                    0x7D, 0xFD,
                0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93,
                    0x53, 0xD3,
                0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB,
                    0x6B, 0xEB,
                0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87,
                    0x47, 0xC7,
                0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7,
                    0x77, 0xF7,
                0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F,
                    0x5F, 0xDF,
                0x3F, 0xBF, 0x7F, 0xFF
        };

        outb(swaptab[thebyte], brd->addr + R_4I34DATA);
}

/*---------------------------------------------------------------------------*/
/* dsm/modules/mesa.c: load_start: outb(5) */
/* dsm/modules/mesa.c: load_start: inb = 220 */
/* dsm/modules/mesa.c: load_start: outb(10) */
static int load_start(struct MESA_Board *brd)
{
        unsigned char config, status;
        unsigned long j;

        config = M_4I34CFGCSOFF | M_4I34CFGINITASSERT |
            M_4I34CFGWRITEDISABLE | M_4I34LEDOFF;

        outb(config, brd->addr + R_4I34CONTROL);
        KLOG_DEBUG("outb(%#x)\n", config);
        status = inb(brd->addr + R_4I34STATUS);
        KLOG_DEBUG("inb = %#x\n", status);

        /* Note that if we see DONE at the start of programming, it's most likely due
         * to an attempt to access the 4I34 at the wrong I/O location.
         */
        if (status & M_4I34PROGDUN) {
                KLOG_ERR
                    ("failed - attempted to access the 4I34 at the wrong I/O location?\n");
                return -ENODEV;
        }
        config = M_4I34CFGCSON | M_4I34CFGINITDEASSERT |
            M_4I34CFGWRITEENABLE | M_4I34LEDON;

        outb(config, brd->addr + R_4I34CONTROL);
        KLOG_DEBUG("outb(%#x)\n", config);

        // Multi task for 100 us
        /* Delay 100 uS. */
        status = inb(brd->addr + R_4I34STATUS);
        j = jiffies + 1;
        while (time_before(jiffies, j))
                schedule();
        KLOG_DEBUG("load_start done, status=%#x\n", status);

        brd->progNbytes = 0;

        return 0;
}

/* -- UTILITY --------------------------------------------------------- */
static int load_finish(struct MESA_Board *brd)
{
        int waitcount, count, ret = -EIO;
        unsigned char config;
        int success = 0;
        unsigned long j;

        KLOG_DEBUG("load_finish, program nbytes=%d\n", brd->progNbytes);
        config = M_4I34CFGCSOFF | M_4I34CFGINITDEASSERT |
            M_4I34CFGWRITEDISABLE | M_4I34LEDON;
        outb(config, brd->addr + R_4I34CONTROL);

        // Wait for Done bit set
        for (waitcount = 0; waitcount < 200; ++waitcount) {
                // this is not called from a real-time thread, so use jiffies
                // to delay
                unsigned char status;
                if ((status =
                     inb(brd->addr + R_4I34STATUS)) & M_4I34PROGDUN) {
                        KLOG_INFO
                            ("waitcount: %d, status=%#x,donebit=%#x\n",
                             waitcount, status, M_4I34PROGDUN);
                        success = 1;
                        break;
                }
                KLOG_INFO("waitcount: %d, status=%#x,donebit=%#x\n",
                            waitcount, status, M_4I34PROGDUN);
                j = jiffies + 1;  // 1/100 of a second.
                while (time_before(jiffies, j))
                        schedule();
        }

        if (success) {
                KLOG_NOTICE("FPGA programming done.\n");

                // Indicate end of programming by turning off 4I34 led
                config = M_4I34CFGCSOFF | M_4I34CFGINITDEASSERT |
                    M_4I34CFGWRITEDISABLE | M_4I34LEDOFF;
                outb(config, brd->addr + R_4I34CONTROL);

                // Now send out extra configuration completion clocks
                for (count = 24; count != 0; --count)
                        outb(0xFF, brd->addr + R_4I34DATA);

                ret = 0;
        } else
                KLOG_ERR("FPGA programming not successful.\n");

        return ret;
}

static int load_program(struct MESA_Board *brd, struct mesa_prog *buf)
{
        int i, ret = buf->len;
        unsigned char *bptr;

        // Now program the FPGA
        bptr = buf->buffer;
        for (i = 0; i < buf->len; i++)
                outportbwswap(brd, *bptr++);
        brd->progNbytes += buf->len;

        return ret;
}

static int close_ports(struct MESA_Board *brd)
{
        int ret = 0;
        // unregister poll function from IRIG module
        if (brd->cntrCallback) {
                ret = unregister_irig_callback(brd->cntrCallback);
                if (ret < 0) return ret;
                brd->cntrCallback = 0;
                KLOG_DEBUG("unregistered read_counter() from IRIG.\n");
        }
        // unregister poll function from IRIG module
        if (brd->radarCallback) {
                ret = unregister_irig_callback(brd->radarCallback);
                if (ret < 0) return ret;
                brd->radarCallback = 0;
                KLOG_DEBUG("unregistered read_radar() from IRIG.\n");
        }
        // unregister poll function from IRIG module
        if (brd->p260xCallback) {
                ret = unregister_irig_callback(brd->p260xCallback);
                if (ret < 0) return ret;
                brd->p260xCallback = 0;
                KLOG_DEBUG("unregistered read_260x() from IRIG.\n");
        }
        ret = flush_irig_callbacks();

        if (brd->cntr_samples.buf)
                free_dsm_circ_buf(&brd->cntr_samples);
        brd->cntr_samples.buf = 0;

        if (brd->radar_samples.buf)
                free_dsm_circ_buf(&brd->radar_samples);
        brd->radar_samples.buf = 0;

        if (brd->p260x_samples.buf)
                free_dsm_circ_buf(&brd->p260x_samples);
        brd->p260x_samples.buf = 0;
        return ret;
}


/*
 * Implemention of open fops.
 */
static int mesa_open(struct inode *inode, struct file *filp)
{
        struct MESA_Board *brd;
        int ib = iminor(inode);
        if (ib >= numboards) return -ENXIO;

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        brd = boards + ib;
        // enforce exclusive open
        if (!atomic_dec_and_test(&brd->available)) {
                atomic_inc(&brd->available);
                return -EBUSY; /* already open */
        }

        filp->private_data = brd;
        brd->cntr_samples.head = brd->cntr_samples.tail = 0;
        brd->radar_samples.head = brd->radar_samples.tail = 0;
        brd->p260x_samples.head = brd->p260x_samples.tail = 0;

        memset(&brd->cntr_read_state,0,
            sizeof(struct sample_read_state));
        memset(&brd->radar_read_state,0,
            sizeof(struct sample_read_state));
        memset(&brd->p260x_read_state,0,
            sizeof(struct sample_read_state));

	/* reads will be notified of new data every 250 milliseconds */
        brd->latencyJiffies = (250 * HZ) / MSECS_PER_SEC;
        if (brd->latencyJiffies == 0) brd->latencyJiffies = HZ / 4;
        KLOG_INFO("%s: latencyJiffies=%ld, HZ=%d\n",
                    brd->devName,brd->latencyJiffies,HZ);
        brd->lastWakeup = jiffies;

        brd->progNbytes = 0;
        brd->nCounters = 0;
        brd->nRadars = 0;
        brd->n260X = 0;

        memset(&brd->status,0,sizeof(brd->status));

        return 0;
}

/*
 * Implemention of release (close) fops.
 */
static int mesa_release(struct inode *inode, struct file *filp)
{
        struct MESA_Board *brd = (struct MESA_Board*) filp->private_data;
        atomic_inc(&brd->available);
        return close_ports(brd);
}

/*
 * Utility for poll and read fops.
 * Return 0: no data available for read
 *          1: data available for read
 */
static inline int data_ready(struct MESA_Board* brd)
{
        return brd->cntr_read_state.bytesLeft > 0 ||
            brd->cntr_samples.head != brd->cntr_samples.tail ||
            brd->radar_read_state.bytesLeft > 0 ||
            brd->radar_samples.head != brd->radar_samples.tail ||
            brd->p260x_read_state.bytesLeft > 0 ||
            brd->p260x_samples.head != brd->p260x_samples.tail;
}

/*
 * Implementation of poll fops.
 */
static unsigned int mesa_poll(struct file *filp, poll_table *wait)
{
        struct MESA_Board *brd = (struct MESA_Board*) filp->private_data;
        unsigned int mask = 0;

        poll_wait(filp, &brd->rwaitq, wait);
        if (data_ready(brd)) mask |= POLLIN | POLLRDNORM;    /* readable */
        return mask;
}

/*
 * Implementation of read fops.
 */
static ssize_t mesa_read(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct MESA_Board *brd = (struct MESA_Board*) filp->private_data;
        size_t countreq = count;
        ssize_t cntres = 0;

        if(!data_ready(brd)) {
                if (filp->f_flags & O_NONBLOCK) return -EAGAIN;
                if (wait_event_interruptible(brd->rwaitq,
                      data_ready(brd))) return -ERESTARTSYS;
        }

        if (brd->cntr_read_state.bytesLeft > 0 ||
            brd->cntr_samples.head != brd->cntr_samples.tail) {
            cntres = nidas_circbuf_read(filp,buf,count,&brd->cntr_samples,
                    &brd->cntr_read_state,&brd->rwaitq);
            if (cntres < 0) return cntres;
            count -= cntres;
            buf += cntres;
        }
        if (count > 0 && (brd->radar_read_state.bytesLeft > 0 ||
            brd->radar_samples.head != brd->radar_samples.tail)) {
            cntres = nidas_circbuf_read(filp,buf,count,&brd->radar_samples,
                    &brd->radar_read_state,&brd->rwaitq);
            if (cntres < 0) return cntres;
            count -= cntres;
            buf += cntres;
        }
        if (count > 0 && (brd->p260x_read_state.bytesLeft > 0 ||
            brd->p260x_samples.head != brd->p260x_samples.tail)) {
            cntres = nidas_circbuf_read(filp,buf,count,&brd->p260x_samples,
                    &brd->p260x_read_state,&brd->rwaitq);
            if (cntres < 0) return cntres;
            count -= cntres;
            buf += cntres;
        }
        return countreq - count;
}

/*
 * Implementation of ioctl fops.
 */
static long mesa_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        struct MESA_Board *brd = (struct MESA_Board*) filp->private_data;
        int ret = -EINVAL;
        void __user *userptr = (void __user *) arg;

        /* don't decode wrong cmds: better returning
         * ENOTTY than EFAULT */
        if (_IOC_TYPE(cmd) != MESA_MAGIC) return -ENOTTY;
        if (_IOC_NR(cmd) > MESA_IOC_MAXNR) return -ENOTTY;
        /*
         * the type is a bitmask, and VERIFY_WRITE catches R/W
         * transfers. Note that the type is user-oriented, while
         * verify_area is kernel-oriented, so the concept of "read" and
         * "write" is reversed
         */
        if (_IOC_DIR(cmd) & _IOC_READ)
                ret = !access_ok(VERIFY_WRITE, userptr,_IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                ret =  !access_ok(VERIFY_READ, userptr, _IOC_SIZE(cmd));
        else ret = 0;
        if (ret) return -EFAULT;

        switch (cmd) {
        case MESA_LOAD_START:
                KLOG_DEBUG("MESA_FPGA_START\n");
                ret = load_start(brd);
                break;

        case MESA_LOAD_BLOCK:
                KLOG_DEBUG("MESA_FPGA_LOAD\n");
                {
                        struct mesa_prog prog;
                        if (copy_from_user(&prog,userptr,
                                sizeof(struct mesa_prog))) return -EFAULT;
                        ret = load_program(brd,&prog);
                }
                break;
        case MESA_LOAD_DONE:
                KLOG_DEBUG("MESA_FPGA_DONE\n");
                ret = load_finish(brd);
                break;
        case DIGITAL_IN_SET:
                KLOG_DEBUG("DIGITAL_IN not implmented.\n");
                ret = -EINVAL;
                break;
        case COUNTERS_SET:
                {
                        // start counter callback
                        struct counters_set cset;
                        enum irigClockRates rate;

                        if (brd->cntrCallback)
                            unregister_irig_callback(brd->cntrCallback);
			brd->cntrCallback = 0;

                        if (copy_from_user(&cset,userptr,
                                sizeof(struct counters_set))) return -EFAULT;
                        if (cset.nChannels > N_COUNTERS) return -EINVAL;
                        brd->nCounters = cset.nChannels;

                        if (brd->cntr_samples.buf)
                                free_dsm_circ_buf(&brd->cntr_samples);
                        ret = alloc_dsm_circ_buf(&brd->cntr_samples,
                                    (brd->nCounters + 1) * sizeof(short),
                                    MESA_CNTR_SAMPLE_QUEUE_SIZE);
                        if (ret < 0) break;
                        brd->cntr_samples.head = brd->cntr_samples.tail = 0;
                        memset(&brd->cntr_read_state,0,
                            sizeof(struct sample_read_state));

                        rate = irigClockRateToEnum(cset.rate);
                        if (rate == IRIG_NUM_RATES) return -EINVAL;
                        brd->cntrCallback =
                            register_irig_callback(read_counter,0,rate,brd,&ret);
                        if (!brd->cntrCallback) break;
                        ret = 0;
                }
                break;
        case RADAR_SET:
                {
                        // create and open radar data FIFOs
                        struct radar_set rset;

                        if (brd->radarCallback)
                                unregister_irig_callback(brd->radarCallback);
                        brd->radarCallback = 0;

                        if (copy_from_user(&rset,userptr,
                                sizeof(struct radar_set))) return -EFAULT;

                        if (rset.nChannels > N_RADARS) return -EINVAL;

                        memset(&brd->rstate, 0, sizeof(struct radar_state));
                        // how many 100Hz callbacks in 1/rate seconds
                        /*
                         * We need to read the radar value 2 or 3 times so
                         * NPOLL can't be less then 4, which is a sampling
                         * rate of 25 Hz.
                         */
                        brd->rstate.NPOLL = MSECS_PER_SEC / rset.rate / 10;
                        if (brd->rstate.NPOLL < 4) {
                                KLOG_ERR
                                    ("%s: RADAR_SET error: sample rate=%d exceeds maximum rate of 25 Hz\n",
                                     brd->devName,rset.rate);
                                ret = -EINVAL;
                                break;
                        }
                        brd->nRadars = rset.nChannels;
                        if (brd->radar_samples.buf)
                                free_dsm_circ_buf(&brd->radar_samples);
                        ret = alloc_dsm_circ_buf(&brd->radar_samples,
                                    (brd->nRadars + 1) * sizeof(short),
                                    MESA_RADAR_SAMPLE_QUEUE_SIZE);
                        if (ret < 0) break;
                        brd->radar_samples.head = brd->radar_samples.tail = 0;
                        memset(&brd->radar_read_state,0,
                            sizeof(struct sample_read_state));

                        /*
                         * We schedule read_radar to be called every 100 Hz,
                         * since we have to read the data more than once to
                         * make sure it is OK.  Then every NPOLL times that
                         * the read_radar function is called, we output a
                         * sample.
                         */
                        brd->radarCallback =
                                register_irig_callback(read_radar,0,
                                    IRIG_100_HZ,brd, &ret);
                        if (!brd->radarCallback) break;
                        ret = 0;
                }
                break;
        case PMS260X_SET:
                {
                        struct pms260x_set pset;
                        enum irigClockRates rate;
                        int nshort;

                        if (brd->p260xCallback)
                            unregister_irig_callback(brd->p260xCallback);
                        brd->p260xCallback = 0;

                        if (copy_from_user(&pset,userptr,
                                sizeof(struct pms260x_set))) return -EFAULT;
                        if (pset.nChannels > N_PMS260X) return -EINVAL;
                        brd->n260X = pset.nChannels;

                        if (brd->p260x_samples.buf)
                                free_dsm_circ_buf(&brd->p260x_samples);
                        nshort = 3 + TWO_SIXTY_BINS;
#ifdef HOUSE_260X
                        nshort += 8;
#endif
                        ret = alloc_dsm_circ_buf(&brd->p260x_samples,
                                    nshort * sizeof(short),
                                    MESA_P260X_SAMPLE_QUEUE_SIZE);
                        if (ret < 0) break;
                        brd->p260x_samples.head = brd->p260x_samples.tail = 0;
                        memset(&brd->p260x_read_state,0,
                            sizeof(struct sample_read_state));

                        // register poll routine with the IRIG driver
                        rate = irigClockRateToEnum(pset.rate);
                        if (rate == IRIG_NUM_RATES) return -EINVAL;
                        brd->p260xCallback =
                            register_irig_callback(read_260x,0,rate,brd, &ret);
                        if (!brd->p260xCallback) break;
                        ret = 0;
                }
                break;
        case MESA_STOP:
                KLOG_DEBUG("MESA_STOP\n");
                ret = close_ports(brd);
                break;

        default:
                ret = -ENOTTY;
                break;
        }
        return ret;
}

static struct file_operations mesa_fops = {
        .owner   = THIS_MODULE,
        .read    = mesa_read,
        .poll    = mesa_poll,
        .open    = mesa_open,
        .unlocked_ioctl   = mesa_ioctl,
        .release = mesa_release,
        .llseek  = no_llseek,
};

/* -- MODULE ---------------------------------------------------------- */
/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
static void mesa_cleanup(void)
{
        int ib;

        if (boards) {

                for (ib = 0; ib < numboards; ib++) {
                        struct MESA_Board *brd = boards + ib;
                        close_ports(brd);
                        if (brd->addr)
                                release_region(brd->addr, MESA_REGION_SIZE);
                }
                kfree(boards);
                boards = 0;
        }

        if (MAJOR(mesa_cdev.dev) != 0) cdev_del(&mesa_cdev);
        if (MAJOR(mesa_device) != 0)
                unregister_chrdev_region(mesa_device, numboards);
}

/* -- MODULE ---------------------------------------------------------- */
static int __init mesa_init(void)
{
        int ib;
        int error = -EINVAL;
        unsigned long addr;

        boards = 0;

        KLOG_NOTICE("version: %s\n", SVNREVISION);

        // When using gcc-4.9 to build against newer linux kernels,
        // the compiler option "-Werror=date-time" is in effect.
        // This option causes a compile error:
        //      macro "__DATE__" might prevent reproducible builds [-Werror=date-time]
        // when it encounters __DATE__ and __TIME__.
        // One can prevent the error by passing "-Wnoerror=date-time",
        // but older compilers cannot parse that option. We could try
        // testing for gcc and/or kernel version, but SVNREVISION
        // should provide enough information, and so we'll comment this:
        // KLOG_NOTICE("compiled on %s at %s\n", __DATE__, __TIME__);

        for (ib = 0; ib < MESA_4I34_MAX_NR_DEVS; ib++)
                if (ioport[ib] == 0) break;
        numboards = ib;

        if (numboards == 0) {
                KLOG_ERR("No boards configured, all ioports[]==0\n");
                goto err;
        }

        error = alloc_chrdev_region(&mesa_device, 0, numboards,
                                 DEVNAME_MESA);
        if (error < 0) goto err;

        error = -ENOMEM;
        boards =
            kmalloc(numboards * sizeof(struct MESA_Board), GFP_KERNEL);
        if (!boards) goto err;
        memset(boards, 0, numboards * sizeof(struct MESA_Board));

        /* initialize each board structure */
        for (ib = 0; ib < numboards; ib++) {
                struct MESA_Board *brd = boards + ib;

                sprintf(brd->devName, "/dev/%s%d", DEVNAME_MESA, ib);

                addr = SYSTEM_ISA_IOPORT_BASE + ioport[ib];

                // reserve the ISA memory region
                if (!request_region(addr, MESA_REGION_SIZE,DEVNAME_MESA)) {
                        KLOG_ERR("%s: ioport at 0x%lx already in use\n",
                                 brd->devName, addr);
                        goto err;
                }
                brd->addr = addr;
		atomic_set(&brd->available,1);

                init_waitqueue_head(&brd->rwaitq);
        }

        cdev_init(&mesa_cdev, &mesa_fops);

        /* after calling cdev_add the devices are live and
         * ready for user operations.
         */
        error = cdev_add(&mesa_cdev, mesa_device, numboards);
        if (error) goto err;

        KLOG_DEBUG("A2D init_module complete.\n");
        return 0;       // success

err:
        mesa_cleanup();
        return error;
}

module_init(mesa_init);
module_exit(mesa_cleanup);

