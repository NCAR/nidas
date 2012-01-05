/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*-
 * vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    Driver module for Diamond Systems Emerald 8 port serial card.

    This driver can get/set the irq and ioport values for each of the
    8 serial ports on a card. Once this driver sets the irq and ioport
    values for each port in the card registers, and enables it, then the
    serial port can be configured with the Linux setserial command and
    accessed as a normal serial port by the Linux tty serial port driver.

    An Emerald also has 8 digital I/O pins. This driver supports the
    get/set of those 8 pins via ioctl calls.

    This module can control up to EMERALD_MAX_NR_DEVS boards, which
    is typically defined in this header to be 4. A unique ioport address
    of each board must be chosen and jumpered on the board. These ioport
    address values must then be passed to this driver module as the "ioports"
    parameter when it is loaded.
    A typical setting in /etc/modprobe.d/diamond is as follows:
            options emerald ioports=0x200,0x240,0x2c0,0x300
    This driver will check if a board responds at each address, so the
    above array of ioports will work if there are less than 4 boards
    installed in the system, assuming there is not a conflict with another
    PC104 board at those addresses.  Each Emerald uses 7 bytes of ioport
    address space.

    The usual device configuration is as follows, where /dev/emerald[0-3]
    are the device files that can be used to address each emerald board
    in the system.  The major number is allocated dynamically when the
    driver module is loaded, and can then be queried with
    "grep emerald /proc/devices".

    name                device minor number
    /dev/emerald0       0
    /dev/emerald1       8
    /dev/emerald2       16
    /dev/emerald3       24

    The digital I/O devices can be named in a similar way to the serial ports
    on the board.  In this example, serial ports ttyS[5-12] are the 8 serial ports
    on the first board, and so the digital I/O devices are named starting at ttyD5:

    /dev/ttyD5          0
    ...
    /dev/ttyD12         7

    Serial ports on second board:
    /dev/ttyD13         8
    ...
    /dev/ttyD20        15

    Third board, etc:
    /dev/ttyD21        16
    ...
    /dev/ttyD28        23

    Notice that /dev/emerald0 and /dev/ttyD5 have the same device numbers, and so
    are synomyms for the same device. Likewise for /dev/emerald1 and /dev/ttyD13.

    The Linux tty driver expects 8 bytes of ioport address space for each serial port.
    An IRQ must also be allocated for each port.  A typical configuration of the
    serial ports is as follows:
    port        device          ioport IRQ
    0           /dev/ttyS5      0x100   3
    1           /dev/ttyS6      0x108   3
    2           /dev/ttyS7      0x110   3
    3           /dev/ttyS8      0x118   3
    4           /dev/ttyS9      0x120   3
    5           /dev/ttyS10     0x128   3
    6           /dev/ttyS11     0x130   3
    7           /dev/ttyS12     0x138   3
    8           /dev/ttyS13     0x140   3
    ...

    Note that these ioport values are for the individual serial ports on the board,
    and are distinct from the ioport values that are used to address the board
    itself.

    The above iport and IRQ values can be set on the Emerald card for each port
    via the EMERALD_IOCSPORTCONFIG ioctl. The set_emerald application can be
    used to invoke the necessary ioctls on this driver.

    To set the ioport and IRQ values for each board and enable the ports:
    set_emerald /dev/emerald0 0x100 3
    set_emerald /dev/emerald1 0x140 3
    ...

    To set the ioport and IRQ values in EEPROM via the EMERALD_IOCSEEPORTCONFIG
    ioctl, and enable the ports:
    set_emerald /dev/emerald0 0x100 3
    set_emerald /dev/emerald1 0x140 3

    To query the values:
    set_emerald /dev/emerald0

    To enable (up) the ports, assuming the existing values are OK
    set_emerald -u /dev/emerald0
    
    After configuring the emerald card as above, the tty driver can
    be configured to access those ports, using the Linux setserial command.
    In this example, minor number 64-68 were used for serial ports /dev/ttyS0-4
    on the CPU board, and so minor numbers 69 and above are for the Emerald ports.

    mknod /dev/ttyS5 c 4 69
    mknod /dev/ttyS6 c 4 70
    ...

    setserial -zvb /dev/ttyS5 port 0x100 irq 3 baud_base 460800 autoconfig
    setserial -zvb /dev/ttyS6 port 0x108 irq 3 baud_base 460800 autoconfig
    ...

    At this point the serial ports should be accessible.

    This module can also set the RS232/422/485 mode for each serial port on an EMM-8P.
    
 ********************************************************************

*/
#ifndef NIDAS_LINUX_EMERALD_H
#define NIDAS_LINUX_EMERALD_H

// #define EMERALD_DEBUG

#undef PDEBUG             /* undef it, just in case */
#ifdef EMERALD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "emerald: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif
#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#define EMERALD_NR_PORTS 8	/* number of serial ports on an emerald-mm-8 */

#ifdef __KERNEL__

#include <linux/cdev.h>
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#define EMERALD_MAX_NR_DEVS 4	/* maximum number of emerald cards in sys */
#define EMERALD_IO_REGION_SIZE 7 /* number of 1-byte registers per card */

/* registers on the emerald starting at the ioport address */
#define EMERALD_APER 0x0	/* address ptr/enable reg (read,write)*/
#define EMERALD_AIDR 0x1	/* address/IRQ data reg (write) */
#define EMERALD_ARR 0x1		/* address register readback (read) */
#define EMERALD_DDR 0x2		/* digital I/O direction reg (write) */
#define EMERALD_ISR 0x2		/* interrupt status reg (read) */
#define EMERALD_DOR 0x3		/* digital output reg (write) */
#define EMERALD_DIR 0x3		/* digital input reg (read) */
#define EMERALD_ECAR 0x4	/* EEPROM cmd and addr reg (write) */
#define EMERALD_EBR 0x4		/* EEPROM busy reg (read) */
#define EMERALD_EDR 0x5		/* EEPROM data reg (read/write) */
#define EMERALD_CRR 0x6		/* Configuration register reload (write) */
                                                                                
#endif /* __KERNEL__ */

typedef struct emerald_serial_port {
        unsigned int ioport;	/* ISA ioport of a serial port, e.g. 0x100 */
        unsigned int irq;	/* ISA IRQ of serial port */
} emerald_serial_port;

typedef struct emerald_config {
        emerald_serial_port ports[EMERALD_NR_PORTS];
} emerald_config;

/*
 * Enumeration of software-setable serial modes for Emm-8P card.
 */
enum EMERALD_MODE {
        EMERALD_RS232,
        EMERALD_RS422,
        EMERALD_RS485_ECHO,
        EMERALD_RS485_NOECHO
};

/* 
 * Module attempts to determine which model of card.
 */
enum EMERALD_MODEL {
        EMERALD_UNKNOWN,
        EMERALD_MM_8,
        EMERALD_MM_8P,
};

typedef struct emerald_mode {
        int port;	                /* serial port, 0-7 */
        enum EMERALD_MODE mode;         /* desired mode */
} emerald_mode;

#ifdef __KERNEL__

typedef struct emerald_board {
        unsigned long addr;	/* virtual ioport addr of the emerald card */
        emerald_config config;	/* ioport and irq of 8 serial ports */
        enum EMERALD_MODEL model;       /* model of card */
        char deviceName[32];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        struct mutex brd_mutex;         // exclusion lock for accessing board registers
#else
        struct semaphore brd_mutex;     // exclusion lock for accessing board registers
#endif
        int digioval;		/* current digital I/O value */
        int digioout;		/* bit=1, dig I/O direction = out */
} emerald_board;

typedef struct emerald_port {
        emerald_board* board;
        struct cdev cdev;
        int portNum;            /* serial port number on  this board, 0-7 */
} emerald_port;


#  endif /* __KERNEL__ */

/* Look in Documentation/ioctl-number.txt */
#define EMERALD_IOC_MAGIC  0xd0

/* set port config in RAM */
#define EMERALD_IOCSPORTCONFIG _IOW(EMERALD_IOC_MAGIC,  0, emerald_config)

/* get port config from RAM */
#define EMERALD_IOCGPORTCONFIG _IOR(EMERALD_IOC_MAGIC,  1, emerald_config)

/* set config in EEPROM */
#define EMERALD_IOCSEEPORTCONFIG _IOW(EMERALD_IOC_MAGIC,  2, emerald_config)

/* get config from EEPROM */
#define EMERALD_IOCGEEPORTCONFIG _IOR(EMERALD_IOC_MAGIC,  3, emerald_config)

/* load config from EEPROM to RAM */
#define EMERALD_IOCEECONFIGLOAD _IO(EMERALD_IOC_MAGIC,  4)

/* enable the UARTs */
#define EMERALD_IOCPORTENABLE _IO(EMERALD_IOC_MAGIC,  5)

/* how many boards are responding at the given ioport addresses */
#define EMERALD_IOCGNBOARD _IOR(EMERALD_IOC_MAGIC,  6, int)

/* what is the base ISA address on this system */
#define EMERALD_IOCGISABASE _IOR(EMERALD_IOC_MAGIC,7,unsigned long)

/* Get direction of digital I/O line for a port, 1=out, 0=in */
#define EMERALD_IOCGDIOOUT _IOR(EMERALD_IOC_MAGIC,8,int)

/* Set direction of digital I/O line for a port, 1=out, 0=in */
#define EMERALD_IOCSDIOOUT _IOW(EMERALD_IOC_MAGIC,9,int)

/* Get value of digital I/O line for a port */
#define EMERALD_IOCGDIO _IOR(EMERALD_IOC_MAGIC,10,int)

/* Set value of digital I/O line for a port */
#define EMERALD_IOCSDIO _IOW(EMERALD_IOC_MAGIC,11,int)

/* Get value of mode for a port */
#define EMERALD_IOCG_MODE _IOWR(EMERALD_IOC_MAGIC,12,emerald_mode)

/* Set value of mode for a port */
#define EMERALD_IOCS_MODE _IOW(EMERALD_IOC_MAGIC,13,emerald_mode)

/* Get value of mode for a port from eeprom */
#define EMERALD_IOCG_EEMODE _IOWR(EMERALD_IOC_MAGIC,14,emerald_mode)

/* Set value of mode for a port into eeprom */
#define EMERALD_IOCS_EEMODE _IOW(EMERALD_IOC_MAGIC,15,emerald_mode)

#define EMERALD_IOC_MAXNR 15

#endif	/* _EMERALD_H */

