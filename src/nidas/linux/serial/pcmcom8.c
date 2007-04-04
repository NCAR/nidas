/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-02-08 23:32:30 -0700 (Thu, 08 Feb 2007) $

    $LastChangedRevision: 3661 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/linux/diamond/pcmcom8.c $

    Linux driver module for WinSystems pcmcom8 serial IO cards.
    This just queries and sets the ioport addresses and irqs on bootup.
    The normal linux serial driver for the 8250 family of uarts
    does the heavy work.
 ********************************************************************
*/


#include <linux/init.h>   /* module_init() */
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>   /* kmalloc() */
#include <linux/fs.h>       /* everything... */
// #include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>    /* O_ACCMODE */
#include <linux/ioport.h>
#include <asm/io.h>		/* outb, inb */
#include <asm/uaccess.h>	/* access_ok */
#include <asm/system.h>     /* cli(), *_flags */

#include <nidas/linux/isa_bus.h>
#define DEBUG
#include <nidas/linux/klog.h>
#include <nidas/linux/serial/pcmcom8.h>	/* local definitions */

static unsigned long ioport_base = SYSTEM_ISA_IOPORT_BASE;

static int pcmcom8_major =   PCMCOM8_MAJOR;
static unsigned long ioports[PCMCOM8_MAX_NR_DEVS] = {0,0,0,0};
static int pcmcom8_numboards = 0;
static int pcmcom8_nr_ok = 0;
static pcmcom8_board* pcmcom8_boards = 0;
static pcmcom8_port* pcmcom8_ports = 0;
static int pcmcom8_nports = 0;

static dev_t pcmcom8_device = MKDEV(0,0);

module_param_array(ioports,ulong,&pcmcom8_numboards,S_IRUGO);	/* io port virtual address */
MODULE_AUTHOR("Gordon Maclean");
MODULE_DESCRIPTION("driver module to initialize pcmcom8 serial port card");
MODULE_LICENSE("Dual BSD/GPL");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
#define PCMCOM8_LOCK(x) mutex_lock_interruptible(x)
#define PCMCOM8_UNLOCK(x) mutex_unlock(x)
#else
#define PCMCOM8_LOCK(x) down_interruptible(x)
#define PCMCOM8_UNLOCK(x) up(x)
#endif


/*
 * return 0 if config is not OK.
 * According to the Emerald-MM-8 User Manual, v2.4,
 * valid port addresses are 0x100-0x3f8.
 * Valid irqs are 2,3,4,5,6,7,10,11,12 and 15.
 * On the viper, ISA irqs 2 & 15 are not mapped.
 */
static int pcmcom8_check_config(struct pcmcom8_config* config)
{
        int i;
        for (i = 0; i < PCMCOM8_NR_PORTS; i++) {
                if (config->ports[i].ioport > 0x400) return 0;
                if (config->ports[i].ioport < 0x100) return 0;
                if (GET_SYSTEM_ISA_IRQ(config->ports[i].irq) < 0) return 0;
        }
        return 1;
}

static void pcmcom8_enable_ports(pcmcom8_board* brd)
{
        int i;
        unsigned char bar;
        for (i = 0; i < PCMCOM8_NR_PORTS; i++) {
                outb(i,brd->ioport+PCMCOM8_IDX);
                bar = inb(brd->ioport+PCMCOM8_ADR);
                bar |= 0x80;
                outb(bar,brd->ioport+PCMCOM8_ADR);	/* enable port */
        }
}


static void pcmcom8_read_config(pcmcom8_board* brd)
{
        int i;
        /* read ioport and irq values. */
        for (i = 0; i < PCMCOM8_NR_PORTS; i++) {
                outb(i,brd->ioport+PCMCOM8_IDX);
                brd->config.ports[i].ioport =
                    (inb(brd->ioport+PCMCOM8_ADR) & 0x7f) << 3;
                brd->config.ports[i].irq = inb(brd->ioport+PCMCOM8_IAR);
        }
}

static int pcmcom8_write_config(pcmcom8_board* brd,struct pcmcom8_config* config) {
        int i;
        /* write ioport and irq values. */
        for (i = 0; i < PCMCOM8_NR_PORTS; i++) {
                outb(i,brd->ioport+PCMCOM8_IDX);
                outb(config->ports[i].ioport >> 3,brd->ioport+PCMCOM8_ADR);
                outb(config->ports[i].irq,brd->ioport+PCMCOM8_IAR);
        }
        brd->config = *config;
        pcmcom8_enable_ports(brd);
        return 0;
}
static int pcmcom8_wait_eedone(pcmcom8_board* brd)
{
        int ntry;
        unsigned char status;

        ntry = 10;
        while (ntry--) {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(1);
                status = inb(brd->ioport + PCMCOM8_STA);
                if ((status & 0xc0) == 0x80) break;
        }
        KLOG_DEBUG("read EERPOM ntry=%d\n",ntry);
        if (ntry == 0) return -ETIMEDOUT;
        return 0;
}

static int pcmcom8_write_eeprom(pcmcom8_board* brd,int eeaddr, unsigned int value)
{
        int res = 0;
        outb(eeaddr+0x40,brd->ioport+PCMCOM8_ECR);

        outb((value >> 8) & 0xff,brd->ioport+PCMCOM8_EHR);
        outb(value & 0xff,brd->ioport+PCMCOM8_ELR);
        outb(0x01,brd->ioport+PCMCOM8_CMD);

        if ((res = pcmcom8_wait_eedone(brd)) != 0) return res;
}

/*
 * Read the ioport and irq configuration for the 8 serial ports
 * from the Emerald EEPROM.
 */
static int pcmcom8_read_eeconfig(pcmcom8_board* brd,struct pcmcom8_config* config)
{
        int i;
        int res = 0;

        /* get ioport values from EEPROM addresses 0-7 */
        for (i = 0; i < PCMCOM8_NR_PORTS; i++) {
                outb(i+0x80,brd->ioport+PCMCOM8_ECR);
                outb(0x01,brd->ioport+PCMCOM8_CMD);

                if ((res = pcmcom8_wait_eedone(brd)) != 0) return res;

                config->ports[i].ioport = inb(brd->ioport + PCMCOM8_EHR) << 3;
                config->ports[i].irq = inb(brd->ioport + PCMCOM8_ELR);
        }
        return 0;
}

/*
 * Write the ioport and irq configuration for the 8 serial ports
 * to the Emerald EEPROM.
 */
static int pcmcom8_write_eeconfig(pcmcom8_board* brd,struct pcmcom8_config* config)
{
        int i;
        int res = 0;

        // enable EEPROM writes
        outb(0x30,brd->ioport + PCMCOM8_ECR);
        outb(0x01,brd->ioport + PCMCOM8_CMD);
        if ((res = pcmcom8_wait_eedone(brd)) != 0) return res;

        /* write ioport values to EEPROM addresses 0-7 */
        for (i = 0; i < PCMCOM8_NR_PORTS; i++) {
                unsigned int val = config->ports[i].ioport >> 3;
                val <<= 8;
                val |= 0x8000;      // enable uart bit
                val += config->ports[i].irq;
                pcmcom8_write_eeprom(brd,i,val);
        }

        // disable EEPROM writes
        outb(0x00,brd->ioport + PCMCOM8_ECR);
        outb(0x01,brd->ioport + PCMCOM8_CMD);
        if ((res = pcmcom8_wait_eedone(brd)) != 0) return res;
        return res;
}

/*
 * Load the the ioport and irq configuration from the Emerald
 * EEPROM into ram.
 */
static int pcmcom8_load_config_from_eeprom(pcmcom8_board* brd,int eeaddr)
{
        int res = 0;
        outb(eeaddr+0x80,brd->ioport+PCMCOM8_ECR);
        outb(0x03,brd->ioport + PCMCOM8_CMD);
        if ((res = pcmcom8_wait_eedone(brd)) != 0) return res;
        return res;
}

#ifdef PCMCOM8_DEBUG /* use proc only if debugging */
/*
 * The proc filesystem: function to read
 */
static int pcmcom8_read_procmem(char *buf, char **start, off_t offset,
                   int count, int *eof, void *data)
{
        int i, j, len = 0;
        int limit = count - 80; /* Don't print more than this */
        int result = 0;
        KLOG_DEBUG("read_proc, count=%d\n",count);
                                                                                    
        for (i = 0; i < pcmcom8_nr_ok && len <= limit; i++) {
                pcmcom8_board *brd = pcmcom8_boards + i;
                KLOG_DEBUG("read_proc, i=%d, device=0x%lx\n",i,(unsigned long)d);
                if ((result = PCMCOM8_LOCK(&brd->mutex))) return result;
                pcmcom8_read_config(brd);
                len += sprintf(buf+len,"\nWinSystems PCMCOM8 %i: ioport %lx\n",
                               i, brd->ioport);
                /* loop over serial ports */
                for (j = 0; len <= limit && j < PCMCOM8_NR_PORTS; j++) {
                    len += sprintf(buf+len, "  port %d, ioport=%x,irq=%d\n",
                        j,brd->config.ports[j].ioport,brd->config.ports[j].irq);
                }
                PCMCOM8_UNLOCK(&brd->mutex);
        }
        *eof = 1;
        return len;
}

static void pcmcom8_create_proc(void)
{
        KLOG_DEBUG("within pcmcom8_create_proc\n");
        create_proc_read_entry("pcmcom8", 0644 /* default mode */,
                           NULL /* parent dir */, pcmcom8_read_procmem,
                           NULL /* client data */);
}
                                                                                
static void pcmcom8_remove_proc(void)
{
        /* no problem if it was not registered */
        remove_proc_entry("pcmcom8", NULL /* parent dir */);
}

#endif

static int pcmcom8_open (struct inode *inode, struct file *filp)
{
        int num = MINOR(inode->i_rdev);
        int result = 0;
        pcmcom8_port *port; /* device information */

        /*  check the device number */
        if (num >= pcmcom8_nports) return -ENODEV;
        port = pcmcom8_ports + num;

        /* and use filp->private_data to point to the device data */
        filp->private_data = port;

        if ((result = PCMCOM8_LOCK(&port->board->mutex))) return result;

        pcmcom8_read_config(port->board);
        
        PCMCOM8_UNLOCK(&port->board->mutex);

        return result;	/* success */
}

static int pcmcom8_release (struct inode *inode, struct file *filp)
{
        return 0;
}

/*
 * The ioctl() implementation
 */

static int pcmcom8_ioctl (struct inode *inode, struct file *filp,
                 unsigned int cmd, unsigned long arg)
{
        pcmcom8_port* port = filp->private_data;
        pcmcom8_board* brd = port->board;
        int err= 0, ret = 0;

        /* don't even decode wrong cmds: better returning  ENOTTY than EFAULT */
        if (_IOC_TYPE(cmd) != PCMCOM8_IOC_MAGIC) return -ENOTTY;
        if (_IOC_NR(cmd) > PCMCOM8_IOC_MAXNR) return -ENOTTY;

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

        case PCMCOM8_IOCGIOPORT:
                if (copy_to_user((unsigned long*) arg,&brd->ioport,
                        sizeof(unsigned long)) != 0) ret = -EFAULT;
                break;
        case PCMCOM8_IOCGPORTCONFIG:	/* get port config */
                if (copy_to_user((struct pcmcom8_config *) arg,&brd->config,
                        sizeof(struct pcmcom8_config)) != 0) ret = -EFAULT;
                break;
            
        case PCMCOM8_IOCSPORTCONFIG:	/* set port config */
                {
                struct pcmcom8_config tmpconfig;
                if (copy_from_user(&tmpconfig,(struct pcmcom8_config *) arg,
                    sizeof(struct pcmcom8_config)) != 0) ret = -EFAULT;
                if (ret) break;

                if ((ret = PCMCOM8_LOCK(&brd->mutex))) return ret;
                ret = pcmcom8_write_config(brd,&tmpconfig);
                memcpy(&brd->config,&tmpconfig,sizeof(brd->config));
                PCMCOM8_UNLOCK(&brd->mutex);
                }
                break;

        case PCMCOM8_IOCGEEPORTCONFIG:	/* get config from eeprom */
                {
                struct pcmcom8_config eeconfig;
                if ((ret = PCMCOM8_LOCK(&brd->mutex))) return ret;
                ret = pcmcom8_read_eeconfig(brd,&eeconfig);
                PCMCOM8_UNLOCK(&brd->mutex);
                if (copy_to_user((struct pcmcom8_config *) arg,&eeconfig,
                          sizeof(struct pcmcom8_config)) != 0) ret = -EFAULT;
                }
                break;
            
        case PCMCOM8_IOCSEEPORTCONFIG:	/* set config in eeprom */
                {
                struct pcmcom8_config eeconfig;
                if (copy_from_user(&eeconfig,(struct pcmcom8_config *) arg,
                    sizeof(struct pcmcom8_config)) != 0) ret = -EFAULT;
                if (ret) break;
                if ((ret = PCMCOM8_LOCK(&brd->mutex))) return ret;
                ret = pcmcom8_write_eeconfig(brd,&eeconfig);
                PCMCOM8_UNLOCK(&brd->mutex);
                }
                break;
        case PCMCOM8_IOCEECONFIGLOAD:	/* load EEPROM config */
                {
                if ((ret = PCMCOM8_LOCK(&brd->mutex))) return ret;
                ret = pcmcom8_load_config_from_eeprom(brd,0);
                PCMCOM8_UNLOCK(&brd->mutex);
                }
                break;
        case PCMCOM8_IOCPORTENABLE:
                {
                if ((ret = PCMCOM8_LOCK(&brd->mutex))) return ret;
                pcmcom8_enable_ports(brd);
                PCMCOM8_UNLOCK(&brd->mutex);
                }
                break;
        case PCMCOM8_IOCGNBOARD:
                if (copy_to_user((int*) arg,&pcmcom8_nr_ok,
                        sizeof(int)) != 0) ret = -EFAULT;
                break;

        case PCMCOM8_IOCGISABASE:
                if (copy_to_user((unsigned long*) arg,&ioport_base,
                        sizeof(unsigned long)) != 0) ret = -EFAULT;
                break;
        default:  /* redundant, as cmd was checked against MAXNR */
            return -ENOTTY;
        }
        return ret;
}

static struct file_operations pcmcom8_fops = {
        ioctl:      pcmcom8_ioctl,
        open:       pcmcom8_open,
        release:    pcmcom8_release,
};
						      
/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static void __exit pcmcom8_cleanup_module(void)
{
        int i;
        /* cleanup_module is never called if registering failed */
        unregister_chrdev(pcmcom8_major, "pcmcom8");
                                                                                
#ifdef PCMCOM8_DEBUG /* use proc only if debugging */
        pcmcom8_remove_proc();
#endif

        if (pcmcom8_ports) kfree(pcmcom8_ports);

        if (pcmcom8_boards) {
                for (i=0; i<pcmcom8_nr_ok; i++) {
                        pcmcom8_board* brd = pcmcom8_boards + i;
                        if (brd->region) 
                            release_region(brd->ioport,PCMCOM8_IO_REGION_SIZE);
                        if (brd->ready) cdev_del(&brd->cdev);
                }
                kfree(pcmcom8_boards);
        }

        if (MAJOR(pcmcom8_device) != 0)
               unregister_chrdev_region(pcmcom8_device,pcmcom8_numboards);
}

static int __init pcmcom8_init_module(void)
{
        int result, i;
        dev_t devno;

        for (i=0; i < PCMCOM8_MAX_NR_DEVS; i++)
          if (ioports[i] == 0) break;
        pcmcom8_numboards = i;
        KLOG_DEBUG("numboards=%d\n",pcmcom8_numboards);

        /*
         * Register your major, and accept a dynamic number. This is the
         * first thing to do, in order to avoid releasing other module's
         * fops in pcmcom8_cleanup_module()
         */
        result = alloc_chrdev_region(&pcmcom8_device, 0,
            pcmcom8_numboards,"pcmcom8");
        if (result < 0) goto fail;

        /*
         * allocate the board structures
         */
        pcmcom8_boards = kmalloc(pcmcom8_numboards * sizeof(pcmcom8_board), GFP_KERNEL);
        if (!pcmcom8_boards) {
                result = -ENOMEM;
                goto fail;
        }
        memset(pcmcom8_boards, 0, pcmcom8_numboards * sizeof(pcmcom8_board));
        for (i=0; i < pcmcom8_numboards; i++) {
                pcmcom8_board* brd = pcmcom8_boards + i;
                brd->ioport = ioports[i] + ioport_base;
                if (!( brd->region =
                    request_region(brd->ioport,PCMCOM8_IO_REGION_SIZE,
                                "pcmcom8"))) {
                    result = -ENODEV;
                    goto fail;
                }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
                mutex_init(&brd->mutex);
#else
                init_MUTEX(&brd->mutex);
#endif
                /*
                 * Read EEPROM configuration and see if it looks OK.
                 * pcmcom8_nr_ok will be the number of the last good board.
                 */
                if (!pcmcom8_read_eeconfig(brd,&brd->config) &&
                        pcmcom8_check_config(&brd->config)) 
                                    pcmcom8_nr_ok = i + 1;
                else {
                        release_region(brd->ioport,PCMCOM8_IO_REGION_SIZE);
                        brd->region = 0;
                }
        }

        pcmcom8_nports = pcmcom8_nr_ok * PCMCOM8_NR_PORTS;
        pcmcom8_ports = kmalloc(pcmcom8_nports * sizeof(pcmcom8_port), GFP_KERNEL);
        if (!pcmcom8_ports) {
                result = -ENOMEM;
                goto fail;
        }
        memset(pcmcom8_ports, 0,pcmcom8_nports * sizeof(pcmcom8_port));

        for (i=0; i < pcmcom8_nports; i++) {
                pcmcom8_port* eport = pcmcom8_ports + i;
                pcmcom8_board* brd = pcmcom8_boards + (i / PCMCOM8_NR_PORTS);
                eport->board = brd;
                eport->portNum = i % PCMCOM8_NR_PORTS;	// 0-7
        }
        for (i=0; i < pcmcom8_nr_ok; i++) {
                pcmcom8_board* brd = pcmcom8_boards + i;
                cdev_init(&brd->cdev,&pcmcom8_fops);
                brd->cdev.owner = THIS_MODULE;
                devno = MKDEV(MAJOR(pcmcom8_device),i);
                // after calling cdev_add the device is read for operations
                result = cdev_add(&brd->cdev,devno,1);
                if (result) return result;
                brd->ready = 1;
        }
  
#ifdef PCMCOM8_DEBUG /* only when debugging */
        KLOG_DEBUG("create_proc\n");
        pcmcom8_create_proc();
#endif
        return result; /* succeed */

fail:
        pcmcom8_cleanup_module();
        return result;
}

module_init(pcmcom8_init_module);
module_exit(pcmcom8_cleanup_module);
