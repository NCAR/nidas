/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $
									      
  $LastChangedRevision: 3648 $
									      
  $LastChangedBy: cjw $
									      
  $HeadURL: http://svn/svn/nids/trunk/src/nidas/rtlinux/dsm_viper.h $

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
#define SYSTEM_ISA_IOMEM_BASE 0x0  /* ? */
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

#endif

#endif

#endif
