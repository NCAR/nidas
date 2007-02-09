/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    Linux driver module for Diamond emerald serial IO cards.
    This just queries and sets the ioport addresses and irqs on bootup.
    The normal linux serial driver for the 8250 family of uarts
    does the heavy work.
 ********************************************************************
*/


#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

// #include <linux/config.h>

#include <linux/init.h>   /* module_init() */
#include <linux/module.h>
#include <linux/moduleparam.h>

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

// #include <nidas/linux/diamond/emerald/emerald.h>	/* local definitions */
#include "emerald.h"	/* local definitions */

#ifdef CONFIG_ARCH_VIPER
#include <asm/arch/viper.h>
static unsigned long ioport_base = VIPER_PC104IO_BASE;
#else
static unsigned long ioport_base = 0;
#endif

static int emerald_major =   EMERALD_MAJOR;
static unsigned long ioports[EMERALD_MAX_NR_DEVS] = {0,0,0,0};
static int emerald_nr_addrs = 0;
static int emerald_nr_ok = 0;
static emerald_board* emerald_boards = 0;
static emerald_port* emerald_ports = 0;
static int emerald_nports = 0;

module_param(emerald_major,int,S_IRUGO);
module_param_array(ioports,ulong,&emerald_nr_addrs,S_IRUGO);	/* io port virtual address */
MODULE_AUTHOR("Gordon Maclean");
MODULE_DESCRIPTION("driver module to initialize emerald serial port card");
MODULE_LICENSE("Dual BSD/GPL");

/* 
 * To detect if compiling for VIPER
 * check ifdef __LINUX_ARM_ARCH
 * or CONFIG_ARCH_VIPER
 */

/*
 * return 0 if config is not OK.
 * According to the Emerald-MM-8 User Manual, v2.4,
 * valid port addresses are 0x100-0x3f8.
 * Valid irqs are 2,3,4,5,6,7,10,11,12 and 15.
 * On the viper, ISA irqs 2 & 15 are not mapped.
 */
static int emerald_check_config(emerald_config* config) {
  int i,j;
#ifdef CONFIG_ARCH_VIPER
  int valid_irqs[]={3,4,5,6,7,10,11,12};
  int nvalid = 8;
#else
  int valid_irqs[]={2,3,4,5,6,7,10,11,12,15};
  int nvalid = 10;
#endif

  for (i = 0; i < EMERALD_NR_PORTS; i++) {
    PDEBUGG(KERN_INFO "emerald: ioport=%d\n",config->ports[i].ioport);
    if (config->ports[i].ioport > 0x3f8) return 0;
    if (config->ports[i].ioport < 0x100) return 0;
    for (j = 0; j < nvalid; j++) {
      PDEBUGG("emerald: checking irq=%d against %d\n",
      	config->ports[i].irq,valid_irqs[j]);
      if (valid_irqs[j] == config->ports[i].irq) break;
    }
    if (j == nvalid) return 0;
  }
  return 1;
}

static void emerald_enable_ports(emerald_board* brd) {
  outb(0x80,brd->ioport+EMERALD_APER);	/* enable ports */
}


static void emerald_read_config(emerald_board* brd) {
  int i;
  /* read ioport values. irqs are not readable, except from eeprom */
  for (i = 0; i < EMERALD_NR_PORTS; i++) {
    outb(i,brd->ioport+EMERALD_APER);
    wmb();		/* flush */
    brd->config.ports[i].ioport = inb(brd->ioport+EMERALD_ARR) << 3;
  }
  emerald_enable_ports(brd);
}

static int emerald_write_config(emerald_board* brd,emerald_config* config) {
  int i;
  /* write ioport and irq values. */
  for (i = 0; i < EMERALD_NR_PORTS; i++) {
    outb(i,brd->ioport+EMERALD_APER);
    wmb();		/* flush */
    outb(config->ports[i].ioport >> 3,brd->ioport+EMERALD_AIDR);
    wmb();		/* flush */

    outb(i+EMERALD_NR_PORTS,brd->ioport+EMERALD_APER);
    wmb();		/* flush */
    outb(config->ports[i].irq,brd->ioport+EMERALD_AIDR);
    wmb();		/* flush */
  }
  brd->config = *config;
  emerald_enable_ports(brd);
  return 0;
}

/*
 * Read the ioport and irq configuration for the 8 serial ports
 * from the Emerald EEPROM.
 */
static int emerald_read_eeconfig(emerald_board* brd,emerald_config* config)
{
  int i;
  unsigned char busy;
  int ntry;

  /* another way to wait */
  /*
  set_current_state(TASK_INTERRUPTIBLE);
  schedule_timeout(1);
  */

  /* get ioport values from EEPROM addresses 0-7 */
  for (i = 0; i < EMERALD_NR_PORTS; i++) {
    outb(i,brd->ioport+EMERALD_ECAR);
    wmb();		/* flush */

    /* wait for busy bit in EMERALD_EBR to clear */
    ntry = 0;
    do {
      unsigned long jwait = jiffies + 1;
      while (jiffies < jwait) schedule();
      busy = inb(brd->ioport+EMERALD_EBR);
    } while(ntry++ < 2 && busy & 0x80);
    if (ntry == 2 && (busy & 0x80)) return -ETIMEDOUT;
    config->ports[i].ioport = inb(brd->ioport+EMERALD_EDR) << 3;
  }

  /* get irq values from EEPROM addresses 8-15 */
  for (i = 0; i < EMERALD_NR_PORTS; i++) {
    outb(i+EMERALD_NR_PORTS,brd->ioport+EMERALD_ECAR);
    wmb();		/* flush */

    /* wait for busy bit in EMERALD_EBR to clear */
    ntry = 0;
    do {
      unsigned long jwait = jiffies + 1;
      while (jiffies < jwait) schedule();
      busy = inb(brd->ioport+EMERALD_EBR);
    } while(ntry++ < 2 && busy & 0x80);
    if (ntry == 2 && (busy & 0x80)) return -ETIMEDOUT;
    config->ports[i].irq = inb(brd->ioport+EMERALD_EDR);
  }
  return 0;
}

/*
 * Write the ioport and irq configuration for the 8 serial ports
 * to the Emerald EEPROM.
 */
static int emerald_write_eeconfig(emerald_board* brd,emerald_config* config) {
  int i;
  unsigned char busy;
  int ntry;

  /* write ioport values to EEPROM addresses 0-7 */
  for (i = 0; i < EMERALD_NR_PORTS; i++) {
    outb(config->ports[i].ioport >> 3,brd->ioport+EMERALD_EDR);
    wmb();		/* flush */
    outb(i + 0x80,brd->ioport+EMERALD_ECAR);
    wmb();		/* flush */

    /* wait for busy bit in EMERALD_EBR to clear */
    ntry = 0;
    do {
      unsigned long jwait = jiffies + 1;
      while (jiffies < jwait) schedule();
      busy = inb(brd->ioport+EMERALD_EBR);
    } while(ntry++ < 2 && busy & 0x80);
    if (ntry == 2 && (busy & 0x80)) return -ETIMEDOUT;
  }

  /* write irq values to EEPROM addresses 8-15 */
  for (i = 0; i < EMERALD_NR_PORTS; i++) {
    outb(config->ports[i].irq,brd->ioport+EMERALD_EDR);
    wmb();		/* flush */
    outb(i + EMERALD_NR_PORTS + 0x80,brd->ioport+EMERALD_ECAR);
    wmb();		/* flush */

    /* wait for busy bit in EMERALD_EBR to clear */
    ntry = 0;
    do {
      unsigned long jwait = jiffies + 1;
      while (jiffies < jwait) schedule();
      busy = inb(brd->ioport+EMERALD_EBR);
    } while(ntry++ < 2 && busy & 0x80);
    if (ntry == 2 && (busy & 0x80)) return -ETIMEDOUT;
  }
  return 0;
}

/*
 * Load the the ioport and irq configuration from the Emerald
 * EEPROM into ram.
 */
static int emerald_load_config_from_eeprom(emerald_board* brd) {

  unsigned char busy;
  int ntry;
  int result;

  outb(0x80,brd->ioport+EMERALD_CRR);	/* reload configuration from eeprom */
  wmb();		/* flush */
  /* wait for busy bit in EMERALD_EBR to clear */
  ntry = 0;
  do {
    unsigned long jwait = jiffies + 1;
    while (jiffies < jwait) schedule();
    busy = inb(brd->ioport+EMERALD_EBR);
  } while(ntry++ < 2 && busy & 0x80);
  if (ntry == 2 && busy & 0x80) return -ETIMEDOUT;

  result = emerald_read_eeconfig(brd,&brd->config);
  emerald_read_config(brd);
  return result;
}

static int emerald_get_digio_port_out(emerald_board* brd,int port)
{
    return (brd->digioout & (1 << port)) != 1;
}

static void emerald_set_digio_out(emerald_board* brd,int val)
{
    outb(val,brd->ioport+EMERALD_DDR);
    brd->digioout = val;
}

static void emerald_set_digio_port_out(emerald_board* brd,int port,int val)
{
    if (val) brd->digioout |= 1 << port;
    else brd->digioout &= ~(1 << port);
    outb(brd->digioout,brd->ioport+EMERALD_DDR);
}

static int emerald_read_digio(emerald_board* brd)
{
    brd->digioval = inb(brd->ioport+EMERALD_DIR);
    return brd->digioval;
}

static int emerald_read_digio_port(emerald_board* brd,int port)
{
    int val = emerald_read_digio(brd);
    return (val & (1 << port)) != 1;
}

static void emerald_write_digio_port(emerald_board* brd,int port,int val)
{
    if (val) brd->digioval |= 1 << port;
    else brd->digioval &= ~(1 << port);

    // this does not effect digital input lines
    outb(brd->digioval,brd->ioport+EMERALD_DOR);
}

#ifdef EMERALD_DEBUG /* use proc only if debugging */
/*
 * The proc filesystem: function to read
 */
                                                                                
static int emerald_read_procmem(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
    int i, j, len = 0;
    int limit = count - 80; /* Don't print more than this */
    PDEBUGG("read_proc, count=%d\n",count);
                                                                                
    for (i = 0; i < emerald_nr_ok && len <= limit; i++) {
        struct emerald_board *d = emerald_boards + i;
	PDEBUGG("read_proc, i=%d, device=0x%lx\n",i,(unsigned long)d);
        if (down_interruptible(&d->sem))
                return -ERESTARTSYS;
	emerald_read_config(d);
        len += sprintf(buf+len,"\nDiamond Emerald-MM-8 %i: ioport %lx\n",
                       i, d->ioport);
	/* loop over serial ports */
        for (j = 0; len <= limit && j < EMERALD_NR_PORTS; j++) {
            len += sprintf(buf+len, "  port %d, ioport=%x,irq=%d\n",
	    	j,d->config.ports[j].ioport,d->config.ports[j].irq);
        }
        up(&d->sem);
    }
    *eof = 1;
    return len;
}

static void emerald_create_proc(void)
{
    PDEBUGG("within emerald_create_proc\n");
    create_proc_read_entry("emerald", 0644 /* default mode */,
                           NULL /* parent dir */, emerald_read_procmem,
                           NULL /* client data */);
}
                                                                                
static void emerald_remove_proc(void)
{
    /* no problem if it was not registered */
    remove_proc_entry("emerald", NULL /* parent dir */);
}

#endif

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static void __exit emerald_cleanup_module(void)
{
    int i;
    /* cleanup_module is never called if registering failed */
    unregister_chrdev(emerald_major, "emerald");
                                                                                
#ifdef EMERALD_DEBUG /* use proc only if debugging */
    emerald_remove_proc();
#endif

    if (emerald_ports) {
        kfree(emerald_ports);
    }

    if (emerald_boards) {
        for (i=0; i<emerald_nr_ok; i++) {
	    if (emerald_boards[i].region) 
		release_region(emerald_boards[i].ioport,EMERALD_IO_REGION_SIZE);
        }
        kfree(emerald_boards);
    }

}

static int emerald_open (struct inode *inode, struct file *filp)
{
    int num = MINOR(inode->i_rdev);
    emerald_port *port; /* device information */

    /*  check the device number */
    if (num >= emerald_nports) return -ENODEV;
    port = emerald_ports + num;

    /* and use filp->private_data to point to the device data */
    filp->private_data = port;

    if (down_interruptible(&port->board->sem)) return -ERESTARTSYS;
    emerald_read_config(port->board);
    up(&port->board->sem);

    return 0;	/* success */
}

static int emerald_release (struct inode *inode, struct file *filp)
{
    return 0;
}

/*
 * The ioctl() implementation
 */

static int emerald_ioctl (struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
    emerald_port* port = filp->private_data;
    emerald_board* brd = port->board;
    int err= 0, ret = 0;

    /* don't even decode wrong cmds: better returning  ENOTTY than EFAULT */
    if (_IOC_TYPE(cmd) != EMERALD_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > EMERALD_IOC_MAXNR) return -ENOTTY;

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

    case EMERALD_IOCGIOPORT:
        if (copy_to_user((unsigned long*) arg,&brd->ioport,
	      	sizeof(unsigned long)) != 0) ret = -EFAULT;
        break;

    case EMERALD_IOCGPORTCONFIG:	/* get port config */
        if (copy_to_user((emerald_config *) arg,&brd->config,
	      	sizeof(emerald_config)) != 0) ret = -EFAULT;
        break;
        
    case EMERALD_IOCSPORTCONFIG:	/* set port config */
{
	    emerald_config tmpconfig;
	    if (copy_from_user(&tmpconfig,(emerald_config *) arg,
	      	sizeof(emerald_config)) != 0) ret = -EFAULT;
	    if (down_interruptible(&brd->sem)) return -ERESTARTSYS;
	    ret = emerald_write_config(brd,&tmpconfig);
	    memcpy(&brd->config,&tmpconfig,sizeof(brd->config));
	    up(&brd->sem);
	}
        break;

    case EMERALD_IOCGEEPORTCONFIG:	/* get config from eeprom */
        {
	  emerald_config eeconfig;
	  if (down_interruptible(&brd->sem)) return -ERESTARTSYS;
	  ret = emerald_read_eeconfig(brd,&eeconfig);
	  up(&brd->sem);
	  if (copy_to_user((emerald_config *) arg,&eeconfig,
		  sizeof(emerald_config)) != 0) ret = -EFAULT;
	}
        break;
        
    case EMERALD_IOCSEEPORTCONFIG:	/* set config in eeprom */
	{
	    emerald_config eeconfig;
	    if (copy_from_user(&eeconfig,(emerald_config *) arg,
	      	sizeof(emerald_config)) != 0) ret = -EFAULT;
	    if (down_interruptible(&brd->sem)) return -ERESTARTSYS;
	    ret = emerald_write_eeconfig(brd,&eeconfig);
	    up(&brd->sem);
	}
        break;

    case EMERALD_IOCEECONFIGLOAD:	/* load EEPROM config */
	{
	    if (down_interruptible(&brd->sem)) return -ERESTARTSYS;
	    ret = emerald_load_config_from_eeprom(brd);
	    up(&brd->sem);
	}
        break;
    case EMERALD_IOCPORTENABLE:
	{
	    if (down_interruptible(&brd->sem)) return -ERESTARTSYS;
	    emerald_enable_ports(brd);
	    up(&brd->sem);
	}
	break;
    case EMERALD_IOCGNBOARD:
        if (copy_to_user((int*) arg,&emerald_nr_ok,
	      	sizeof(int)) != 0) ret = -EFAULT;
	break;

    case EMERALD_IOCGISABASE:
        if (copy_to_user((unsigned long*) arg,&ioport_base,
	      	sizeof(unsigned long)) != 0) ret = -EFAULT;
	break;

    /* get digio direction for a port, 1=out, 0=in */
    case EMERALD_IOCGDIOOUT:
	{
	    int iport = port->portNum;
	    int val;
	    if (down_interruptible(&brd->sem)) return -ERESTARTSYS;
	    val = emerald_get_digio_port_out(brd,iport);
	    if (copy_to_user((int*) arg,&val,
		    sizeof(int)) != 0) ret = -EFAULT;
	    up(&brd->sem);
	}
	break;
    /* set digio direction for a port, 1=out, 0=in */
    case EMERALD_IOCSDIOOUT:
	{
	    int iport = port->portNum;
	    int val = (int) arg;
	    if (down_interruptible(&brd->sem)) return -ERESTARTSYS;
	    emerald_set_digio_port_out(brd,iport,val);
	    up(&brd->sem);
	}
	break;

    /* get digio value for a port */
    case EMERALD_IOCGDIO:
	{
	    int iport = port->portNum;
	    int val;
	    if (down_interruptible(&brd->sem)) return -ERESTARTSYS;
	    val = emerald_read_digio_port(brd,iport);
	    if (copy_to_user((int*) arg,&val,
		    sizeof(int)) != 0) ret = -EFAULT;
	    up(&brd->sem);
	}
	break;

    /* set digio value for a port */
    case EMERALD_IOCSDIO:
	{
	    int iport = port->portNum;
	    int val = (int) arg;
	    // digio line must be an output
	    if (! (brd->digioout & (1 << iport))) return -EINVAL;
	    if (down_interruptible(&brd->sem)) return -ERESTARTSYS;
	    emerald_write_digio_port(brd,iport,val);
	    up(&brd->sem);
	}
	break;
    default:  /* redundant, as cmd was checked against MAXNR */
        return -ENOTTY;
    }
    return ret;
}

static struct file_operations emerald_fops = {
    /* llseek:     emerald_llseek, */
    /* read:       emerald_read, */
    /* write:      emerald_write, */
    ioctl:      emerald_ioctl,
    open:       emerald_open,
    release:    emerald_release,
};
						      
static int __init emerald_init_module(void)
{
    int result, i;

    /*
     * Register your major, and accept a dynamic number. This is the
     * first thing to do, in order to avoid releasing other module's
     * fops in emerald_cleanup_module()
     */
    result = register_chrdev(emerald_major, "emerald", &emerald_fops);
    if (result < 0) {
        printk(KERN_WARNING "emerald: can't get major %d\n",emerald_major);
        return result;
    }
    if (emerald_major == 0) emerald_major = result; /* dynamic */
    PDEBUGG("major=%d\n",emerald_major);

    for (i=0; i < EMERALD_MAX_NR_DEVS; i++)
      if (ioports[i] == 0) break;
    emerald_nr_addrs = i;
    PDEBUGG("nr_addrs=%d\n",emerald_nr_addrs);

    /*
     * allocate the board structures
     */
    emerald_boards = kmalloc(emerald_nr_addrs * sizeof(emerald_board), GFP_KERNEL);
    if (!emerald_boards) {
        result = -ENOMEM;
        goto fail;
    }
    memset(emerald_boards, 0, emerald_nr_addrs * sizeof(emerald_board));
    for (i=0; i < emerald_nr_addrs; i++) {
	emerald_board* ebrd = emerald_boards + i;
        ebrd->ioport = ioports[i] + ioport_base;
	if (!( ebrd->region =
	    request_region(ebrd->ioport,EMERALD_IO_REGION_SIZE,
			"emerald"))) {
	    result = -ENODEV;
	    goto fail;
	}
	sema_init(&ebrd->sem,1);
	/*
	 * Read EEPROM configuration and see if it looks OK.
	 * emerald_nr_ok will be the number of the last good board.
	 */
	if (!emerald_read_eeconfig(ebrd,&ebrd->config) &&
	    emerald_check_config(&ebrd->config)) 
			emerald_nr_ok = i + 1;
	else {
	    release_region(ebrd->ioport,EMERALD_IO_REGION_SIZE);
	    ebrd->region = 0;
	}
	emerald_set_digio_out(ebrd,0);
	emerald_read_digio(ebrd);
    }

    emerald_nports = emerald_nr_ok * EMERALD_NR_PORTS;
    emerald_ports = kmalloc(emerald_nports * sizeof(emerald_port), GFP_KERNEL);
    if (!emerald_ports) {
        result = -ENOMEM;
        goto fail;
    }
    memset(emerald_ports, 0,emerald_nports * sizeof(emerald_port));

    for (i=0; i < emerald_nports; i++) {
	emerald_port* eport = emerald_ports + i;
	emerald_board* ebrd = emerald_boards + (i / EMERALD_NR_PORTS);
	eport->board = ebrd;
	eport->portNum = i % EMERALD_NR_PORTS;	// 0-7
    }
    /* ... */
                                                                                
#ifndef EMERALD_DEBUG
    // EXPORT_NO_SYMBOLS; /* otherwise, leave global symbols visible */
#endif
                                                                                
#ifdef EMERALD_DEBUG /* only when debugging */
    PDEBUGG("create_proc\n");
    emerald_create_proc();
#endif
    return 0; /* succeed */

  fail:
    emerald_cleanup_module();
    return result;
}

module_init(emerald_init_module);
module_exit(emerald_cleanup_module);
