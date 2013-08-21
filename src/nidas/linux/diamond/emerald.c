/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*-
 * vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    Linux driver module for Diamond Emerald serial IO cards.
    This just queries and sets the ioport addresses and irqs of the UARTs.
    The normal linux serial driver for the 8250 family of UARTs does the heavy work.

    This driver also supports the 8 digital I/O lines on a Diamond Emerald card.
    Each digio line is accessed via a minor number, ranging from 0 to 7 for the
    eight lines. Each digio line then has an associated device, which can be
    named similarly to the serial ports:
        serial port: /dev/ttyS9 
        digio line:  /dev/ttyD9 

    Any of the minor device numbers 0-7 can be used when doing any of the ioctls to
    set/get parameters on the whole board.

    This module can also set the RS232/422/485 mode for each serial port on an EMM-8P.

 ********************************************************************
*/

#ifndef __KERNEL__
#  define __KERNEL__
#endif
#ifndef MODULE
#  define MODULE
#endif

#include <linux/module.h>
#include <linux/version.h>

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/fs.h>       /* everything... */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>    /* O_ACCMODE */
#include <linux/ioport.h>
#include <linux/sched.h>    /* schedule() */
#include <asm/io.h>		/* outb, inb */
#include <asm/uaccess.h>	/* access_ok */
// #include <linux/delay.h>     /* msleep */

/* for testing UART registers */
#include <linux/serial_reg.h>

#include "emerald.h"	/* local definitions */

#include <nidas/linux/isa_bus.h>
#include <nidas/linux/klog.h>
#include <nidas/linux/SvnInfo.h>    // SVNREVISION

static dev_t emerald_device = MKDEV(0,0);

static unsigned long ioport_base = SYSTEM_ISA_IOPORT_BASE;

static unsigned int ioports[EMERALD_MAX_NR_DEVS] = {0,0,0,0};
static int emerald_nr_addrs = 0;
static int emerald_nr_ok = 0;
static emerald_board* emerald_boards = 0;
static emerald_port* emerald_ports = 0;
static int emerald_nports = 0;


#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(ioports, uint, &emerald_nr_addrs, S_IRUGO);	/* io port address */
#else
module_param_array(ioports, uint, emerald_nr_addrs, S_IRUGO);	/* io port address */
#endif

MODULE_AUTHOR("Gordon Maclean");
MODULE_DESCRIPTION("driver module supporting initialization and digital I/O on Diamond System Emerald serial port card");
MODULE_LICENSE("Dual BSD/GPL");

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
#define mutex_init(x)               init_MUTEX(x)
#define mutex_lock_interruptible(x) ( down_interruptible(x) ? -ERESTARTSYS : 0)
#define mutex_unlock(x)             up(x)
#endif

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
static int emm_check_config(emerald_config* config,const char* devname) {
        int i,j;
#ifdef CONFIG_ARCH_VIPER
        int valid_irqs[]={3,4,5,6,7,10,11,12};
        int nvalid = 8;
#elif defined(CONFIG_MACH_ARCOM_TITAN)
        int valid_irqs[]={3,4,5,6,7,10,11,12};
        int nvalid = 8;
#else
        int valid_irqs[]={2,3,4,5,6,7,10,11,12,15};
        int nvalid = 10;
#endif

        for (i = 0; i < EMERALD_NR_PORTS; i++) {
                KLOG_DEBUG("ioport=%x\n",config->ports[i].ioport);
                if (config->ports[i].ioport > 0x3f8 || config->ports[i].ioport < 0x100) {
                        KLOG_NOTICE("%s: invalid ioport[%d]=%#x\n",
                                        devname,i,config->ports[i].ioport);
                }
                if (i > 0 && (config->ports[i].ioport - config->ports[i-1].ioport) != 8) {
                        KLOG_NOTICE("%s: invalid ioport diff: ioport[%d]=%#x,ioport[%d]=%#x\n",
                                devname,i-1,config->ports[i-1].ioport,i,config->ports[i].ioport);
                        return 0;
                }
                for (j = 0; j < nvalid; j++) {
                        KLOG_DEBUG("checking irq=%d against %d\n",
                          config->ports[i].irq,valid_irqs[j]);
                        if (valid_irqs[j] == config->ports[i].irq) break;
                }
                if (j == nvalid) {
                        KLOG_NOTICE("%s: invalid irq[%d]=%d\n",
                                devname,i,config->ports[i].irq);
                        return 0;
                }
        }
        return 1;
}

static void emm_enable_ports(emerald_board* brd)
{
        outb(0x80,brd->addr+EMERALD_APER);	/* enable ports */
}
static void emm_disable_ports(emerald_board* brd)
{
        outb(0x00,brd->addr+EMERALD_APER);	/* disable ports */
}

/*
 * An EMM-8P provides registers 16 and 17 for setting
 * the RSXXX mode. If a value is written to one of those
 * registers, that same value should be read back.
 *
 * On an EMM-8, which doesn't support setting the mode,
 * registers 16 and 17 are undefined.  Testing indicates
 * that on an EMM-8, a value written to regs 16 or 17,
 * is read back, except the high order bit is always 0.
 *
 * We're using undefined behaviour to try to detect which model
 * the card is. Otherwise, the model or version is
 * not available at run-time.
 */
static int emm_check_model(emerald_board* brd)
{
        unsigned int val, newval;

        /* read value of reg 16 */
        outb(EMERALD_NR_PORTS * 2,brd->addr+EMERALD_APER);
        val = inb(brd->addr+EMERALD_ARR);

        /* Set high order bit in register 16 and read the value back. */
        outb(EMERALD_NR_PORTS * 2,brd->addr+EMERALD_APER);
        outb(0x80 | val,brd->addr+EMERALD_AIDR);

        /* check if value read matches written, if not its an EMM-8 */
        outb(EMERALD_NR_PORTS * 2,brd->addr+EMERALD_APER);
        newval = inb(brd->addr+EMERALD_ARR);

        // set reg 16 back to the original value.
        outb(EMERALD_NR_PORTS * 2,brd->addr+EMERALD_APER);
        outb(val,brd->addr+EMERALD_AIDR);

        KLOG_DEBUG("%s: val=%x, expected=%x\n",brd->deviceName,
                        newval, (unsigned int)(val | 0x80));

        if (newval != (0x80 | val)) return EMERALD_MM_8;
        return EMERALD_MM_8P;
}

static int emm_read_config(emerald_board* brd)
{
        int i,val;
        /* read ioport values. */
        for (i = 0; i < EMERALD_NR_PORTS; i++) {
                outb(i,brd->addr+EMERALD_APER);
                val = inb(brd->addr+EMERALD_ARR);
                if (val == 0xff) return -ENODEV;
                brd->config.ports[i].ioport = val << 3;

                // According to the Emerald manual the IRQ registers
                // cannot be read back. It does work just after
                // a emerald_write_config, but not after a bootup.
#ifdef TRY_READ_IRQ_FROM_REGISTER
                outb(i+EMERALD_NR_PORTS,brd->addr+EMERALD_APER);
                val = inb(brd->addr+EMERALD_ARR);
                if (val == 0xff) return -ENODEV;
                brd->config.ports[i].irq = val;
#endif
        }
        return 0;
}

static int emm_write_config(emerald_board* brd,emerald_config* config)
{
        int i;
        /* write ioport and irq values. */
        for (i = 0; i < EMERALD_NR_PORTS; i++) {
                outb(i,brd->addr+EMERALD_APER);
                outb(config->ports[i].ioport >> 3,brd->addr+EMERALD_AIDR);

                outb(i+EMERALD_NR_PORTS,brd->addr+EMERALD_APER);
                outb(config->ports[i].irq,brd->addr+EMERALD_AIDR);
        }
        // copy config because we can't read IRQ values from registers
        brd->config = *config;
        /* ports will not be enabled after writing the configuration */
        return 0;
}

/*
 * Read the ioport and irq configuration for the 8 serial ports
 * from the Emerald EEPROM.
 */
static int emm_read_eeconfig(emerald_board* brd,emerald_config* config)
{
        int i;
        unsigned char busy;
        int ntry;

        /* get ioport values from EEPROM addresses 0-7 */
        for (i = 0; i < EMERALD_NR_PORTS; i++) {
                outb(i,brd->addr+EMERALD_ECAR);
                /* wait for busy bit in EMERALD_EBR to clear */
                ntry = 5;
                do {
                        unsigned long jwait = jiffies + 1;
                        while (time_before(jiffies,jwait)) schedule();
                        busy = inb(brd->addr+EMERALD_EBR);
                        if (busy == 0xff) return -ENODEV;
                } while(busy & 0x80 && --ntry);
                if (!ntry) return -ETIMEDOUT;
                config->ports[i].ioport = (int)inb(brd->addr+EMERALD_EDR) << 3;
        }

        /* get irq values from EEPROM addresses 8-15 */
        for (i = 0; i < EMERALD_NR_PORTS; i++) {
                outb(i+EMERALD_NR_PORTS,brd->addr+EMERALD_ECAR);
                /* wait for busy bit in EMERALD_EBR to clear */
                ntry = 5;
                do {
                        unsigned long jwait = jiffies + 1;
                        while (time_before(jiffies,jwait)) schedule();
                        busy = inb(brd->addr+EMERALD_EBR);
                } while(busy & 0x80 && --ntry);
                if (!ntry) return -ETIMEDOUT;
                config->ports[i].irq = inb(brd->addr+EMERALD_EDR);
        }
        return 0;
}

/*
 * Write the ioport and irq configuration for the 8 serial ports
 * to the Emerald EEPROM.
 */
static int emm_write_eeconfig(emerald_board* brd,emerald_config* config)
{
        int i;
        unsigned char busy;
        int ntry;

        /* write ioport values to EEPROM addresses 0-7 */
        for (i = 0; i < EMERALD_NR_PORTS; i++) {
                outb(config->ports[i].ioport >> 3,brd->addr+EMERALD_EDR);
                outb(i + 0x80,brd->addr+EMERALD_ECAR);

                /* wait for busy bit in EMERALD_EBR to clear */
                ntry = 5;
                do {
                        unsigned long jwait = jiffies + 1;
                        while (time_before(jiffies,jwait)) schedule();
                        busy = inb(brd->addr+EMERALD_EBR);
                } while(busy & 0x80 && --ntry);
                if (!ntry) return -ETIMEDOUT;
        }

        /* write irq values to EEPROM addresses 8-15 */
        for (i = 0; i < EMERALD_NR_PORTS; i++) {
                outb(config->ports[i].irq,brd->addr+EMERALD_EDR);
                outb(i + EMERALD_NR_PORTS + 0x80,brd->addr+EMERALD_ECAR);

                /* wait for busy bit in EMERALD_EBR to clear */
                ntry = 5;
                do {
                        unsigned long jwait = jiffies + 1;
                        while (time_before(jiffies,jwait)) schedule();
                        busy = inb(brd->addr+EMERALD_EBR);
                } while(busy & 0x80 && --ntry);
                if (!ntry) return -ETIMEDOUT;
        }
        return 0;
}

/*
 * Load the the ioport and irq configuration from the Emerald
 * EEPROM into registers.
 */
static int emm_load_config_from_eeprom(emerald_board* brd)
{

        unsigned char busy;
        int ntry;
        int result = 0;

        outb(0x80,brd->addr+EMERALD_CRR);	/* reload configuration from eeprom */

        /* wait for busy bit in EMERALD_EBR to clear */
        ntry = 5;
        do {
                unsigned long jwait = jiffies + 1;
                while (time_before(jiffies,jwait)) schedule();
                busy = inb(brd->addr+EMERALD_EBR);
        } while(busy & 0x80 && --ntry);
        if (!ntry) return -ETIMEDOUT;

        // read back. Must get IRQs from EEPROM
        result = emm_read_eeconfig(brd,&brd->config);
        // re-read registers to make sure
        if (!result) result = emm_read_config(brd);
        return result;
}

static const char* emm_mode_to_string(int mode)
{
        switch (mode) {
        case EMERALD_RS232:
                return "RS232";
        case EMERALD_RS422:
                return "RS422";
        case EMERALD_RS485_ECHO:
                return "RS485_ECHO";
        case EMERALD_RS485_NOECHO:
                return "RS485_NOECHO";
        default:
                return "UNKNOWN";
        }
}

/*
 * printk the mode settings
 */
static void emm_printk_port_modes(emerald_board* brd)
{
        unsigned int i,j,val,enabled;
        char outstr[128],*cp;

        /* not supported on an EMM-8 */
        if (brd->model != EMERALD_MM_8P) return;

        enabled = inb(brd->addr+EMERALD_APER);

        cp = outstr;
        for (i = 0; i < 2; i++) {
                outb(EMERALD_NR_PORTS*2 + i,brd->addr+EMERALD_APER);
                val = inb(brd->addr+EMERALD_ARR);
                for (j = 0; j < 4; j++) {
                        int mode = (val >> (j * 2)) & 0x03;
                        if (i+j > 0) *cp++ = ',';
                        cp += sprintf(cp,"%d=%s",i*4+j,emm_mode_to_string(mode));
                }
        }

        if (enabled & 0x80) emm_enable_ports(brd);
        KLOG_INFO("%s: port %s\n",brd->deviceName,outstr);
}

/*
 * Set the mode on a serial port.
 * 0=RS232,1=RS422,2=RS485 with echo, 3=RS485 no echo
 */
static int emm_set_port_mode(emerald_board* brd,int port,int mode)
{
        unsigned int val,cfg,enabled;

        /* not supported on an EMM-8 */
        if (brd->model != EMERALD_MM_8P) return -EINVAL;

        enabled = inb(brd->addr+EMERALD_APER);

        /* addresses 16 and 17 each have 4 2-bit fields containing
         * the RS232/422/485 mode for 4 ports */
        outb(EMERALD_NR_PORTS*2 + (port / 4),brd->addr+EMERALD_APER);

        val = inb(brd->addr+EMERALD_ARR);
        KLOG_DEBUG("port=%d,reg=%d,curr val=%x\n",
                        port,EMERALD_NR_PORTS*2 + (port / 4),val);
        cfg = 3 << ((port % 4) * 2);
        val &= ~cfg;
        cfg = mode << ((port % 4) * 2);
        val |= cfg;
        KLOG_DEBUG("port=%d,reg=%d,new val=%x\n",
                        port,EMERALD_NR_PORTS*2 + (port / 4),val);

        outb(EMERALD_NR_PORTS*2 + (port / 4),brd->addr+EMERALD_APER);
        outb(val,brd->addr+EMERALD_AIDR);

        /* Read back. If it is not what was written
         * then the address does not point to a EMM-8P board.
         * This issue should have been detected by checking the model.
         */
        outb(EMERALD_NR_PORTS*2 + (port / 4),brd->addr+EMERALD_APER);
        cfg = inb(brd->addr+EMERALD_ARR);

        if (enabled & 0x80) emm_enable_ports(brd);

        if (cfg != val) {
                KLOG_WARNING("%s: port=%d,reg=%d, wrote value %#x not equal to read value %#x\n",
                                brd->deviceName,port,EMERALD_NR_PORTS*2 + (port / 4),val,cfg);
                return -ENODEV;
        }
        return 0;
}

/*
 * Get the mode setting of a serial port.
 */
static int emm_get_port_mode(emerald_board* brd,int port)
{
        unsigned int val,enabled;

        /* not supported on an EMM-8 */
        if (brd->model != EMERALD_MM_8P) return -EINVAL;

        enabled = inb(brd->addr+EMERALD_APER);

        outb(EMERALD_NR_PORTS*2 + (port / 4),brd->addr+EMERALD_APER);
        val = inb(brd->addr+EMERALD_ARR);
        KLOG_DEBUG("port=%d,reg=%d,read back val=%x,prot=%d\n",
                        port,EMERALD_NR_PORTS*2 + (port / 4),val,
                        (val >> ((port % 4) * 2)) & 0x03);

        if (enabled & 0x80) emm_enable_ports(brd);
        return (val >> ((port % 4) * 2)) & 0x03;
}

/*
 * Set the mode on a serial port into eeprom.
 * 0=RS232,1=RS422,2=RS485 with echo, 3=RS485 no echo
 */
static int emm_set_port_mode_eeprom(emerald_board* brd,int port, int mode)
{
        unsigned int val,cfg;
        unsigned char busy;
        int ntry;

        /* not supported on an EMM-8 */
        if (brd->model != EMERALD_MM_8P) return -EINVAL;

        /* get mode configuration from EEPROM address 16 or 17 */
        outb(EMERALD_NR_PORTS*2 + (port / 4),brd->addr+EMERALD_ECAR);
        /* wait for busy bit in EMERALD_EBR to clear */
        ntry = 5;
        do {
                unsigned long jwait = jiffies + 1;
                while (time_before(jiffies,jwait)) schedule();
                busy = inb(brd->addr+EMERALD_EBR);
                if (busy == 0xff) return -ENODEV;
        } while(busy & 0x80 && --ntry);
        if (!ntry) return -ETIMEDOUT;
        val = (int)inb(brd->addr+EMERALD_EDR);

        cfg = 3 << ((port % 4) * 2);
        val &= ~cfg;
        cfg = mode << ((port % 4) * 2);
        val |= cfg;

        /* write mode configuration to EEPROM address 16 or 17 */
        outb(val,brd->addr+EMERALD_EDR);
        outb(EMERALD_NR_PORTS*2 + (port / 4) + 0x80,brd->addr+EMERALD_ECAR);

        /* wait for busy bit in EMERALD_EBR to clear */
        ntry = 5;
        do {
                unsigned long jwait = jiffies + 1;
                while (time_before(jiffies,jwait)) schedule();
                busy = inb(brd->addr+EMERALD_EBR);
        } while(busy & 0x80 && --ntry);
        if (!ntry) return -ETIMEDOUT;

        return 0;
}

/*
 * Get the mode on a serial port from eeprom.
 */
static int emm_get_port_mode_eeprom(emerald_board* brd,int port)
{
        unsigned int val;
        unsigned char busy;
        int ntry;

        /* not supported on an EMM-8 */
        if (brd->model != EMERALD_MM_8P) return -EINVAL;

        /* get mode configuration from EEPROM address 16 or 17 */
        outb(EMERALD_NR_PORTS*2 + (port / 4),brd->addr+EMERALD_ECAR);
        /* wait for busy bit in EMERALD_EBR to clear */
        ntry = 5;
        do {
                unsigned long jwait = jiffies + 1;
                while (time_before(jiffies,jwait)) schedule();
                busy = inb(brd->addr+EMERALD_EBR);
                if (busy == 0xff) return -ENODEV;
        } while(busy & 0x80 && --ntry);
        if (!ntry) return -ETIMEDOUT;
        val = (int)inb(brd->addr+EMERALD_EDR);

        return (val >> ((port % 4) * 2)) & 0x03;
}

static int emm_get_digio_port_out(emerald_board* brd,int port)
{
        return (brd->digioout & (1 << port)) != 0;
}

static void emm_set_digio_out(emerald_board* brd,int val)
{
        outb(val,brd->addr+EMERALD_DDR);
        brd->digioout = val;
}

static void emm_set_digio_port_out(emerald_board* brd,int port,int val)
{
        if (val) brd->digioout |= 1 << port;
        else brd->digioout &= ~(1 << port);
        outb(brd->digioout,brd->addr+EMERALD_DDR);
}

static int emm_read_digio(emerald_board* brd)
{
        brd->digioval = inb(brd->addr+EMERALD_DIR);
        return brd->digioval;
}

static int emm_read_digio_port(emerald_board* brd,int port)
{
        int val = emm_read_digio(brd);
        return (val & (1 << port)) != 0;
}

static void emm_write_digio_port(emerald_board* brd,int port,int val)
{
        if (val) brd->digioval |= 1 << port;
        else brd->digioval &= ~(1 << port);

        // this does not effect digital input lines
        outb(brd->digioval,brd->addr+EMERALD_DOR);
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
        KLOG_DEBUG("count=%d\n",count);
                                                                                    
        for (i = 0; i < emerald_nr_ok && len <= limit; i++) {
                struct emerald_board *brd = emerald_boards + i;
                KLOG_DEBUG("i=%d, device=0x%lx\n",i,(unsigned long)brd);
                len += sprintf(buf+len,"\nDiamond Emerald-MM-8 %i: ioport %lx\n",
                               i, brd->addr);
                /* loop over serial ports */
                for (j = 0; len <= limit && j < EMERALD_NR_PORTS; j++) {
                        len += sprintf(buf+len, "  port %d, ioport=%x,irq=%d\n",
                            j,brd->config.ports[j].ioport,brd->config.ports[j].irq);
                }
        }
        *eof = 1;
        return len;
}

static void emerald_create_proc(void)
{
        KLOG_DEBUG("within create_proc\n");
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

static int emerald_open (struct inode *inode, struct file *filp)
{
        int num = MINOR(inode->i_rdev);
        emerald_port *port; /* device information */

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        /*  check the device number */
        if (num >= emerald_nports) return -ENODEV;
        port = emerald_ports + num;

        /* and use filp->private_data to point to the device data */
        filp->private_data = port;

        // Don't access the ioport/irq configuration registers here.
        // It may interfere with simultaneous serial port accesses.
        return 0;
}
						      
static int emerald_release (struct inode *inode, struct file *filp)
{
        return 0;
}

/*
 * The ioctl() implementation
 */

static long emerald_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
        emerald_port* port = filp->private_data;
        emerald_board* brd = port->board;
        int err= 0, ret = 0;

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
        case EMERALD_IOCGPORTCONFIG:	/* get current irq and ioport configuration for all serial ports */
                if (copy_to_user((emerald_config *) arg,&brd->config,
                        sizeof(emerald_config)) != 0) ret = -EFAULT;
                break;
        case EMERALD_IOCSPORTCONFIG:	/* set irq and ioport configuration in board registers */
                /* Warning: interferes with concurrent serial driver operations on the ports.
                 * Can cause system crash if serial driver is accessing tty ports. */
                {
                        emerald_config tmpconfig;
                        if (copy_from_user(&tmpconfig,(emerald_config *) arg,
                            sizeof(emerald_config)) != 0) ret = -EFAULT;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_write_config(brd,&tmpconfig);
                        // read the ioport values back
                        if (!ret) ret = emm_read_config(brd);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCGEEPORTCONFIG:	/* get irq/ioport configuration from eeprom */
                {
                        emerald_config eeconfig;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_read_eeconfig(brd,&eeconfig);
                        mutex_unlock(&brd->brd_mutex);
                        if (copy_to_user((emerald_config *) arg,&eeconfig,
                                sizeof(emerald_config)) != 0) ret = -EFAULT;
                }
                break;
        case EMERALD_IOCSEEPORTCONFIG:	/* set irq/ioport configuration in eeprom. User should then load it */
                {
                        emerald_config eeconfig;
                        if (copy_from_user(&eeconfig,(emerald_config *) arg,
                            sizeof(emerald_config)) != 0) ret = -EFAULT;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_write_eeconfig(brd,&eeconfig);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCEECONFIGLOAD:	/* load EEPROM config */
                /* Warning: interferes with concurrent serial driver operations on the ports.
                 * Can cause system crash is serial driver is accessing tty ports. */
                {
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_load_config_from_eeprom(brd);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCPORTENABLE:
                /* Warning: interferes with concurrent serial driver operations on the ports.
                 * Can cause system crash is serial driver is accessing tty ports. */
                {
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        emm_enable_ports(brd);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCGNBOARD:    /* how many boards are responding at the given ioport addresses */
                if (copy_to_user((int*) arg,&emerald_nr_ok,
                        sizeof(int)) != 0) ret = -EFAULT;
                break;
        case EMERALD_IOCGISABASE:   /* what is the base ISA address on this system */
                if (copy_to_user((unsigned long*) arg,&ioport_base,
                        sizeof(unsigned long)) != 0) ret = -EFAULT;
                break;
        case EMERALD_IOCGDIOOUT:    /* get digio direction for a port, 1=out, 0=in */
                {
                        int iport = port->portNum;
                        int val;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        val = emm_get_digio_port_out(brd,iport);
                        if (copy_to_user((int*) arg,&val,
                                sizeof(int)) != 0) ret = -EFAULT;
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCSDIOOUT:    /* set digio direction for a port, 1=out, 0=in */
                {
                        int iport = port->portNum;
                        int val = (int) arg;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        emm_set_digio_port_out(brd,iport,val);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCGDIO:   /* get digio value for a port */
                {
                        int iport = port->portNum;
                        int val;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        val = emm_read_digio_port(brd,iport);
                        if (copy_to_user((int*) arg,&val,
                                sizeof(int)) != 0) ret = -EFAULT;
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCSDIO:   /* set digio value for a port */
                {
                        int iport = port->portNum;
                        int val = (int) arg;

                        // digio line must be an output
                        if (! (brd->digioout & (1 << iport))) return -EINVAL;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        emm_write_digio_port(brd,iport,val);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCG_MODE:	/* get current mode for a port */
                {
                        emerald_mode tmp;
                        if (copy_from_user(&tmp,(emerald_mode *) arg,
                            sizeof(emerald_mode)) != 0) ret = -EFAULT;
                        if (tmp.port < 0 || tmp.port >= EMERALD_NR_PORTS)
                                return -EINVAL;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_get_port_mode(brd,tmp.port);
                        mutex_unlock(&brd->brd_mutex);
                        if (ret >= 0) {
                                tmp.mode = ret;
                                if (copy_to_user((emerald_mode *) arg,&tmp,
                                        sizeof(emerald_mode)) != 0) ret = -EFAULT;
                                else ret = 0;
                        }
                }
                break;
        case EMERALD_IOCS_MODE:	/* set mode for a port */
                {
                        emerald_mode tmp;
                        if (copy_from_user(&tmp,(emerald_mode *) arg,
                            sizeof(emerald_mode)) != 0) ret = -EFAULT;
                        if (tmp.port < 0 || tmp.port >= EMERALD_NR_PORTS)
                                return -EINVAL;
                        if (tmp.mode < 0 || tmp.mode > EMERALD_RS485_NOECHO)
                                return -EINVAL;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_set_port_mode(brd,tmp.port,tmp.mode);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCG_EEMODE:	/* get current mode from eeprom for a port */
                {
                        emerald_mode tmp;
                        if (copy_from_user(&tmp,(emerald_mode *) arg,
                            sizeof(emerald_mode)) != 0) ret = -EFAULT;
                        if (tmp.port < 0 || tmp.port >= EMERALD_NR_PORTS)
                                return -EINVAL;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_get_port_mode_eeprom(brd,tmp.port);
                        mutex_unlock(&brd->brd_mutex);
                        if (ret >= 0) {
                                tmp.mode = ret;
                                if (copy_to_user((emerald_mode *) arg,&tmp,
                                        sizeof(emerald_mode)) != 0) ret = -EFAULT;
                                else ret = 0;
                        }
                }
                break;
        case EMERALD_IOCS_EEMODE:	/* set mode in eeprom for a port */
                {
                        emerald_mode tmp;
                        if (copy_from_user(&tmp,(emerald_mode *) arg,
                            sizeof(emerald_mode)) != 0) ret = -EFAULT;
                        if (tmp.port < 0 || tmp.port >= EMERALD_NR_PORTS)
                                return -EINVAL;
                        if (tmp.mode < 0 || tmp.mode > EMERALD_RS485_NOECHO)
                                return -EINVAL;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_set_port_mode_eeprom(brd,tmp.port,tmp.mode);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        default:  /* redundant, as cmd was checked against MAXNR */
                return -ENOTTY;
        }
        return ret;
}

static struct file_operations emerald_fops = {
        .owner   = THIS_MODULE,
        .unlocked_ioctl   = emerald_ioctl,
        .open    = emerald_open,
        .release = emerald_release,
        .llseek  = no_llseek,
};

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
static void emerald_cleanup_module(void)
{
        int i;
                                                                                
#ifdef EMERALD_DEBUG /* use proc only if debugging */
        emerald_remove_proc();
#endif

        if (emerald_ports) {
                for (i=0; i < emerald_nports; i++) {
                        emerald_port* eport = emerald_ports + i;
                        if (MAJOR(eport->cdev.dev) != 0) cdev_del(&eport->cdev);
                }
                kfree(emerald_ports);
        }

        if (emerald_boards) {
                for (i=0; i<emerald_nr_ok; i++) {
                        if (emerald_boards[i].addr) 
                            release_region(emerald_boards[i].addr,EMERALD_IO_REGION_SIZE);
                }
                kfree(emerald_boards);
        }

        if (MAJOR(emerald_device) != 0)
            unregister_chrdev_region(emerald_device, emerald_nr_addrs);
}

static int __init emerald_init_module(void)
{
        int result, ib,ip;
        emerald_board* ebrd;

#ifndef SVNREVISION
#define SVNREVISION "unknown"
#endif
        KLOG_NOTICE("version: %s\n", SVNREVISION);

        for (ib=0; ib < EMERALD_MAX_NR_DEVS; ib++)
                if (ioports[ib] == 0) break;
        emerald_nr_addrs = ib;

        result = alloc_chrdev_region(&emerald_device,0,emerald_nr_addrs, "emerald");
        if (result < 0) goto fail;
        
        /*
         * allocate the board structures
         */
        emerald_boards = kmalloc(emerald_nr_addrs * sizeof(emerald_board), GFP_KERNEL);
        if (!emerald_boards) {
                result = -ENOMEM;
                goto fail;
        }
        memset(emerald_boards, 0, emerald_nr_addrs * sizeof(emerald_board));

        ebrd = emerald_boards;
        for (ib=0; ib < emerald_nr_addrs; ib++) {
                int boardOK = 0;
                unsigned long addr = ioports[ib] + ioport_base;

                // If a board doesn't respond we reuse this structure space,
                // so zero it again
                memset(ebrd, 0, sizeof(emerald_board));
                KLOG_DEBUG("addr=0x%lx\n",ebrd->addr);
                if (!request_region(addr,EMERALD_IO_REGION_SIZE, "emerald")) {
                        result = -EBUSY;
                        goto fail;
                }
                ebrd->addr = addr;
                mutex_init(&ebrd->brd_mutex);

                /* create device name for printk messages.
                 * The actual device file used to open the device is
                 * created outside of this module, and may not necessarily
                 * match this name.
                 */
                sprintf(ebrd->deviceName,"/dev/emerald%d",emerald_nr_ok);

                /*
                 * Read ioport and irq configuration from EEPROM and see if
                 * it looks OK.  emerald_nr_ok will be the number of boards
                 * that are configured correctly or are configurable.
                 */
                result = emm_read_eeconfig(ebrd,&ebrd->config);
                if (result == -ENODEV) {
                        release_region(addr,EMERALD_IO_REGION_SIZE);
                        ebrd->addr = 0;
                        continue;
                }

                if (result) {
                        /*
                         * We have seen situations where the EMM-8P EEPROM is not
                         * accessible, which appeared to be due to a +0.4 V
                         * over-voltage on the 5V PC104 power supply. (This doesn't effect
                         * an EMM-8). When the EEPROM accesses fail here, we try
                         * to read the register values with emm_read_config,
                         * since they should have been initialized from EEPROM at boot.
                         * However it appears that the boot initialization must have
                         * failed too, since the register values are all zeroes.
                         * In this case we initialize the register values with
                         * some defaults, and proceed.
                         */
                        KLOG_WARNING("%s: failure reading config from eeprom at ioports[%d]=0x%x. Will read from registers\n",
                                        ebrd->deviceName,ib,ioports[ib]);
                        /* disable the ports in case of a conflict between board address and uart address */
                        emm_disable_ports(ebrd);
                        result = emm_read_config(ebrd);
                }
                if (!result && emm_check_config(&ebrd->config,ebrd->deviceName)) boardOK = 1;
                else {
                        emerald_config tmpconfig;

                        emm_disable_ports(ebrd);
                        KLOG_NOTICE("invalid config on board at ioports[%d]=0x%x, ioport[0]=0x%x, irq[0]=%d\n",ib,ioports[ib],
                            ebrd->config.ports[0].ioport,ebrd->config.ports[0].irq);
                        // write a default configuration to registers and check
                        // to see if it worked.
                        for (ip = 0; ip < EMERALD_NR_PORTS; ip++) {
                                tmpconfig.ports[ip].ioport = 0x100 + (emerald_nr_ok * EMERALD_NR_PORTS + ip) * 8;
                                tmpconfig.ports[ip].irq = 3;
                        }
                        emm_write_config(ebrd,&tmpconfig);
                        result = emm_read_config(ebrd);
                        if (!result && emm_check_config(&ebrd->config,ebrd->deviceName)) {
                                KLOG_NOTICE("%s: valid config written to registers on board at ioports[%d]=0x%x\n",
                                                ebrd->deviceName,ib,ioports[ib]);
                                boardOK = 1;
                        }
                        else {
                                KLOG_ERR("%s: cannot write valid config to registers on board at ioports[%d]=0x%x\n",
                                        ebrd->deviceName,ib,ioports[ib]);
                                if (!result) result = -EINVAL;
                        }
                }
                
                if (boardOK) {
                        char* modelstrs[] = {"UNKNOWN","EMM=8","EMM-8P"};
                        ebrd->model = emm_check_model(ebrd);

                        KLOG_INFO("%s at ioport %#x is an %s\n",ebrd->deviceName,ioports[ib],modelstrs[ebrd->model]);
                        emm_printk_port_modes(ebrd);
                        emerald_nr_ok++;

                        emm_enable_ports(ebrd);
                        emm_set_digio_out(ebrd,0);
                        emm_read_digio(ebrd);

                        ebrd++;
                }
                else release_region(ebrd->addr,EMERALD_IO_REGION_SIZE);
        }
        if (emerald_nr_ok == 0 && result != 0) goto fail;

#ifdef PC104_TIMING_DEBUG
        /* code for repeated writing and reading from SPR (scratch pad register)
         * on an Exar 16654 chip in order to observe the ISA bus timing of 8 bit
         * transfers.  */
        for (;;) {
                for (ib=0; ib < emerald_nr_ok; ib++) {
                        unsigned char scratch2, scratch3;
                        /* uarts must be configured as these addresses */
                        unsigned int ports[2] = {0x100,0x140};
                        ebrd = emerald_boards + ib;

                        msleep(250);
                        outb(0,ioport_base + ports[ib] + 7);

                        msleep(250);
                        scratch2 = inb(ioport_base + ports[ib] + 7);

                        msleep(250);
                        outb(0x0F,ioport_base + ports[ib] + 7);

                        msleep(250);
                        scratch3 = inb(ioport_base + ports[ib] + 7);

                        if ((scratch2 & 0x0f) != 0 || (scratch3 & 0x0f) != 0x0F) {
                                KLOG_INFO("%s: SPR test failed (%02x, %02x)\n",
                                              ebrd->deviceName, scratch2, scratch3);
                        }
                        else
                                KLOG_INFO("%s: SPR test OK (%02x, %02x)\n",
                                              ebrd->deviceName, scratch2, scratch3);
                }
        }
#endif

        emerald_nports = emerald_nr_ok * EMERALD_NR_PORTS;
        emerald_ports = kmalloc(emerald_nports * sizeof(emerald_port), GFP_KERNEL);
        if (!emerald_ports) {
                result = -ENOMEM;
                goto fail;
        }
        memset(emerald_ports, 0,emerald_nports * sizeof(emerald_port));

        for (ip=0; ip < emerald_nports; ip++) {
                dev_t devno;
                emerald_port* eport = emerald_ports + ip;
                emerald_board* ebrd = emerald_boards + (ip / EMERALD_NR_PORTS);
                eport->board = ebrd;
                eport->portNum = ip % EMERALD_NR_PORTS;	// 0-7
                cdev_init(&eport->cdev,&emerald_fops);
                eport->cdev.owner = THIS_MODULE;
                devno = MKDEV(MAJOR(emerald_device),ip);
                /* After calling cdev_add the device is "live" and ready for user operation. */
                result = cdev_add(&eport->cdev, devno, 1);
                if (result) goto fail;
        }

#ifdef EMERALD_DEBUG /* only when debugging */
        KLOG_DEBUG("create_proc\n");
        emerald_create_proc();
#endif
        return 0; /* succeed */

fail:
        emerald_cleanup_module();
        return result;
}

module_init(emerald_init_module);
module_exit(emerald_cleanup_module);
