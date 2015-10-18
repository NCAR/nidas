/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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
/*
 * Original author:	Gordon Maclean
*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>        /* access_ok */

#include <nidas/linux/diamond/ir104.h>
#include <nidas/linux/Revision.h>    // REPO_REVISION

// #define DEBUG
#include <nidas/linux/klog.h>
#include <nidas/linux/isa_bus.h>

static dev_t ir104_device = MKDEV(0,0);

static unsigned int ioports[IR104_MAX_BOARDS] = {0,0,0,0};

static int num_boards = 0;

#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(ioports, int, &num_boards, S_IRUGO);   /* io port virtual address */
#else
module_param_array(ioports, int, num_boards, S_IRUGO);    /* io port virtual address */
#endif

#ifndef REPO_REVISION
#define REPO_REVISION "unknown"
#endif

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_DESCRIPTION("driver module for Diamond Systems IR104 card");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(REPO_REVISION);


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
#define mutex_init(x)               init_MUTEX(x)
#define mutex_lock_interruptible(x) ( down_interruptible(x) ? -ERESTARTSYS : 0)
#define mutex_unlock(x)             up(x)
#endif

static struct IR104* boards = 0;

static void add_sample(struct IR104* brd)
{
        struct dsm_sample* samp;

        samp = GET_HEAD(brd->relay_samples,brd->relay_samples.size);
        if (!samp) {                // no output sample available
                brd->missedSamples++;
                return;
        }

        samp->timetag = getSystemTimeTMsecs();

        memcpy(samp->data,brd->outputs,sizeof(brd->outputs));
        samp->length = sizeof(brd->outputs);

        /* increment head. This sample is ready for reading */
        INCREMENT_HEAD(brd->relay_samples,brd->relay_samples.size);
}

/* For each bit that is set, set that output high. Leave others alone */
static void set_douts(struct IR104* brd,const unsigned char* bits)
{
        int i;
        for (i = 0; i < 3; i++) {
                brd->outputs[i] |= bits[i];
                outb(brd->outputs[i],brd->addr+i);
        }
}

/* For each bit that is set, set that output low. Leave others alone */
static void clear_douts(struct IR104* brd,const unsigned char* bits)
{
        int i;
        for (i = 0; i < 3; i++) {
                brd->outputs[i] &= ~bits[i];
                outb(brd->outputs[i],brd->addr+i);
        }
}

/* For each bit in bits, set that output to corresponding bit in value */
static void set_douts_val(struct IR104* brd,const unsigned char* bits,const unsigned char* value)
{
        int i;
        for (i = 0; i < 3; i++) {
                if (bits[i]) {
                        brd->outputs[i] |= (bits[i] & value[i]);
                        brd->outputs[i] &= ~(bits[i] & ~value[i]);
                        outb(brd->outputs[i],brd->addr+i);
                }
        }
}

static void get_douts(struct IR104* brd,unsigned char* bits)
{
        memcpy(bits,brd->outputs,3);
}

static void get_dins(struct IR104* brd,unsigned char* bits)
{
        int i;
        for (i = 0; i <  3; i++) {
                bits[i] = inb(brd->addr+4+i);
        }
}

/************ File Operations ****************/

/* More than one thread can open this device, but only one
 * should be reading from it. Multiple threads can be
 * controlling the relays through ioctls, including the reader.
 * If more then one thread is reading samples then
 * it is possible for the read_state to get out of whack.
 * If one thread reads a partial sample, and the next read
 * is from another thread with its own read buffer, the state
 * of the sample stream in both threads will be wrong, since
 * the length of the samples are contained in the stream.
 * There isn't a current need for multiple readers. A corrupt 
 * sample stream also isn't likely in the typical mode of
 * operations where the relays are not being changed very
 * rapidly, and each change just generates one sample.
*/
static int ir104_open(struct inode *inode, struct file *filp)
{
        int minor = MINOR(inode->i_rdev);
        struct IR104* brd;
        int result;

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        /*  check the minor number */
        if (minor >= num_boards) return -ENODEV;

        brd = boards + minor;

        if ((result = mutex_lock_interruptible(&brd->mutex))) return result;
        atomic_inc(&brd->num_opened);
        mutex_unlock(&brd->mutex);

        /* and use filp->private_data to point to the board struct */
        filp->private_data = boards + minor;

        return result;
}

static int ir104_release(struct inode *inode, struct file *filp)
{
        struct IR104* brd = filp->private_data;
        int result;

        if ((result = mutex_lock_interruptible(&brd->mutex))) return result;

        /* If nobody's listening, discard the samples */
        if (atomic_dec_and_test(&brd->num_opened)) {
                brd->relay_samples.head = ACCESS_ONCE(brd->relay_samples.tail);
        }
        mutex_unlock(&brd->mutex);

        return result;
}

static long ir104_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        int result = -EINVAL,err = 0;

        struct IR104* brd = filp->private_data;

         /* don't even decode wrong cmds: better returning
          * ENOTTY than EFAULT */
        if (_IOC_TYPE(cmd) != IR104_IOC_MAGIC) return -ENOTTY;
        if (_IOC_NR(cmd) > IR104_IOC_MAXNR) return -ENOTTY;

        /*
         * the type is a bitmask, and VERIFY_WRITE catches R/W
         * transfers. Note that the type is user-oriented, while
         * verify_area is kernel-oriented, so the concept of "read" and
         * "write" is reversed
         */
        if (_IOC_DIR(cmd) & _IOC_READ)
                err = !access_ok(VERIFY_WRITE, (void __user *)arg,
                    _IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                err =  !access_ok(VERIFY_READ, (void __user *)arg,
                    _IOC_SIZE(cmd));
        if (err) return -EFAULT;

        switch (cmd) 
        {

        case IR104_GET_NOUT:
                result = IR104_NOUT;
                break;
        case IR104_GET_NIN:
                result = IR104_NIN;
                break;
        case IR104_CLEAR:	/* user set */
                {
                unsigned char bits[3];
                if (copy_from_user(bits,(void __user *)arg,
                    sizeof(bits))) return -EFAULT;
                if ((result = mutex_lock_interruptible(&brd->mutex))) return result;
                clear_douts(brd,bits);
                add_sample(brd);
                mutex_unlock(&brd->mutex);
                result = 0;
                }
                break;
        case IR104_SET:	/* user set */
                {
                unsigned char bits[3];
                if (copy_from_user(bits,(void __user *)arg,
                    sizeof(bits))) return -EFAULT;
                if ((result = mutex_lock_interruptible(&brd->mutex))) return result;
                set_douts(brd,bits);
                add_sample(brd);
                mutex_unlock(&brd->mutex);
                result = 0;
                break;
                }
        case IR104_SET_TO_VAL:      /* user set */
            {
                unsigned char bits[6];
                if (copy_from_user(bits,(void __user *)arg,
                        sizeof(bits))) return -EFAULT;
                if ((result = mutex_lock_interruptible(&brd->mutex))) return result;
                set_douts_val(brd,bits,bits+3);
                add_sample(brd);
                mutex_unlock(&brd->mutex);
                result = 0;
            }
	    break;
        case IR104_GET_DOUT:      /* user get */
            {
                unsigned char bits[3];
                // currently this doesn't access board registers, just brd->outputs
                // so we won't lock mutex.
                get_douts(brd,bits);
                if (copy_to_user((void __user *)arg,bits,
                        sizeof(bits))) return -EFAULT;
                result = 0;
            }
	    break;
        case IR104_GET_DIN:      /* user get */
            {
                unsigned char bits[3];
                if ((result = mutex_lock_interruptible(&brd->mutex))) return result;
                get_dins(brd,bits);
                mutex_unlock(&brd->mutex);
                if (copy_to_user((void __user *)arg,bits,
                        sizeof(bits))) return -EFAULT;
                result = 0;
            }
	    break;
        default:
                result = -ENOTTY;
                break;
        }
        return result;
}

/*
 * poll whether relay samples are available for read
 */
static unsigned int ir104_relay_poll(struct file *filp, poll_table *wait)
{
        struct IR104* brd = filp->private_data;
        unsigned int mask = 0;
        poll_wait(filp, &brd->read_queue, wait);

        if (sample_remains(&brd->read_state) ||
                GET_TAIL(brd->relay_samples,brd->relay_samples.size))
                mask |= POLLIN | POLLRDNORM;    /* readable */
        return mask;
}

/*
 * read relay samples.
 */
static ssize_t ir104_relay_read(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct IR104* brd = filp->private_data;
        return nidas_circbuf_read(filp,buf,count,&brd->relay_samples,&brd->read_state,
            &brd->read_queue);
}


static struct file_operations ir104_fops = {
        .owner = THIS_MODULE,
        .open = ir104_open,
        .unlocked_ioctl = ir104_ioctl,
        .read = ir104_relay_read,
        .poll = ir104_relay_poll,
        .release = ir104_release,
        .llseek  = no_llseek,
};

/*-----------------------Module ------------------------------*/

/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
static void ir104_cleanup(void)
{
        int i;
        if (boards) {
                for (i=0; i < num_boards; i++) {
                        struct IR104* brd = boards + i;
                        if (MAJOR(brd->cdev.dev) != 0) cdev_del(&brd->cdev);
                        if (brd->addr)
                            release_region(brd->addr,IR104_IO_REGION_SIZE);
                        free_dsm_circ_buf(&brd->relay_samples);
                }
                kfree(boards);
        }

        if (MAJOR(ir104_device) != 0)
            unregister_chrdev_region(ir104_device, num_boards);
}

static int __init ir104_init(void)
{	
        int result = -EINVAL;
        int ib;

        KLOG_NOTICE("version: %s\n",REPO_REVISION);

        for (ib=0; ib < IR104_MAX_BOARDS; ib++)
                if (ioports[ib] == 0) break;
        num_boards = ib;

        result = alloc_chrdev_region(&ir104_device,0,num_boards, "ir104");
        if (result < 0) goto err;

        /*
         * allocate the board structures
         */
        boards = kmalloc(num_boards * sizeof(struct IR104), GFP_KERNEL);
        if (!boards) {
                result = -ENOMEM;
                goto err;
        }
        memset(boards, 0, num_boards * sizeof(struct IR104));

        for (ib=0; ib < num_boards; ib++) {
                struct IR104* brd = boards + ib;
                unsigned long addr = ioports[ib] + SYSTEM_ISA_IOPORT_BASE;
                int i;
                dev_t devno;

                /* for informational messages only */
                sprintf(brd->deviceName,"/dev/ir104_%d",ib);

                if (!request_region(addr,IR104_IO_REGION_SIZE, "ir104")) {
                        result = -EBUSY;
                        goto err;
                }
                brd->addr = addr;
                mutex_init(&brd->mutex);
                atomic_set(&brd->num_opened,0);

                /* read initial values */
                for (i = 0; i < 3; i++) {
                        brd->outputs[i] = inb(brd->addr+i);
                }

                /*
                 * Check unused bits of register 2. They should be 0.
                 * If not, then a board is not responding at this address.
                 */
                if (brd->outputs[2] & 0xf0) {
                        KLOG_NOTICE("%s: board not responding at ioport=%#03x\n",
                                        brd->deviceName,ioports[ib]);
                        if (ib > 0) {
                                num_boards = ib;
                                release_region(brd->addr,IR104_IO_REGION_SIZE);
                                brd->addr = 0;
                                break;
                        }
                        result = -ENODEV;
                        goto err;
                }

                /*
                 * Small circular buffer of relay samples.
                 */
                result = alloc_dsm_circ_buf(&brd->relay_samples, sizeof(brd->outputs), 4);
                if (result) goto err;

                init_waitqueue_head(&brd->read_queue);

                memset(&brd->read_state,0, sizeof(struct sample_read_state));

                cdev_init(&brd->cdev,&ir104_fops);
                brd->cdev.owner = THIS_MODULE;

                /* After calling cdev_all the device is "live"
                 * and ready for user operation.
                 */
                devno = MKDEV(MAJOR(ir104_device),ib);
                result = cdev_add(&brd->cdev, devno, 1);
                if (result) break;
        }

        return result;
err:
        ir104_cleanup();
        return result;
}

module_init(ir104_init);
module_exit(ir104_cleanup);

