/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

    Linux driver module for Diamond Emerald serial IO cards.
    This just queries and sets the ioport addresses and irqs of the UARTs.
    The normal linux serial driver for the 8250 family of UARTs does
    the heavy work.

    This driver can handle up to EMERALD_MAX_NR_DEVS boards, 
    which is currently set to 4.

    The following devices will be created for each board that
    responds in an expected manner at the ioport addresses that
    are passed as module parameters.

    /dev/emerald0, minor 0
    /dev/emerald1, minor 1
    /dev/emerald2, minor 2
    /dev/emerald3, minor 3

    Each board has EMERALD_NR_PORTS=8 serial ports.

    The ioports and irqs of the serial ports on a board
    can be set/get by ioctls to the /dev/emeraldN devices.

    Typically an init script sets/gets the ioport and irq values
    of the serial ports on an Emerald, and then creates the usual
    /dev/ttySn serial ports and configures them with the setserial
    system command.
    
    Each Emerald serial port has a digital I/O pin, which can
    be controlled through this driver via a /dev/ttyDn device
    for each port.

    To make things easier for the user, the device names
    of the Emerald digital ports are the same as the tty serial port
    names, with a 'D' instead of an 'S':
        serial port: /dev/ttyS5 
        digio pin:  /dev/ttyD5 

    Since there are likely to be other serial ports
    on the system, the device name, /dev/ttySN, of the first
    serial port on the first Emerald board, will likely have N > 0.
    This N is passed as a driver parameter, tty_port_offset, and
    defaults to 5, since our usual systems have 5 serial ports
    on the motherboard, so the Emerald ports begin at /dev/ttyS5.

    This module can also set the RS232/422/485 mode for each serial
    port on an EMM-8P via the /dev/ttyDn device.

    The ttyDn devices will have minor numbers starting at
    EMERALD_MAX_NR_DEVS (4), up to
    (EMERALD_MAX_NR_DEVS * EMERALD_NR_PORTS)+4-1 = 35.

    With tty_port_offset = 5:

    /dev/ttyD5, minor 4, port 0, board 0
            ...
    /dev/ttyD12, minor 11, port 7, board 0

    /dev/ttyD13, minor 12, port 0, board 1
            ...
    /dev/ttyD20, minor 19, port 7, board 1
            ...
    /dev/ttyD36, minor 35, port 7, board 3
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
#include <linux/io.h>		/* outb, inb */
#include <asm/uaccess.h>	/* access_ok */

/* for testing UART registers */
#include <linux/serial_reg.h>

#include "emerald.h"	/* local definitions */

#include <nidas/linux/ver_macros.h>
#include <nidas/linux/isa_bus.h>
#include <nidas/linux/klog.h>
#include <nidas/linux/Revision.h>    // REPO_REVISION

static dev_t emerald_device = MKDEV(0,0);

static unsigned long ioport_base = (unsigned long) SYSTEM_ISA_IOPORT_BASE;

static unsigned int ioports[EMERALD_MAX_NR_DEVS] = {0,0,0,0};
static int emerald_nr_addrs = 0;
static int emerald_nr_ok = 0;
static emerald_board* emerald_boards = 0;
static emerald_port* emerald_ports = 0;
static int emerald_nports = 0;

/*
 * In order to number the emerald port devices /dev/ttyDn, to correspond
 * to the associated serial port device, /dev/ttySn, we need to know
 * serial port number /dev/ttySn assigned to the first emerald serial port. 
 * Once the user has fetched the ioport and irq configuration of
 * ane
 */
static int tty_port_offset = 5;

static struct class* emerald_class;

#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(ioports, uint, &emerald_nr_addrs, S_IRUGO);	/* io port address */
#else
module_param_array(ioports, uint, emerald_nr_addrs, S_IRUGO);	/* io port address */
#endif

module_param(tty_port_offset, int, S_IRUGO);

#ifndef REPO_REVISION
#define REPO_REVISION "unknown"
#endif

MODULE_AUTHOR("Gordon Maclean");
MODULE_DESCRIPTION("driver module supporting initialization and digital I/O on Diamond System Emerald serial port card");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(REPO_REVISION);

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

static void emm_read_config(emerald_board* brd)
{
        int i,val;
        /* read ioport values. */
        for (i = 0; i < EMERALD_NR_PORTS; i++) {
                outb(i,brd->addr+EMERALD_APER);
                val = inb(brd->addr+EMERALD_ARR);
                brd->config.ports[i].ioport = val << 3;

                // According to the Emerald manual the IRQ registers
                // cannot be read back. It does work just after
                // a emerald_write_config, but not after a bootup.
#ifdef TRY_READ_IRQ_FROM_REGISTER
                outb(i+EMERALD_NR_PORTS,brd->addr+EMERALD_APER);
                val = inb(brd->addr+EMERALD_ARR);
                brd->config.ports[i].irq = val;
#endif
        }
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
                } while(busy & 0x80 && --ntry);
                // if (busy == 0xff) return -ENODEV;
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
        emm_read_config(brd);
        if (!emm_check_config(&brd->config,brd->deviceName)) result = -EINVAL;
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
        } while(busy & 0x80 && --ntry);
        // if (busy == 0xff) return -ENODEV;
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
        } while(busy & 0x80 && --ntry);
        // if (busy == 0xff) return -ENODEV;
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

static int emerald_open(struct inode *inode, struct file *filp)
{
        int num = MINOR(inode->i_rdev);
        emerald_port *port; /* device information */

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        /* Assuming EMERALD_MAX_NR_DEVS is 4, and EMERALD_NR_PORTS is 8
         *  minor number
         *  0: is board 0, and port 0 on that board
         *  1: is board 1, and port 0 on that board
         *  2: is board 2, and port 0 on that board
         *  3: is board 3, and port 0 on that board
         *  4-11: is ports 0-7 on board 0
         *  12-19: is ports 0-7 on board 1
         *  20-27: is ports 0-7 on board 2
         *  28-35: is ports 0-7 on board 3
         *
         *  If 4 boards are reporting, then there will be
         *  4*8=32 emerald_port structures. Convert
         *  the minor number to a pointer to the emerald_port.
         */      

        if (num < EMERALD_MAX_NR_DEVS) num *= EMERALD_NR_PORTS;
        else num -= EMERALD_MAX_NR_DEVS; 

        if (num >= emerald_nports) return -ENODEV;
        port = emerald_ports + num;

        /* use filp->private_data to point to the device data */
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

static long emerald_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        emerald_port* port = filp->private_data;
        emerald_board* brd = port->board;
        int portNum = port->portNum;
        int err= 0, ret = 0, val;

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
        case EMERALD_IOCGPORTCONFIG:
                /* get current irq and ioport configuration for all
                 * serial ports */
                if (copy_to_user((emerald_config *) arg,&brd->config,
                        sizeof(emerald_config)) != 0) ret = -EFAULT;
                break;
        case EMERALD_IOCSPORTCONFIG:
                /* set irq and ioport configuration in board registers */
                /* Warning: interferes with concurrent serial driver
                 * operations on the ports.  Can cause system crash if
                 * serial driver is accessing tty ports. */
                {
                        emerald_config tmpconfig;
                        if (copy_from_user(&tmpconfig,(emerald_config *) arg,
                            sizeof(emerald_config)) != 0) ret = -EFAULT;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_write_config(brd,&tmpconfig);
                        // read the ioport values back
                        emm_read_config(brd);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCGEEPORTCONFIG:
                /* get irq/ioport configuration from eeprom */
                {
                        emerald_config eeconfig;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_read_eeconfig(brd,&eeconfig);
                        mutex_unlock(&brd->brd_mutex);
                        if (copy_to_user((emerald_config *) arg,&eeconfig,
                                sizeof(emerald_config)) != 0) ret = -EFAULT;
                }
                break;
        case EMERALD_IOCSEEPORTCONFIG:
                /* set irq/ioport configuration in eeprom. User should then load it */
                {
                        emerald_config eeconfig;
                        if (copy_from_user(&eeconfig,(emerald_config *) arg,
                            sizeof(emerald_config)) != 0) ret = -EFAULT;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_write_eeconfig(brd,&eeconfig);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCEECONFIGLOAD:
                /* load EEPROM config */
                /* Warning: interferes with concurrent serial driver
                 * operations on the ports.  Can cause system crash
                 * if serial driver is accessing tty ports. */
                if ((ret = mutex_lock_interruptible(&brd->brd_mutex)))
                        return ret;
                ret = emm_load_config_from_eeprom(brd);
                mutex_unlock(&brd->brd_mutex);
                break;
        case EMERALD_IOCPORTENABLE:
                /* Warning: interferes with concurrent serial driver
                 * operations on the ports.  Can cause system crash if
                 * serial driver is accessing tty ports. */
                if ((ret = mutex_lock_interruptible(&brd->brd_mutex)))
                        return ret;
                emm_enable_ports(brd);
                mutex_unlock(&brd->brd_mutex);
                break;
        case EMERALD_IOCGNBOARD:
                /* how many boards are responding at the given
                 * ioport addresses */
                if (copy_to_user((int*) arg,&emerald_nr_ok,
                        sizeof(int)) != 0) ret = -EFAULT;
                break;
        case EMERALD_IOCGISABASE:
                /* what is the base ISA address on this system */
                if (copy_to_user((unsigned long*) arg,&ioport_base,
                        sizeof(unsigned long)) != 0) ret = -EFAULT;
                break;
        case EMERALD_IOCGDIOOUT:
                /* get digio direction for a port, 1=out, 0=in */
                if ((ret = mutex_lock_interruptible(&brd->brd_mutex)))
                        return ret;
                val = emm_get_digio_port_out(brd, portNum);
                if (copy_to_user((int*) arg,&val,
                        sizeof(int)) != 0) ret = -EFAULT;
                mutex_unlock(&brd->brd_mutex);
                break;
        case EMERALD_IOCSDIOOUT:
                /* set digio direction for a port, 1=out, 0=in */
                val = (int) arg;
                if ((ret = mutex_lock_interruptible(&brd->brd_mutex)))
                        return ret;
                emm_set_digio_port_out(brd, portNum, val);
                mutex_unlock(&brd->brd_mutex);
                break;
        case EMERALD_IOCGDIO:
                /* get digio value for a port */
                if ((ret = mutex_lock_interruptible(&brd->brd_mutex)))
                        return ret;
                val = emm_read_digio_port(brd, portNum);
                if (copy_to_user((int*) arg,&val,
                        sizeof(int)) != 0) ret = -EFAULT;
                mutex_unlock(&brd->brd_mutex);
                break;
        case EMERALD_IOCSDIO:
                /* set digio value for a port */
                val = (int) arg;
                // digio line must be an output
                if (! (brd->digioout & (1 << portNum))) return -EINVAL;
                if ((ret = mutex_lock_interruptible(&brd->brd_mutex)))
                        return ret;
                emm_write_digio_port(brd, portNum, val);
                mutex_unlock(&brd->brd_mutex);
                break;
        case EMERALD_IOCG_MODE:
                /* get current mode for a port */
                {
                        emerald_mode tmp;
                        if (copy_from_user(&tmp,(emerald_mode *) arg,
                            sizeof(emerald_mode)) != 0) ret = -EFAULT;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_get_port_mode(brd, portNum);
                        mutex_unlock(&brd->brd_mutex);
                        if (ret >= 0) {
                                tmp.mode = ret;
                                if (copy_to_user((emerald_mode *) arg,&tmp,
                                        sizeof(emerald_mode)) != 0) ret = -EFAULT;
                                else ret = 0;
                        }
                }
                break;
        case EMERALD_IOCS_MODE:
                /* set mode for a port */
                {
                        emerald_mode tmp;
                        if (copy_from_user(&tmp,(emerald_mode *) arg,
                            sizeof(emerald_mode)) != 0) ret = -EFAULT;
                        if (tmp.mode < 0 || tmp.mode > EMERALD_RS485_NOECHO)
                                return -EINVAL;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_set_port_mode(brd, portNum, tmp.mode);
                        mutex_unlock(&brd->brd_mutex);
                }
                break;
        case EMERALD_IOCG_EEMODE:
                /* get current mode from eeprom for a port */
                {
                        emerald_mode tmp;
                        if (copy_from_user(&tmp,(emerald_mode *) arg,
                            sizeof(emerald_mode)) != 0) ret = -EFAULT;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_get_port_mode_eeprom(brd, portNum);
                        mutex_unlock(&brd->brd_mutex);
                        if (ret >= 0) {
                                tmp.mode = ret;
                                if (copy_to_user((emerald_mode *) arg,&tmp,
                                        sizeof(emerald_mode)) != 0) ret = -EFAULT;
                                else ret = 0;
                        }
                }
                break;
        case EMERALD_IOCS_EEMODE:
                /* set mode in eeprom for a port */
                {
                        emerald_mode tmp;
                        if (copy_from_user(&tmp,(emerald_mode *) arg,
                            sizeof(emerald_mode)) != 0) ret = -EFAULT;
                        if (tmp.mode < 0 || tmp.mode > EMERALD_RS485_NOECHO)
                                return -EINVAL;
                        if ((ret = mutex_lock_interruptible(&brd->brd_mutex))) return ret;
                        ret = emm_set_port_mode_eeprom(brd, portNum, tmp.mode);
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
 * Thefore, it must be work correctly even if some of the items
 * have not been initialized
 *
 * Don't add __exit macro to the declaration of this cleanup function
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
                        if (MAJOR(eport->cdev.dev) != 0) {
                                if (eport->device && !IS_ERR(eport->device))
                                        device_destroy(emerald_class, eport->cdev.dev);
                                cdev_del(&eport->cdev);
                        }
                }
                kfree(emerald_ports);
                emerald_ports = 0;
        }

        if (emerald_boards) {
                for (i=0; i<emerald_nr_ok; i++) {
                        emerald_board* ebrd = emerald_boards + i;
                        if (MAJOR(ebrd->cdev.dev) != 0) {
                                if (ebrd->device && !IS_ERR(ebrd->device))
                                        device_destroy(emerald_class, ebrd->cdev.dev);
                                cdev_del(&ebrd->cdev);
                        }
                        if (ebrd->ioport) 
                            release_region(ebrd->ioport,EMERALD_IO_REGION_SIZE);
                }
                kfree(emerald_boards);
                emerald_boards = 0;
        }

        if (emerald_class && !IS_ERR(emerald_class))
                class_destroy(emerald_class);
        emerald_class = 0;

        if (MAJOR(emerald_device) != 0)
                unregister_chrdev_region(emerald_device, emerald_nr_addrs);
}

static int __init emerald_init_module(void)
{
        int ib,ip;
        emerald_board* ebrd;
        int result = -EINVAL;

        KLOG_NOTICE("version: %s\n", REPO_REVISION);

        for (ib=0; ib < EMERALD_MAX_NR_DEVS; ib++)
                if (ioports[ib] == 0) break;
        if (ib == 0) goto fail;
        emerald_nr_addrs = ib;

        result = alloc_chrdev_region(&emerald_device, 0,
                emerald_nr_addrs, "emerald");
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

        emerald_class = class_create(THIS_MODULE, "emerald");
        if (IS_ERR(emerald_class)) {
                result = PTR_ERR(emerald_class);
                goto fail;
        }

        ebrd = emerald_boards;
        for (ib=0; ib < emerald_nr_addrs; ib++) {
                int boardOK = 0;
                unsigned char regval;

                // If a board doesn't respond we reuse this structure space,
                // so zero it again
                memset(ebrd, 0, sizeof(emerald_board));

                /* device name for printk messages  */
                sprintf(ebrd->deviceName,"/dev/emerald%d",emerald_nr_ok);

                if (!request_region(ioports[ib],EMERALD_IO_REGION_SIZE, "emerald")) {
                        KLOG_ERR("%s: request_region(%#x,%d,\"emerald\") failed\n",
                                ebrd->deviceName, ioports[ib], EMERALD_IO_REGION_SIZE);
                        result = -EBUSY;
                        goto fail;
                }
                ebrd->ioport = ioports[ib];
                ebrd->addr = ebrd->ioport + ioport_base;
                mutex_init(&ebrd->brd_mutex);

                /*
                 * Simple check if board is responding at this address.
                 * Note that this disables the ports, setting bit 7 in APER to 0.
                 * This may help in the (unlikely) situation that a port is
                 * wrongly configured with the same ioport address as the board.
                 * It is probably a good idea to set the jumper on the board so the
                 * ports are not enabled on power up.
                 */
                regval = 0x05;
                outb(regval,ebrd->addr+EMERALD_APER);
                regval = inb(ebrd->addr+EMERALD_APER) & 0x8f;
                if (regval != 0x05) {
                        KLOG_WARNING("%s: Emerald not responding at ioports[%d]=%#x, val=%#x\n",
                                ebrd->deviceName,ib,ioports[ib],
                                (unsigned int) regval);
                        result = -ENODEV;
                        release_region(ebrd->ioport,EMERALD_IO_REGION_SIZE);
                        ebrd->ioport = 0;
                        ebrd->addr = 0;
                        continue;
                }

                emm_read_config(ebrd);
                if (emm_check_config(&ebrd->config,ebrd->deviceName)) {
                        boardOK = 1;
                }
                else {
                        emerald_config tmpconfig;
                        KLOG_NOTICE("invalid ioport/irq config on emerald at ioports[%d]=0x%x, ioport[0]=0x%x, irq[0]=%d\n",ib,ioports[ib],
                            ebrd->config.ports[0].ioport,ebrd->config.ports[0].irq);

                        // write a default configuration to registers and check
                        // to see if it worked.
                        for (ip = 0; ip < EMERALD_NR_PORTS; ip++) {
                                tmpconfig.ports[ip].ioport = 0x100 + (emerald_nr_ok * EMERALD_NR_PORTS + ip) * 8;
                                tmpconfig.ports[ip].irq = 3;
                        }
                        emm_write_config(ebrd,&tmpconfig);
                        emm_read_config(ebrd);
                        if (emm_check_config(&ebrd->config,ebrd->deviceName)) {
                                KLOG_NOTICE("%s: valid ioport/irq config written to emerald at ioports[%d]=0x%x\n",
                                                ebrd->deviceName,ib,ioports[ib]);
                                // update EEPROM. Issue warning on failure, but don't make it fatal
                                if (emm_write_eeconfig(ebrd,&tmpconfig))
                                        KLOG_WARNING("%s: cannot write ioport/irq config to emerald EEPROM at ioports[%d]=0x%x\n",
                                                ebrd->deviceName,ib,ioports[ib]);
                                boardOK = 1;
                        }
                        else {
                                KLOG_ERR("%s: cannot write valid ioport/irq config to board at ioports[%d]=0x%x\n",
                                        ebrd->deviceName,ib,ioports[ib]);
                                result = -ENODEV;
                        }
                }
                
                if (boardOK) {
                        char* modelstrs[] = {"UNKNOWN","EMM=8","EMM-8P"};
                        dev_t devno;
                        ebrd->model = emm_check_model(ebrd);

                        KLOG_INFO("%s at ioport %#x is an %s\n",ebrd->deviceName,ioports[ib],modelstrs[ebrd->model]);
                        emm_printk_port_modes(ebrd);

                        emm_enable_ports(ebrd);
                        emm_set_digio_out(ebrd,0);
                        emm_read_digio(ebrd);

                        cdev_init(&ebrd->cdev, &emerald_fops);
                        ebrd->cdev.owner = THIS_MODULE;

                        devno = MKDEV(MAJOR(emerald_device),emerald_nr_ok);

                        /* After calling cdev_add the device is "live"
                         * and ready for user operation. */
                        result = cdev_add(&ebrd->cdev, devno, 1);
                        if (result) goto fail;

                        ebrd->device = device_create_x(emerald_class, NULL,
                                devno, "emerald%d", emerald_nr_ok);
                        if (IS_ERR(ebrd->device)) {
                                result = PTR_ERR(ebrd->device);
                                goto fail;
                        }

                        emerald_nr_ok++;
                        ebrd++;
                }
                else release_region(ebrd->ioport,EMERALD_IO_REGION_SIZE);
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

                cdev_init(&eport->cdev, &emerald_fops);
                eport->cdev.owner = THIS_MODULE;
                devno = MKDEV(MAJOR(emerald_device),ip + EMERALD_MAX_NR_DEVS);

                /* After calling cdev_add the device is "live"
                 * and ready for user operation. */
                result = cdev_add(&eport->cdev, devno, 1);
                if (result) goto fail;

                eport->device = device_create_x(emerald_class, NULL,
                        devno, "ttyD%d", ip + tty_port_offset);
                if (IS_ERR(eport->device)) {
                        result = PTR_ERR(eport->device);
                        goto fail;
                }
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
