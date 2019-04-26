/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */

/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2012, Copyright University Corporation for Atmospheric Research
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
Module which provides a watchdog for PC104 interrupts.

Original author:        Gordon Maclean

*/

#include <linux/module.h>
#include <linux/timer.h>
#include <linux/spinlock.h>
#include <nidas/linux/isa_bus.h>
#include <nidas/linux/Revision.h>    // REPO_REVISION

#if defined(CONFIG_MACH_ARCOM_TITAN)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
#include <mach/pxa2xx-regs.h>
#else
#include <asm/arch/pxa-regs.h>
#endif

struct irq_desc;

/* These symbols are exported from arch/arm/mach-pxa/titan.c */
extern void titan_irq_handler(unsigned int,struct irq_desc*);
extern unsigned long titan_irq_enabled_mask;

#endif

#include <nidas/linux/klog.h>

#ifndef REPO_REVISION
#define REPO_REVISION "unknown"
#endif

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_DESCRIPTION("NCAR pc104 IRQ watchdog");
MODULE_LICENSE("GPL");
MODULE_VERSION(REPO_REVISION);

static struct timer_list pc104_irq_watchdog_timer;

#ifdef SPIN_LOCK_UNLOCKED
static spinlock_t pc104_irq_watchdog_spinlock = SPIN_LOCK_UNLOCKED;
#else
DEFINE_SPINLOCK(pc104_irq_watchdog_spinlock);
#endif

static unsigned int watchdogDetectedInterrupts = 0;

static inline unsigned long pc104_irq_pending(void)
{
#if defined(CONFIG_MACH_ARCOM_TITAN)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
        return (TITAN_HI_IRQ_STATUS << 8 | TITAN_LO_IRQ_STATUS) &
                                        titan_irq_enabled_mask;
#else
        return TITAN_CPLD_ISA_IRQ & titan_irq_enabled_mask;
#endif
#else
        return 0;
#endif
}

/* Note that a pointer to a valid struct irq_desc is not
 * available here, and so is passed as a NULL pointer to
 * titan_irq_handler. This is OK because titan_irq_handler
 * does nothing with it.
 * When other handlers are added which have a
 * struct irq_desc* argument, make sure it can be NULL.
 * The irq_desc array symbol is not exported from the kernel code,
 * probably for good reasons.
 */
static void pc104_irq_handler(unsigned int irq)
{
        for (;;) {
                unsigned long pending = pc104_irq_pending();
                if (!pending)
                        break;
#if defined(CONFIG_MACH_ARCOM_TITAN)
                titan_irq_handler(TITAN_ISA_IRQ,0);
#else
                break;
#endif
        }
}

static void pc104_irq_watchdog_timer_func(unsigned long arg)
{
        /* Note that this runs in software interrupt context. */
        unsigned long flags;
        unsigned long pending;

        spin_lock_irqsave(&pc104_irq_watchdog_spinlock,flags);
        pending = pc104_irq_pending();

        if (pending) {
#if defined(CONFIG_MACH_ARCOM_TITAN)
                pc104_irq_handler(TITAN_ISA_IRQ);
#else
#endif
                if (!(watchdogDetectedInterrupts++ % 1))
                        printk(KERN_NOTICE "%d interrupts caught by PC104 IRQ watchdog\n",
                                watchdogDetectedInterrupts);
        }
        spin_unlock_irqrestore(&pc104_irq_watchdog_spinlock,flags);

        mod_timer(&pc104_irq_watchdog_timer, jiffies + HZ / 10);
}

static void __exit pc104_irq_watchdog_cleanup(void)
{
        del_timer(&pc104_irq_watchdog_timer);
}

static int __init pc104_irq_watchdog_init(void)
{
        KLOG_NOTICE("version: %s\n",REPO_REVISION);

        init_timer(&pc104_irq_watchdog_timer);
        pc104_irq_watchdog_timer.function = pc104_irq_watchdog_timer_func;
        pc104_irq_watchdog_timer.expires = jiffies + HZ / 10;
        pc104_irq_watchdog_timer.data = (unsigned long) 0;
        add_timer(&pc104_irq_watchdog_timer);
        return 0;

}

module_init(pc104_irq_watchdog_init);
module_exit(pc104_irq_watchdog_cleanup);
                                             
