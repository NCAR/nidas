/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*-
 * vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 * Driver for Diamond IR104 relay card made by Tri-M Engineering.
 * This pc104 card has 20 output relays and 20 opto-isolated inputs.
 * Newer versions of this card can generate interrupts on inputs,
 * but as of now, this driver does not support interrupts.
 *
 * Original Author: Gordon Maclean
 * Copyright 2005 UCAR, NCAR, All Rights Reserved
*/

#ifndef NIDAS_DIAMOND_IR104_H
#define NIDAS_DIAMOND_IR104_H

#ifndef __KERNEL__

/* User programs need this for the _IO macros, but kernel
 * modules get their's elsewhere.
 */
#include <sys/ioctl.h>
#include <sys/types.h>

#endif

#define IR104_IOC_MAGIC 'i'

/** Number of boards on a system that are supported by this driver */
#define IR104_MAX_BOARDS 4

#define IR104_IO_REGION_SIZE 15

/** Ioctls */

/** get number of outputs */
#define IR104_GET_NOUT  _IO(IR104_IOC_MAGIC,0)

/** get number of inputs */
#define IR104_GET_NIN   _IO(IR104_IOC_MAGIC,1)

/** clear (to low) digital output ports, as selected by bits of a 32 bit int */
#define IR104_CLEAR \
    _IOW(IR104_IOC_MAGIC,2,unsigned char[3])

/** set (to high) digital output ports, as selected by bits of a 32 bit int */
#define IR104_SET \
    _IOW(IR104_IOC_MAGIC,3,unsigned char[3])

/** set ports, selected by bits of the first 3 chars,
 * to 0(low) or high(1) based on bits of the second 3 chars */
#define IR104_SET_TO_VAL \
    _IOW(IR104_IOC_MAGIC,4,unsigned char[6])

/** get current settings of outputs */
#define IR104_GET_DOUT \
    _IOR(IR104_IOC_MAGIC,5,unsigned char[3])

/** get current settings of inputs */
#define IR104_GET_DIN \
    _IOR(IR104_IOC_MAGIC,6,unsigned char[3])

#define IR104_IOC_MAXNR 6

#define IR104_NOUT 20

#define IR104_NIN 20

#ifdef __KERNEL__
/********  Start of definitions used by the driver module only **********/

#include <linux/cdev.h>
#include <linux/types.h>

#include <nidas/linux/util.h>

/**
 * Device structure for each ir104 board.
 */
struct IR104 {
        unsigned long addr;     /* virtual ioport addr of the IR104 */

        char deviceName[16];

        struct cdev cdev;

        atomic_t num_opened;                     // number of times opened

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)
        struct mutex reg_mutex;         // enforce atomic access to dio regs
#else
        struct semaphore reg_mutex;     // enforce atomic access to dio regs
#endif
        unsigned char outputs[3];                 // current output settings.

        /**
         * Circular buffer of samples containing bit settings of relays.
         * A sample is added to this buffer after the relays are changed
         * via an ioctl.
         */
        struct dsm_sample_circ_buf relay_samples;

        /**
         * wait queue for user read & poll of relay samples.
         */
        wait_queue_head_t read_queue;

        /**
         * saved state of user reads of relay samples.
         */
        struct sample_read_state read_state;

        unsigned int missedSamples;

};

#endif

#endif
