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
static const char* driver_name = "gpio_mm";

/* C/T ioport addresses of installed boards, 0=board not used */
static unsigned long ioports[MAX_GPIO_MM_BOARDS] ={ 0x080, 0, 0, 0 , 0};

/* DIO ioport addresses of installed boards, 0=DIO not used.
 * If the DIO ports are used on a board, then the ioports address must
 * also be specified.
 */
static unsigned long ioports_dio[MAX_GPIO_MM_BOARDS] = { 0x040, 0, 0, 0, 0 };

/* number of GPIO_MM boards in system (number of non-zero ioport_ct values) */
static int numboards = 0;

/* number of boards passed to alloc_chrdev_region(). This value must
 * be saved and passed to unregister_chrdev_region().  If an ioport
 * address is found to be not valid (board not responding) we may
 * alter numboards. Then if the altered numboards is passed to
 * unregister_chrdev_region() some devices remain allocated (a device leak).
 */
static int numboards_alloc = 0;

/* Address of dio registers does not have to be configured if dio is not used */
static int numboards_dio = 0;

/* ISA irqs, required for each board. Can be shared. */
static int irqa[MAX_GPIO_MM_BOARDS] = { 3, 0, 0, 0, 0 };
static int irqb[MAX_GPIO_MM_BOARDS] = { 0, 0, 0, 0, 0 };
static int numirqa = 0;
static int numirqb = 0;

static int clockHZ = GPIO_MM_CT_CLOCK_HZ;

#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(ioports_dio,ulong,&numboards_dio,0);
module_param_array(ioports,ulong,&numboards,0);
module_param_array(irqa,int,&numirqa,0);
module_param_array(irqb,int,&numirqb,0);
#else
module_param_array(ioports_dio,ulong,numboards_dio,0);
module_param_array(ioports,ulong,numboards,0);
module_param_array(irqa,int,numirqa,0);
module_param_array(irqb,int,numirqb,0);
#endif

module_param(clockHZ,int,0);

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

/* Greatest common divisor. Use Euclidian algorimthm, thanks to Wikipedia */
static int gcd(unsigned int a, unsigned int b)
{
        if (b == 0) return a;
        return gcd(b,a % b);
}

#ifdef DEBUG
static void print_timeval(const char* msg,struct timeval* tvp)
{
        int hr,mn,sc;
        sc = tvp->tv_sec % 86400;
        hr = sc / 3600;
        sc -= hr * 3600;
        mn = sc / 60;
        sc -= mn * 60;
        KLOG_DEBUG("%s: %02d:%02d:%02d.%06ld\n",
            msg,hr,mn,sc,tvp->tv_usec);
}
static void print_time_of_day(const char* msg)
{
        struct timeval tv;
        do_gettimeofday(&tv);
        print_timeval(msg,&tv);
}
#endif

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

#ifdef DEBUG2
static unsigned short gpio_mm_get_master_mode(struct GPIO_MM* brd,int chip)
{
        unsigned char b1,b2;
        BUG_ON(chip > 2);
        outb(CTS9513_DPTR_MASTER_MODE,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));

        /* least significant byte of master mode value */
        b1 = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));

        /* most significant byte of master mode value */
        b2 = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        KLOG_DEBUG("master_mode, b1=%#02x,b2=%#02x\n",
            b1,b2);
        return b1 + (b2 << 8);
}
#endif

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
        BUG_ON(icntr < 0 || icntr >= GPIO_MM_CNTR_PER_BOARD);

        /* point to counter mode */
        outb(CTS9513_DPTR_CNTR1_MODE+ic,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));

        /* least significant byte of counter mode value */
        outb(lmode,brd->ct_addr + GPIO_MM_9513_DATA(chip));

        /* most significant byte of counter mode value */
        outb(hmode,brd->ct_addr + GPIO_MM_9513_DATA(chip));

#ifdef DEBUG2
        KLOG_DEBUG("setup counter #%d,chip=%d,lmode=%#02x,hmode=%#02x\n",
            icntr,chip,lmode,hmode);

        /* point to counter mode */
        outb(CTS9513_DPTR_CNTR1_MODE+ic,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
        lmode = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        hmode = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        KLOG_DEBUG("read  counter #%d,chip=%d,lmode=%#02x,hmode=%#02x\n",
            icntr,chip,lmode,hmode);
#endif
}

static void gpio_mm_cmd_counter(struct GPIO_MM* brd,int icntr,
    unsigned char cmd)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        outb(cmd + (1 << ic),
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
}

static void gpio_mm_cmd_counters(struct GPIO_MM* brd,
    int icntr1,int icntr2,unsigned char cmd)
{
        /* load and arm counter icntr1 and icntr2 */
        int chip1 = icntr1 / GPIO_MM_CNTR_PER_CHIP;
        int chip2 = icntr2 / GPIO_MM_CNTR_PER_CHIP;
        int ic1 = icntr1 % GPIO_MM_CNTR_PER_CHIP;
        int ic2 = icntr2 % GPIO_MM_CNTR_PER_CHIP;
        if (chip1 == chip2)
            outb(cmd + (1 << ic1) + (1 << ic2),
                brd->ct_addr + GPIO_MM_9513_PTR(chip1));
        else {
            outb(cmd + (1 << ic1),
                brd->ct_addr + GPIO_MM_9513_PTR(chip1));
            outb(cmd + (1 << ic2),
                brd->ct_addr + GPIO_MM_9513_PTR(chip2));
        }
}
static inline void gpio_mm_arm_counter(struct GPIO_MM* brd,int icntr)
{
        gpio_mm_cmd_counter(brd,icntr,CTS9513_ARM);
}

static inline void gpio_mm_arm_counters(struct GPIO_MM* brd,
    int icntr1,int icntr2)
{
        gpio_mm_cmd_counters(brd,icntr1,icntr2,CTS9513_ARM);
}

static void gpio_mm_load_counter(struct GPIO_MM* brd, int icntr)
{
        gpio_mm_cmd_counter(brd,icntr,CTS9513_LOAD);
}

static inline void gpio_mm_load_arm_counter(struct GPIO_MM* brd, int icntr)
{
        gpio_mm_cmd_counter(brd,icntr,CTS9513_LOAD_ARM);
}

static inline void gpio_mm_load_arm_counters(struct GPIO_MM* brd,
    int icntr1,int icntr2)
{
        gpio_mm_cmd_counters(brd,icntr1,icntr2,CTS9513_LOAD_ARM);
}

static inline void gpio_mm_disarm_counter(struct GPIO_MM* brd, int icntr)
{
        gpio_mm_cmd_counter(brd,icntr,CTS9513_DISARM);
}

static inline void gpio_mm_disarm_counters(struct GPIO_MM* brd,
    int icntr1,int icntr2)
{
        gpio_mm_cmd_counters(brd,icntr1,icntr2,CTS9513_DISARM);
}

static void gpio_mm_disarm_save_counter(struct GPIO_MM* brd, int icntr)
{
        gpio_mm_cmd_counter(brd,icntr,CTS9513_DISARM_SAVE);
}

static inline void gpio_mm_disarm_save_counters(struct GPIO_MM* brd,
    int icntr1,int icntr2)
{
        gpio_mm_cmd_counters(brd,icntr1,icntr2,CTS9513_DISARM_SAVE);
}

static void gpio_mm_set_reg(struct GPIO_MM* brd,
    int icntr,unsigned short val,unsigned char reg)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;

        outb(reg+ic,brd->ct_addr + GPIO_MM_9513_PTR(chip));
        outb(val & 0xff,brd->ct_addr + GPIO_MM_9513_DATA(chip));
        outb(val >> 8,brd->ct_addr + GPIO_MM_9513_DATA(chip));
#ifdef DEBUG
        KLOG_DEBUG("set reg, icntr=%d, cmd=%#02x, ic=%d,chip=%d,lmode=%#02x,hmode=%#02x\n",
            icntr,ic,chip,val & 0xff,val >> 8);
#endif
}

static unsigned short gpio_mm_get_reg(struct GPIO_MM* brd,
    int icntr,unsigned char reg)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        unsigned short hl,hh;

        outb(reg+ic,brd->ct_addr + GPIO_MM_9513_PTR(chip));
        hl = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        hh = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        return (hh << 8) + hl;
}

static inline void gpio_mm_set_load_reg(struct GPIO_MM* brd,
    int icntr,unsigned short val)
{
        gpio_mm_set_reg(brd,icntr,val,CTS9513_DPTR_CNTR1_LOAD);
}

static inline void gpio_mm_set_hold_reg(struct GPIO_MM* brd,
    int icntr,unsigned short val)
{
        gpio_mm_set_reg(brd,icntr,val,CTS9513_DPTR_CNTR1_HOLD);
}

static inline unsigned short gpio_mm_get_hold_reg(struct GPIO_MM* brd,
    int icntr)
{
        return gpio_mm_get_reg(brd,icntr,CTS9513_DPTR_CNTR1_HOLD);
}

static void gpio_mm_set_toggle_out(struct GPIO_MM* brd, int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        outb(CTS9513_SET_TOGGLE_OUT + ic + 1,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
}

#ifdef __GNUC__
static void gpio_mm_clear_toggle_out(struct GPIO_MM* brd, int icntr)
  __attribute__((__unused__));
#endif
static void gpio_mm_clear_toggle_out(struct GPIO_MM* brd, int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        outb(CTS9513_CLEAR_TOGGLE_OUT + ic + 1,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
}

static unsigned char gpio_mm_get_output_status(struct GPIO_MM* brd,int icntr)
{
        /* which 9513 chip */
        int chip = icntr / GPIO_MM_CNTR_PER_CHIP;
        /* which of the 5 counters on the 9513 */
        int ic = icntr % GPIO_MM_CNTR_PER_CHIP;
        unsigned char hl,hh;

        outb(CTS9513_DPTR_STATUS,
            brd->ct_addr + GPIO_MM_9513_PTR(chip));
        hl = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        hh = inb(brd->ct_addr + GPIO_MM_9513_DATA(chip));
        return hl & (2 << ic);
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
                KLOG_DEBUG("entry usecs=%d\n",
                        cbentry->usecs);
                if (a == 0) a = cbentry->usecs;
                else a = gcd(a,cbentry->usecs);
                maxUsecs = max(cbentry->usecs,maxUsecs);
        }
        if (a != 0)
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

        /* arbitrary lower limit - actually should be tested. */
        if (usecs < USECS_PER_SEC / 1000) {
                KLOG_ERR("GPIO-MM board %d, usecs=%d is too small\n",
                    brd->num,usecs);
                return -EINVAL;
        }
        /* overflows at usecs >= 214,748,364 or 214 seconds */
        if (2 * usecs > UINT_MAX / GPIO_MM_CT_CLOCK_HZ * USECS_PER_SEC) {
                KLOG_ERR("GPIO-MM board %d, usecs=%d is too large\n",
                    brd->num,usecs);
                return -EINVAL;
        }
        tics = clockHZ / USECS_PER_SEC * usecs;

        /* Compute BCD scaler of F1 clock.
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

static void set_ticks(struct GPIO_MM_timer* timer,unsigned int usecs,
    unsigned int maxUsecs,unsigned short* initial_ticsp)
{
        struct timeval tv;
        timer->tickLimit = UINT_MAX - (UINT_MAX % (unsigned)(maxUsecs / usecs) );
        /* determine initial amount of time to count so that
         * we are (somewhat) in sync with the system clock.
         */
        do_gettimeofday(&tv);
        if (initial_ticsp) {
                unsigned short initial_tics;
                unsigned int initial_usecs;
                initial_usecs = 2 * usecs - (tv.tv_usec % usecs);
                initial_tics = clockHZ / USECS_PER_SEC * initial_usecs /
                    timer->scaler;
                timer->tick =
                    ((tv.tv_usec + initial_usecs - usecs) / usecs) % timer->tickLimit;
                *initial_ticsp = initial_tics;
                KLOG_DEBUG("usecs=%d,maxUsecs=%d,tv_usec=%ld,initial_usecs=%d,initial_tics=%d,tick=%u,tickLimit=%u\n",
                           usecs,maxUsecs,tv.tv_usec,initial_usecs,initial_tics,
                           timer->tick,timer->tickLimit);
        }
        else timer->tick = (tv.tv_usec / usecs);

}
/*
 * Called from software interrupt and user context.
 */
static void start_gpio_timer(struct GPIO_MM_timer* timer,
    unsigned int usecs, unsigned int maxUsecs)
{
        struct GPIO_MM* brd = timer->brd;
        unsigned char lmode;
        unsigned char hmode;
        unsigned int tics;
        unsigned long flags;
        int tc = GPIO_MM_TIMER_COUNTER;
        int scaler;
        unsigned short initial_tics;

        /* timer is mode D: Rate Generator with No Hardware Gating */
        lmode = CTS9513_CML_OUT_HIGH_ON_TC |
                CTS9513_CML_CNT_DN |
                CTS9513_CML_CNT_BIN |
                CTS9513_CML_REPEAT |
                CTS9513_CML_RELOAD_LOAD |
                CTS9513_CML_GATE_NORETRIG;
        hmode = CTS9513_CMH_SRC_F1 |
                CTS9513_CMH_EDGE_RISING |
                CTS9513_CMH_NO_GATE;

        /* overflow of this should have been checked already 
         * by check_timer_interval.
         */
        /* clock on one card was actually 19,999,962 Hz
         * After a time of X seconds, the GPIO clock 
         * will be slow by X*(38/20000000) = X * 1.9E-6 seconds
         * This is about 1 second in 6 days.
         */
        tics = clockHZ / USECS_PER_SEC * usecs;

        /* Compute BCD scaler of F1 clock.
         * Initially we may wait as much as two periods.
         */
        for (scaler = 1; scaler <= 10000; scaler *= 10) {
            if (2 * tics / scaler <= 0xFFFF) break;
            hmode++;
        }
        tics /= scaler;
        timer->scaler = scaler;

        spin_lock_irqsave(&brd->reglock,flags);

        /* disable interrupt A */
        outb(0x02,brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS);

        gpio_mm_setup_counter(brd,tc,lmode,hmode);

        /* IRQA source is counter/timer output */
        outb(GPIO_MM_TIMER_COUNTER,brd->ct_addr + GPIO_MM_IRQ_SRC);
        /* enable interrupt A */
        outb(0x04,brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS);

        timer->irqsReceived = 0;

        KLOG_DEBUG("usecs=%d,scaler=%d,tics=%d\n",
                   usecs,timer->scaler,tics);

        set_ticks(timer,usecs,maxUsecs,&initial_tics);

        gpio_mm_set_load_reg(brd,tc,initial_tics);
        gpio_mm_load_counter(brd,tc);

        gpio_mm_set_load_reg(brd,tc,tics);
#ifdef DEBUG
        print_time_of_day("arming counter");
#endif
        gpio_mm_arm_counter(brd,tc);

        spin_unlock_irqrestore(&brd->reglock,flags);

        timer->usecs = usecs;
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
        unsigned int usecs,maxUsecs;

        spin_lock(&timer->callbackLock);

        /* Remove pending callbacks from the active lists. */
        for (i = 0; i < timer->nPendingRemoves; i++) {
                cbentry = timer->pendingRemoves[i];
                /* remove entry from the active list for the rate */
                KLOG_DEBUG("removing entry with usecs=%d\n",
                    cbentry->usecs);
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
                KLOG_DEBUG("pending entry with usecs=%d\n",
                    cbentry->usecs);
                /* add to sorted active list */
                for (ptr2 = timer->callbackList.next; 
                        ptr2 != &timer->callbackList; ptr2 = ptr2->next) {
                        cbentry2 =
                            list_entry(ptr2, struct gpio_timer_callback, list);
                        KLOG_DEBUG("cbentry2 with usecs=%d\n",
                            cbentry2->usecs);
                        if (cbentry->usecs < cbentry2->usecs) {
                            KLOG_DEBUG("adding entry with usecs=%d\n",
                                    cbentry->usecs);
                                list_add(&cbentry->list,ptr2->prev);
                                break;
                        }
                }
                if (ptr2 == &timer->callbackList)
                        list_add(&cbentry->list, ptr2);
        }
        /*
         * determine timer sleep period, the greatest common divisor.
         * If it is a different sleep period than the existing:
         *   if timer is running, stop_gpio_timer
         *   if new sleep period is != 0, start_gpio_timer
         */
        usecs = compute_timer_ticks(&timer->callbackList,&maxUsecs);
        KLOG_DEBUG("compute_timer_ticks, usecs=%d,max=%d\n",
            usecs,maxUsecs);
        if (usecs != 0) set_ticks(timer,usecs,maxUsecs,0);
        if (usecs != timer->usecs) {
                if (timer->usecs != 0) stop_gpio_timer(timer);
                if (usecs != 0) start_gpio_timer(timer,usecs,maxUsecs);
        }
        timer->callbacksChanged = 0;
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
        timer->callbacksChanged = 0;
        wake_up_interruptible(&timer->callbackWaitQ);
        spin_unlock_bh(&timer->callbackLock);
}

/*
 * Timer bottom half tasklet function that is invoked after receipt
 * of a timer interrupt.  Invokes callbacks that have been registered.
 * Note that this does not hold locks when the callbacks are being called,
 * because we want to allow callbacks to unregister themselves.
 */
static void gpio_mm_timer_bottom_half(unsigned long dev)
{
        struct GPIO_MM_timer* timer = (struct GPIO_MM_timer*) dev;
        struct gpio_timer_callback *cbentry;

        if (timer->callbacksChanged)
                handlePendingCallbacks(timer);

        list_for_each_entry(cbentry,&timer->callbackList,list) {
                KLOG_DEBUG("tick=%d,tickModulus=%d\n",
                    timer->tick,cbentry->tickModulus);
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

        /* check interrupt A */
        status = inb(brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS) & 0x01;
        if (!status) {     
                /* not my interrupt. Hopefully somebody cares! */
                spin_unlock(&brd->reglock);
                return IRQ_NONE;
        }

#ifdef DEBUG2
        print_time_of_day("irq_handler");
#endif

        /* acknowledge interrupt A */
        outb(0x1,brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS);

        timer->irqsReceived++;

        tick = (timer->tick + 1) % timer->tickLimit;
        timer->tick = tick;
#ifdef DEBUG2
        KLOG_DEBUG("board %d: irq %d, tick=%d,limit=%u,rcvd=%d\n",
               brd->num,irq,timer->tick,timer->tickLimit,
                timer->irqsReceived);
#endif

        spin_unlock(&brd->reglock);

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
                KLOG_DEBUG("board %d: requesting irq: %d, system irq: %d\n",
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
 * counter. This is the first of 3 counters used to implement
 * a frequency counter.
 */
static int get_pulse_counter(struct GPIO_MM_fcntr* fcntr) 
{
#if GPIO_MM_TIMER_COUNTER > 0
        return fcntr->num * GPIO_MM_CNTR_PER_FCNTR;
#else
#ifdef ENABLE_TIMER_ON_EACH_BOARD
        return fcntr->num * GPIO_MM_CNTR_PER_FCNTR + 1;
#else
        return fcntr->num * GPIO_MM_CNTR_PER_FCNTR +
              ((fcntr->brd->num == 0) ? 1 : 0);
#endif
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
        int chip1, chip2;
        int result;
#ifdef DEBUG
        unsigned char pcstatus;
#endif

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
        hmode = (CTS9513_CMH_SRC_S1 + (pc % GPIO_MM_CNTR_PER_CHIP)) |
                CTS9513_CMH_EDGE_RISING |
                CTS9513_CMH_NO_GATE;
        gpio_mm_setup_counter(brd,pc,lmode,hmode);

        /* Start with output high, count a few pulses,
         * toggle output to low, count numPulses, toggle
         * output back to high.
         * The initial count must be at least 3 pulses.
         * Setting the initial count in the load register
         * to a value to 1 or 2 doesn't seem to work, the output
         * toggle level is always low after counting pulses.
         * CTS9513 specs mention a bug in Mode J, where a hold
         * or load value of 1 doesn't work. Mode J is similar to
         * G except that J automatically repeats.  Seems that
         * npulse=2 doesn't work either. Perhaps it would work with
         * lower input frequencies.
         */
        gpio_mm_set_load_reg(brd,pc,3);
        gpio_mm_set_hold_reg(brd,pc,fcntr->numPulses);

        /* set output toggle high, which is the gate for the next counter */
        gpio_mm_set_toggle_out(brd,pc);

        /* Low order 16 bits of the F1 clock counter.
         * mode=E, Rate Generator with Level Gating. */
        /* configure least significant word */
        lmode = CTS9513_CML_OUT_HIGH_ON_TC |
                CTS9513_CML_CNT_UP |
                CTS9513_CML_CNT_BIN |
                CTS9513_CML_REPEAT |
                CTS9513_CML_RELOAD_LOAD |
                CTS9513_CML_GATE_NORETRIG;
        /* source F1 is the 20 MHz clock.
         * Cannot gate this counter with the terminal count output
         * of the pulse counter, with option CTS9513_CMH_GATE_HI_TCNM1,
         * because that output is just one pulse wide. See the
         * diagram for Mode=E in the CTS9513 spec.
         * One has to hardwire the output of the pulse counter,
         * which is shown as "toggle output" in the diagram,
         * into the gate of tc1, and use the CTS9513_CMH_GATE_HI_GN mode
         * on this counter.
         */
        hmode = CTS9513_CMH_SRC_F1 |
                CTS9513_CMH_EDGE_RISING |
                CTS9513_CMH_GATE_LO_GN;
        gpio_mm_setup_counter(brd,tc1,lmode,hmode);
        gpio_mm_set_load_reg(brd,tc1,0);

        /* High order 16 bits of the F1 clock counter.
         * mode=A, Software Triggered with no Gating.
         */
        /* configure least significant word */
        lmode = CTS9513_CML_OUT_LOW |
                CTS9513_CML_CNT_UP |
                CTS9513_CML_CNT_BIN |
                CTS9513_CML_ONCE |
                CTS9513_CML_RELOAD_LOAD |
                CTS9513_CML_GATE_NORETRIG;
        /* Source is the output of the previous counter,
         * except when the counter is on another chip, 
         * in which case the output of the previous counter must be
         * hardwired to the input of the current counter on
         * connector J3 of the GPIO-MM. For example, with the
         * current numbering scheme of using counters 0-2 for fcntr0,
         * counters 3-5 for fcntr1 and counters 6-8 for fnctr2,
         * you must connect the output of counter 4 (J3:OUT5)
         * to the input of counter 5 (J3:IN6).
         *
         */
        chip1 = tc1 / GPIO_MM_CNTR_PER_CHIP;
        chip2 = tc2 / GPIO_MM_CNTR_PER_CHIP;
        if (chip1 == chip2) {
                hmode = CTS9513_CMH_SRC_TCNM1 |
                        CTS9513_CMH_EDGE_RISING |
                        CTS9513_CMH_NO_GATE;
        }
        else {
                hmode = (CTS9513_CMH_SRC_S1 + (tc2 % GPIO_MM_CNTR_PER_CHIP)) |
                        CTS9513_CMH_EDGE_RISING |
                        CTS9513_CMH_NO_GATE;
        }
        gpio_mm_setup_counter(brd,tc2,lmode,hmode);
        gpio_mm_set_load_reg(brd,tc2,0);

        /* load and arm F1 tic counters */
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
        /* disarm F1 tic counters */
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
 * 3. Look at the status of the pulse counter output. 
 *    If it is high, then either it didn't finish counting the initial 
 *    small number of pulses, or it is finished counting.
 *    According to the CTS9513 doc, when it finishes counting it
 *    reloads itself from the load register, which has value 3,
 *    but actually it appears to be 2 in that case.
 *    To differentiate between these cases we can also look at the number of
 *    tics counted, which should be 0 if there were no pulses.
 *    If the output is low, then it hasn't finished counting numPulses,
 *    and the value of the counter will be the number of pulses left
 *    to count.
 * 4. Determine the total number of F1 MHz tics from the other 2 counter
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
        unsigned char pcstatus;
        unsigned long flags;
        struct freq_sample* samp;

        spin_lock_irqsave(&brd->reglock,flags);

        gpio_mm_disarm_save_counters(brd, tc1,tc2);
        gpio_mm_disarm_save_counter(brd,pc);

        /* If pulse counter has finished counting numPulses, then
         * its output should be high.
         */
        pcstatus = gpio_mm_get_output_status(brd,pc);

        pcn = gpio_mm_get_hold_reg(brd,pc);
        tc1n = gpio_mm_get_hold_reg(brd,tc1);
        tc2n = gpio_mm_get_hold_reg(brd,tc2);
        tcn = ((unsigned int)tc2n << 16) + tc1n;

        /* hold register was overwritten by save command */
        gpio_mm_set_hold_reg(brd,pc,fcntr->numPulses);

        /* set output high, which is the gate for the next counter */
        // if (!pcstatus) gpio_mm_set_toggle_out(brd,pc);
        gpio_mm_set_toggle_out(brd,pc);

        /* load and arm F1 tic counters */
        gpio_mm_load_arm_counters(brd,tc1,tc2);

        /* load and arm pulse counter. It is already loaded
         * if it finished counting pulses, but we'll load it anyway */
        gpio_mm_load_arm_counter(brd,pc);

        spin_unlock_irqrestore(&brd->reglock,flags);

        KLOG_DEBUG("%s: pcn=%d,pulses=%d,tc1n=%d,tc2n=%d,tcn=%d,status=%#02x\n",
            fcntr->deviceName,pcn,fcntr->numPulses-pcn,tc1n,tc2n,tcn,pcstatus);

        samp = (struct freq_sample*)
            GET_HEAD(fcntr->samples,GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE);
        if (!samp) {                // no output sample available
                if (!(fcntr->status.lostSamples++ % 1000))
		    KLOG_WARNING("%s: lostSamples=%d\n",
                       fcntr->deviceName,fcntr->status.lostSamples);
        }
        else {
                samp->timetag = getSystemTimeTMsecs();
                samp->length = 2 * sizeof(int);
                // Data is little endian
                samp->ticks = cpu_to_le32(tcn);
                 
                if (pcstatus)  {
                        // pc output high, all pulses counted, or not finished counting the initial few
                        switch (pcn) {
                        case 3:
                                /* On a GPIO-MM, if the output status is high
				 * and the counter has value 3, then no pulses
                                 * were counted.
                                 * If there is no hardwire connection from the
                                 * output of pc and the gate of tc1, and
                                 * J8 is jumpered so that the counter/timer
                                 * pins are held low (J8:G), then
                                 * the counters will run continously,
                                 * their gate is always enabled.
                                 */
				if (tcn == 0) samp->pulses = 0;
                                else {
					samp->pulses = cpu_to_le32(fcntr->numPulses - pcn);
					if (!(fcntr->status.badGateWarning++ % 1000))
					    KLOG_WARNING(
			    "%s: warning #%d: output status high, pcn=%d, expected ntics to be 0, but it is %d. Is J3:OUT%d wired to J3:GATE%d?\n",
					    fcntr->deviceName,fcntr->status.badGateWarning,pcn,tcn,pc+1,tc1+1);
				}
                                break;
                        case 2:
				/* On a GPIO-MM, when the counter finishes, it is supposed
                                 * to load itself again from the load register
                                 * which has value 3, but for some reason
                                 * the counter value saved to the hold
                                 * register is 2.
                                 */
                                /* If there is no hardwire connection from the
                                 * output of pc to the gate of tc1, and
                                 * J8 is jumpered so that the counter/timer
                                 * pins are held high (J8:+5), then
                                 * the counters will not run, since the
                                 * gate is always high.
                                 */
                                if (tcn == 0) {
					samp->pulses = 0;
					if (!(fcntr->status.badGateWarning++ % 1000))
					    KLOG_WARNING(
					    "%s: warning #%d: output status high, pcn=%d, but tics is 0. Is J3:OUT%d wired to J3:GATE%d?\n",
					    fcntr->deviceName,fcntr->status.badGateWarning,pcn,pc+1,tc1+1);
				}
				else samp->pulses = cpu_to_le32(fcntr->numPulses);
                                break;
                        case 1:
				if (tcn == 0) samp->pulses = 0;
				else samp->pulses = cpu_to_le32(fcntr->numPulses - pcn);
				break;
                        default:
				if (tcn == 0) samp->pulses = 0;
				else samp->pulses = cpu_to_le32(fcntr->numPulses - pcn);
                                if (!(fcntr->status.badStatusWarning++ % 1000)) 
					KLOG_WARNING(
                            "%s: warning #%d: pc output high, pcn should be 1, 2 or 3, instead it is %d, tcn=%d\n",
                                fcntr->deviceName,fcntr->status.badStatusWarning,pcn,tcn);
                                break;
                        }
                }
                else {
                        if (!(fcntr->status.pulseUnderflow++ % 1000))
                                KLOG_WARNING(
                                    "%s: warning #%d: pulse underflow, failed to count %d pulses. Is numPulses too high?\n",
                                    fcntr->deviceName,fcntr->status.pulseUnderflow,fcntr->numPulses);
                        samp->pulses = cpu_to_le32(fcntr->numPulses - pcn);
                }
                // Wake up the read queue if latencyJiffies have elapsed
                // or if the queue is half or more full.
                INCREMENT_HEAD(fcntr->samples,GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE);
		if (((long)jiffies - (long)fcntr->lastWakeup) > fcntr->latencyJiffies ||
			CIRC_SPACE(fcntr->samples.head,fcntr->samples.tail,
			GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE) <
			    GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE/2) {
			wake_up_interruptible(&fcntr->rwaitq);
			fcntr->lastWakeup = jiffies;
		}
        }
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

        fcntr->latencyJiffies = (cfg->latencyUsecs * HZ) / USECS_PER_SEC;
        /* If latencyUsecs is a bad value, fallback to 1/4 second */
        if (fcntr->latencyJiffies == 0) fcntr->latencyJiffies = HZ / 4;
        fcntr->lastWakeup = jiffies;
        KLOG_DEBUG("%s: latencyJiffies=%ld, HZ=%d\n",
               fcntr->deviceName,fcntr->latencyJiffies,HZ);

        fcntr->outputPeriodUsec = cfg->outputPeriodUsec;
        fcntr->numPulses = cfg->numPulses;

        memset(&fcntr->status, 0, sizeof (struct GPIO_MM_fcntr_status));

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

        KLOG_DEBUG("open_fcntr, minor=%d,ibrd=%d,ifcntr=%d,numboards=%d\n",
            i,ibrd,ifcntr,numboards);

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        if (ibrd >= numboards) return -ENXIO;
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
        int result = 0;

        int i = iminor(inode);
        int ibrd = i / GPIO_MM_MINORS_PER_BOARD;
        int ifcntr = i % GPIO_MM_MINORS_PER_BOARD;

        struct GPIO_MM* brd;

        if (ibrd >= numboards) return -ENXIO;
        if (ifcntr >= GPIO_MM_FCNTR_PER_BOARD) return -ENXIO;

        brd = board + ibrd;
        BUG_ON(fcntr != brd->fcntrs + ifcntr);

        if (atomic_dec_and_test(&fcntr->num_opened))
            result = stop_fcntr(fcntr);
	KLOG_DEBUG("%s: num_opened=%d\n",
	    fcntr->deviceName,atomic_read(&fcntr->num_opened));
        return result;
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
#define BUFFER_POLL
#ifdef BUFFER_POLL
	if (((long)jiffies - (long)fcntr->lastWakeup) > fcntr->latencyJiffies ||
		CIRC_SPACE(fcntr->samples.head,fcntr->samples.tail,
		GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE) <
		    GPIO_MM_FCNTR_SAMPLE_QUEUE_SIZE/2) {
                mask |= POLLIN | POLLRDNORM;    /* readable */
		fcntr->lastWakeup = jiffies;
	}
#else
        if (sample_remains(&fcntr->read_state) ||
            fcntr->samples.head != fcntr->samples.tail)
                mask |= POLLIN | POLLRDNORM;    /* readable */
#endif
        return mask;
}

static struct file_operations fcntr_fops = {
        .owner   = THIS_MODULE,
        .read    = gpio_mm_read_fcntr,
        .poll    = gpio_mm_poll_fcntr,
        .open    = gpio_mm_open_fcntr,
        .ioctl   = gpio_mm_ioctl_fcntr,
        .release = gpio_mm_release_fcntr,
        .llseek  = no_llseek,
};

/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
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
                /* minor numbers are 0,1,2 for first board,  13,14,15 for 2nd */
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

/* register a callback on the given timer */
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
        timer->callbacksChanged = 1;
        if (timer->usecs == 0) {
                /* First callback registered, start timer */
                cbentry->tickModulus = 1;
                start_gpio_timer(timer,usecs,usecs);
        }

        spin_unlock_bh(&timer->callbackLock);
        return cbentry;
}

static long unregister_gpio_timer_callback_priv(struct GPIO_MM_timer* timer,
    struct gpio_timer_callback *cb,int wait)
{
        long ret = 0;

        spin_lock_bh(&timer->callbackLock);
        timer->pendingRemoves[timer->nPendingRemoves++] = cb;
        timer->callbacksChanged = 1;
        spin_unlock_bh(&timer->callbackLock);

        if (wait)
            // check the return of this. Is it negative if interrupted?
            ret = wait_event_interruptible_timeout(timer->callbackWaitQ,
                    timer->callbacksChanged == 0,HZ);

        return ret;
}

/* register a callback on the timer of board 0 */
struct gpio_timer_callback *register_gpio_timer_callback(
    gpio_timer_callback_func_t callback,unsigned int usecs,
                     void *privateData, int *errp)
{
        struct GPIO_MM_timer* timer = board->timer;
        return register_gpio_timer_callback_priv(timer,callback,usecs,
            privateData,errp);
}
EXPORT_SYMBOL(register_gpio_timer_callback);

long unregister_gpio_timer_callback(struct gpio_timer_callback *cb,int wait)
{
        struct GPIO_MM_timer* timer = board->timer;
        return unregister_gpio_timer_callback_priv(timer,cb,wait);
}
EXPORT_SYMBOL(unregister_gpio_timer_callback);

/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
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

        timer->callbacksChanged = 0;
        init_waitqueue_head(&timer->callbackWaitQ);

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

// #define DEBUG_CALLBACKS
#ifdef DEBUG_CALLBACKS
struct test_callback_data
{
        struct timeval tv;
        struct gpio_timer_callback *cbh;
};

void test_callback(void* ptr)
{
        struct test_callback_data* cbd = ptr;
        struct timeval tv;
        int hr,mn,sc;
        int diff = 0;
        do_gettimeofday(&tv);
        if (cbd->tv.tv_usec > 0) {
            int sd = tv.tv_sec - cbd->tv.tv_sec;
            diff = tv.tv_usec - cbd->tv.tv_usec;
            if (sd > 0) diff += sd * USECS_PER_SEC;
        }
        sc = tv.tv_sec % 86400;
        hr = sc / 3600;
        sc -= hr * 3600;
        mn = sc / 60;
        sc -= mn * 60;
        KLOG_INFO("callback %02d:%02d:%02d.%06ld, usecs=%d, diff=%6d\n",
            hr,mn,sc,tv.tv_usec,cbd->cbh->usecs,diff);
        memcpy(&cbd->tv,&tv,sizeof(struct timeval));
}

struct test_callback_data testcbd1;
struct test_callback_data testcbd2;
#endif

/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
static void gpio_mm_cleanup(void)
{

        int ib;

        if (board) {

#ifdef DEBUG_CALLBACKS
        if (testcbd1.cbh) unregister_gpio_timer_callback(testcbd1.cbh,1);
        if (testcbd2.cbh) unregister_gpio_timer_callback(testcbd2.cbh,1);
#endif

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
                    if (ioports[ib]) {
                        if (brd->ct_addr) {
                            // reset board
                            outb(0x01,brd->ct_addr + GPIO_MM_RESET_ID);
                            release_region(brd->ct_addr, GPIO_MM_CT_IOPORT_WIDTH);
                        }
                    }
                }
                kfree(board);
                board = 0;
        }

        if (MAJOR(gpio_mm_device) != 0)
                unregister_chrdev_region(gpio_mm_device,
                    numboards_alloc * GPIO_MM_MINORS_PER_BOARD);

        KLOG_DEBUG("complete\n");
        return;
}

static int __init gpio_mm_init(void)
{	
        int result = -EINVAL;
        int ib;
        int chip;
        unsigned long addr;
	unsigned char fpgaRev;

        board = 0;

        // DSM_VERSION_STRING is found in dsm_version.h
        KLOG_NOTICE("version: %s, HZ=%d\n",DSM_VERSION_STRING,HZ);

        // check for reasonable clock rate. Don't expect an error > 1%
        if (clockHZ < (GPIO_MM_CT_CLOCK_HZ * 99)/100 ||
            clockHZ > (GPIO_MM_CT_CLOCK_HZ * 101)/100) {
                KLOG_ERR("Bad clock rate module parameter: %d Hz. Is it really that different from %d?\n",
                clockHZ,GPIO_MM_CT_CLOCK_HZ);
            goto err;
        }

        /* count non-zero ioport addresses, gives us the number of boards */
        for (ib = 0; ib < numboards_dio; ib++)
            if (ioports_dio[ib] == 0) break;
        numboards_dio = ib;
        for (ib = 0; ib < numboards; ib++)
            if (ioports[ib] == 0) break;
        numboards = ib;

        if (numboards == 0) {
            KLOG_ERR("No boards configured, all ioports[] are zero\n");
            goto err;
        }

        result = alloc_chrdev_region(&gpio_mm_device, 0,
            numboards * GPIO_MM_MINORS_PER_BOARD,driver_name);
        if (result < 0) goto err;
        numboards_alloc = numboards;
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
                addr =  ioports[ib] + SYSTEM_ISA_IOPORT_BASE;
                // Get the mapped board address
                if (!request_region(addr, GPIO_MM_CT_IOPORT_WIDTH, driver_name)) {
                    KLOG_ERR("ioport at 0x%lx already in use\n", addr);
                    goto err;
                }
                brd->ct_addr = addr;
                result = -ENODEV;
                brd->boardID = inb(addr + GPIO_MM_RESET_ID);

                /* We'll try to be resilient here. If there does not seem
                 * to be a board at the given address, but one or more
                 * preceeding boards are working, we will just
                 * report a warning and continue, ignoring any
                 * subsequent boards. The user will get a
                 * "no such device" error when trying to open a
                 * device on a non-functional or missing board.
                 * We could always return an error, in which case this
                 * module won't be loaded, but this way things
                 * will *mostly* work if the last configured board
                 * is removed. Better some data than none at all.
                 */
		// GPIO-MM boardID is 0x11, GPIO-MM-12 boardID is 0x12
		// GPIO-MM-12 v1.00 manual is incorrect
                if (brd->boardID != 0x11 && brd->boardID != 0x12) {
                    KLOG_ERR("Does not seem to be a GPIO-MM board present at ioport address %lx, ID read from %lx is 0x%x (should be 0x11 or 0x12)\n",
                        ioports[ib],ioports[ib]+GPIO_MM_RESET_ID,
                        brd->boardID);
                    if (ib == 0) goto err;      // nutt'in working
                    ioports[ib] = 0;
                    release_region(brd->ct_addr, GPIO_MM_CT_IOPORT_WIDTH);
                    brd->ct_addr = 0;
                    numboards = ib;
                    continue;
                }
                if (ioports_dio[ib] > 0) {
                        result = -EBUSY;
                        addr =  ioports_dio[ib] + SYSTEM_ISA_IOPORT_BASE;
                        // Get the mapped board address
                        if (!request_region(addr, GPIO_MM_DIO_IOPORT_WIDTH, driver_name)) {
                            KLOG_ERR("ioport at 0x%lx already in use\n", addr);
                            goto err;
                        }
                        brd->dio_addr = addr;
                }

                result = -EINVAL;
                // irqs are requested at open time.
                if (irqa[ib] <= 0) {
                        KLOG_ERR("zero value for IRQA for board #%d\n",ib);
                        goto err;
                }
                brd->irqs[0] = irqa[ib];
                brd->irqs[1] = irqb[ib];

		// read FPGA revision code
                fpgaRev = inb(brd->ct_addr + GPIO_MM_FPGA_REV);
		KLOG_INFO("GPIO-MM#%d, FPGA ID=%#x, FPGA rev=%#x\n",
			ib,brd->boardID,fpgaRev);
                // reset board
                outb(0x01,brd->ct_addr + GPIO_MM_RESET_ID);

                // wait a 1/10 second for things to settle
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(HZ/10);

                /* disable interrupts A and B */
                outb(0x22,brd->ct_addr + GPIO_MM_IRQ_CTL_STATUS);

                /* Initialize 9513 master mode register values */
                for (chip = 0; chip < 2; chip++) {
                        brd->mmode_lsb[chip] =
                            CTS9513_MML_NO_COMP |
                            CTS9513_MML_FOUT_G2;
                            // CTS9513_MML_FOUT_FX1;
                        /* most significant byte of master mode value */
                        /* GPIO_MM only supports 8 bit transfers.
                         * Use BCD scaling of the F1 clock */
                        brd->mmode_msb[chip] =
                            CTS9513_MMH_FOUT_DIV1 |
                            CTS9513_MMH_FOUT_ON |
                            // CTS9513_MMH_FOUT_OFF |
                            CTS9513_MMH_WIDTH_8 |
                            CTS9513_MMH_DP_ENABLE |
                            CTS9513_MMH_BCD;
#ifdef DEBUG2
                        KLOG_DEBUG("set master_mode, b1=%#02x,b2=%#02x\n",
                            brd->mmode_lsb[chip],brd->mmode_msb[chip]);
#endif
                        gpio_mm_set_master_mode(brd,chip);
#ifdef DEBUG2
                        gpio_mm_get_master_mode(brd,chip);
#endif
                }

                // Choose high rate 20MHz clock
		// The GPIO-MM-12 manual makes one believe that
		// the on-board input clock can tic at 40MHz,
		// but it is a lie.  It is a 20MHz input, just
		// like the GPIO-MM.
		// This is with a GPIO-MM-12, FPGA ID=0x10, FPGA rev=0x10.
		// The GPIO-MM-12 v1.00 manual also does not document
		// writing to this register to select the clock rate,
		// but it seems to be required. If one does not
		// write a 0x1 to this register then one gets
		// a 4MHz clock, just as with the GPIO-MM.
		// As a hack, tried writing 0x2, resulted in a 4MHz clock.
		// Writing 0x3, resulted in a 20MHz clock, as did 0xff.
                outb(0x01,brd->ct_addr + GPIO_MM_FPGA_REV);

                // todo: setup DIO
                //
                // setup counter/timers
                result = init_fcntrs(brd);
                if (result) goto err;

#ifndef ENABLE_TIMER_ON_EACH_BOARD
                if (ib == 0) {
#endif
                        result = -ENOMEM;
                        brd->timer = init_gpio_timer(brd);
                        if (!brd->timer) goto err;
                        result = gpio_mm_reserve_cntrs(brd,
                                GPIO_MM_TIMER_COUNTER ,GPIO_MM_TIMER_COUNTER);
                        if (result) goto err;
#ifndef ENABLE_TIMER_ON_EACH_BOARD
                }
#endif
        }

#ifdef DEBUG_CALLBACKS
        testcbd1.cbh = register_gpio_timer_callback(
                test_callback,USECS_PER_SEC/1,&testcbd1,&result);
        // testcbd2.cbh = register_gpio_timer_callback(
        //         test_callback,1000000,&testcbd2,&result);
        KLOG_INFO("registered test callbacks, result=%d\n",
            result);
#endif

        KLOG_DEBUG("complete.\n");

        return 0;
err:
        gpio_mm_cleanup();
        return result;
}

module_init(gpio_mm_init);
module_exit(gpio_mm_cleanup);

