/*  gpio_mm driver

Driver and utility modules for Diamond System MM AT analog IO cards.

Copyright 2005 UCAR, NCAR, All Rights Reserved

Original author:	Gordon Maclean

Revisions:

*/

#include <linux/types.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/fs.h>           /* everything... */
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include <nidas/linux/diamond/gpio_mm.h>
#include <nidas/rtlinux/dsm_version.h>
// #define DEBUG
#include <nidas/linux/klog.h>
#include <nidas/linux/isa_bus.h>

/* SA_SHIRQ is deprecated starting in 2.6.22 kernels */
#ifndef IRQF_SHARED
# define IRQF_SHARED SA_SHIRQ
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
#define mutex_init(x)               init_MUTEX(x)
#define mutex_lock_interruptible(x) down_interruptible(x)
#define mutex_unlock(x)             up(x)
#endif

/* info string used in various places */
static const char* driver_name = "gpio-mm";

/* DIO ioport addresses of installed boards, 0=DIO not used */
static unsigned long ioports_dio[MAX_GPIO_MM_BOARDS] = { 0x040, 0, 0, 0 };

/* C/T ioport addresses of installed boards, 0=C/T not used */
static unsigned long ioports_ct[MAX_GPIO_MM_BOARDS] ={ 0x080, 0x1c0, 0x3c0, 0 };

/* number of GPIO_MM boards in system (number of non-zero ioport values) */
static int numboards_dio = 0;
static int numboards_ct = 0;
static int numboards = 0;

/* ISA irqs, required for each board. Can be shared. */
static int irqa[MAX_GPIO_MM_BOARDS] = { 3, 3, 3, 0 };
static int irqb[MAX_GPIO_MM_BOARDS] = { 0, 0, 0, 0 };
static int numirqa = 0;
static int numirqb = 0;

#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(ioports_dio,ulong,&numboards_dio,0);
module_param_array(ioports_ct,ulong,&numboards_ct,0);
module_param_array(irqa,int,&numirqa,0);
module_param_array(irqb,int,&numirqb,0);
#else
module_param_array(ioports_dio,ulong,numboards_dio,0);
module_param_array(ioports_ct,ulong,numboards_ct,0);
module_param_array(irqa,int,numirqa,0);
module_param_array(irqb,int,numirqb,0);
#endif

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_LICENSE("Dual BSD/GPL");

/*
 * Holds the major number of all GPIO_MM devices.
 */
static dev_t gpio_mm_device = MKDEV(0,0);

/*
 * Pointer to first of dynamically allocated structures containing
 * all data pertaining to the configured GPIO_MM boards on the system.
 */
static struct GPIO_MM* board = 0;

/* Greatest common divisor. Use euclidian algorimthm, thanks to Wikipedia */
static int gcd(unsigned int a, unsigned int b)
{
        if (b == 0) return a;
        return gcd(b,a % b);
}

/*********** Board Utility Functions *******************/
static void gpio_mm_set_master_mode(struct GPIO_MM* brd,int chip)
{
        BUG_ON(chip > 2);
        outb(CTS9513_DPTR_MASTER_MODE,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));

        /* least significant byte of master mode value */
        outb(brd->mmode_lsb[chip],
            brd->ct_addr + GPIO_MM_9513_DATA(chip));

        /* most significant byte of master mode value */
        outb(brd->mmode_msb[chip],
            brd->ct_addr + GPIO_MM_9513_DATA(chip));
}

/*
 * Setup a counter, with the given mode.
 * Does not spin_lock the board->reglock;
 * icntr: numbered from 0 to 9.
 */
static void gpio_mm_setup_counter(struct GPIO_MM* brd,
    int icntr,unsigned char lmode, unsigned char hmode)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        BUG_ON(chip > 2);

        /* point to counter mode */
        outb(CTS9513_DPTR_CNTR1_MODE+ic,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));

        /* least significant byte of counter mode value */
        outb(lmode,brd->ct_addr + GPIO_MM_9513_DATA(chip));

        /* most significant byte of counter mode value */
        outb(hmode,brd->ct_addr + GPIO_MM_9513_DATA(chip));
}

static void gpio_mm_arm_counter(struct GPIO_MM* brd,int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        /* disarm */
        outb(CTS9513_ARM + (1 << ic),
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
}

static void gpio_mm_load_counter(struct GPIO_MM* brd, int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        /* disarm */
        outb(CTS9513_LOAD + (1 << ic),
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
}

static void gpio_mm_load_arm_counter(struct GPIO_MM* brd, int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        /* disarm */
        outb(CTS9513_LOAD_ARM + (1 << ic),
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
}

static void gpio_mm_load_arm_counters(struct GPIO_MM* brd,
    int icntr1,int icntr2)
{
        /* load and arm counter 2 and 3 */
        int chip1 = icntr1 / GPIO_MM_CNTR_PER_CHIP;
        int chip2 = icntr2 / GPIO_MM_CNTR_PER_CHIP;
        if (chip1 == chip2)
            outb(CTS9513_LOAD_ARM + (1 << (icntr1 % GPIO_MM_CNTR_PER_CHIP)) +
                (1 << (icntr2 % GPIO_MM_CNTR_PER_CHIP)),
                brd->ct_addr + GPIO_MM_9513_PTR(chip1));
        else {
            outb(CTS9513_LOAD_ARM + (1 << (icntr1 % GPIO_MM_CNTR_PER_CHIP)),
                brd->ct_addr + GPIO_MM_9513_PTR(chip1));
            outb(CTS9513_LOAD_ARM + (1 << (icntr2 % GPIO_MM_CNTR_PER_CHIP)),
                brd->ct_addr + GPIO_MM_9513_PTR(chip2));
        }
}

static void gpio_mm_disarm_counter(struct GPIO_MM* brd, int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        /* disarm */
        outb(CTS9513_DISARM + (1 << ic),
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
}

static void gpio_mm_disarm_save_counter(struct GPIO_MM* brd, int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        /* disarm */
        outb(CTS9513_DISARM_SAVE + (1 << ic),
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
}

static void gpio_mm_disarm_counters(struct GPIO_MM* brd,
    int icntr1,int icntr2)
{
        /* load and arm counter 2 and 3 */
        int chip1 = icntr1 / GPIO_MM_CNTR_PER_CHIP;
        int chip2 = icntr2 / GPIO_MM_CNTR_PER_CHIP;
        if (chip1 == chip2)
            outb(CTS9513_DISARM + (1 << (icntr1 % GPIO_MM_CNTR_PER_CHIP)) +
                (1 << (icntr2 % GPIO_MM_CNTR_PER_CHIP)),
                brd->ct_addr + GPIO_MM_9513_PTR(chip1));
        else {
            outb(CTS9513_DISARM + (1 << (icntr1 % GPIO_MM_CNTR_PER_CHIP)),
                brd->ct_addr + GPIO_MM_9513_PTR(chip1));
            outb(CTS9513_DISARM + (1 << (icntr2 % GPIO_MM_CNTR_PER_CHIP)),
                brd->ct_addr + GPIO_MM_9513_PTR(chip2));
        }
}

static void gpio_mm_disarm_save_counters(struct GPIO_MM* brd,
    int icntr1,int icntr2)
{
        /* load and arm counter 2 and 3 */
        int chip1 = icntr1 / GPIO_MM_CNTR_PER_CHIP;
        int chip2 = icntr2 / GPIO_MM_CNTR_PER_CHIP;
        if (chip1 == chip2)
            outb(CTS9513_DISARM_SAVE + (1 << (icntr1 % GPIO_MM_CNTR_PER_CHIP)) +
                (1 << (icntr2 % GPIO_MM_CNTR_PER_CHIP)),
                brd->ct_addr + GPIO_MM_9513_PTR(chip1));
        else {
            outb(CTS9513_DISARM_SAVE + (1 << (icntr1 % GPIO_MM_CNTR_PER_CHIP)),
                brd->ct_addr + GPIO_MM_9513_PTR(chip1));
            outb(CTS9513_DISARM_SAVE + (1 << (icntr2 % GPIO_MM_CNTR_PER_CHIP)),
                brd->ct_addr + GPIO_MM_9513_PTR(chip2));
        }
}

static void gpio_mm_set_load_reg(struct GPIO_MM* brd,
    int icntr,unsigned short load)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;

        outb(CTS9513_DPTR_CNTR1_LOAD+ic,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
        outb(load & 0xff,brd->ct_addr + GPIO_MM_9513_DATA(chip));
        outb(load >> 8,brd->ct_addr + GPIO_MM_9513_DATA(chip));
}

static void gpio_mm_set_hold_reg(struct GPIO_MM* brd,
    int icntr,unsigned short hold)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;

        outb(CTS9513_DPTR_CNTR1_HOLD+ic,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
        outb(hold & 0xff,brd->ct_addr + GPIO_MM_9513_DATA(chip));
        outb(hold >> 8,brd->ct_addr + GPIO_MM_9513_DATA(chip));
}

static unsigned short gpio_mm_get_hold_reg(struct GPIO_MM* brd,
    int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        unsigned char hl,hh;

        outb(CTS9513_DPTR_CNTR1_HOLD+ic,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
        hl = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        hh = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        return (hh << 8) + hl;
}

static void gpio_mm_clear_output(struct GPIO_MM* brd, int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        outb(CTS9513_CLEAR_OUT + ic + 1,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
}

static int gpio_mm_get_output_status(struct GPIO_MM* brd,int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        unsigned char hl,hh;

        outb(CTS9513_DPTR_STATUS,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
        hl = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        hh = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        return hl & (2 << icntr);
}

/*
 * Compute greatest common divisor of requested timer periods.
 * Also return the largest timer period.
 */
static int compute_timer_ticks(struct list_head* cblist,
    unsigned int *maxUsecsp)
{
        struct gpio_timer_callback *cbentry;
        int a = 0;
        unsigned int maxUsecs = 0;
        list_for_each_entry(cbentry,cblist,list) {
                if (a == 0) a = cbentry->usecs;
                else a = gcd(a,cbentry->usecs);
                maxUsecs = max(cbentry->usecs,maxUsecs);
        }
        list_for_each_entry(cbentry,cblist,list)
                cbentry->tickModulus = cbentry->usecs / a;
        *maxUsecsp = maxUsecs;
        return a;
}

/*
 * Check that requested timer interval is within range.
 */
static int check_timer_interval(struct GPIO_MM*brd, unsigned int usecs)
{
        unsigned int tics;
        int scaler;
        /* overflows at usecs >= 107,374,182 or 107 seconds */
        if (2 * usecs > UINT_MAX / GPIO_MM_CT_CLOCK_HZ * USECS_PER_SEC) {
                KLOG_ERR("GPIO-MM board %d, usecs=%d is too large\n",
                    brd->num,usecs);
                return -EINVAL;
        }
        tics = GPIO_MM_CT_CLOCK_HZ / USECS_PER_SEC * usecs;

        /* Compute BCD scaler of F1 40MHz clock.
         * Initially we may wait as much as two periods.
         */
        for (scaler = 1; scaler <= 10000; scaler *= 10) {
            if (2 * tics / scaler <= 0xFFFF) break;
        }
        if (2 * tics / scaler > 0xFFFF) {
                KLOG_ERR("GPIO-MM board %d, usecs=%d is too large, tics=%d\n",
                    brd->num,usecs,tics);
                return -EINVAL;
        }
        return 0;
}
/*
 * Called from software interrupt and user context.
 */
static void start_gpio_timer(struct GPIO_MM_timer* timer,
    unsigned int usecs, unsigned int maxUsecs)
{
        /* counter 1 is mode=D, baud rate generator */
        struct GPIO_MM* brd = timer->brd;
        unsigned char lmode;
        unsigned char hmode;
        unsigned int tics;
        unsigned long flags;
        int tc = 0;         /* timer is counter 1 */
        struct timeval tv;
        unsigned int initial_tics,initial_usecs;
        int scaler;

        /* overflow of this should have been checked already 
         * by check_timer_interval.
         */
        tics = GPIO_MM_CT_CLOCK_HZ / USECS_PER_SEC * usecs;

        /* Compute BCD scaler of F1 40MHz clock.
         * Initially we may wait as much as two periods.
         */
        for (scaler = 1; scaler <= 10000; scaler *= 10) {
            if (2 * tics / scaler <= 0xFFFF) break;
            hmode++;
        }
        tics /= scaler;

        spin_lock_irqsave(&brd->reglock,flags);

        /* disable interrupt A */
        outb(0x02,brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS);

        KLOG_INFO("gpio %d: usecs=%d,scaler=%d,tics=%d\n",
                    brd->num,usecs,scaler,tics);

        /* timer is mode D: Rate Generator with No Hardware Gating */
        lmode = CTS9513_CML_OUT_LOW |
                CTS9513_CML_CNT_DN |
                CTS9513_CML_CNT_BIN |
                CTS9513_CML_REPEAT |
                CTS9513_CML_RELOAD_LOAD |
                CTS9513_CML_GATE_NORETRIG;
        hmode = CTS9513_CMH_SRC_F1 |
                CTS9513_CMH_EDGE_RISING |
                CTS9513_CMH_NO_GATE;
        gpio_mm_setup_counter(brd,tc,lmode,hmode);

        /* IRQA source is counter/timer 10 output */
        outb(0x09,brd->ct_addr + GPIO_MM_IRQ_SRC);
        /* clear and enable interrupt A */
        outb(0x05,brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS);

        /* determine initial amount of time to count so that
         * we are (somewhat) in sync with the system clock.
         */
        do_gettimeofday(&tv);
        initial_usecs = 2 * usecs - (tv.tv_usec % usecs);
        initial_tics = GPIO_MM_CT_CLOCK_HZ / USECS_PER_SEC * initial_usecs /
            scaler;
        timer->tickLimit = maxUsecs / usecs;    // rollover
        timer->tick =
            ((tv.tv_usec + initial_usecs - usecs) / usecs) % timer->tickLimit;

        timer->irqsReceived = 0;

        gpio_mm_set_load_reg(brd,tc,initial_tics);
        gpio_mm_load_counter(brd,tc);

        gpio_mm_set_load_reg(brd,tc,tics);
        gpio_mm_arm_counter(brd,tc);

        spin_unlock_irqrestore(&brd->reglock,flags);

        timer->usecs = usecs;
        atomic_set(&timer->running,1);
}

/*
 * Called from software interrupt and user context.
 */
static void stop_gpio_timer(struct GPIO_MM_timer* timer)
{
        struct GPIO_MM* brd = timer->brd;
        unsigned long flags;

        spin_lock_irqsave(&brd->reglock,flags);
        gpio_mm_disarm_counter(brd,0);

        /* disable interrupt A */
        outb(0x02,brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS);
        spin_unlock_irqrestore(&brd->reglock,flags);

        atomic_set(&timer->running,0);
        timer->usecs = 0;
}

/*
 * Handle pending adds and removes of callbacks from the active list.
 * This should only be called from the timer bottom half tasklet function.
 */
static void handlePendingCallbacks(struct GPIO_MM_timer* timer)
{
        struct list_head *ptr,*ptr2;
        struct gpio_timer_callback *cbentry,*cbentry2;
        int i;
        unsigned int usecs = 0,maxUsecs;

        spin_lock(&timer->callbackLock);

        /* Remove pending callbacks from the active lists. */
        for (i = 0; i < timer->nPendingRemoves; i++) {
                cbentry = timer->pendingRemoves[i];
                /* remove entry from the active list for the rate */
                list_del(&cbentry->list);
                /* and add back to the pool. */
                list_add(&cbentry->list, &timer->callbackPool);
        }
        timer->nPendingRemoves = 0;
        /* Adding pending callbacks to the sorted active list. */
        for (ptr = timer->pendingAdds.next; ptr != &timer->pendingAdds;) {
                cbentry = list_entry(ptr, struct gpio_timer_callback, list);
                ptr = ptr->next;
                /* remove entry from pendingAdds list */
                list_del(&cbentry->list);
                /* add to sorted active list */
                for (ptr2 = timer->callbackList.next; 
                        ptr2 != &timer->callbackList; ptr2 = ptr2->next) {
                        cbentry2 =
                            list_entry(ptr2, struct gpio_timer_callback, list);
                        if (cbentry->usecs < cbentry2->usecs) {
                                list_add(&cbentry->list,ptr2->prev);
                                break;
                        }
                }
                if (ptr2 == &timer->callbackList)
                        list_add(&cbentry->list, ptr2);
        }
        /*
         * determine sleep period, the greatest common divisor.
         * If it is a different sleep period than the existing:
         *   if timer is running, stop_gpio_timer
         *   if new sleep period is != 0, start_gpio_timer
         */
        usecs = compute_timer_ticks(&timer->callbackList,&maxUsecs);
        if (usecs != timer->usecs) {
                if (timer->usecs != 0) stop_gpio_timer(timer);
                if (usecs != 0) start_gpio_timer(timer,usecs,maxUsecs);
        }
        atomic_set(&timer->callbacksChanged,0);
        wake_up_interruptible(&timer->callbackWaitQ);
        spin_unlock(&timer->callbackLock);
}

/*
 * Cleanup function that un-registers all callbacks, and kfrees the pool.
 * This should only be called if one is sure that the timer bottom
 * half is *not* running.
 */
static void free_timer_callbacks(struct GPIO_MM_timer* timer)
{
        struct list_head *ptr,*ptr2;
        struct gpio_timer_callback *cbentry,*cbentry2;

        spin_lock_bh(&timer->callbackLock);

        list_for_each_safe(ptr,ptr2,&timer->callbackList) {
                /* remove it from the active list and add to the pool. */
                list_del(ptr);
                list_add(ptr, &timer->callbackPool);
        }

        list_for_each_entry_safe(cbentry,cbentry2,&timer->callbackPool,list) {
                list_del(&cbentry->list);
                kfree(cbentry);
        }
        atomic_set(&timer->callbacksChanged,0);
        wake_up_interruptible(&timer->callbackWaitQ);
        spin_unlock_bh(&timer->callbackLock);
}

/*
 * Timer bottom half tasklet function that is invoked after receipt
 * of a timer interrupt.  Invokes callbacks that have been registered.
 * Note that this does not hold locks when the
 * callbacks are being called, because we want to allow
 * callbacks to unregister themselves.
 */
static void gpio_mm_timer_bottom_half(unsigned long dev)
{
        struct GPIO_MM_timer* timer = (struct GPIO_MM_timer*) dev;
        struct gpio_timer_callback *cbentry;

        if (atomic_read(&timer->callbacksChanged))
                handlePendingCallbacks(timer);

        list_for_each_entry(cbentry,&timer->callbackList,list) {
                if (!(timer->tick % cbentry->tickModulus))
                    cbentry->callbackFunc(cbentry->privateData);
        }

}

/*
 * IRQ handler for timer interrupts on the board.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
static irqreturn_t gpio_mm_timer_irq_handler(int irq, void* dev_id)
#else
static irqreturn_t gpio_mm_timer_irq_handler(int irq, void* dev_id, struct pt_regs *regs)
#endif
{
        struct GPIO_MM* brd = (struct GPIO_MM*) dev_id;
        struct GPIO_MM_timer* timer = brd->timer;
        unsigned char status;
        unsigned int tick;

        spin_lock(&brd->reglock);
        status = inb(brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS) & 0x01;

        if (!status) {      // not my interrupt
                spin_unlock(&brd->reglock);
                return IRQ_NONE;
        }

        // acknowledge interrupt
        outb(0x1,brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS);

        spin_unlock(&brd->reglock);

        timer->irqsReceived++;

        tick = (timer->tick + 1) % timer->tickLimit;
        barrier();
        timer->tick = tick;

        /* nudge bottom half */
        tasklet_schedule(&timer->tasklet);

        return IRQ_HANDLED;
}

/*
 * Request that a handler be registered for the given interrupt.
 * Set irq_ab=0  for interrupt A, and irq_ab=1 for interrupt B.
 * If an interrupt handler has already been set up for this
 * board, then do nothing.
 * Called from user context or module initialization.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
static int gpio_mm_add_irq_user(struct GPIO_MM* brd,int irq_ab,
    irq_handler_t handler)
#else
static int gpio_mm_add_irq_user(struct GPIO_MM* brd,int irq_ab,
    irqreturn_t(*handler)(int,void*, struct pt_regs*))
#endif
{
        int result;
        if ((result = mutex_lock_interruptible(&brd->brd_mutex)))
                return result;
        if (brd->irq_users[irq_ab] == 0) {
                int irq = brd->irqs[irq_ab];
                if (irq == 0) return -EINVAL;
                BUG_ON(brd->reqirqs[irq_ab] != 0);

                /* We don't use SA_INTERRUPT flag here.  We don't
                 * need to block other interrupts while we're running.
                 * Note: request_irq can wait, so spin_lock not advised.
                 */
#ifdef GET_SYSTEM_ISA_IRQ
                irq = GET_SYSTEM_ISA_IRQ(irq);
#endif
                KLOG_INFO("board %d: requesting irq: %d,%d\n",
                    brd->num,brd->irqs[irq_ab],irq);
                result = request_irq(irq,handler,IRQF_SHARED,driver_name,brd);
                if (result) {
                        mutex_unlock(&brd->brd_mutex);
                        return result;
                }
                brd->reqirqs[irq_ab] = irq;
        }
        else BUG_ON(brd->irq_users[irq_ab] <= 0);
        brd->irq_users[irq_ab]++;
        mutex_unlock(&brd->brd_mutex);
        return result;
}

/*
 * Called from user context or module shutdown.
 */
static int gpio_mm_remove_irq_user(struct GPIO_MM* brd,int irq_ab)
{
        int result;
        if ((result = mutex_lock_interruptible(&brd->brd_mutex)))
                return result;
        brd->irq_users[irq_ab]--;
        if (brd->irq_users[irq_ab] == 0) {
                KLOG_NOTICE("freeing irq %d\n",brd->irqs[irq_ab]);
                free_irq(brd->reqirqs[irq_ab],brd);
                brd->irqs[irq_ab] = 0;
        }
        mutex_unlock(&brd->brd_mutex);
        return result;
}

static int gpio_mm_add_timer_irq_user(struct GPIO_MM* brd)
{
    return gpio_mm_add_irq_user(brd,0,gpio_mm_timer_irq_handler);
}

static int gpio_mm_remove_timer_irq_user(struct GPIO_MM* brd)
{
    return gpio_mm_remove_irq_user(brd,0);
}

/*
 * Reserve one or more counters on a GPIO board,
 * from ic1 to icn inclusive, numbered from 0.
 * Called from user context.
 */
static int gpio_mm_reserve_cntrs(struct GPIO_MM* brd,int ic1,int icn)
{
        int result = 0,ic;
        if ((result = mutex_lock_interruptible(&brd->brd_mutex)))
                return result;
        /* check if counters are available */
        for (ic = ic1; ic <= icn; ic++) {
                if (brd->cntr_used[ic]) {
                        KLOG_ERR("board %d: counter %d is already in use\n",
                            brd->num,ic);
                        result = -EBUSY;
                }
        }
        if (!result)
                for (ic = ic1; ic <= icn; ic++) brd->cntr_used[ic] = 1;

        mutex_unlock(&brd->brd_mutex);
        return result;
}

/*
 * Deallocate one or more counters to a device.
 * Called from user context.
 */
static int gpio_mm_free_cntrs(struct GPIO_MM* brd,int ic0,int icn)
{
        int result = 0,ic;
        if ((result = mutex_lock_interruptible(&brd->brd_mutex)))
                return result;
        for (ic = ic0; ic <= icn; ic++) brd->cntr_used[ic] = 0;

        mutex_unlock(&brd->brd_mutex);
        return result;
}

/*
 * Get the counter number (0-9) of the pulse counter for the frequency
 * counter.
 */
static int get_pulse_counter(struct GPIO_MM_fcntr* fcntr) 
{
#ifdef ENABLE_TIMER_ON_EACH_BOARD
        return fcntr->num * GPIO_MM_CNTR_PER_FCNTR + 1;   /* numbered from 0 */
#else
        return fcntr->num * GPIO_MM_CNTR_PER_FCNTR +
            ((fcntr->brd->num == 0) ? 1 : 0);
#endif
}

/*
 * Do the hardware steps necessary to setup the frequency counter.
 * Called from user context.
 */
static int gpio_mm_setup_fcntr(struct GPIO_MM_fcntr* fcntr)
{
        struct GPIO_MM* brd = fcntr->brd;
        unsigned char lmode;
        unsigned char hmode;
        int pc = get_pulse_counter(fcntr);
        int tc1 = pc + 1;
        int tc2 = tc1 + 1;
        unsigned long flags;
        int result;

        /* Check that counters are available */
        result = gpio_mm_reserve_cntrs(brd,pc,tc2);
        if (result) return result;

        spin_lock_irqsave(&brd->reglock,flags);

        /* configure pulse counter
         * mode=G: Software triggered, delayed pulse, one shot
         * load=1,hold=numPulses, toggle on TC.
         */
        lmode = CTS9513_CML_OUT_TC_TOGGLE |
                CTS9513_CML_CNT_DN |
                CTS9513_CML_CNT_BIN |
                CTS9513_CML_ONCE |
                CTS9513_CML_RELOAD_BOTH |
                CTS9513_CML_GATE_NORETRIG;
        hmode = (CTS9513_CMH_SRC_S1 + fcntr->num) |
                CTS9513_CMH_EDGE_RISING |
                CTS9513_CMH_NO_GATE;
        gpio_mm_setup_counter(brd,pc,lmode,hmode);

        /* count one initial pulse, then raise output toggle,
         * lower output toggle after numPulses.
         */
        gpio_mm_set_load_reg(brd,pc,1);
        gpio_mm_set_hold_reg(brd,pc,fcntr->numPulses);

        /* clear output toggle, which is the gate for the next counter */
        gpio_mm_clear_output(brd,pc);

        /* Low order 16 bits of the 40MHz tic counter.
         * mode=E, Rate Generator with Level Gating. */
        /* configure least significant word */
        lmode = CTS9513_CML_OUT_HIGH_ON_TC |
                CTS9513_CML_CNT_UP |
                CTS9513_CML_CNT_BIN |
                CTS9513_CML_REPEAT |
                CTS9513_CML_RELOAD_LOAD |
                CTS9513_CML_GATE_NORETRIG;
        /* source F1 is the 40 MHz clock.
         * Gate with terminal count of previous counter, the pulse counter.
         */
        hmode = CTS9513_CMH_SRC_F1 |
                CTS9513_CMH_EDGE_RISING |
                CTS9513_CMH_GATE_HI_TCNM1;
        gpio_mm_setup_counter(brd,tc1,lmode,hmode);
        gpio_mm_set_load_reg(brd,tc1,0);

        /* High order 16 bits of the 40MHz tic counter.
         * mode=A, Software Triggered with no Gating.
         */
        /* configure least significant word */
        lmode = CTS9513_CML_OUT_LOW |
                CTS9513_CML_CNT_UP |
                CTS9513_CML_CNT_BIN |
                CTS9513_CML_ONCE |
                CTS9513_CML_RELOAD_LOAD |
                CTS9513_CML_GATE_NORETRIG;
        /* source is the previous counter */
        hmode = CTS9513_CMH_SRC_TCNM1 |
                CTS9513_CMH_EDGE_RISING |
                CTS9513_CMH_NO_GATE;
        gpio_mm_setup_counter(brd,tc2,lmode,hmode);
        gpio_mm_set_load_reg(brd,tc2,0);

        /* load and arm 40Hz tic counters */
        gpio_mm_load_arm_counters(brd,tc1,tc2);
        /* load and arm pulse counter */
        gpio_mm_load_arm_counter(brd,pc);

        spin_unlock_irqrestore(&brd->reglock,flags);
        return result;
}

/*
 * Do the hardware steps necessary to stop the frequency counter.
 * Called from user context.
 */
static int gpio_mm_stop_fcntr(struct GPIO_MM_fcntr* fcntr)
{
        struct GPIO_MM* brd = fcntr->brd;
        int pc = get_pulse_counter(fcntr);
        int tc1 = pc + 1;
        int tc2 = tc1 + 1;
        unsigned long flags;
        int result;

        spin_lock_irqsave(&brd->reglock,flags);
        /* disarm 40Hz tic counters */
        gpio_mm_disarm_counters(brd,tc1,tc2);
        /* disarm pulse counter */
        gpio_mm_disarm_counter(brd,pc);
        spin_unlock_irqrestore(&brd->reglock,flags);

        result = gpio_mm_free_cntrs(brd,pc,tc2);
        return result;
}

/* 
 * Here's where the actual work of the frequency counter actually
 * get's done.  This function is called periodically by the
 * gpio_timer in sofware interrupt context.
 * 1. Disarm and save the values of the three counters to their hold
 *    registers.
 * 2. Get the values of the hold registers.
 * 3. Look at the value of the pulse counter. If it finished counting
 *    numPulses, then it will have reloaded itself from the
 *    load register value, which is 1.  Otherwise it
 *    will be the number of pulses left to count. One can check
 *    its output state to see if it is finished.
 * 4. Determine the total number of 40 MHz tics from the other 2 counter
 *    values.
 * 5. Create a data sample, put it in the circular buffer, notify the
 *    reader wait queue.
 * 6. Re-arm the counters.
 */
static void fcntr_timer_callback_func(void *privateData)
{
        struct GPIO_MM_fcntr* fcntr = (struct GPIO_MM_fcntr*) privateData;
        struct GPIO_MM* brd = fcntr->brd;
        int pc = get_pulse_counter(fcntr);
        int tc1 = pc + 1;
        int tc2 = tc1 + 1;
        unsigned short pcn;
        unsigned short tc1n;
        unsigned short tc2n;
        unsigned int tcn;
        int pcstatus;
        unsigned long flags;
        struct freq_sample* samp;

        spin_lock_irqsave(&brd->reglock,flags);

        gpio_mm_disarm_save_counters(brd, tc1,tc2);
        gpio_mm_disarm_save_counter(brd,pc);

        /* If pulse counter has finished counting numPulses, then
         * its output should be low.
         */
        pcstatus = gpio_mm_get_output_status(brd,pc);

        pcn = gpio_mm_get_hold_reg(brd,pc);
        tc1n = gpio_mm_get_hold_reg(brd,tc1);
        tc2n = gpio_mm_get_hold_reg(brd,tc2);
        tcn = (tc2n << 16) + tc1n;

        samp = (struct freq_sample*)
            GET_HEAD(fcntr->samples,GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE);
        if (!samp) {                // no output sample available
                fcntr->status.lostSamples++;
                KLOG_WARNING("%s: lostSamples=%d\n",
                       fcntr->deviceName,fcntr->status.lostSamples);
        }
        else {
                samp->timetag = getSystemTimeTMsecs();
                samp->length = 2 * sizeof(int);
                samp->ticks40Mhz = tcn;
                 
                if (!pcstatus)  {   // all pulses counted
                        if (pcn != 1) {
                                KLOG_WARNING(
                            "%s: pcn should be 1, instead it is %d\n",
                                fcntr->deviceName,pcn);
                                samp->pulses = fcntr->numPulses - pcn;
                        }
                        else samp->pulses = fcntr->numPulses;
                }
                else {
                        fcntr->status.pulseUnderflow++;
                        samp->pulses = fcntr->numPulses - pcn;
                }
                // todo: implement buffering and latency
                INCREMENT_HEAD(fcntr->samples,GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE);
                wake_up_interruptible(&fcntr->rwaitq);
        }

        gpio_mm_set_hold_reg(brd,pc,fcntr->numPulses);

        /* clear output toggle, which is the gate for the next counter */
        if (pcstatus) gpio_mm_clear_output(brd,pc);

        /* load and arm 40Hz tic counters */
        gpio_mm_load_arm_counters(brd,tc1,tc2);
        /* load and arm pulse counter */
        gpio_mm_load_arm_counter(brd,pc);

        spin_unlock_irqrestore(&brd->reglock,flags);
}

/*
 * Called from user context.
 */
static int start_fcntr(struct GPIO_MM_fcntr* fcntr,
    struct GPIO_MM_fcntr_config* cfg)
{
        int result;
#ifdef ENABLE_TIMER_ON_EACH_BOARD
        struct GPIO_MM* brd = fcntr->brd;
#endif

        fcntr->latencyMsecs = cfg->latencyUsecs / USECS_PER_MSEC;
        fcntr->outputPeriodUsec = cfg->outputPeriodUsec;
        fcntr->numPulses = cfg->numPulses;

        /* setup the counters */
        result = gpio_mm_setup_fcntr(fcntr);
        if (result) return result;

        fcntr->samples.head = fcntr->samples.tail = 0;
        memset(&fcntr->read_state, 0, sizeof (struct sample_read_state));

#ifdef ENABLE_TIMER_ON_EACH_BOARD
        fcntr->timer_callback = register_gpio_timer_callback_priv(
               brd->timer,fcntr_timer_callback_func,fcntr->outputPeriodUsec,
               fcntr,&result);
#else
        fcntr->timer_callback = register_gpio_timer_callback(
               fcntr_timer_callback_func,fcntr->outputPeriodUsec,
               fcntr,&result);
#endif
        if (!fcntr->timer_callback) return result;
        return result;
}

/*
 * Called from user context.
 */
static int stop_fcntr(struct GPIO_MM_fcntr* fcntr)
{
        int result;
        long jleft;
#ifdef ENABLE_TIMER_ON_EACH_BOARD
        struct GPIO_MM* brd = fcntr->brd;
#endif
        result = gpio_mm_stop_fcntr(fcntr);
        if (result) return result;

        if (fcntr->timer_callback) {
#ifdef ENABLE_TIMER_ON_EACH_BOARD
                jleft = unregister_gpio_timer_callback_priv(
                       brd->timer,fcntr->timer_callback,1);
#else
                jleft = unregister_gpio_timer_callback(
                        fcntr->timer_callback,1);
#endif
                if (jleft == 0) {
                        KLOG_WARNING(
                            "%s: timeout on unregister_gpio_timer_callback\n",
                            fcntr->deviceName);
                        result = -EBUSY;
                }
                else fcntr->timer_callback = 0;
        }
        return result;
}

/************ Frequency Counter File Operations ****************/
static int gpio_mm_open_fcntr(struct inode *inode, struct file *filp)
{
        int i = iminor(inode);
        int ibrd = i / GPIO_MM_MINORS_PER_BOARD;
        int ifcntr = i % GPIO_MM_MINORS_PER_BOARD;

        struct GPIO_MM* brd;
        struct GPIO_MM_fcntr* fcntr;
        int result = 0;

        KLOG_DEBUG("open_fcntr, minor=%d,ibrd=%d,ifcntr=%d,numboards_ct=%d\n",
            minor,ibrd,ifcntr,numboards_ct);

        if (ibrd >= numboards_ct || ioports_ct[ibrd] == 0) return -ENXIO;
        if (ifcntr >= GPIO_MM_FCNTR_PER_BOARD) return -ENXIO;

        brd = board + ibrd;
        fcntr = brd->fcntrs + ifcntr;

        if (atomic_inc_return(&fcntr->num_opened) == 1) {
                fcntr->samples.head = fcntr->samples.tail = 0;
                memset(&fcntr->read_state, 0, sizeof (struct sample_read_state));
        }
        filp->private_data = fcntr;

        return result;
}

static int gpio_mm_release_fcntr(struct inode *inode, struct file *filp)
{
        struct GPIO_MM_fcntr* fcntr = (struct GPIO_MM_fcntr*)
            filp->private_data;

        int i = iminor(inode);
        int ibrd = i / GPIO_MM_MINORS_PER_BOARD;
        int ifcntr = i % GPIO_MM_MINORS_PER_BOARD;

        struct GPIO_MM* brd;

        if (ibrd >= numboards_ct || ioports_ct[ibrd] == 0) return -ENXIO;
        if (ifcntr >= GPIO_MM_FCNTR_PER_BOARD) return -ENXIO;

        brd = board + ibrd;
        BUG_ON(fcntr != brd->fcntrs + ifcntr);

        atomic_dec(&fcntr->num_opened);
        return 0;
}

static ssize_t gpio_mm_read_fcntr(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct GPIO_MM_fcntr* fcntr = (struct GPIO_MM_fcntr*)
                filp->private_data;

        return nidas_circbuf_read(filp,buf,count,
            &fcntr->samples,&fcntr->read_state,
            &fcntr->rwaitq);
}

static int gpio_mm_ioctl_fcntr(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
        struct GPIO_MM_fcntr* fcntr = (struct GPIO_MM_fcntr*)
            filp->private_data;

        int result = -EINVAL,err = 0;
        void __user *userptr = (void __user *) arg;

         /* don't even decode wrong cmds: better returning
          * ENOTTY than EFAULT */
        if (_IOC_TYPE(cmd) != GPIO_MM_IOC_MAGIC) return -ENOTTY;
        if (_IOC_NR(cmd) > GPIO_MM_IOC_MAXNR) return -ENOTTY;

        /*
         * the type is a bitmask, and VERIFY_WRITE catches R/W
         * transfers. Note that the type is user-oriented, while
         * verify_area is kernel-oriented, so the concept of "read" and
         * "write" is reversed
         */
        if (_IOC_DIR(cmd) & _IOC_READ)
                err = !access_ok(VERIFY_WRITE, userptr,
                    _IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                err =  !access_ok(VERIFY_READ, userptr,
                    _IOC_SIZE(cmd));
        if (err) return -EFAULT;

        switch (cmd)
        {
        case GPIO_MM_FCNTR_START:
                {
                struct GPIO_MM_fcntr_config cfg;
                if (copy_from_user(&cfg,userptr,
                        sizeof(struct GPIO_MM_fcntr_config))) return -EFAULT;
                result = start_fcntr(fcntr,&cfg);
                }
                break;
        case GPIO_MM_FCNTR_STOP:      /* user set */
                result = stop_fcntr(fcntr);
                break;
        case GPIO_MM_FCNTR_GET_STATUS:	/* user get of status struct */
                if (copy_to_user(userptr,&fcntr->status,
                    sizeof(struct GPIO_MM_fcntr_status))) return -EFAULT;
                result = 0;
                break;
        default:
                result = -ENOTTY;
                break;
        }
        return result;
}

/*
 * Implementation of poll fops.
 */
unsigned int gpio_mm_poll_fcntr(struct file *filp, poll_table *wait)
{
        struct GPIO_MM_fcntr* fcntr = (struct GPIO_MM_fcntr*)
            filp->private_data;
        unsigned int mask = 0;

        poll_wait(filp, &fcntr->rwaitq, wait);
        if (sample_remains(&fcntr->read_state) ||
            fcntr->samples.head != fcntr->samples.tail)
                mask |= POLLIN | POLLRDNORM;    /* readable */
        return mask;
}

static struct file_operations fcntr_fops = {
        .owner   = THIS_MODULE,
        .read    = gpio_mm_read_fcntr,
        .poll    = gpio_mm_poll_fcntr,
        .open    = gpio_mm_open_fcntr,
        .ioctl   = gpio_mm_ioctl_fcntr,
        .release = gpio_mm_release_fcntr,
};

static int cleanup_fcntrs(struct GPIO_MM* brd)
{
        int result = 0;
        struct GPIO_MM_fcntr* fcntrs = brd->fcntrs;
        int ic;
        if (!fcntrs) return 0;
        for (ic = 0; ic < GPIO_MM_FCNTR_PER_BOARD; ic++) {
                struct GPIO_MM_fcntr* fcntr = fcntrs + ic;
                result = stop_fcntr(fcntr);
                cdev_del(&fcntr->cdev);
                free_dsm_circ_buf(&fcntr->samples);
        }
        kfree(fcntrs);
        return result;
}
static int init_fcntrs(struct GPIO_MM* brd)
{
        int result;
        struct GPIO_MM_fcntr* fcntrs;
        dev_t devno;
        int ic;

        result = -ENOMEM;
        brd->fcntrs = fcntrs =
            kmalloc(sizeof(struct GPIO_MM_fcntr) * GPIO_MM_FCNTR_PER_BOARD,
                GFP_KERNEL);
        if (!fcntrs) return result;
        memset(fcntrs,0,
            sizeof(struct GPIO_MM_fcntr) * GPIO_MM_FCNTR_PER_BOARD);

        for (ic = 0; ic < GPIO_MM_FCNTR_PER_BOARD; ic++) {

                struct GPIO_MM_fcntr* fcntr = fcntrs + ic;
                fcntr->brd = brd;
                fcntr->num = ic;

                // for informational messages only at this point
                sprintf(fcntr->deviceName,"/dev/gpiomm_fcntr%d",
                    brd->num * GPIO_MM_FCNTR_PER_BOARD + ic);

                /* setup fcntr device */
                cdev_init(&fcntr->cdev,&fcntr_fops);
                fcntr->cdev.owner = THIS_MODULE;
                /* minor numbers are 0,1,2 for first board,  20,21,22 for 2nd */
                devno = MKDEV(MAJOR(gpio_mm_device),
                    brd->num*GPIO_MM_MINORS_PER_BOARD + ic);
                KLOG_DEBUG("%s: MKDEV, major=%d minor=%d\n",
                    fcntr->deviceName,MAJOR(devno),MINOR(devno));

                atomic_set(&fcntr->num_opened,0);

                /*
                 * Output samples. Data portion is 2 integers.
                 */
                result = alloc_dsm_circ_buf(&fcntr->samples,
                    2 * sizeof(int),GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE);
                if (result) return result;

                init_waitqueue_head(&fcntr->rwaitq);

                /* After calling cdev_all the device is "live"
                 * and ready for user operation.
                 */
                result = cdev_add(&fcntr->cdev, devno, 1);
                if (result) return result;
        }
        return result;
}

static struct gpio_timer_callback *register_gpio_timer_callback_priv(
    struct GPIO_MM_timer* timer,gpio_timer_callback_func_t callback,
        unsigned int usecs, void *privateData, int *errp)
{
        struct list_head *ptr;
        struct gpio_timer_callback *cbentry;
        int result;

        *errp = 0;
        if (usecs == 0) {
                *errp = -EINVAL;
                return 0;
        }
        result =  check_timer_interval(timer->brd,usecs);
        if (result) {
                *errp = result;
                return 0;
        }

        spin_lock_bh(&timer->callbackLock);

        ptr = timer->callbackPool.next;
        if (ptr == &timer->callbackPool) {     /* none left */
                spin_unlock_bh(&timer->callbackLock);
                *errp = -ENOMEM;
                return 0;
        }

        cbentry = list_entry(ptr, struct gpio_timer_callback, list);
        list_del(&cbentry->list);

        cbentry->callbackFunc = callback;
        cbentry->privateData = privateData;
        cbentry->usecs = usecs;
        list_add(&cbentry->list, &timer->pendingAdds);

        if (!atomic_read(&timer->running)) {
                /* First callback registered, start timer */
                cbentry->tickModulus = 1;
                start_gpio_timer(timer,usecs,usecs);
        }

        atomic_set(&timer->callbacksChanged,1);
        spin_unlock_bh(&timer->callbackLock);
        return cbentry;
}

/*
 * Call this function to un-register a callback.
 * A callback function can unregister itself, or other callback functions.
 * wait=1: do a wait_event_interruptible_timeout until it is certain
 *          that the callback will not be called again by the timer.
 *          The timeout is one second.
 * wait=0: don't wait. The callback may be called once more
 *          after this call to unregister_gpio_timer_callback
 * return:
 *   if wait==0, return will be 0.
 *   if wait != 0, the return will be value of
 *     wait_event_interruptible_timeout(). The return will be 0
 *     if the callback function was never deactivated,
 *     meaning the 1 second timeout occured while waiting
 *     for the callback to be deactivated
 *     Or, if the callback was deactivated, the return is the
 *     positive number of jiffies (1/HZ seconds) that remained
 *     in the timeout.
 */
static long unregister_gpio_timer_callback_priv(struct GPIO_MM_timer* timer,
    struct gpio_timer_callback *cb,int wait)
{
        long ret = 0;

        spin_lock_bh(&timer->callbackLock);
        timer->pendingRemoves[timer->nPendingRemoves++] = cb;
        atomic_set(&timer->callbacksChanged,1);
        spin_unlock_bh(&timer->callbackLock);

        if (wait)
            // check the return of this. Is it negative if interrupted?
            ret = wait_event_interruptible_timeout(timer->callbackWaitQ,
                    atomic_read(&timer->callbacksChanged) == 0,HZ);

        return ret;
}

/*
 * Module function that allows other modules to register their callback
 * function to be called at the given rate. This registers with the
 * timer on the first GPIO-MM board in the system.
 * register_gpio_timer_callback can be called at anytime, even from 
 * interrupt context.
 * register_gpio_timer_callback and unregister_gpio_timer_callback can be
 * called within a callback function itself.
 */
struct gpio_timer_callback *register_gpio_timer_callback(
    gpio_timer_callback_func_t callback,unsigned int usecs,
                     void *privateData, int *errp)
{
        struct GPIO_MM_timer* timer = board->timer;
        return register_gpio_timer_callback_priv(timer,callback,usecs,
            privateData,errp);
}
EXPORT_SYMBOL(register_gpio_timer_callback);

/*
 * External modules call this function to un-register their callbacks.
 * A callback function can unregister itself, or any other
 * callback function.  This unregisters with the
 * timer on the first GPIO-MM board in the system.
 * wait=1: do a wait_event_interruptible_timeout until it is certain
 *          that the callback will not be called again by the timer.
 *          The timeout is one second.
 * wait=0: don't wait to be sure. The callback may be called once more
 *          after this call to unregister_gpio_timer_callback
 */
long unregister_gpio_timer_callback(struct gpio_timer_callback *cb,int wait)
{
        struct GPIO_MM_timer* timer = board->timer;
        return unregister_gpio_timer_callback_priv(timer,cb,wait);
}
EXPORT_SYMBOL(unregister_gpio_timer_callback);

static void cleanup_gpio_timer(struct GPIO_MM_timer* timer)
{
        stop_gpio_timer(timer);
        gpio_mm_remove_timer_irq_user(timer->brd);
        tasklet_kill(&timer->tasklet);
        free_timer_callbacks(timer);
        kfree(timer);
}


static struct GPIO_MM_timer* init_gpio_timer(struct GPIO_MM* brd)
{
        struct GPIO_MM_timer* timer;
        struct gpio_timer_callback *cbentry, *cbentry2;
        int i;

        timer = kmalloc(sizeof(struct GPIO_MM_timer),GFP_KERNEL);
        if (!timer) return timer;
        memset(timer,0,sizeof(struct GPIO_MM_timer));
        timer->brd = brd;
        tasklet_init(&timer->tasklet,gpio_mm_timer_bottom_half,
                    (long)timer);
        brd->timer = timer;
        INIT_LIST_HEAD(&timer->callbackPool);

        /* create a pool of callback entries */
        for (i = 0; i < CALLBACK_POOL_SIZE; i++) {
                struct gpio_timer_callback* cbentry =
                    (struct gpio_timer_callback*)
                    kmalloc(sizeof(struct gpio_timer_callback),GFP_KERNEL);
                if (!cbentry) goto err0;
                list_add(&cbentry->list, &timer->callbackPool);
        }

        INIT_LIST_HEAD(&timer->pendingAdds);
        INIT_LIST_HEAD(&timer->callbackList);

        atomic_set(&timer->callbacksChanged,0);
        init_waitqueue_head(&timer->callbackWaitQ);

        atomic_set(&timer->running,0);
        timer->usecs = 0;
        gpio_mm_add_timer_irq_user(brd);
        return timer;
err0:
        list_for_each_entry_safe(cbentry,cbentry2,&timer->callbackPool,list) {
                list_del(&cbentry->list);
                kfree(cbentry);
        }
        return 0;
}

/*-----------------------Module ------------------------------*/

void gpio_mm_cleanup(void)
{

    int ib;

    if (board) {

        for (ib = 0; ib < numboards; ib++) {
            struct GPIO_MM* brd = board + ib;
            if (brd->fcntrs) {
                cleanup_fcntrs(brd);
                brd->fcntrs = 0;
            }
            if (brd->timer) {
                cleanup_gpio_timer(brd->timer);
                brd->timer = 0;
            }
            if (ioports_dio[ib]) {
                if (brd->dio_addr)
                    release_region(brd->dio_addr, GPIO_MM_DIO_IOPORT_WIDTH);
            }
            if (ioports_ct[ib]) {
                if (brd->ct_addr)
                    release_region(brd->ct_addr, GPIO_MM_CT_IOPORT_WIDTH);
            }
        }
        kfree(board);
        board = 0;
    }

    if (MAJOR(gpio_mm_device) != 0)
            unregister_chrdev_region(gpio_mm_device,
                numboards * GPIO_MM_MINORS_PER_BOARD);

    KLOG_DEBUG("complete\n");

    return;
}

int gpio_mm_init(void)
{	
        int result = -EINVAL;
        int ib;
        int chip;

        board = 0;

        // DSM_VERSION_STRING is found in dsm_version.h
        KLOG_NOTICE("version: %s, HZ=%d\n",DSM_VERSION_STRING,HZ);

        /* count non-zero ioport addresses, gives us the number of boards */
        for (ib = 0; ib < MAX_GPIO_MM_BOARDS; ib++)
            if (ioports_dio[ib] == 0) break;
        numboards_dio = ib;
        for (ib = 0; ib < MAX_GPIO_MM_BOARDS; ib++)
            if (ioports_ct[ib] == 0) break;
        numboards_ct = ib;

        if (numboards_dio + numboards_ct == 0) {
            KLOG_ERR("No boards configured, all ioports_dio[] and ioports_ct[] are zero\n");
            goto err;
        }

        numboards = max(numboards_dio,numboards_ct);

        result = alloc_chrdev_region(&gpio_mm_device, 0,
            numboards * GPIO_MM_MINORS_PER_BOARD,driver_name);
        if (result < 0) goto err;
        KLOG_DEBUG("alloc_chrdev_region done, major=%d minor=%d\n",
                MAJOR(gpio_mm_device),MINOR(gpio_mm_device));

        result = -ENOMEM;
        board = kmalloc(numboards * sizeof(struct GPIO_MM),GFP_KERNEL);
        if (!board) goto err;
        memset(board,0,numboards * sizeof(struct GPIO_MM));

        for (ib = 0; ib < numboards; ib++) {
                struct GPIO_MM* brd = board + ib;
                brd->num = ib;
                spin_lock_init(&brd->reglock);
                mutex_init(&brd->brd_mutex);
                result = -EBUSY;
                if (ioports_dio[ib] > 0) {
                        unsigned long addr =  ioports_dio[ib] +
                            SYSTEM_ISA_IOPORT_BASE;
                        unsigned char boardID;
                        // Get the mapped board address
                        if (!request_region(addr, GPIO_MM_DIO_IOPORT_WIDTH, driver_name)) {
                            KLOG_ERR("ioport at 0x%lx already in use\n", addr);
                            goto err;
                        }
                        brd->dio_addr = addr;
                        boardID = inb(addr + GPIO_MM_RESET_ID);
                        if (boardID != 0x11) {
                            KLOG_ERR("Does not seem to be a GPIO-MM board present at ioport address %lx, ID read from %lx is %x (should be 0x11)\n",
                                ioports_dio[ib],ioports_dio[ib]+GPIO_MM_RESET_ID,
                                boardID);
                            goto err;
                        }
                }
                if (ioports_ct[ib] > 0) {
                        unsigned long addr =  ioports_ct[ib] +
                            SYSTEM_ISA_IOPORT_BASE;
                        // Get the mapped board address
                        if (!request_region(addr, GPIO_MM_CT_IOPORT_WIDTH, driver_name)) {
                            KLOG_ERR("ioport at 0x%lx already in use\n", addr);
                            goto err;
                        }
                        brd->ct_addr = addr;
                }

                result = -EINVAL;
                // irqs are requested at open time.
                if (irqa[ib] <= 0) {
                        KLOG_ERR("zero value for IRQA for board #%d\n",ib);
                        goto err;
                }
                brd->irqs[0] = irqa[ib];
                brd->irqs[1] = irqb[ib];

                // reset board
                if (brd->dio_addr) outb(0x01,brd->dio_addr + GPIO_MM_RESET_ID);

                /* disable interrupts A and B */
                outb(0x22,brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS);

                /* Initialize 9513 master mode register values */
                for (chip = 0; chip < 2; chip++) {
                        brd->mmode_lsb[chip] = 0;
                        /* most significant byte of master mode value */
                        /* GPIO_MM only supports 8 bit transfers.
                         * Use BCD scaling of the 40 MHz clock */
                        brd->mmode_msb[chip] =
                            CTS9513_MMH_FOUT_DIV1 |
                            CTS9513_MMH_FOUT_OFF |
                            CTS9513_MMH_WIDTH_8 |
                            CTS9513_MMH_DP_ENABLE |
                            CTS9513_MMH_BCD;
                        gpio_mm_set_master_mode(brd,chip);
                }
                // todo: setup DIO
                //
                // setup counter/timers
                if (ioports_ct[ib]) {
                        result = init_fcntrs(brd);
                        if (result) goto err;

#ifndef ENABLE_TIMER_ON_EACH_BOARD
                        if (ib == 0) {
#endif
                        result = -ENOMEM;
                        brd->timer = init_gpio_timer(brd);
                        if (!brd->timer) goto err;
                        result = gpio_mm_reserve_cntrs(brd,0,0);
                        if (result) goto err;
#ifndef ENABLE_TIMER_ON_EACH_BOARD
                        }
#endif
                }
        }

        KLOG_DEBUG("complete.\n");

        return 0;
err:
        gpio_mm_cleanup();
        return result;
}

module_init(gpio_mm_init);
module_exit(gpio_mm_cleanup);

