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
// #include <rtl_pthread.h>
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

#define NUM_ISA_IRQS VIPER_NUM_ISA_IRQS

static unsigned int rtl_isa_irq_enabled_mask = 0;

struct isrData {
    isa_irq_hander_t handler;
    void* callbackPtr;
};

/* 
 * isa_isrs[] is an array of isrData structures, containing function
 * pointers to interrupt service routines for devices that are
 * connected to the ISA bus.  This implementation
 * doesn't support sharing ISA interrupts, only one ISR is called
 * for each interrupt.
 * This current version must be called from your modules init_module
 * function since it does a kmalloc() here. To remove that restriction,
 * one could kmalloc all the structures during this init_module,
 * and initialize the function pointers to 0.
 *
 * It could be enhanced to support multiple ISRs.
 * If one requires that rtl_request_isa_irq be called at init_module
 * time, then you could kmalloc entries in a list.
 * Otherwise, one could declare fixed arrays of ISRs (say 5) for
 * each IRQ.
 */

static struct isrData* isa_isrs[NUM_ISA_IRQS];
static int isa_irqs[NUM_ISA_IRQS] = { 3,4,5,6,7,10,11,12 };

static rtl_spinlock_t irq_controller_lock;

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
  int imask = 1;

  irq_status = VIPER_ISA_IRQ_STATUS & 0xFF;
  // rtl_printf("isa irq=%d, status=%x, mask=%x\n",irq,irq_status,
  // 	rtl_isa_irq_enabled_mask);

  if (!irq_status) {
      // Don't do the rtl_global_pend_irq(irq), just return
      rtl_hard_enable_irq(irq);
      return 0;
  }

  /* Continue processing multiplexed ISA ISRs until all are processed... */

  while (irq_status & rtl_isa_irq_enabled_mask) {
    imask = 1;
    rtl_spin_lock(&irq_controller_lock);
    for (i=0; i < NUM_ISA_IRQS; i++) {
      if (irq_status & imask) {
        /* Ack the indicated ISR's register bit. */
        rtl_mask_and_ack_isa_irq(i);

        /* Call the indicated ISR. */
	if (isa_isrs[i]) res =
	    (*isa_isrs[i]->handler)(isa_irqs[i], isa_isrs[i]->callbackPtr,regs);

	/* Enable (unmask) the indicated ISR again. */
        rtl_unmask_isa_irq(i);

	irq_status &= ~imask;
	if (!irq_status) break;		// done
      }
      imask <<= 1;
    }
    rtl_spin_unlock(&irq_controller_lock);
    irq_status = VIPER_ISA_IRQ_STATUS & 0xFF;
    // rtl_printf("isa irq=%d, status=%x, mask=%x\n",irq,irq_status,
  	// rtl_isa_irq_enabled_mask);
  }

  rtl_global_pend_irq(irq);
  rtl_hard_enable_irq(irq);
  return res;
}

int rtl_request_isa_irq(unsigned int irq, isa_irq_hander_t handler, void* callbackPtr)
{
    int i;
    int ret = -EINVAL;		/* no such isa irq */
    unsigned long flags;

    rtl_spin_lock_irqsave(&irq_controller_lock,flags);
    for (i = 0; i < NUM_ISA_IRQS; i++) {
	if (isa_irqs[i] == irq) {

	    rtl_printf("requesting isa irq=%d\n",irq);

	    ret = -EBUSY;
	    if (isa_isrs[i]) break;

	    ret = -ENOMEM;
	    isa_isrs[i] = kmalloc(sizeof(struct isrData),GFP_KERNEL);
	    if (!isa_isrs[i]) break;

	    isa_isrs[i]->handler = handler;
	    isa_isrs[i]->callbackPtr = callbackPtr;
	    rtl_unmask_isa_irq(i);
	    ret = 0;
	    break;
	}
    }
    rtl_spin_unlock_irqrestore(&irq_controller_lock,flags);
    return ret;
}

int rtl_free_isa_irq(unsigned int irq)
{
    int i;
    int ret = -EINVAL;		/* no such isa irq */
    unsigned long flags;
    rtl_spin_lock_irqsave(&irq_controller_lock,flags);

    for (i = 0; i < NUM_ISA_IRQS; i++) {
	if (isa_irqs[i] == irq && isa_isrs[i]) {
	    rtl_printf("freeing isa irq=%d\n",irq);
	    rtl_mask_isa_irq(i);
	    if (isa_isrs[i]) kfree(isa_isrs[i]);
	    isa_isrs[i] = NULL;
	    ret = 0;
	    break;
	}
    }

    rtl_spin_unlock_irqrestore(&irq_controller_lock,flags);

    if (ret == -EINVAL) rtl_printf("can't free isa irq=%d\n",irq);
    return ret;
}

/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
    int i;

    unsigned long flags;
    rtl_spin_lock_irqsave(&irq_controller_lock,flags);

    /* disable ISRs */
    for (i=0; i<NUM_ISA_IRQS; i++) {
	if (isa_isrs[i]) {
	    rtl_printf("(%s) %s:\t IRQ %2d disabled\n",
		     __FILE__, __FUNCTION__, isa_irqs[i]);
	    rtl_mask_isa_irq(i);
	    kfree(isa_isrs[i]);
	    isa_isrs[i] = NULL;
	}
    }
    rtl_spin_unlock_irqrestore(&irq_controller_lock,flags);

    /* release interrupt handler */
    rtl_free_irq(VIPER_CPLD_IRQ);
}
/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  int i;

  rtl_spin_lock_init(&irq_controller_lock);

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
