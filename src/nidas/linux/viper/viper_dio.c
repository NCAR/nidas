/*

Driver for Viper digital IO ports

Copyright 2005 UCAR, NCAR, All Rights Reserved

Original author:	Gordon Maclean

*/

#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <asm/uaccess.h>        /* access_ok */

#include <asm/arch-pxa/hardware.h>
#include <asm/arch-pxa/pxa-regs.h>

#include <nidas/linux/viper/viper_dio.h>
#include <nidas/rtlinux/dsm_version.h>

// #define DEBUG
#include <nidas/linux/klog.h>
#include <nidas/linux/isa_bus.h>

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_LICENSE("Dual BSD/GPL");

/*
 * Device structure.
 */
static struct VIPER_DIO viper_dio;

static void set_douts(unsigned char bits)
{
        GPSR(20) = (u32)bits << 20;
}

static void clear_douts(unsigned char bits)
{
        GPCR(20) = (u32)bits << 20;
}

static void set_douts_val(unsigned char bits,unsigned char value)
{
        unsigned char vbits = bits & value;
        set_douts(vbits);
        vbits = bits & ~value;
        clear_douts(vbits);
}

static unsigned char get_douts(void)
{
        return GPLR(20) >> 20;
}

static unsigned char get_dins(void)
{
        return VIPER_GPIO & 0xff;
}

/************ File Operations ****************/

/* More than one thread can open this device.  */
static int viper_dio_open(struct inode *inode, struct file *filp)
{
        int result = 0;

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        return result;
}

static int viper_dio_release(struct inode *inode, struct file *filp)
{
        int result = 0;
        return result;
}

static int viper_dio_ioctl(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
        int result = -EINVAL,err = 0;

         /* don't even decode wrong cmds: better returning
          * ENOTTY than EFAULT */
        if (_IOC_TYPE(cmd) != VIPER_DIO_IOC_MAGIC) return -ENOTTY;
        if (_IOC_NR(cmd) > VIPER_DIO_IOC_MAXNR) return -ENOTTY;

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

        case VIPER_DIO_GET_NOUT:
        case VIPER_DIO_GET_NIN:
                result = VIPER_DIO_NOUT;
                break;
        case VIPER_DIO_CLEAR:	/* user set */
                {
                unsigned char bits;
                if (copy_from_user(&bits,(void __user *)arg,
                    sizeof(bits))) return -EFAULT;
                clear_douts(bits);
                result = 0;
                }
                break;
        case VIPER_DIO_SET:	/* user set */
                {
                unsigned char bits;
                if (copy_from_user(&bits,(void __user *)arg,
                    sizeof(bits))) return -EFAULT;
                set_douts(bits);
                result = 0;
                break;
                }
        case VIPER_DIO_SET_TO_VAL:      /* user set */
            {
                unsigned char bits[2];
                if (copy_from_user(bits,(void __user *)arg,
                        sizeof(bits))) return -EFAULT;
                set_douts_val(bits[0],bits[1]);
                result = 0;
            }
	    break;
        case VIPER_DIO_GET_DOUT:      /* user get */
            {
                unsigned char bits = get_douts();
                if (copy_to_user((void __user *)arg,&bits,
                        sizeof(bits))) return -EFAULT;
                result = 0;
            }
	    break;
        case VIPER_DIO_GET_DIN:      /* user get */
            {
                unsigned char bits = get_dins();
                if (copy_to_user((void __user *)arg,&bits,
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

static struct file_operations viper_dio_fops = {
        .owner   = THIS_MODULE,
        .open    = viper_dio_open,
        .ioctl   = viper_dio_ioctl,
        .release = viper_dio_release,
        .llseek  = no_llseek,
};

/*-----------------------Module ------------------------------*/

void viper_dio_cleanup(void)
{
        cdev_del(&viper_dio.cdev);
        if (MAJOR(viper_dio.devno) != 0)
            unregister_chrdev_region(viper_dio.devno,1);
        KLOG_DEBUG("complete\n");
}

int viper_dio_init(void)
{	
        int result = -EINVAL;

        // DSM_VERSION_STRING is found in dsm_version.h
        KLOG_NOTICE("version: %s\n",DSM_VERSION_STRING);

        viper_dio.devno = MKDEV(0,0);
        result = alloc_chrdev_region(&viper_dio.devno,0,1,"viper_dio");
        if (result < 0) goto err;
        KLOG_DEBUG("alloc_chrdev_region done, major=%d minor=%d\n",
                MAJOR(dmmat_device),MINOR(dmmat_device));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        mutex_init(&viper_dio.reg_mutex);
#else
        init_MUTEX(&viper_dio.reg_mutex);
#endif

        // for informational messages only at this point
        sprintf(viper_dio.deviceName,"/dev/viper_dio%d",0);

        cdev_init(&viper_dio.cdev,&viper_dio_fops);
        viper_dio.cdev.owner = THIS_MODULE;

        /* After calling cdev_all the device is "live"
         * and ready for user operation.
         */
        result = cdev_add(&viper_dio.cdev, viper_dio.devno, 1);

        KLOG_DEBUG("complete.\n");

        return 0;
err:
        viper_dio_cleanup();
        return result;
}

module_init(viper_dio_init);
module_exit(viper_dio_cleanup);

