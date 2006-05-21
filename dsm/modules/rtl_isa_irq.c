/* rtl_isa_irq.c

   Time-stamp: <Wed 13-Apr-2005 05:52:11 pm>

   RTLinux module for de-multiplexing the ISA bus interrupts.

   Original Author: John Wasinger/Gordon Maclean
   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
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
#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl_posixio.h>

/* Linux module includes... */
#include <linux/autoconf.h>
#include <asm/arch/irqs.h>
// #include <asm/arch/pxa-regs.h>
// #include <asm/arch/viper.h>
#include <linux/module.h>

#include <dsmlog.h>
#include <rtl_isa_irq.h>
#include <dsm_viper.h>
#include <dsm_version.h>

RTLINUX_MODULE(rtl_isa_irq);
MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("RTLinux ISA IRQ de-multiplexing Module");

#define NUM_ISA_IRQS VIPER_NUM_ISA_IRQS

/*
 * constants from dsm_viper.h 
 * Possible settings for
 * set_GPIO_IRQ_edge:
 *	set_GPIO_IRQ_edge(IRQ_TO_GPIO(VIPER_CPLD_IRQ), GPIO_FALLING_EDGE);
 *	set_GPIO_IRQ_edge(IRQ_TO_GPIO(VIPER_CPLD_IRQ), GPIO_RISING_EDGE);
 *	set_GPIO_IRQ_edge(IRQ_TO_GPIO(VIPER_CPLD_IRQ), GPIO_BOTH_EDGES);
 * Possible settings for
 *  VIPER_ISA_IRQ_ICR = VIPER_ISA_IRQ_ICR_RETRIG | VIPER_ISA_IRQ_ICR_AUTOCLR;
 */

/*
  From VIPER_Tech_Manual.pdf, the following settings are available on
  the VIPER_ISA_IRQ_ICR (ISA interrupt control register)
  	RETRIG: 0 No interrupt retrigger (embedded Linux and VxWorks).
	        1 Interrupt retrigger (Windows CE .NET).
	AUTO_CLR: 0 No auto clear interrupt / Toggle GPIO1 on new interrupt.
	          1 Auto clear interrupt / Low to high transition on GPIO1
		  	on First Interrupt.
  As copied straight from the pdf:
  LINUX_MODE (RETRIG=0, AUTO_CLR=0):
      Leave the ICR register set to its default value, so that a new
      interrupt causes the microprocessor PC/104 interrupt pin GPIO1
      to be toggled for every new interrupt on a different PC/104
      interrupt source. Ensure the GPIO1 input is set up in a level
      triggered mode. The retrigger interrupt function is not required
      for embedded Linux or VxWorks.
      Once the VIPER microprocessor has serviced a PC/104 interrupt,
      clear the corresponding bit in the PC104I register by writing  1  to it.

   WINDOWS mode (RETRIG=1, AUTO_CLR=1)
    Write 0x2 (AUTO_CLR) to the ICR Register so that the first PC/104
    interrupt source causes the PXA255 PC/104 interrupt pin GPIO1 to
    receive a low to high transition. When the first PC/104 interrupt
    occurs the Interrupt service routine will start polling through the
    PC/104 interrupt sources in the PC104I register. The first bit it
    sees set to a  1 , sets a semaphore to make a program run to service
    the corresponding interrupt. Once this program has serviced the
    interrupt the interrupting source returns its interrupt output to the
    inactive state ( 0 ) if it hasn't requested another interrupt whilst
    the microprocessor serviced the last interrupt. Once this happens
    the corresponding bit in the PC104I register shall be automatically
    cleared. Each PC/104 board requesting an interrupt shall keep its
    interrupt in the active state ( 1 ) until the interrupt has been
    serviced by the microprocessor. When there are no interrupts
    outstanding the level of the PC/104 interrupt on GPIO1 shall
    automatically return to logic  0 . If it is still  1  then there
    are interrupts outstanding, which would have occurred during the
    servicing of the last interrupt. To capture any interrupts that could
    have occurred whilst the last interrupt was serviced, the retrigger
    interrupt bit in the ICR register is set to  1  to retrigger a low to
    high transition on GPIO1 to restart the interrupt polling mechanism
    if there are any outstanding interrupts.
*/

/* TEST RESULTS (Jan 14, 2005 GDM)
 * Keep in mind here that this discussion applies to the
 * viper GPIO interrupt, asserted by the CPLD,
 * which is multiplexing the ISA interrupts.  It does
 * not apply to the ISA interrupts themselves.
 *
 * Test system:  IRIG programmed to interrupt at 1000 hz, three
 *    serial ports being prompted at 100Hz each, 115200 baud.
 *    The simulated sensor responses are coming from two of Viper's
 *    own ports /dev/ttyS1 and /dev/ttyS2.
 *    This is a total ISA interrupt load of  1000 + 100*4 + 100 = 1500
 *    interrupts per second.
 * DO_STATUS_LOOP means that the demux isr loops, for as long as a requested
 *    irq bit is set in status register.
 * CLEAR_STATUS_BIT controls whether a bit is written to the status
 *    register after calling each irq handler like so:
 * 	VIPER_ISA_IRQ_STATUS = imask;
 *
 * WIN_MODE (AUTO_CLR=1, RETRIG=1, rising edge, DO_STATUS_LOOP on,
 * 	CLEAR_STATUS_BIT off)
 *    The 100Hz IRIG loop reports semaphore timeouts about
 *    every 2 minutes. A typical sequence showed a timeout
 *    after 256 seconds, then 347, 225,9 and 10 seconds. These timeouts
 *    will occur if the IRIG isr doesn't post the semaphore within
 *    a period of 13 milliseconds. It indicates that one of the
 *    10 interrupts in 10 milliseconds wasn't serviced.
 *    Also seeing about 10 spurious interrupts (status==0) each second.
 *
 *    Second test: timeout sequence  68,99,99,3,6,105,48,143,52,1 secs
 *      about 20 spurious status=0 interrupts every second
 *    Third test: 119,163,165
 *    Fourth test: 40,29,27,36,0,539(yay!),
 *
 *    DO_STATUS_LOOP on, CLEAR_STATUS_BIT on
 *	first test: saw similar results to 256,347,255,9,10 sequence.
 *      second test: 182,373 with about 20 spurious status=0 ints/sec
 *
 *     no DO_STATUS_LOOP, no CLEAR_STATUS_BIT:
 *       zillions of timeouts when serial ports running, nic died
 *     no DO_STATUS_LOOP, CLEAR_STATUS_BIT on:
 *       zillions of timeouts when serial ports running
 *
 *  AUTO_CLR MODE (AUTO_CLR=1, RETRIG=0, rising edge,
 *     no CLEAR_STATUS_BIT, do DO_STATUS_LOOP
 *       Semaphore timeouts: 154,109,266,134,21,122,15 seconds 
 *       20 status=0 spurious interrupts per sec
 *    no CLEAR_STATUS_BIT, no DO_STATUS_LOOP:
 *       zillions of timeouts when serial ports running,
 *    CLEAR_STATUS_BIT on, DO_STATUS_LOOP:
 *       Semaphore timeouts: 66,62,87,125,67,32,47,123 seconds
 *          10-20 status=0 spurious interrupts/sec
 *
 *  LINUX MODE (AUTO_CLR=0, RETRIG=0, level based=rising&falling edge,
 *     CLEAR_STATUS_BIT (must be on), no DO_STATUS_LOOP)
 *    Saw semaphore timeouts about every 10 seconds, with
 *    no spurious interrupts. Second test same results
 *          timeout sequence: 20,6,37,8,1,18,0,35,31,31,19 secs
 *     IF DO_STATUS_LOOP: similiar timeout rate:  44,35,4,24,0,0,27,67,35,2,75
 *        with 200 status=0 interrupts every second
 *     
 *  RETRIG MODE (AUTO_CLR=0, RETRIG=1, level based, CLEAR_STATUS_BIT on,
 *    no DO_STATUS_LOOP)
 *    Saw semaphore timeouts about every 10 seconds, with
 *    no spurious interrupts.  Same as native LINUX/no DO_STATUS_LOOP
 *    Second test: 49,0,39,2,35,21,37,0,10,27,0,9
 *
 *    IF DO_STATUS_LOOP:
 *    Saw timeouts about every 10 seconds, with 100 spurious
 *    interrupts/sec.
 *    Second test: 4,32,32,0,39,56,9,28,37,28
 *
 * Conclusions: AUTO_CLR=1 is better than AUTO_CLR=0
 *	must use DO_STATUS_LOOP if AUTO_CLR=1
 *	must use CLEAR_STATUS_BIT if AUTO_CLR=0
 *	RETRIG and CLEAR_STATUS_BIT don't seem to have much effect
 *      with AUTO_CLR=1.  We'll set RETRIG, but not CLEAR_STATUS_BIT
 */

/* Define GPI01 interrupt mode */
#define AUTOCLR_MODE
#define RETRIG_MODE


/********* AUTOCLR_MODE ****************************************/
#ifdef AUTOCLR_MODE

#define DO_STATUS_LOOP
#define ICR_AUTOCLR VIPER_ISA_IRQ_ICR_AUTOCLR
#define IRQ_EDGE GPIO_RISING_EDGE

#else	/* not AUTOCLR_MODE */

#define ICR_AUTOCLR 0
#define IRQ_EDGE GPIO_BOTH_EDGES
#define CLEAR_STATUS_BIT

#endif	/* end AUTOCLR_MODE */
/************ AUTOCLR_MODE end *********************************/


/*********** RETRIG_MODE ***************************************/
#ifdef RETRIG_MODE

#define ICR_RETRIG VIPER_ISA_IRQ_ICR_RETRIG

#else

#define ICR_RETRIG 0
// #define DO_STATUS_LOOP	// optional

#endif
/************ RETRIG_MODE end **********************************/

static unsigned short rtl_isa_irq_enabled_mask = 0;

struct isrData {
    isa_irq_handler_t handler;
    void* callbackPtr;
};

/* 
 * isa_isrs[] is an array of isrData structures, containing function
 * pointers to interrupt service routines for devices that are
 * connected to the ISA bus.
 * This implementation
 * doesn't support sharing ISA interrupts, only one ISR is called
 * for each interrupt.
 *
 * It could be enhanced to support multiple ISRs.
 * If one requires that rtl_request_isa_irq be called at init_module
 * time, then you could rtl_gpos_malloc entries in a list.
 * Otherwise, one could declare fixed arrays of ISRs (say 5) for
 * each IRQ.
 */

static struct isrData* isa_isrs[NUM_ISA_IRQS];

/* The requestable ISA irqs */
static int isa_irqs[NUM_ISA_IRQS] = { 3,4,5,6,7,10,11,12 };

/* number of requested ISA irqs */
static int nirqreq = 0;

/* vector of indices of requested ISA irqs */
static int irq_req[NUM_ISA_IRQS];

static rtl_spinlock_t irq_controller_lock;

/* -- Utility --------------------------------------------------------- */
static inline void rtl_mask_and_ack_isa_irq(unsigned int isa_irq_index)
{
    rtl_isa_irq_enabled_mask &= ~(1 << isa_irq_index);
    VIPER_ISA_IRQ_STATUS = 1 << isa_irq_index;
}
static inline void rtl_ack_isa_irq(unsigned int isa_irq_index)
{
    VIPER_ISA_IRQ_STATUS = 1 << isa_irq_index;
}
/* -- Utility --------------------------------------------------------- */
static inline void rtl_mask_isa_irq(unsigned int isa_irq_index)
{
    rtl_isa_irq_enabled_mask &= ~(1 << isa_irq_index);
}
/* -- Utility --------------------------------------------------------- */
static inline void rtl_unmask_isa_irq(unsigned int isa_irq_index)
{
    rtl_isa_irq_enabled_mask |= (1 << isa_irq_index);
}
/* -- RTLinux --------------------------------------------------------- */
static unsigned int rtl_isa_irq_demux_isr (unsigned int irq, struct rtl_frame *regs)
{
    unsigned short irq_status;
    int i, idx;
    unsigned short imask = 1;

// #define CHECK_REENTRANT
// #define COUNT_SPURIOUS_0
// #define COUNT_SPURIOUS

#ifdef CHECK_REENTRANT
    static int running = 0;
    static int n_reentrant = 0;
#endif

#ifdef COUNT_SPURIOUS_0
    static int spurious0 = 0;
#endif

#ifdef COUNT_SPURIOUS
    static int spurious = 0;
#endif

#ifdef CHECK_REENTRANT
    if (running) {
	if (!(n_reentrant++ % 1000))
		DSMLOG_DEBUG("demux running, n_reentrant=%d\n",
			n_reentrant);
    }
    running = 1;
#endif

    irq_status = VIPER_ISA_IRQ_STATUS & 0x00FF;

    if (!irq_status) {
#ifdef COUNT_SPURIOUS_0
	if (!(spurious0++ % 1000)) DSMLOG_DEBUG("%d ISA interrupts with status==0\n",
		spurious0);
#endif
#ifdef CHECK_REENTRANT
	running = 0;
#endif
	rtl_hard_enable_irq(irq);
	return 0;
    }


#ifdef DO_STATUS_LOOP
    /* Continue processing multiplexed ISA ISRs until all are processed... */
    do {
#endif
	rtl_spin_lock(&irq_controller_lock);
	for (i=0; i < nirqreq; i++) {
	    idx = irq_req[i];
	    imask = 1 << idx;
	    if (irq_status & imask) {
		/* Call the indicated ISR. */
		// if (isa_isrs[idx])
		(*isa_isrs[idx]->handler)(isa_irqs[i],
		    isa_isrs[idx]->callbackPtr,regs);

#ifdef CLEAR_STATUS_BIT
		VIPER_ISA_IRQ_STATUS = imask;	// clear the status bit
#endif
		irq_status &= ~imask;
	    }
	    if (!irq_status) break;		// done
	}
	rtl_spin_unlock(&irq_controller_lock);

#ifdef COUNT_SPURIOUS
	if (irq_status) {	// not completely cleared
	    if (!(spurious++ % 1))
		DSMLOG_DEBUG("%d spurious ISA interrupts, status=0x%x\n",
		    spurious,irq_status);
	}
#endif

#ifdef DO_STATUS_LOOP
#ifdef COUNT_SPURIOUS
	irq_status = VIPER_ISA_IRQ_STATUS & 0x00FF;
	if (irq_status != (irq_status & rtl_isa_irq_enabled_mask)) {
	    if (!(spurious++ % 1))
		    DSMLOG_DEBUG("%d spurious ISA interrupts, status=0x%x\n",
			spurious,irq_status);
	    irq_status &= rtl_isa_irq_enabled_mask;
	}
#else
	irq_status = VIPER_ISA_IRQ_STATUS & rtl_isa_irq_enabled_mask;
#endif
    } while (irq_status);
#endif

#ifdef CHECK_REENTRANT
    running = 0;
#endif
    // rtl_global_pend_irq(irq);
    rtl_hard_enable_irq(irq);

    return 0;
}

/**
 * Function callable by external modules to register an ISR
 * for an ISA interrupt.
 * Currently this must be called from your modules init_module
 * function since it does a rtl_gpos_malloc() here.
 * To remove that restriction, one could rtl_gpos_malloc all the
 * structures during this init_module, and initialize the function
 * pointers to 0.
 *
 * This implementation also doesn't support sharing ISA interrupts,
 * only one ISR registered for each interrupt.
 *
 * It could be enhanced to support multiple ISRs.
 * If one requires that rtl_request_isa_irq be called at init_module
 * time, then you could rtl_gpos_malloc entries in a list.
 * Otherwise, one could declare fixed arrays of ISRs (say 5) for
 * each IRQ. Or use a pool of entries.
 */
int rtl_request_isa_irq(unsigned int irq, isa_irq_handler_t handler, void* callbackPtr)
{
    int i;
    int ret = -EINVAL;		/* no such isa irq */
    unsigned long flags;

    rtl_spin_lock_irqsave(&irq_controller_lock,flags);
    for (i = 0; i < NUM_ISA_IRQS; i++) {
	if (isa_irqs[i] == irq) {

	    DSMLOG_DEBUG("requesting isa irq=%d, index=%d\n",irq,i);

	    ret = -EBUSY;
	    if (isa_isrs[i]) break;	// already requested (no sharing!)

	    ret = -ENOMEM;
	    isa_isrs[i] = rtl_gpos_malloc( sizeof(struct isrData) );
	    if (!isa_isrs[i]) break;

	    isa_isrs[i]->handler = handler;
	    isa_isrs[i]->callbackPtr = callbackPtr;

	    rtl_unmask_isa_irq(i);
	    ret = 0;
	    break;
	}
    }
    // keep vector of requested irq indexes, sorted by irq
    nirqreq = 0;
    for (i = 0; i < NUM_ISA_IRQS; i++)
	if (isa_isrs[i]) irq_req[nirqreq++] = i;

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
	    DSMLOG_DEBUG("freeing isa irq=%d\n",irq);
	    rtl_mask_isa_irq(i);
	    if (isa_isrs[i]) rtl_gpos_free(isa_isrs[i]);
	    isa_isrs[i] = NULL;
	    ret = 0;
	    break;
	}
    }
    nirqreq = 0;
    for (i = 0; i < NUM_ISA_IRQS; i++)
	if (isa_isrs[i]) irq_req[nirqreq++] = i;

    rtl_spin_unlock_irqrestore(&irq_controller_lock,flags);

    if (ret == -EINVAL) DSMLOG_ERR("can't free isa irq=%d\n",irq);
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
	    DSMLOG_NOTICE("IRQ %2d disabled\n",isa_irqs[i]);
	    rtl_mask_isa_irq(i);
	    rtl_gpos_free(isa_isrs[i]);
	    isa_isrs[i] = NULL;
	}
    }
    nirqreq = 0;
    rtl_spin_unlock_irqrestore(&irq_controller_lock,flags);

    /* release interrupt handler */
    rtl_free_irq(VIPER_CPLD_IRQ);
    DSMLOG_NOTICE("done\n");
}

/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  int i;
  int irq;

  rtl_spin_lock_init(&irq_controller_lock);

  // softwareVersion is found in dsm_version.h
  DSMLOG_NOTICE("version: %s\n",softwareVersion);

  for (i=0; i<NUM_ISA_IRQS; i++) {
    isa_isrs[i] = NULL;
    rtl_mask_isa_irq(i);
  }
  nirqreq = 0;

  VIPER_ISA_IRQ_ICR = ICR_AUTOCLR | ICR_RETRIG;
  set_GPIO_IRQ_edge(IRQ_TO_GPIO(VIPER_CPLD_IRQ), IRQ_EDGE);

#ifdef DEBUG
  DSMLOG_DEBUG("VIPER_ISA_IRQ_ICR setting=0x%x, reg=0x%x\n",
  	 ICR_AUTOCLR | ICR_RETRIG,VIPER_ISA_IRQ_ICR & 0xF);
#endif

  irq = rtl_map_gpos_irq(VIPER_CPLD_IRQ);	/* does nothing on viper */

  /* install a handler for the multiplexed interrupt */
  if ( rtl_request_irq(irq, rtl_isa_irq_demux_isr ) < 0 )
  {
    /* failed... */
    cleanup_module();
    DSMLOG_ERR("could not allocate IRQ %d\n",VIPER_CPLD_IRQ);
    return -EIO;
  }
  DSMLOG_DEBUG("allocated IRQ %d\n",irq);

  return 0;
}
