
/* rtl_isa_irq.h
 *
 * Interface for modules which want to register an ISA interrupt
 * handler.
 */

#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_core.h>

// #define NUM_ISA_IRQS 8

typedef unsigned int (*isa_irq_hander_t) (unsigned int irq, void* callbackPtr,
	struct rtl_frame *regs);

/* register an ISA interrupt handler to be called on receipt of ISA
 * interrupt.
 * Params:
 *    irq: an ISA interrupt: 3,4,5,6,7,10,11 or 12
 *    handler: a pointer to a function of type isa_irq_handler_t
 *    callbackPtr: the pointer that you want passed to your handler,
 *	which may be (void*)0 (NULL) if you don't need it.
 */
int rtl_request_isa_irq(unsigned int irq, isa_irq_hander_t handler,void* callbackPtr);

int rtl_free_isa_irq(unsigned int irq);
