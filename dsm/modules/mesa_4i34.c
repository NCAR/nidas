
#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif
                                                                                
#include <linux/config.h>
#include <linux/module.h>
                                                                                
#include <linux/init.h>   /* module_init() */
#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>   /* kmalloc() */
#include <linux/fs.h>       /* everything... */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>    /* O_ACCMODE */
#include <linux/ioport.h>
#include <asm/io.h>		/* outb, inb */
#include <asm/uaccess.h>	/* access_ok */
                                                                                
#include <asm/system.h>     /* cli(), *_flags */
                                                                                
#include "mesa_4i34.h"          /* local definitions */

static int mesa_4i34_major =   MESA_4I34_MAJOR;
static unsigned long mesa_4i34_ioports[MESA_4I34_MAX_NR_DEVS] = {0,0,0,0};
static int mesa_4i34_nr_devs = 0;
static mesa_4i34_device* mesa_4i34_devices = 0;
                                                                                
MODULE_PARM(mesa_4i34_major,"i");
MODULE_PARM(mesa_4i34_ioports,"1-4l");	/* io port virtual address */
MODULE_AUTHOR("Gordon Maclean");
MODULE_LICENSE("GPL");

#ifdef CONFIG_DEVFS_FS
devfs_handle_t mesa_4i34_devfs_dir = 0;
static char devname[4];
#endif

/* 
 * To detect if compiling for VIPER
 * check ifdef __LINUX_ARM_ARCH
 * or CONFIG_ARCH_VIPER
 */

/*
 * Setup card to be programmed.
 */
int mesa_4i34_prog_start(mesa_4i34_device* dev) {
  unsigned char status;
  unsigned char cmd;

  cmd = M_4I34CFGCSOFF | M_4I34CFGINITASSERT |
  	M_4I34CFGWRITEDISABLE | M_4I34LEDOFF;
  outb(cmd,dev->ioport+R_4I34CONTROL);
  wmb();		/* flush */
  status = inb(dev->ioport+R_4I34STATUS);
  /* Note that if we see DONE at the start of programming, it's most likely due
     to an attempt to access the 4I34 at the wrong I/O location. */
  if (status & M_4I34PROGDUN) return -EFAULT;

  cmd = M_4I34CFGCSON | M_4I34CFGINITDEASSERT |
  	M_4I34CFGWRITEENABLE | M_4I34LEDON;
  outb(cmd,dev->ioport+R_4I34CONTROL);
  wmb();		/* flush */

  set_current_state(TASK_INTERRUPTIBLE);
  schedule_timeout(5);		/* wait 5 jiffies */
  return 0;
}

/*
 * Finish up programming of card.
 */
int mesa_4i34_prog_end(mesa_4i34_device* dev) {
  unsigned char status;
  unsigned char cmd;
  int ntry;

  /* first send cmd with LED ON */
  cmd = M_4I34CFGCSOFF | M_4I34CFGINITDEASSERT |
  	M_4I34CFGWRITEDISABLE | M_4I34LEDON;
  outb(cmd,dev->ioport+R_4I34CONTROL);
  wmb();		/* flush */

  /* check for DUN */
  ntry = 0;
  do {
    unsigned long jwait = jiffies + MESA_4I34_WAIT_JIFFY;
    while (jiffies < jwait) schedule();
    status = inb(dev->ioport+R_4I34STATUS);
  } while(ntry++ < MESA_4I34_WAIT_FOR_DUN && !(status & M_4I34PROGDUN));

  if(!(status & M_4I34PROGDUN)) return -ENOTTY;	/* failed */

  PDEBUGG("PROGDUN after %d checks\n",ntry);

  /* Comment in original CSC4I34.C:
    "Programming seems to be complete; make sure the state sticks and we're not
    seeing some sort of I/O conflict-related foo."

    This is probably a DOS kludge and unnecessary in linux, but if
    you really want to double check, set DOUBLECHECK_FOR_DUN to something
    more than 0. CSS4I34.C checked 100 times!
  */

  for (ntry = 0; ntry < MESA_4I34_DOUBLE_CHECK_FOR_DUN; ntry++) {
    status = inb(dev->ioport+R_4I34STATUS);
    if (!(status & M_4I34PROGDUN)) {
      PDEBUGG("PROGDUN doesn't stick! ntry = %d\n",ntry);
      return -ENOTTY;
    }
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(MESA_4I34_WAIT_JIFFY);
  }

  /* We're OK, so send again with LED OFF */
  cmd = M_4I34CFGCSOFF | M_4I34CFGINITDEASSERT |
  	M_4I34CFGWRITEDISABLE | M_4I34LEDOFF;
  outb(cmd,dev->ioport+R_4I34CONTROL);
  wmb();		/* flush */

  /* Send configuration completion clocks.  (6 should be enough, but we send
     lots for good measure.) */
  for(ntry = 0 ; ntry < MESA_4I34_NUM_CONFIG_COMP_CLOCKS ; ntry++)
	  outb(0xFF,dev->ioport+R_4I34DATA);
  return 0;
}

#ifdef MESA_4I34_DEBUG /* use proc only if debugging */
/*
 * The proc filesystem: function to read
 */
                                                                                
int mesa_4i34_read_procmem(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
    int i, j, len = 0;
    int limit = count - 80; /* Don't print more than this */
    PDEBUGG("read_proc, count=%d\n",count);
                                                                                
    for (i = 0; i < mesa_4i34_nr_devs && len <= limit; i++) {
        struct mesa_4i34_device *d = mesa_4i34_devices + i;
	PDEBUGG("read_proc, i=%d, device=0x%lx\n",i,(unsigned long)d);
        if (down_interruptible(&d->sem))
                return -ERESTARTSYS;
	/* mesa_4i34_read_config(d);  do something interesting */
        len += sprintf(buf+len,"\nMesa 4I34 %i: ioport %lx\n",
                       i, d->ioport);
        up(&d->sem);
    }
    *eof = 1;
    return len;
}

static void mesa_4i34_create_proc()
{
    PDEBUGG("within mesa_4i34_create_proc\n");
    create_proc_read_entry("mesa_4i34", 0 /* default mode */,
                           NULL /* parent dir */, mesa_4i34_read_procmem,
                           NULL /* client data */);
}
                                                                                
static void mesa_4i34_remove_proc()
{
    /* no problem if it was not registered */
    remove_proc_entry("mesa_4i34", NULL /* parent dir */);
}

#endif

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void mesa_4i34_cleanup_module(void)
{
    int i;
                                                                                
#ifndef CONFIG_DEVFS_FS
    /* cleanup_module is never called if registering failed */
    unregister_chrdev(mesa_4i34_major, "mesa_4i34");
#endif
                                                                                
#ifdef MESA_4I34_DEBUG /* use proc only if debugging */
    mesa_4i34_remove_proc();
#endif
    if (mesa_4i34_devices) {
        for (i=0; i<mesa_4i34_nr_devs; i++) {
	    if (mesa_4i34_devices[i].region) 
		release_region(mesa_4i34_devices[i].ioport,MESA_4I34_IO_REGION_SIZE);
            /* the following line is only used for devfs */
            if (mesa_4i34_devices[i].handle) 
	    	devfs_unregister(mesa_4i34_devices[i].handle);
        }
        kfree(mesa_4i34_devices);
    }
#ifdef CONFIG_DEVFS_FS
    /* once again, only for devfs */
    devfs_unregister(mesa_4i34_devfs_dir);
#endif /* CONFIG_DEVFS_FS */
}

int mesa_4i34_init_module(void)
{
    int result, i;
                                                                                
#ifdef CONFIG_DEVFS_FS
    /* If we have devfs, create /dev/mesa_4i34 to put files in there */
    mesa_4i34_devfs_dir = devfs_mk_dir(NULL, "mesa_4i34", NULL);
    if (!mesa_4i34_devfs_dir) return -EBUSY; /* problem */
                                                                                
#else /* no devfs, do it the "classic" way  */
                                                                                
    /*
     * Register your major, and accept a dynamic number. This is the
     * first thing to do, in order to avoid releasing other module's
     * fops in mesa_4i34_cleanup_module()
     */
    result = register_chrdev(mesa_4i34_major, "mesa_4i34", &mesa_4i34_fops);
    if (result < 0) {
        printk(KERN_WARNING "mesa_4i34: can't get major %d\n",mesa_4i34_major);
        return result;
    }
    if (mesa_4i34_major == 0) mesa_4i34_major = result; /* dynamic */
    PDEBUGG("major=%d\n",mesa_4i34_major);

#endif /* CONFIG_DEVFS_FS */

    for (i=0; i < MESA_4I34_MAX_NR_DEVS; i++)
      if (mesa_4i34_ioports[i] == 0) break;
    mesa_4i34_nr_devs = i;
    PDEBUGG("nr_devs=%d\n",mesa_4i34_nr_devs);

    /*
     * allocate the devices 
     */
    mesa_4i34_devices = kmalloc(mesa_4i34_nr_devs * sizeof(mesa_4i34_device), GFP_KERNEL);
    if (!mesa_4i34_devices) {
        result = -ENOMEM;
        goto fail;
    }
    memset(mesa_4i34_devices, 0, mesa_4i34_nr_devs * sizeof(mesa_4i34_device));
    for (i=0; i < mesa_4i34_nr_devs; i++) {
        mesa_4i34_devices[i].ioport = mesa_4i34_ioports[i];
	if (!( mesa_4i34_devices[i].region =
	    request_region(mesa_4i34_devices[i].ioport,MESA_4I34_IO_REGION_SIZE,
			"mesa_4i34"))) {
	    result = -ENODEV;
	    goto fail;
	}
	sema_init(&mesa_4i34_devices[i].sem,1);
#ifdef CONFIG_DEVFS_FS
        sprintf(devname, "%i", i);
        mesa_4i34_devices[i].handle =
		devfs_register(mesa_4i34_devfs_dir, devname,
                       DEVFS_FL_AUTO_DEVNUM,
                       0, 0, S_IFCHR | S_IRUGO | S_IWUGO,
                       &mesa_4i34_fops,
		       mesa_4i34_devices+i);	// private data
#endif
    }
                                                                                
    /* ... */
                                                                                
#ifndef MESA_4I34_DEBUG
    EXPORT_NO_SYMBOLS; /* otherwise, leave global symbols visible */
#endif
                                                                                
#ifdef MESA_4I34_DEBUG /* only when debugging */
    PDEBUGG("create_proc\n");
    mesa_4i34_create_proc();
#endif
                                                                                
    return 0; /* succeed */
                                                                                
  fail:
    mesa_4i34_cleanup_module();
    return result;
}

int mesa_4i34_open (struct inode *inode, struct file *filp)
{
    int num = MINOR(inode->i_rdev);
    mesa_4i34_device *dev; /* device information */

    /*  check the device number */
    if (num >= mesa_4i34_nr_devs) return -ENODEV;
    dev = mesa_4i34_devices + num;

    /* and use filp->private_data to point to the device data */
    filp->private_data = dev;

    if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
    /* mesa_4i34_read_config(dev); */
    up(&dev->sem);

    MOD_INC_USE_COUNT;
    return 0;	/* success */
}

int mesa_4i34_release (struct inode *inode, struct file *filp)
{
    MOD_DEC_USE_COUNT;
    return 0;
}

ssize_t mesa_4i34_write(struct file *filp, const char *buf, size_t count,
                loff_t *f_pos)
{
    mesa_4i34_device* dev = filp->private_data;
    ssize_t ret = -ENOMEM; /* value used in "goto out" statements */
    const char* ein = buf + count;
    const char* eout;
    const char* outptr;
                                                                                
    if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;

    if (!dev->buf) {
      dev->buf = kmalloc(MESA_4I34_BUFFER_SIZE,GFP_KERNEL);
      if (!dev->buf) goto out;
    }

    while(buf < ein) {
      size_t l = ein - buf;
      if (l > MESA_4I34_BUFFER_SIZE) l = MESA_4I34_BUFFER_SIZE;
      if (copy_from_user(dev->buf, buf, l)) {
        ret = -EFAULT;
        goto out;
      }
      buf += l;
      eout = dev->buf + l;
      for (outptr = dev->buf; outptr < eout; )
      	outb(*outptr++,dev->ioport+R_4I34DATA);
    }
    *f_pos += count;
    ret = count;

  out:
    up(&dev->sem);
    return ret;
}

/*
 * The ioctl() implementation
 */

int mesa_4i34_ioctl (struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
    mesa_4i34_device* dev = filp->private_data;
    int err= 0, ret = 0;

    /* don't even decode wrong cmds: better returning  ENOTTY than EFAULT */
    if (_IOC_TYPE(cmd) != MESA_4I34_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > MESA_4I34_IOC_MAXNR) return -ENOTTY;

    /*
     * the type is a bitmask, and VERIFY_WRITE catches R/W
     * transfers. Note that the type is user-oriented, while
     * verify_area is kernel-oriented, so the concept of "read" and
     * "write" is reversed
     */
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
    if (err) return -EFAULT;

    switch(cmd) {
      case MESA_4I34_IOCPROGSTART:
	ret = mesa_4i34_prog_start(dev);
	break;

#ifdef SAVE_THESE_FOR_REFERENCE
      case MESA_4I34_IOCGIOPORT:
        ret = put_user(dev->ioport, (unsigned long *) arg);
        break;

      case MESA_4I34_IOCGPORTCONFIG:	/* get port config */
        if (copy_to_user((mesa_4i34_config *) arg,&dev->config,
	      	sizeof(mesa_4i34_config)) != 0) ret = -EFAULT;
        break;
        
      case MESA_4I34_IOCSPORTCONFIG:	/* set port config */
	{
	    mesa_4i34_config tmpconfig;
	    if (copy_from_user(&tmpconfig,(mesa_4i34_config *) arg,
	      	sizeof(mesa_4i34_config)) != 0) ret = -EFAULT;
	    if (!mesa_4i34_check_config(&tmpconfig)) ret = -EINVAL;
	    else {
		if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
	        mesa_4i34_write_config(dev,&tmpconfig);
		up(&dev->sem);
	    }
	}
        break;

      case MESA_4I34_IOCGEEPORTCONFIG:	/* get config from eeprom */
        {
	  mesa_4i34_config eeconfig;
	  if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
	  mesa_4i34_read_eeconfig(dev,&eeconfig);
	  up(&dev->sem);
	  if (copy_to_user((mesa_4i34_config *) arg,&eeconfig,
		  sizeof(mesa_4i34_config)) != 0) ret = -EFAULT;
	}
        break;
        
      case MESA_4I34_IOCSEEPORTCONFIG:	/* set config in eeprom */
	{
	    mesa_4i34_config eeconfig;
	    if (copy_from_user(&eeconfig,(mesa_4i34_config *) arg,
	      	sizeof(mesa_4i34_config)) != 0) ret = -EFAULT;
	    if (!mesa_4i34_check_config(&eeconfig)) ret = -EINVAL;
	    else {
		if (down_interruptible(&dev->sem)) return -ERESTARTSYS;
	        mesa_4i34_write_eeconfig(dev,&eeconfig);
		up(&dev->sem);
	    }
	}
        break;
#endif

      default:  /* redundant, as cmd was checked against MAXNR */
        return -ENOTTY;
    }
    return ret;
}

struct file_operations mesa_4i34_fops = {
    /* llseek:     mesa_4i34_llseek, */
    /* read:       mesa_4i34_read, */
    write:      mesa_4i34_write,
    ioctl:      mesa_4i34_ioctl,
    open:       mesa_4i34_open,
    release:    mesa_4i34_release,
};
						      
module_init(mesa_4i34_init_module);
module_exit(mesa_4i34_cleanup_module);
