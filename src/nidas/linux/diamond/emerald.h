/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/
#ifndef NIDAS_LINUX_EMERALD_H
#define NIDAS_LINUX_EMERALD_H

#define EMERALD_DEBUG

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

#  ifdef __KERNEL__

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#ifndef EMERALD_MAJOR
/* look in Documentation/devices.txt 
 * 60-63,120-127,240-254 LOCAL/EXPERIMENTAL USE
 * 231-239 unassigned
 * 208,209: user space serial ports - what are these?
 */
#define EMERALD_MAJOR 0   /* dynamic major by default */
#endif

#define EMERALD_MAX_NR_DEVS 4	/* maximum number of emerald cards in sys */
#define EMERALD_IO_REGION_SIZE 7 /* number of 1-byte registers */

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
                                                                                
#  endif /* __KERNEL__ */

typedef struct emerald_serial_port {
    unsigned int ioport;	/* ISA ioport address, e.g. 0x100 */
    unsigned int irq;		/* ISA IRQ */
} emerald_serial_port;

typedef struct emerald_config {
    emerald_serial_port ports[EMERALD_NR_PORTS];
} emerald_config;

#  ifdef __KERNEL__

typedef struct emerald_board {
    unsigned long ioport;	/* virtual ioport addr of the emerald card */
    emerald_config config;	/* ioport and irq of 8 serial ports */
    struct semaphore sem;	/* mutual exclusion semaphore */
    struct resource* region;
    int digioval;		/* current digital I/O value */
    int digioout;		/* bit=1, dig I/O direction = out */
} emerald_board;

typedef struct emerald_port {
    emerald_board* board;
    int portNum;
} emerald_port;


#ifdef NEEDED
// extern int emerald_major;	/* main.c */
extern int emerald_nr_devices;	/* main.c */

ssize_t emerald_read (struct file *filp, char *buf, size_t count,
                    loff_t *f_pos);
ssize_t emerald_write (struct file *filp, const char *buf, size_t count,
                     loff_t *f_pos);
loff_t  emerald_llseek (struct file *filp, loff_t off, int whence);
int     emerald_ioctl (struct inode *inode, struct file *filp,
                     unsigned int cmd, unsigned long arg);

#endif

#  endif /* __KERNEL__ */

/* Look in Documentation/ioctl-number.txt */
#define EMERALD_IOC_MAGIC  0xd0
#define EMERALD_IOCGIOPORT _IOR(EMERALD_IOC_MAGIC,  1, unsigned long)
#define EMERALD_IOCSPORTCONFIG _IOW(EMERALD_IOC_MAGIC,  2, emerald_config)
#define EMERALD_IOCGPORTCONFIG _IOR(EMERALD_IOC_MAGIC,  3, emerald_config)
#define EMERALD_IOCSEEPORTCONFIG _IOW(EMERALD_IOC_MAGIC,  4, emerald_config)
#define EMERALD_IOCGEEPORTCONFIG _IOR(EMERALD_IOC_MAGIC,  5, emerald_config)
#define EMERALD_IOCEECONFIGLOAD _IO(EMERALD_IOC_MAGIC,  6)
#define EMERALD_IOCPORTENABLE _IO(EMERALD_IOC_MAGIC,  7)
#define EMERALD_IOCGNBOARD _IOR(EMERALD_IOC_MAGIC,  8, int)
#define EMERALD_IOCGISABASE _IOR(EMERALD_IOC_MAGIC,9,unsigned long)

/* Get direction of digital I/O line for a port, 1=out, 0=in */
#define EMERALD_IOCGDIOOUT _IOR(EMERALD_IOC_MAGIC,10,int)

/* Set direction of digital I/O line for a port, 1=out, 0=in */
#define EMERALD_IOCSDIOOUT _IOW(EMERALD_IOC_MAGIC,11,int)

/* Get value of digital I/O line for a port */
#define EMERALD_IOCGDIO _IOR(EMERALD_IOC_MAGIC,12,int)

/* Set value of digital I/O line for a port */
#define EMERALD_IOCSDIO _IOW(EMERALD_IOC_MAGIC,13,int)

#define EMERALD_IOC_MAXNR 13

#endif	/* _EMERALD_H */

