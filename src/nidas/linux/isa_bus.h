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

#include <asm/arch/viper.h>
#define SYSTEM_ISA_IOPORT_BASE VIPER_PC104IO_BASE
#define SYSTEM_ISA_IOMEM_BASE 0x3c000000

/*
 * "Windowed" versions of 16 bit I/O operations, provided
 * so that it can be redefined for VULCANs.
 */
#define inw_w(a)        inw(a)
#define insw_w(a,p,n)   insw(a,p,n)
#define outw_w(v,a)     outw(v,a)

/* The viper maps ISA irq 3,4,5,... to viper interrupts 104,105,106,etc.
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
#define VULCAN_IO_WINDOW_1_START 0x1000

/*
 * "Windowed" versions of 16 bit I/O operations. Add 0x1000
 * to the addresses on VULCANs so that the 16 bit operations
 * happen in I/O window 1 on the PCI1520, which is configured
 * for 16 bit accesses.
 */
#define inw_w(a)        inw((a)+VULCAN_IO_WINDOW_1_START)
#define insw_w(a,p,n)   insw((a)+VULCAN_IO_WINDOW_1_START,p,n)
#define outw_w(v,a)     outw(v,(a)+VULCAN_IO_WINDOW_1_START)

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

#else  /* nothing machine/architecture specific */

#define SYSTEM_ISA_IOPORT_BASE 0x0
#define SYSTEM_ISA_IOMEM_BASE 0x0
#define GET_SYSTEM_ISA_IRQ(x) (x)

#define inw_w(a) inw(a)
#define insw_w(a,p,n) insw(a,p,n)
#define outw_w(v,a) outw(v,a)

#endif

#endif

#endif
