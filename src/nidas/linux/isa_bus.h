/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*-
 * vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate$
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL$

  *******************************************************************

  Macros needed for ISA drivers to get the right ioport addresses
  and IRQs on various processors.
*/

#ifndef NIDAS_LINUX_ISA_BUS_H
#define NIDAS_LINUX_ISA_BUS_H

#ifdef __KERNEL__

#ifdef CONFIG_ARCH_VIPER  /* Arcom Viper */

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
#include <mach/irqs.h>
#include <mach/viper.h>
#else
#include <asm/arch/viper.h>
#endif

#define SYSTEM_ISA_IOPORT_BASE VIPER_PC104IO_BASE
#define SYSTEM_ISA_IOMEM_BASE 0x3c000000

#define ISA_16BIT_ADDR_OFFSET 0
/*
 * Special versions of 16 bit I/O operations, that add an address
 * offset as necessary on a given CPU. See VULCAN section below
 */
#define inw_16o(a)        inw(a)
#define insw_16o(a,p,n)   insw(a,p,n)
#define outw_16o(v,a)     outw(v,a)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
/* In earlier kernels the viper maps ISA irqs
 * 3,4,5,6,7,10,11,12,9,14,15 to system interrupts
 * VIPER_IRQ(0)+0,+1, etc.
 * Note that 9 is out of order, and 1,2,8 and 13 aren't available.
 * ISA interrupts 9,14 and 15 are only supported on version 2 Vipers.
 *
 * VIPER_IRQ(0) is 104 for kernel 2.6.16, so the above irqs are
 * mapped to interrupts 104-114.
 * VIPER_IRQ(0) is 108 for kernel 2.6.21, so the above irqs are
 * mapped to interrupts 108-118.
 *
 * VIPER_IRQ(0)+n are not actual hardware interrupt lines.
 *
 * All the ISA interrupts are multiplexed by CPLD code into actual
 * GPIO interrupt GPIO1=VIPER_CPLD_IRQ. The handler for that interrupt
 * then calls the interrupt handlers for the PC104 interrupts based on the
 * bits set in the PC104 interrupt status register.
 * See <linux_2.6_source>/arch/arm/mach-pxa/viper.c.
 * Return -1 if interrupt is not available.
 */
#define GET_SYSTEM_ISA_IRQ(x) \
({                                          \
    const int irq_map[] = { -1, -1, -1,     \
                    VIPER_IRQ(0)+0,  \
                    VIPER_IRQ(0)+1,  \
                    VIPER_IRQ(0)+2,  \
                    VIPER_IRQ(0)+3,  \
                    VIPER_IRQ(0)+4,  \
                    -1,             \
                    VIPER_IRQ(0)+8,  \
                    VIPER_IRQ(0)+5,  \
                    VIPER_IRQ(0)+6,  \
                    VIPER_IRQ(0)+7,  \
                    -1,             \
                    VIPER_IRQ(0)+9,  \
                    VIPER_IRQ(0)+10, \
                    };  \
    int n = -1;                                                         \
    if ((x) >= 0 && (x) < sizeof(irq_map)/sizeof(irq_map[0]))           \
	n = irq_map[(x)];						\
    n;                                                                  \
})

#else

/* Starting in kernel 2.6.35 the viper maps ISA irq 3,4,5,...
 * to system irqs 3,4,5,...  How about that! They're equal.
 * PXA_ISA_IRQ(0) is 0.
 * See linux-source_2.6.35.../arch/arm/mach-pxa/viper.c.
 * Return -1 if interrupt is not available.
 */
#define GET_SYSTEM_ISA_IRQ(x) \
({                                          \
    const int irq_map[] = { -1, -1, -1,     \
                    PXA_ISA_IRQ(0)+3,     /* PC104 IRQ 3 */  \
                    PXA_ISA_IRQ(0)+4,     /* PC104 IRQ 4 */  \
                    PXA_ISA_IRQ(0)+5,     /* PC104 IRQ 5 */  \
                    PXA_ISA_IRQ(0)+6,     /* PC104 IRQ 6 */  \
                    PXA_ISA_IRQ(0)+7,     /* PC104 IRQ 7 */  \
                    -1,             \
                    PXA_ISA_IRQ(0)+9,       /* PC104 IRQ 9 */ \
                    PXA_ISA_IRQ(0)+10,     /* PC104 IRQ 10 */  \
                    PXA_ISA_IRQ(0)+11,     /* PC104 IRQ 11 */  \
                    PXA_ISA_IRQ(0)+12,     /* PC104 IRQ 12 */  \
                    -1,             \
                    PXA_ISA_IRQ(0)+14,     /* PC104 IRQ 14 */ \
                    PXA_ISA_IRQ(0)+15,     /* PC104 IRQ 15 */ \
                    };  \
    int n = -1;                                                         \
    if ((x) >= 0 && (x) < sizeof(irq_map)/sizeof(irq_map[0]))           \
	n = irq_map[(x)];						\
    n;                                                                  \
})

#endif

#elif defined(CONFIG_MACH_ARCOM_MERCURY) || defined(CONFIG_MACH_ARCOM_VULCAN) /* Arcom Mercury (or Vulcan) */

#include <asm/irq.h>
#define SYSTEM_ISA_IOPORT_BASE 0x0
#define SYSTEM_ISA_IOMEM_BASE 0x049000000       /* 8 bit memory space. 16 bit is at 0x4a000000 */

/*
 * On VULCANs I/O window 1 of the PCI1520 bridge chip is configured for 16 bit
 * accesses, and starts at address 0x1000.  0x1000 is subtracted from addresses
 * that are placed on the ISA bus, so that if an outw/inw is done to address 0x1XXX
 * the address on the PC104 bus will be 0x0XXX.
 */
#define ISA_16BIT_ADDR_OFFSET 0x1000

/*
 * Add 0x1000 to addresses on VULCANs for 16 bit operations, so they
 * happen in I/O window 1 on the PCI1520, which is configured
 * for 16 bit accesses.
 */
#define inw_16o(a)        inw((a) + ISA_16BIT_ADDR_OFFSET )
#define insw_16o(a,p,n)   insw((a) + ISA_16BIT_ADDR_OFFSET ,p,n)
#define outw_16o(v,a)     outw(v,(a) + ISA_16BIT_ADDR_OFFSET )

/* 
 * On the Mercury/Vulcan, most of the ISA interrupts are routed to GPIO
 * pins on the processor.  Handle those mappings here.
 * Return -1 if interrupt is not available.
 */
#define GET_SYSTEM_ISA_IRQ(x) \
({					  \
    const int irq_map[] = { -1, -1, -1, IRQ_IXP4XX_GPIO5, IRQ_IXP4XX_GPIO6,\
                            IRQ_IXP4XX_GPIO7, IRQ_IXP4XX_GPIO8,         \
                            IRQ_IXP4XX_GPIO9, -1, -1, IRQ_IXP4XX_GPIO10,\
                            IRQ_IXP4XX_GPIO11, IRQ_IXP4XX_GPIO12};      \
    int n = -1;                                                         \
    if ((x) >= 0 && (x) < sizeof(irq_map)/sizeof(irq_map[0]))           \
	n = irq_map[(x)];						\
    n;									\
})

#elif defined(CONFIG_MACH_ARCOM_TITAN)  /* Arcom Titan */

#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
#include <mach/irqs.h>
#include <mach/titan.h>
#else
#include <asm/arch/titan.h>
#endif

#ifdef TITAN_PC104IO_BASE
#define SYSTEM_ISA_IOPORT_BASE TITAN_PC104IO_BASE
#elif defined(TITAN_PC104IO_VIRT)
#define SYSTEM_ISA_IOPORT_BASE TITAN_PC104IO_VIRT
#endif

#define SYSTEM_ISA_IOMEM_BASE 0xfc000000

#define ISA_16BIT_ADDR_OFFSET 0
/*
 * Special versions of 16 bit I/O operations, that add an address
 * offset as necessary on a given CPU. See VULCAN section.
 */
#define inw_16o(a)        inw(a)
#define insw_16o(a,p,n)   insw(a,p,n)
#define outw_16o(v,a)     outw(v,a)

/* The titan maps ISA irqs 3,4,5,... to system irqs 3,4,5,...
 * How about that! They're equal.
 * PXA_ISA_IRQ(0) is 0.
 * See linux-source_2.6.35.../arch/arm/mach-pxa/titan.c.
 * Return -1 if interrupt is not available.
 */
#define GET_SYSTEM_ISA_IRQ(x) \
({                                          \
    const int irq_map[] = { -1, -1, -1,     \
                    PXA_ISA_IRQ(0)+3,     /* PC104 IRQ 3 */  \
                    PXA_ISA_IRQ(0)+4,     /* PC104 IRQ 4 */  \
                    PXA_ISA_IRQ(0)+5,     /* PC104 IRQ 5 */  \
                    PXA_ISA_IRQ(0)+6,     /* PC104 IRQ 6 */  \
                    PXA_ISA_IRQ(0)+7,     /* PC104 IRQ 7 */  \
                    -1,             \
                    PXA_ISA_IRQ(0)+9,       /* PC104 IRQ 9 */ \
                    PXA_ISA_IRQ(0)+10,     /* PC104 IRQ 10 */  \
                    PXA_ISA_IRQ(0)+11,     /* PC104 IRQ 11 */  \
                    PXA_ISA_IRQ(0)+12,     /* PC104 IRQ 12 */  \
                    -1,             \
                    PXA_ISA_IRQ(0)+14,     /* PC104 IRQ 14 */ \
                    PXA_ISA_IRQ(0)+15,     /* PC104 IRQ 15 */ \
                    };  \
    int n = -1;                                                         \
    if ((x) >= 0 && (x) < sizeof(irq_map)/sizeof(irq_map[0]))           \
	n = irq_map[(x)];						\
    n;                                                                  \
})

#else  /* nothing machine/architecture specific */

#define SYSTEM_ISA_IOPORT_BASE 0x0
#define SYSTEM_ISA_IOMEM_BASE 0x0
#define GET_SYSTEM_ISA_IRQ(x) (x)

#define ISA_16BIT_ADDR_OFFSET 0
#define inw_16o(a) inw(a)
#define insw_16o(a,p,n) insw(a,p,n)
#define outw_16o(v,a) outw(v,a)

#endif

#endif

#endif
