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
#ifndef NIDAS_LINUX_PCMCOM8_H
#define NIDAS_LINUX_PCMCOM8_H


#define PCMCOM8_NR_PORTS 8	/* number of serial ports on a board */

#  ifdef __KERNEL__

#include <linux/version.h>
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#include <linux/cdev.h>

#ifndef PCMCOM8_MAJOR
/* look in Documentation/devices.txt 
 * 60-63,120-127,240-254 LOCAL/EXPERIMENTAL USE
 * 231-239 unassigned
 * 208,209: user space serial ports - what are these?
 */
#define PCMCOM8_MAJOR 0   /* dynamic major by default */
#endif

#define PCMCOM8_MAX_NR_DEVS 4	/* maximum number of pcmcom8 cards in sys */
#define PCMCOM8_IO_REGION_SIZE 8 /* number of 1-byte registers */

/* registers on the pcmcom8 starting at the ioport address */
#define PCMCOM8_IDX 0x0	        /* index register, R/W */
#define PCMCOM8_ADR 0x1 	/* address and enable reg, R/W */
#define PCMCOM8_IAR 0x2		/* IRQ assigmment register, R/W */
#define PCMCOM8_IIR 0x3		/* IRQ id reg, RO */
#define PCMCOM8_ECR 0x4	        /* EEPROM cmd, R/W  */
#define PCMCOM8_EHR 0x5		/* EEPROM high data register, R/W */
#define PCMCOM8_ELR 0x6		/* EEPROM low data register, R/W */
#define PCMCOM8_CMD 0x7		/* command register W */
#define PCMCOM8_STA 0x7		/* status register RO */
                                                                                
#  endif /* __KERNEL__ */

struct pcmcom8_serial_port {
        unsigned int ioport;	/* ISA ioport address, e.g. 0x100 */
        unsigned int irq;		/* ISA IRQ */
        unsigned int enable;    /* whether to enable uart */
} pcmcom8_serial_port;

struct pcmcom8_config {
        struct pcmcom8_serial_port ports[PCMCOM8_NR_PORTS];
} pcmcom8_config;

#  ifdef __KERNEL__

typedef struct pcmcom8_board {
        unsigned long ioport;	/* virtual ioport addr of the pcmcom8 card */
        struct pcmcom8_config config;	/* ioport and irq of 8 serial ports */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        struct mutex mutex;
#else
        struct semaphore mutex;
#endif
        struct cdev cdev;
        int region_req;     /* ioport region requested */
        int cdev_ready;          /* cdev_add done */
} pcmcom8_board;


#  endif /* __KERNEL__ */

/* Look in Documentation/ioctl-number.txt */
#define PCMCOM8_IOC_MAGIC  'p'
#define PCMCOM8_IOCGIOPORT _IOR(PCMCOM8_IOC_MAGIC,  1, unsigned long)
#define PCMCOM8_IOCSPORTCONFIG _IOW(PCMCOM8_IOC_MAGIC,  2, struct pcmcom8_config)
#define PCMCOM8_IOCGPORTCONFIG _IOR(PCMCOM8_IOC_MAGIC,  3, struct pcmcom8_config)
#define PCMCOM8_IOCSEEPORTCONFIG _IOW(PCMCOM8_IOC_MAGIC,  4, struct pcmcom8_config)
#define PCMCOM8_IOCGEEPORTCONFIG _IOR(PCMCOM8_IOC_MAGIC,  5, struct pcmcom8_config)
#define PCMCOM8_IOCEECONFIGLOAD _IO(PCMCOM8_IOC_MAGIC,  6)
#define PCMCOM8_IOCPORTENABLE _IO(PCMCOM8_IOC_MAGIC,  7)
#define PCMCOM8_IOCGNBOARD _IOR(PCMCOM8_IOC_MAGIC,  8, int)
#define PCMCOM8_IOCGISABASE _IOR(PCMCOM8_IOC_MAGIC,9,unsigned long)

#define PCMCOM8_IOC_MAXNR 9

#endif	/* _PCMCOM8_H */

