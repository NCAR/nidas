
/* rtl_isa_irq.h
 *
 * Interface for modules which want to register an ISA interrupt
 * handler.
 */

#include <rtl.h>

#define NUM_ISA_IRQS 8

typedef unsigned int (*isa_irq_hander_t) (unsigned int irq, struct rtl_frame *regs);

int rtl_request_isa_irq(unsigned int irq, isa_irq_hander_t handler);

int rtl_free_isa_irq(unsigned int irq);
