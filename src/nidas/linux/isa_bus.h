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
 */
#define GET_SYSTEM_ISA_IRQ(x) \
({                                                                      \
    const int isa_irqs[] = { 3, 4, 5, 6, 7, 10, 11, 12, 9, 14, 15 };    \
    int i;                                                              \
    int n = -1;                                                         \
    for (i = 0; i < sizeof(isa_irqs)/sizeof(isa_irqs[0]); i++)          \
        if (isa_irqs[i] == (x)) { n = VIPER_IRQ(0) + i; break; }        \
    n;                                                                  \
})

#elif defined(CONFIG_MACH_ARCOM_MERCURY) || defined(CONFIG_MACH_ARCOM_VULCAN) /* Arcom Mercury (or Vulcan) */

#include <asm/irq.h>
#define SYSTEM_ISA_IOPORT_BASE 0x0
#define SYSTEM_ISA_IOMEM_BASE 0x0  /* ? */
/* 
 * On the Mercury/Vulcan, most of the ISA interrupts are routed to GPIO
 * pins on the processor.  Handle those mappings here.
 */
#define GET_SYSTEM_ISA_IRQ(x) \
({					  \
    const int irq_map[] = { 0, 1, 2, IRQ_IXP4XX_GPIO5, IRQ_IXP4XX_GPIO6,\
                            IRQ_IXP4XX_GPIO7, IRQ_IXP4XX_GPIO8,         \
                            IRQ_IXP4XX_GPIO9, 8, 9, IRQ_IXP4XX_GPIO10,  \
                            IRQ_IXP4XX_GPIO11, IRQ_IXP4XX_GPIO12, 13,   \
                            14, 15 };                                   \
    int n;                                                              \
    if ((x) >= 0 && (x) < sizeof(irq_map)/sizeof(irq_map[0]))           \
	n = irq_map[(x)];						\
    else                                                                \
	n = (x);							\
    n;									\
})

#else  /* nothing machine/architecture specific */

#define SYSTEM_ISA_IOPORT_BASE 0x0
#define SYSTEM_ISA_IOMEM_BASE 0x0
#define GET_SYSTEM_ISA_IRQ(x) (x)

#endif

#endif

#endif
