/*
  *******************************************************************
  Copyright by the National Center for Atmospheric Research
									      
  $LastChangedDate$
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL$

  *******************************************************************

  Constants needed for driver code on an Arcom Viper card.

*/

#ifndef DSM_VIPER_H
#define DSM_VIPER_H

#include <asm/arch/pxa-regs.h>
#include <asm/arch/viper.h>

#define SYSTEM_ISA_IOPORT_BASE 0xf7000000

/*
 * Register definitions.
 * One can twiddle with the IRQ_ICR register to set the ISA interrupt
 * mode - either what they call a "linux" or windows style.
 * See the Viper Tech Manual, "PC/104 interrupts" section.
 * Also see: linux/include/asm/arch-pxa/viper.h
 */
#ifndef VIPER_ISA_IRQ_ICR 

#define _VIPER_ISA_IRQ_ICR           (VIPER_CPLD_PHYS + 0x100002)
#define VIPER_ISA_IRQ_ICR            __VIPER_CPLD_REG(_VIPER_ISA_IRQ_ICR)

#define VIPER_ISA_IRQ_ICR_RETRIG	1	/* retrigger interrupt */
#define VIPER_ISA_IRQ_ICR_AUTOCLR	2	/* auto clear first interrupt */
#define VIPER_ISA_IRQ_ICR_LINUX_MODE	0
#define VIPER_ISA_IRQ_ICR_WIN_MODE	(VIPER_ISA_IRQ_ICR_RETRIG | \
					VIPER_ISA_IRQ_ICR_AUTOCLR)

#endif


#endif
