/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $
									      
  $LastChangedRevision: 3648 $
									      
  $LastChangedBy: cjw $
									      
  $HeadURL: http://svn/svn/nids/trunk/src/nidas/rtlinux/dsm_viper.h $

  *******************************************************************

  Constants needed for driver code on an Arcom Viper card.

*/

#ifndef NIDAS_LINUX_ISA_BUS_H
#define NIDAS_LINUX_ISA_BUS_H

#ifdef __KERNEL__

#ifdef CONFIG_ARCH_VIPER
#include <asm/arch/viper.h>
#define SYSTEM_ISA_IOPORT_BASE VIPER_PC104IO_BASE
#define SYSTEM_ISA_IOMEM_BASE 0x3c000000

/* The viper maps ISA irq 3,4,5,... to viper interrupts 104,105,106,etc.
 * See <linux_2.6_source>/arch/arm/mach-pxa/viper.c.
 */
#define GET_VIPER_IRQ(x) \
({                                                                      \
    const int isa_irqs[] = { 3, 4, 5, 6, 7, 10, 11, 12, 9, 14, 15 };    \
    int i;                                                              \
    int n = -1;                                                         \
    for (i = 0; i < sizeof(isa_irqs)/sizeof(isa_irqs[0]); i++)          \
        if (isa_irqs[i] == (x)) { n = VIPER_IRQ(0) + i; break; }        \
    n;                                                                  \
})

#else

#define SYSTEM_ISA_IOPORT_BASE 0x0
#define SYSTEM_ISA_IOMEM_BASE 0x0

#endif

#endif

#endif
