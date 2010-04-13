/*

   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   Driver for Viper digital IO ports. There are 8 independent inputs, IN0-7,
   and 8 independent outputs OUT0-7.  The value of the inputs can be read,
   and the value of the outputs written or read.

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#ifndef NIDAS_ARCOM_VIPER_DIO_H
#define NIDAS_ARCOM_VIPER_DIO_H

#ifndef __KERNEL__

/* User programs need this for the _IO macros, but kernel
 * modules get their's elsewhere.
 */
#include <sys/ioctl.h>
#include <sys/types.h>

#endif

#define VIPER_DIO_IOC_MAGIC 'v'

/** Ioctls */

/** get number of output pins (8, OUT0-7) */
#define VIPER_DIO_GET_NOUT  _IO(VIPER_DIO_IOC_MAGIC,0)

/** get number of input pins (8, IN0-7) */
#define VIPER_DIO_GET_NIN   _IO(VIPER_DIO_IOC_MAGIC,1)

/** clear (to low) viper digital output ports OUT0-7, as selected by bits 0-7 of a char */
#define VIPER_DIO_CLEAR \
    _IOW(VIPER_DIO_IOC_MAGIC,2,unsigned char)

/** set (to high) viper digital output ports OUT0-7, as selected by bits 0-7 of a char */
#define VIPER_DIO_SET \
    _IOW(VIPER_DIO_IOC_MAGIC,3,unsigned char)

/** set ports OUT0-7, selected by bits 0-7 of the first char,
 * to 0(low) or high(1) based on bits 0-7 of the second char */
#define VIPER_DIO_SET_TO_VAL \
    _IOW(VIPER_DIO_IOC_MAGIC,4,unsigned char[2])

/** get current settings of OUT0-7 */
#define VIPER_DIO_GET_DOUT \
    _IOR(VIPER_DIO_IOC_MAGIC,5,unsigned char)

/** get current settings of IN0-7 */
#define VIPER_DIO_GET_DIN \
    _IOR(VIPER_DIO_IOC_MAGIC,6,unsigned char)

#define VIPER_DIO_IOC_MAXNR 6

#define VIPER_DIO_NOUT 8

#define VIPER_DIO_NIN 8

#ifdef __KERNEL__
/********  Start of definitions used by the driver module only **********/

#include <linux/cdev.h>
#include <asm/arch/viper.h>

/*
  Definitions missing from asm/arch/viper.h for GPIO read register.
  Physical address of 16 bit GPIO read register */
#define _VIPER_GPIO_PHYS (VIPER_CPLD_PHYS + 0x500000)
/* Virtual address of 16 bit GPIO read register */
#define VIPER_GPIO __VIPER_CPLD_REG(_VIPER_GPIO_PHYS)

/**
 * Device structure.
 */
struct VIPER_DIO {

        char deviceName[64];

        dev_t devno;

        struct cdev cdev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        struct mutex reg_mutex;         // enforce atomic access to dio regs
#else
        struct semaphore reg_mutex;     // enforce atomic access to dio regs
#endif
};

#endif

#endif
