/* isa_demux.c

   Time-stamp: <Mon 16-Aug-2004 03:24:03 pm>

   RTLinux module for de-multiplexing the ISA bus interrupts.

   Original Author: John Wasinger
   Copyright by the National Center for Atmospheric Research
 
   Implementation notes:

      I have tagged areas which need development with a 'TODO' comment.

      CPLD multiplexes 8 ISA interrupts. All are falling edge.

      ISA IRQs are 3,4,5,6,7,10,11 and 12 which correspond to IRQs 104-111.

      Note this module is needed because FSMLab's RTLinux does not support
      Arcom's ISA multiplexing code.  I have copied Arcom's code into this
      module.

   Loading instructions:

      This module should be insmod'ed after all the modules that provide
      ISA bus based interrupt service routines are loaded.

   Revisions:

      (code copied and renamed from 'asm/arch/viper.c')
 */

/* RTLinux includes...  */
#include <rtl.h>
#include <rtl_posixio.h>

/* Linux module includes... */
#include <linux/autoconf.h>
#include <asm/arch/irqs.h>
#include <asm/arch/pxa-regs.h>
#include <asm/arch/viper.h>
#include <linux/module.h>

RTLINUX_MODULE(isa_demux);
MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("RTLinux ISA de-multiplexing Module");

typedef unsigned int (*funcPtr) (unsigned int irq, struct rtl_frame *regs);

static unsigned int rtl_isa_irq_enabled_mask = 0;
static int rtl_isa_irqs[] = { 3,4,5,6,7,10,11,12 };

/* TODO - add extern listings to your driver's ISR function here. */
extern int module_loading;
extern unsigned int irig_100hz_isr (unsigned int irq, struct rtl_frame *regs);
extern unsigned int rtl_serial_irq (unsigned int irq, struct rtl_frame *regs);

extern void setHeartBeatOutput (int rate);
extern void enableHeartBeatInt (void);
extern void disableHeartBeatInt (void);
extern void setRate2Output (int rate);

/* TODO - add your driver's ISR function here for its given IRQ #.
 *
 * isa_isr[] is an array of function pointers to interrupt service routines
 * for devices that are connected to the ISA bus.
 */
static funcPtr isa_isr[] = {
  NULL,                   // IRQ  3
  NULL,                   // IRQ  4
  NULL,                   // IRQ  5
  NULL,                   // IRQ  6
  NULL,                   // IRQ  7
  irig_100hz_isr,         // IRQ 10
  rtl_serial_irq,         // IRQ 11
  NULL                    // IRQ 12
};
/* -- Utility --------------------------------------------------------- */
static void rtl_mask_and_ack_isa_irq(unsigned int irq)
{
  int rtl_irq = (irq - VIPER_ISA_IRQ(0));
  rtl_isa_irq_enabled_mask &= ~(1 << rtl_irq);
  VIPER_ISA_IRQ_STATUS = 1 << rtl_irq;
}
/* -- Utility --------------------------------------------------------- */
static void rtl_mask_isa_irq(unsigned int irq)
{
  int rtl_irq = (irq - VIPER_ISA_IRQ(0));
  rtl_isa_irq_enabled_mask &= ~(1 << rtl_irq);
}
/* -- Utility --------------------------------------------------------- */
static void rtl_unmask_isa_irq(unsigned int irq)
{
  int rtl_irq = (irq - VIPER_ISA_IRQ(0));
  rtl_isa_irq_enabled_mask |= (1 << rtl_irq);
}
/* -- RTLinux --------------------------------------------------------- */
static unsigned int rtl_isa_irq_demux_isr (unsigned int irq, struct rtl_frame *regs)
{
  unsigned int irq_status;
  int i, res = 0;

  irq_status = VIPER_ISA_IRQ_STATUS & 0xFF;

  /* Continue processing multiplexed ISA ISRs until all are processed... */
  while ( irq_status & rtl_isa_irq_enabled_mask )
  {
    for (i=0; i < VIPER_NUM_ISA_IRQS; i++)
    {
      if (irq_status & (1<<i))
      {
        /* Ack the indicated ISR's register bit. */
        rtl_mask_and_ack_isa_irq(VIPER_ISA_IRQ(i));

        /* Call the indicated ISR. */
	res = (*isa_isr[i])(irq, regs);

	/* Enable (unmask) the indicated ISR again. */
        rtl_unmask_isa_irq(VIPER_ISA_IRQ(i));
      }
    }
    irq_status = VIPER_ISA_IRQ_STATUS & 0xFF;
  }
  return res;
}
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  int i;

  /* disable (mask) the provided ISRs */
  for (i=0; i<8; i++)
    if (isa_isr[i])
    {
      rtl_printf("(%s) %s:\t IRQ %2d disabled\n",
                 __FILE__, __FUNCTION__, rtl_isa_irqs[i]);
      rtl_mask_isa_irq(VIPER_ISA_IRQ(i));
    }

  /* TODO (HERE?) - define an array of per device IO regions */
//release_region(isa_address, ioWidth);

  /* stop generating IRIG interrupts */
  disableHeartBeatInt();

  /* release interrupt handler */
  rtl_free_irq(VIPER_CPLD_IRQ);
}
/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
             __FILE__, __FUNCTION__, __DATE__, __TIME__);
  int i;

  /* enable (unmask) the provided ISRs */
  for (i=0; i<8; i++)
    if (isa_isr[i])
    {
      rtl_printf("(%s) %s:\t IRQ %2d enabled\n",
                 __FILE__, __FUNCTION__, rtl_isa_irqs[i]);
      rtl_unmask_isa_irq(VIPER_ISA_IRQ(i));
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

  /* activate the IRIG-B board */
  /* TODO - pend the activation of the interrupts until after
   * the 'rtl_request_irq' in isa_demux.c has been called... */
  setHeartBeatOutput(100);
  rtl_printf("(%s) %s:\t setHeartBeatOutput(%d) done\n", __FILE__, __FUNCTION__, 100);
  setRate2Output(50000);
  rtl_printf("(%s) %s:\t setRate2Output(%d) done\n",     __FILE__, __FUNCTION__, 50000);
  enableHeartBeatInt();
  rtl_printf("(%s) %s:\t enableHeartBeatInt  done\n",    __FILE__, __FUNCTION__);

  module_loading = 0;
  return 0;
}
