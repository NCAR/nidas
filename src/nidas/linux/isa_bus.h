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

#else

#define SYSTEM_ISA_IOPORT_BASE 0x0
#define SYSTEM_ISA_IOMEM_BASE 0x0

#endif

#endif

#endif
