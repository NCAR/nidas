/* rtl_isa_irq.c

   Time-stamp: <Thu 26-Aug-2004 06:48:03 pm>

   RTLinux module for de-multiplexing the ISA bus interrupts.

   Original Author: John Wasinger/Gordon Maclean
   Copyright by the National Center for Atmospheric Research
 
   Implementation notes:

      CPLD multiplexes 8 ISA interrupts. All are falling edge.

      ISA IRQs are 3,4,5,6,7,10,11 and 12

      Note this module is needed because FSMLab's RTLinux does not support
      Arcom's ISA multiplexing code.  Arcom's code has been adapted
      into this module.

   Loading instructions:

      This module should be insmod'ed early in the bootup sequence,
      before any modules that want to call rtl_request_isa_irq().

   Revisions:

      (code revised from 'asm/arch/viper.c')
 */

/* RTLinux includes...  */
#include <rtl_posixio.h>
#include <rtl_pthread.h>
#include <rtl_spinlock.h>

/* Linux module includes... */
#include <linux/autoconf.h>
#include <asm/arch/irqs.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/viper.h>
#include <linux/module.h>

#include <rtl_isa_irq.h>

RTLINUX_MODULE(rtl_isa_irq);
MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("RTLinux ISA IRQ de-multiplexing Module");

static unsigned int rtl_isa_irq_enabled_mask = 0;

/* 
 * isa_isrs[] is an array of function pointers to interrupt service routines
 * for devices that are connected to the ISA bus.  This implementation
 * doesn't support sharing ISA interrupts, only one ISR is called
 * for each interrupt.
 * It could be enhanced to support multiple ISRs.
 * If one requires that rtl_request_isa_irq be called at module init
 * time, then you could kmalloc entries in a list.
 * Otherwise, one could declare fixed arrays of ISRs (say 5) for
 * each IRQ.
 */

static isa_irq_hander_t isa_isrs[NUM_ISA_IRQS];
static int isa_irqs[NUM_ISA_IRQS] = { 3,4,5,6,7,10,11,12 };

static rtl_pthread_spinlock_t irq_controller_lock;

/* -- Utility --------------------------------------------------------- */
static void rtl_mask_and_ack_isa_irq(unsigned int isa_irq_index)
{
  rtl_isa_irq_enabled_mask &= ~(1 << isa_irq_index);
  VIPER_ISA_IRQ_STATUS = 1 << isa_irq_index;
}
/* -- Utility --------------------------------------------------------- */
static void rtl_mask_isa_irq(unsigned int isa_irq_index)
{
  rtl_isa_irq_enabled_mask &= ~(1 << isa_irq_index);
}
/* -- Utility --------------------------------------------------------- */
static void rtl_unmask_isa_irq(unsigned int isa_irq_index)
{
  rtl_isa_irq_enabled_mask |= (1 << isa_irq_index);
}
/* -- RTLinux --------------------------------------------------------- */
static unsigned int rtl_isa_irq_demux_isr (unsigned int irq, struct rtl_frame *regs)
{
  unsigned int irq_status;
  int i, res = 0;

  rtl_global_pend_irq(irq);
  // rtl_printf("isa demux, irq=%d\n",irq);

  irq_status = VIPER_ISA_IRQ_STATUS & 0xFF;

  /* Continue processing multiplexed ISA ISRs until all are processed... */
  while ( irq_status & rtl_isa_irq_enabled_mask )
  {
    rtl_pthread_spin_lock(&irq_controller_lock);
    for (i=0; i < VIPER_NUM_ISA_IRQS; i++)
    {
      if (irq_status & (1<<i))
      {
        /* Ack the indicated ISR's register bit. */
        rtl_mask_and_ack_isa_irq(i);

	// rtl_printf("isa irq=%d\n",i);
        /* Call the indicated ISR. */
	if (isa_isrs[i]) res = (*isa_isrs[i])(isa_irqs[i], regs);

	/* Enable (unmask) the indicated ISR again. */
        rtl_unmask_isa_irq(i);
      }
    }
    irq_status = VIPER_ISA_IRQ_STATUS & 0xFF;
    rtl_pthread_spin_unlock(&irq_controller_lock);
  }
  rtl_hard_enable_irq(irq);
  return res;
}

int rtl_request_isa_irq(unsigned int irq, isa_irq_hander_t handler)
{
    int i;
    rtl_pthread_spin_lock(&irq_controller_lock);
    for (i = 0; i < NUM_ISA_IRQS; i++) {
	if (isa_irqs[i] == irq) {
	    rtl_printf("requesting isa irq=%d\n",irq);
	    isa_isrs[i] = handler;
	    rtl_unmask_isa_irq(i);
	    rtl_pthread_spin_unlock(&irq_controller_lock);
	    return 0;
	}
    }
    rtl_pthread_spin_unlock(&irq_controller_lock);
    return -RTL_EINVAL;		/* no such isa irq */
}

int rtl_free_isa_irq(unsigned int irq)
{
    int i;
    rtl_pthread_spin_lock(&irq_controller_lock);
    for (i = 0; i < NUM_ISA_IRQS; i++) {
	if (isa_irqs[i] == irq) {
	    rtl_printf("freeing isa irq=%d\n",irq);
	    isa_isrs[i] = NULL;
	    rtl_mask_isa_irq(i);
	    rtl_pthread_spin_unlock(&irq_controller_lock);
	    return 0;
	}
    }
    rtl_pthread_spin_unlock(&irq_controller_lock);
    rtl_printf("can't free isa irq=%d\n",irq);
    return -RTL_EINVAL;		/* no such isa irq */
}

/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  int i;

  rtl_pthread_spin_lock(&irq_controller_lock);
  /* disable ISRs */
  for (i=0; i<NUM_ISA_IRQS; i++) {
    if (isa_isrs[i]) {
	rtl_printf("(%s) %s:\t IRQ %2d disabled\n",
                 __FILE__, __FUNCTION__, isa_irqs[i]);
	rtl_mask_isa_irq(i);
	isa_isrs[i] = NULL;
    }
  }
  rtl_pthread_spin_unlock(&irq_controller_lock);

  /* release interrupt handler */
  rtl_free_irq(VIPER_CPLD_IRQ);
}
/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  int i;

  rtl_pthread_spin_init(&irq_controller_lock,0);

  rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
             __FILE__, __FUNCTION__, __DATE__, __TIME__);
  for (i=0; i<NUM_ISA_IRQS; i++) {
    isa_isrs[i] = NULL;
    rtl_mask_isa_irq(i);
  }

  /* install a handler for the multiplexed interrupt */
  if ( rtl_request_irq( VIPER_CPLD_IRQ, rtl_isa_irq_demux_isr ) < 0 )
  {
    /* failed... */
    cleanup_module();
    rtl_printf("(%s) %s:\t could not allocate IRQ at #%d\n",
               __FILE__, __FUNCTION__, VIPER_CPLD_IRQ);
    return -EIO;
  }
  rtl_printf("(%s) %s:\t allocated IRQ at #%d\n",
	     __FILE__, __FUNCTION__, VIPER_CPLD_IRQ);

  return 0;
}
