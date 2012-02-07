/* -*- mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*-
 * vim: set shiftwidth=8 softtabstop=8 expandtab: */

/* pc104sg.c
 * driver for Brandywine's PC104-SG IRIG card
 * (adapted from Gordon Maclean's RT-Linux driver for the card)
 * Copyright 2007 UCAR, NCAR, All Rights Reserved
 * Revisions:
 * $LastChangedRevision$
 * $LastChangedDate$
 * $LastChangedBy$
 * $HeadURL$
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/fs.h>           /* has to be before <linux/cdev.h>! GRRR! */
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/syscalls.h>
#include <linux/time.h>
#include <linux/unistd.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/slab.h>		/* kmalloc, kfree */

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <nidas/linux/irigclock.h>
#include <nidas/linux/isa_bus.h>
#include <nidas/linux/SvnInfo.h>    // SVNREVISION
// #define DEBUG
#include <nidas/linux/klog.h>

#include "pc104sg.h"

static char* driver_name = "pc104sg";

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_DESCRIPTION("PC104-SG IRIG Card Driver");
MODULE_LICENSE("GPL");

/* SA_SHIRQ is deprecated starting in 2.6.22 kernels */
#ifndef IRQF_SHARED
# define IRQF_SHARED SA_SHIRQ
#endif

/* desired IRIG interrupt rate, in Hz */
#define INTERRUPT_RATE 100

/* desired IRIG clock frequency for A/D, in Hz */
static unsigned int A2DClockFreq = 10000;

/* module parameters (can be passed in via command line) */
static unsigned int Irq = 10;

static int IoPort = 0x2a0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)
module_param(Irq, int, 0);
module_param(IoPort, int, 0);
module_param(A2DClockFreq, int, 0);
#else
MODULE_PARM(Irq, "1i");
MODULE_PARM_DESC(Irq, "IRQ number");
MODULE_PARM(IoPort, "1i");
MODULE_PARM_DESC(IoPort, "I/O base address");
MODULE_PARM(A2DClockFreq, "1i");
MODULE_PARM_DESC(A2DClockFreq, "clock rate for A/D signal");
#endif

/** number of milliseconds per interrupt */
#define MSEC_PER_INTERRUPT (MSECS_PER_SEC / INTERRUPT_RATE)

/** number of 1/10ths of milliseconds per interrupt */
#define TMSEC_PER_INTERRUPT (TMSECS_PER_SEC / INTERRUPT_RATE)

/**
 * We schedule the bottom-half tasklet after every
 * INTERRUPTS_PER_TASKLET interrupts.
 */
#define INTERRUPTS_PER_TASKLET  1

/**
 * Increment of the software clock on each schedule of the tasklet.
 */
#define TMSEC_PER_SOFT_TIC (TMSEC_PER_INTERRUPT *		\
				 INTERRUPTS_PER_TASKLET)
/**
 * Allow for counting up to 10 seconds, so that we can do 0.1hz callbacks.
 */
#define MAX_TASKLET_COUNTER (10 * INTERRUPT_RATE * INTERRUPTS_PER_TASKLET)

#define CALLBACK_POOL_SIZE 64   /* number of callbacks we can support */

/**
 * Length of the circular buffer of output samples.
 */
#define PC104SG_SAMPLE_QUEUE_SIZE 16

/**
 * Values that are used to detect whether the bottom half tasklet
 * has gotten out of sync with real-time.
 */
#define MAX_TMSEC_SINCE_LAST_SYNC (5 * TMSECS_PER_SEC)

/**
 * A toggle buffer containing the current clock value
 * in tenths of milliseconds since UTC midnight.
 */
int TMsecClock[2] = { 0, 0 };

/**
 * Index into TMsecClock of the value to be read.
 */
int ReadClock = 0;

EXPORT_SYMBOL(TMsecClock);
EXPORT_SYMBOL(ReadClock);

/**
 * What to do with the with the software clock and loop counter
 * in the bottom-half tasklet.
 *
 * For USER_SET_REQUESTED, the software clock will be set once
 * from a time provided by the user if the card does not have sync,
 * and then adjusted from either the IRIG clock or unix clock.
 * This is not currently used.
 *
 * If USER_OVERRIDE_REQUESTED the software clock will be set once
 * from a time provided by the user in an ioctl and not thereafter
 * adjusted. USER_OVERRIDE_REQUESTED is not currently used and
 * could probably be removed.
 */
enum clockAction
{
        RESET_COUNTERS,
        NO_ACTION,
#ifdef SUPPORT_USER_SET
        USER_SET_REQUESTED,     /* user has requested to set the clock via ioctl */
#endif
#ifdef SUPPORT_USER_OVERRIDE
        USER_OVERRIDE_REQUESTED /* user has requested override of clock */
#endif
};

/**
 * Enumeration of the state of the software clock.
 */
enum clockState
{
        SYNCD_SET,              /* good state, clock set from irig clock */
        UNSYNCD_SET,            /* card does not have sync, set from unix clock */
#ifdef SUPPORT_USER_SET
        USER_SET,               /* software clock set from value provided by user */
#endif
#ifdef SUPPORT_USER_OVERRIDE
        USER_OVERRIDE           /* clock has been overridden */
#endif
};

/**
 * Whether to notify callback clients if clockAction is RESET_COUNTERS.
 */
enum notifyClients
{
        NO_NOTIFY,
        NOTIFY_CLIENTS
};

/**
 * One a second, or when clockAction == RESET_COUNTERS, the
 * interrupt service routine grabs a snapshot of the following values.
 */
struct clockSnapShot
{
        /**
         * The current time from the IRIG registers
         */
        struct timeval32 irig_time;             

        /**
         * current unix time
         */
        struct timeval32 unix_time;

        /**
         * value of the software clock
         */
        dsm_sample_time_t clock_time;

        /**
         * OR of the status values
         */
        unsigned char statusOr;

};

/**
 * Device structure used in the file operations of the character device
 * which provides IRIG samples.
 */
struct irig_device
{
        struct dsm_sample_circ_buf samples;     // samples for reading
        struct sample_read_state read_state;
        wait_queue_head_t rwaitq;
        unsigned int skippedSamples;
        /**
         * counter of the 1/sec samples written to buffer for user-side read.
         */
        unsigned char seqnum;
};

/**
 * Everything needed to access the board.
 */
struct pc104sg_board
{

        /**
         * Actual physical address of this card. Set in init_module
         */
        unsigned long addr;

        int irq;

        char deviceName[32];    /* assumed device name for log messages */

        /**
         * Linux device 
         */
        dev_t pc104sg_device;

        /**
         * Linux character device
         */
        struct cdev pc104sg_cdev;

        /**
         * Information needed by file operations.
         */
        struct irig_device* dev;

        /**
         * spinlock used to control access to irig_device member.
         * dev member is accessed by a tasklet in software interrupt
         * context so we must use a spinlock, not a mutex.
         */
        spinlock_t dev_lock;

        /**
         * How many concurrent opens of the device.
         */
        atomic_t num_opened;

        /**
         * The three possible interrupts generated by this card are enabled with
         * bits 5,6,7 of the status port:
         *
         *    5 = heartbeat
         *    6 = match
         *    7 = external-time-tag
         *
         * Writing a 0 to bits 0-4 causes other things, like board reset,
         * so we set those bits to 1 here in the interrupt mask.
         */
        unsigned char IntMask;

        /**
         * Spinlock to control concurrent access to board registers and
         * shared variables in this structure.
         */
        spinlock_t lock;

        /**
         * Our clock ticker, 1/10s of milliseconds since 00:00 GMT.
         * It is signed, since we are often computing time differences,
         * and it is handy to initialize it to -1.
         * There are 864,000,000 1/10 milliseconds in a day, so as a 32 bit
         * signed integer, it has enough range for +- 2.4 days, but generally we
         * restrict its value to the range 0:863,999,999.
         */
        int TMsecClockTicker;

        /**
         * Index into TMsecClock of the next clock value to be written.
         */
        int WriteClock;

        /*
         * The 100 Hz counter.
         */
        int count100Hz;

        /**
         * Current clock state.
         */
        enum clockState clockState;

        /**
         * Action to be performed at the beginning of the next 100 Hz tasklet,
         * either NO_ACTION or RESET_COUNTERS.
         */
        enum clockAction clockAction;

        /**
         * If clockAction is RESET_COUNTERS, whether to notify clients 
         * after resetting our software clocks.
         */
        enum notifyClients notifyClients;

        /**
         * Current status.
         */
        struct pc104sg_status status;

        /**
         * Value of last status bits, so we can detect SYNC/NOSYNC transitions
         */
        unsigned char lastStatus;

        /**
         * last time that software clock was set against a sync'd irig clock.
         */
        int lastSyncTime;

        /**
         * Set to true if ISR should send requests for status from dual-ported RAM.
         */
        int DP_RamExtStatusEnabled;

        /**
         * Set to true if ISR has sent a request for status from dual-ported RAM.
         */
        int DP_RamExtStatusRequested;

        /*
         * Tasklet which calls registered callbacks.
         */
        struct tasklet_struct tasklet100Hz;

        /**
         * How many 100Hz ticks are yet unhandled in tasklet?
         */
        atomic_t pending100Hz;

        struct irig_callback *oneHzCallback;

        /**
         * Snapshot of the hardware and software clock and status.
         */
        struct clockSnapShot snapshot;

        /**
         * Set to true if the interrupt service routine should take a snapshot
         */
        int doSnapShot;

        /**
         * Toward the end of a second, the bottom half asks the ISR for
         * a clock snapshot.  This is used so that it doesn't ask twice
         * in the second.
         */
        int askedSnapShot;

        /**
         * Snapshot taken by the ISR when clockAction == RESET_COUNTERS.
         */
        struct clockSnapShot resetSnapshot;

        int resetSnapshotDone;

        /**
         * An OR of the status bits since the last snapshot
         */
        unsigned char statusOr;

        /*
         * Active callback entries for each rate
         */
        struct list_head CallbackLists[IRIG_NUM_RATES];

        /**
         * Pool of allocated callback entries
         */
        struct list_head CallbackPool;

        /**
         * Spinlock to control concurrent access to callback lists.
         */
        spinlock_t cblist_lock;

        /**
         * Callback entries that are to be added to the active list
         * on next pass though loop
         */
        struct list_head pendingAdds;

        /**
         * Callback entries that are to be removed on next pass though loop
         */
        struct irig_callback *pendingRemoves[CALLBACK_POOL_SIZE];

        int nPendingRemoves;

        atomic_t nPendingCallbackChanges;

        /**
         * wait_queue for tasks that want to wait until their callback is
         * definitely not running.
         */
        wait_queue_head_t callbackWaitQ;

#if defined(SUPPORT_USER_SET) || defined(SUPPORT_USER_OVERRIDE)
        /**
         * value passed by user via ioctl who wants to set the IRIG clock.
         */
        struct timeval32 userClock;
#endif


};

static struct pc104sg_board board;

/**
 * The year field in the pc104sg time registers
 * ranges from 0-99, so we keep track of the century.
 */
static int StaticYear;


/** macros borrowed from glibc/time functions */
#define SECS_PER_HOUR   (60 * 60)

#ifndef SECS_PER_DAY
#define SECS_PER_DAY    (SECS_PER_HOUR * 24)
#endif

/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
#define my_isleap(year)							\
    ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

#define DIV(a, b) ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y) (DIV (y, 4) - DIV (y, 100) + DIV (y, 400))


static const char *clockStateString(void)
{
        switch (board.clockState) {
        case SYNCD_SET:
                return "SYNCD_SET";
        case UNSYNCD_SET:
                return "UNSYNCD_SET";
#ifdef SUPPORT_USER_SET
        case USER_SET:
                return "USER_SET";
#endif
#ifdef SUPPORT_USER_OVERRIDE
        case USER_OVERRIDE:
                return "USER_OVERRIDE";
#endif
        default:
                break;
        }
        return "unknown";
}

/**
 * Module function that allows other modules to register their callback
 * function to be called at the given rate.  register_irig_callback
 * can be called at anytime.
 * Note that register_irig_callback and unregister_irig_callback do not
 * alter the active callback lists but instead add requests
 * to a group of pending requests.  The loop function, pc104sg_bh_100Hz,
 * does not do any locking on the active list.  Once on each pass
 * through the loop it acts on the pending requests.
 */
struct irig_callback *register_irig_callback(irig_callback_func * callback,
                                             irig_callback_func * resync,
                                             enum irigClockRates rate,
                                             void *privateData, int *errp)
{
        struct list_head *ptr;
        struct irig_callback *cbentry;
        *errp = 0;

        if (rate >= IRIG_NUM_RATES) {
                *errp = -EINVAL;
                return 0;
        }

        /* We could do a kmalloc of the entry here, but so that
         * we don't have to worry about whether to
         * use GFP_KERNEL or GFP_ATOMIC, we instead allocate
         * a pool of entries in the module init function.
         */
        spin_lock_bh(&board.cblist_lock);

        ptr = board.CallbackPool.next;
        if (ptr == &board.CallbackPool) {       /* none left */
                spin_unlock_bh(&board.cblist_lock);
                *errp = -ENOMEM;
                return 0;
        }

        cbentry = list_entry(ptr, struct irig_callback, list);
        list_del(&cbentry->list);

        cbentry->callback = callback;
        cbentry->resyncCallback = resync;
        cbentry->privateData = privateData;
        cbentry->rate = rate;

        list_add(&cbentry->list, &board.pendingAdds);
        atomic_inc(&board.nPendingCallbackChanges);

        spin_unlock_bh(&board.cblist_lock);

        return cbentry;
}

EXPORT_SYMBOL(register_irig_callback);

/**
 * Modules call this function to dequeue their callbacks.
 * A callback function can cancel itself, or any other
 * callback function.
 * @return 0: OK, but callback might be called once more. Do flush_irig_callback
 *         to wait until it is definitely finished.
 *      <0: errno
 */
int unregister_irig_callback(struct irig_callback *cb)
{
        int ret = 0;

        spin_lock_bh(&board.cblist_lock);

        if (cb->rate < 0 || cb->rate >= IRIG_NUM_RATES)
                ret = -EINVAL;

        board.pendingRemoves[board.nPendingRemoves++] = cb;
        atomic_inc(&board.nPendingCallbackChanges);

        spin_unlock_bh(&board.cblist_lock);
        return ret;
}

EXPORT_SYMBOL(unregister_irig_callback);

/*
 * Wait until there are no pending callback changes */
int flush_irig_callbacks(void)
{
        if (wait_event_interruptible(board.callbackWaitQ,
                atomic_read(&board.nPendingCallbackChanges) == 0))
                return -ERESTARTSYS;
        return 0;
}

EXPORT_SYMBOL(flush_irig_callbacks);

/**
 * Handle pending adds and removes of callbacks from the active list
 * for the appropriate rate.
 */
static void handlePendingCallbacks(void)
{
        struct list_head *ptr;
        struct irig_callback *cbentry;
        int i;

        /* Remove pending callbacks from the active lists. */
        for (i = 0; i < board.nPendingRemoves; i++) {
                cbentry = board.pendingRemoves[i];
                /* remove entry from the active list for the rate */
                list_del(&cbentry->list);
                /* and add back to the pool. */
                list_add(&cbentry->list, &board.CallbackPool);
                atomic_dec(&board.nPendingCallbackChanges);
        }
        board.nPendingRemoves = 0;

        /* Adding pending callbacks to the active list for the appropriate rate. */
        for (ptr = board.pendingAdds.next; ptr != &board.pendingAdds;) {
                cbentry = list_entry(ptr, struct irig_callback, list);
                ptr = ptr->next;
                /* remove entry from pendingAdds list */
                list_del(&cbentry->list);
                /* add to active list for rate */
                list_add(&cbentry->list,
                         board.CallbackLists + cbentry->rate);
                atomic_dec(&board.nPendingCallbackChanges);
        }
        if (atomic_read(&board.nPendingCallbackChanges) == 0)
            wake_up_interruptible(&board.callbackWaitQ);
}

/**
 * Cleanup function that un-registers all callbacks.
 */
static void free_callbacks(void)
{
        int i;

        struct list_head *ptr;
        struct irig_callback *cbentry;

        spin_lock_bh(&board.cblist_lock);
        handlePendingCallbacks();

        for (i = 0; i < IRIG_NUM_RATES; i++) {
                for (ptr = board.CallbackLists[i].next;
                     ptr != board.CallbackLists + i;
                     ptr = board.CallbackLists[i].next) {
                        cbentry =
                            list_entry(ptr, struct irig_callback, list);
                        /* remove it from the list for the rate, and add to the pool. */
                        list_del(&cbentry->list);
                        list_add(&cbentry->list, &board.CallbackPool);
                }
        }

        for (ptr = board.CallbackPool.next; ptr != &board.CallbackPool;
             ptr = board.CallbackPool.next) {
                cbentry = list_entry(ptr, struct irig_callback, list);
                list_del(&cbentry->list);
                kfree(cbentry);
        }

        spin_unlock_bh(&board.cblist_lock);
}

/**
 * After receiving a heartbeat interrupt, one must reset the heartbeat
 * latch in the handler.  Otherwise either of two bad things will happen,
 * depending on how the system is configured to handle interrupt levels:
 *  1. the interrupt handler will be called again immediately after it returns,
 *      resulting in a system lockup.
 *  2. you won't get any further interrupts.
 */
static void inline resetHeartBeatLatch(void)
{
        /* reset heart beat latch, write a 0 to bit 4, leave others alone */
        outb(board.IntMask & ~Heartbeat, board.addr + Reset_Port);
}

/**
 * Enable heart beat interrupts
 */
static void enableHeartBeatInt(void)
{
        board.IntMask |= Heartbeat_Int_Enb;
#ifdef DEBUG
        KLOG_DEBUG("IntMask=0x%x\n", board.IntMask);
#endif
        // also reset heart beat latch to avoid immediate interrupt
        resetHeartBeatLatch();     
}

// /**
//  * Disable heart beat interrupts
//  */
// static void 
// disableHeartBeatInt(void)
// {
//     board.IntMask &= ~Heartbeat_Int_Enb;
//     outb(board.IntMask, board.addr + Reset_Port);
// }

/**
 * After receiving a match interrupt, one must reset
 * the match flag in order to receive further interrupts.
 */
static void inline resetMatchLatch(void)
{
        /* reset match flag, write a 0 to bit 3, leave others alone */
        outb(board.IntMask & ~Match, board.addr + Reset_Port);
}

// /**
//  * Enable external time tag interrupt. These are caused by
//  * a TTL input on a pin, and allows one to tag external
//  * events.  This may be useful for synchronization tests of DSMs.
//  */
// static void 
// enableExternEventInt(void)
// {
//     board.IntMask |= Ext_Ready_Int_Enb;
//     outb(board.IntMask, board.addr + Reset_Port);
// }

// /**
//  * Disable external time tag interrupt.
//  */
// static void 
// disableExternEventInt(void)
// {
//     board.IntMask &= ~Ext_Ready_Int_Enb;
//     outb(board.IntMask, board.addr + Reset_Port);
// }

static void disableAllInts(void)
{
        /* disable all interrupts */
        board.IntMask = 0x1f;
#ifdef DEBUG
        KLOG_DEBUG("IntMask=0x%x\n", board.IntMask);
#endif
        outb(board.IntMask, board.addr + Reset_Port);
}

/**
 * Read dual port RAM.
 * @param addr	dual-port RAM address to read
 *
 * Called by (all by module init)
 *      setHeartBeatOutput(int rate)
 *      setPrimarySyncReference(unsigned char val)
 *      setRate2Output(int rate)
 *          init
 */
static int ReadDualPortRAM(unsigned char addr, unsigned char *val)
{
        int attempts;
        int waitcount;
        int ret = -1;
        unsigned long flags;
        unsigned char status = 0;
        unsigned int delay_usec = 10;  // wait time in microseconds

        spin_lock_irqsave(&board.lock, flags);

        for (attempts = 0; attempts < 10; attempts++) {
                if (attempts > 0) {
                        /* 
                         * Unlock and wait briefly before regaining the lock and trying
                         * again.
                         */
                        spin_unlock_irqrestore(&board.lock, flags);
                        KLOG_NOTICE("try again\n");
                        udelay(200);
                        spin_lock_irqsave(&board.lock, flags);
                }

                /* clear Response_Ready */
                inb(board.addr + Dual_Port_Data_Port);

                /* select dual port address */
                outb(addr, board.addr + Dual_Port_Address_Port);
                mb();

                /* 
                 * wait for the PC104SG to acknowledge
                 */
                for (waitcount = 0; waitcount < 10; waitcount++) {
                        udelay(delay_usec);
                        status = inb(board.addr + Extended_Status_Port);
                        if ((status & Response_Ready) != 0)
                                break;
                }

#ifdef DEBUG
                if (waitcount > 3)
                        KLOG_DEBUG
                            ("ReadDualPortRAM, waitcount=%d (* %d us)\n",
                             waitcount, delay_usec);
#endif

                /* check for a time out on the response... */
                if ((status & Response_Ready) == 0) {
                        KLOG_NOTICE("timed out...\n");
                        continue;
                }

                /* read the requested value */
                *val = inb(board.addr + Dual_Port_Data_Port);

                /* success */
                if (attempts > 0)
                        KLOG_NOTICE("done\n");
                ret = 0;
                goto done;
        }

        /* failure */
        KLOG_WARNING("failed dual-port read after %d attempts\n",
                     attempts);
        ret = -1;

      done:
        spin_unlock_irqrestore(&board.lock, flags);
        return ret;
}


/**
 * The RequestDualPortRAM()/GetRequestedDualPortRAM() pair allow us to
 * break a dual-port read into two parts.  This is used by
 * the interrupt service routine.  Each entry into the ISR
 * reads the value ready from the previous request, then issues
 * another request.  In this way, the necessary delay between
 * requesting dual-port RAM and actually getting
 * the value happens between interrupts, rather than by a busy wait.
 *
 * If non ISR code wants to access dual ported RAM, it should do the
 * following, which prevents the ISR from accessing it, without disabling
 * interrupts while accessing the DP ram:
 *	spin_lock_irqsave(&board.lock, flags);
 *	board.DP_RamExtStatusEnabled = 0;
 *	board.DP_RamExtStatusRequested = 0;
 *	spin_unlock_irqrestore(&board.lock, flags);
 * Then access the RAM, for example:
 *      setYear(n);
 *      ...
 * Then re-enable access by the ISR
 *	spin_lock_irqsave(&board.lock, flags);
 *	board.DP_RamExtStatusEnabled = 1;
 *	spin_unlock_irqrestore(&board.lock, flags);
 */

static inline void RequestDualPortRAM(unsigned char addr)
{
        /* clear Response_Ready */
        inb(board.addr + Dual_Port_Data_Port);

        /* specify dual port address */
        outb(addr, board.addr + Dual_Port_Address_Port);
}

static inline void GetRequestedDualPortRAM(unsigned char *val)
{
        static int ntimeouts = 0;
        unsigned char status;
        status = inb(board.addr + Extended_Status_Port);

        /* make sure the response is ready */
        if (!(status & Response_Ready)) {
                if (!(++ntimeouts % 100))
                        KLOG_WARNING("%d timeouts\n", ntimeouts);
                return;         // val is unchanged
        }

        /* return requested dual-port value */
        *val = inb(board.addr + Dual_Port_Data_Port);
}

/**
 * Set a value in dual port RAM.
 * @param addr	dual-port RAM address to write
 *
 * Called by:
 *      setHeartBeatOutput(int rate)
 *      setPrimarySyncReference(unsigned char val)
 *      setTimeCodeInputSelect(unsigned char val)
 *      setRate2Output(int rate)
 *      counterRejam(void)
 *          module init
 *      setYear(int val)
 *      setMajorTime(struct irigTime* ti)
 *          ioctl
 */
static int WriteDualPortRAM(unsigned char addr, unsigned char value)
{
        int attempts;
        int waitcount;
        int ret = -1;
        unsigned char status = 0;
        unsigned long flags;
        unsigned int delay_usec = 10;  // wait time in microseconds

        spin_lock_irqsave(&board.lock, flags);

        for (attempts = 0; attempts < 10; attempts++) {
                if (attempts > 0) {
                        spin_unlock_irqrestore(&board.lock, flags);
                        KLOG_NOTICE("try again\n");
                        udelay(200);    /* wait briefly before trying again */
                        spin_lock_irqsave(&board.lock, flags);
                }

                /* clear Response_Ready */
                inb(board.addr + Dual_Port_Data_Port);

                /* select dual port address */
                outb(addr, board.addr + Dual_Port_Address_Port);
                mb();

                /* wait for PC104SG to acknowledge */
                for (waitcount = 0; waitcount < 10; waitcount++) {
                        udelay(delay_usec);
                        status = inb(board.addr + Extended_Status_Port);
                        if ((status & Response_Ready) != 0)
                                break;
                }

                if (waitcount > 3)
                        KLOG_DEBUG("WriteDualPortRAM 1, waitcount=%d\n",
                                   waitcount);

                /* check for a time out on the response... */
                if ((status & Response_Ready) == 0) {
                        KLOG_NOTICE("timed out 1...\n");
                        continue;       /* try again */
                }

                /* clear Response_Ready */
                inb(board.addr + Dual_Port_Data_Port);

                /* write new value to DP RAM */
                outb(value, board.addr + Dual_Port_Data_Port);
                mb();

                for (waitcount = 0; waitcount < 10; waitcount++) {
                        udelay(delay_usec);
                        status = inb(board.addr + Extended_Status_Port);
                        if ((status & Response_Ready) != 0)
                                break;
                }

                if (waitcount > 3)
                        KLOG_DEBUG("WriteDualPortRAM 2, waitcount=%d\n",
                                   waitcount);

                /* check for a time out on the response... */
                if ((status & Response_Ready) == 0) {
                        KLOG_NOTICE("timed out 2...\n");
                        continue;       /* try again */
                }

                /*
                 * Wait a bit more here.  Why?  I don't know.  But this (or
                 * a KLOG to print a message here) seems to create enough
                 * delay to make setRate2Output() work properly.
                 */
                udelay(50);

                /* check that the written value matches */
                if (inb(board.addr + Dual_Port_Data_Port) != value) {
                        KLOG_WARNING("no match on read-back\n");
                        continue;       /* try again */
                }

                /* success */
                if (attempts > 0)
                        KLOG_NOTICE("done\n");
                ret = 0;
                goto done;
        }


        /* failure */
        KLOG_WARNING("failed dual-port write after %d attempts\n",
                     attempts);
        ret = -1;

      done:
        spin_unlock_irqrestore(&board.lock, flags);
        return ret;
}


/* This controls COUNTER 1 on the PC104SG card */
static void setHeartBeatOutput(int rate)
{
        int ticks_3MHz;
        int attempts;
        unsigned char lsb, msb;
        unsigned char test;

        ticks_3MHz = 3000000 / rate;    // How many ticks of the 3 MHz clock?

        lsb = (unsigned char) (ticks_3MHz & 0xff);
        msb = (unsigned char) (ticks_3MHz >> 8);

        for (attempts = 0; attempts < 10; attempts++) {
                if (attempts > 0) {
                        KLOG_NOTICE("try again\n");
                        udelay(100);
                }

                WriteDualPortRAM(DP_Ctr1_ctl,
                                 DP_Ctr1_ctl_sel | DP_ctl_rw | DP_ctl_mode3
                                 | DP_ctl_bin);
                WriteDualPortRAM(DP_Ctr1_lsb, lsb);
                WriteDualPortRAM(DP_Ctr1_msb, msb);

                ReadDualPortRAM(DP_Ctr1_lsb, &test);
                if (test != lsb) {
                        KLOG_WARNING("LSB does not match!\n");
                        continue;
                }

                ReadDualPortRAM(DP_Ctr1_msb, &test);
                if (test != msb) {
                        KLOG_WARNING("MSB does not match!\n");
                        continue;
                }
                return;         // success!
        }

        KLOG_WARNING("failed after %d attempts\n", attempts);
        return;
}

/**
 * Set the primary time reference.
 * @param val 0=PPS is primary time reference, 1=time code is primary
 */
static void setPrimarySyncReference(unsigned char val)
{
        unsigned char control0;
        ReadDualPortRAM(DP_Control0, &control0);

        if (val)
                control0 |= DP_Control0_CodePriority;
        else
                control0 &= ~DP_Control0_CodePriority;

#ifdef DEBUG
        KLOG_DEBUG("setting DP_Control0 to 0x%x\n", control0);
#endif
        WriteDualPortRAM(DP_Control0, control0);
}

static void setTimeCodeInputSelect(unsigned char val)
{
        WriteDualPortRAM(DP_CodeSelect, val);
}

// static void 
// getTimeCodeInputSelect(unsigned char *val)
// {
//     ReadDualPortRAM(DP_CodeSelect, val);
// }

/* -- Utility --------------------------------------------------------- */

/* 
 * Set the frequency for the Rate2 signal from the card.
 */
void setRate2Output(int rate)
{
        int attempts;
        int ticks_3MHz;
        unsigned char lsb, msb;
        unsigned char test;

        KLOG_INFO("setting rate 2 signal frequency to %d Hz\n", rate);
        ticks_3MHz = 3000000 / rate;

        lsb = (unsigned char) (ticks_3MHz & 0xff);
        msb = (unsigned char) (ticks_3MHz >> 8);
        for (attempts = 0; attempts < 10; attempts++) {
                if (attempts > 0) {
                        KLOG_NOTICE("try again\n");
                        udelay(100);
                }

                WriteDualPortRAM(DP_Ctr0_ctl,
                                 DP_Ctr0_ctl_sel | DP_ctl_rw | DP_ctl_mode3
                                 | DP_ctl_bin);
                WriteDualPortRAM(DP_Ctr0_lsb, lsb);
                WriteDualPortRAM(DP_Ctr0_msb, msb);

                ReadDualPortRAM(DP_Ctr0_lsb, &test);
                if (test != lsb) {
                        KLOG_WARNING("LSB does not match!\n");
                        continue;
                }

                ReadDualPortRAM(DP_Ctr0_msb, &test);
                if (test != msb) {
                        KLOG_WARNING("MSB does not match!\n");
                        continue;
                }
                return;         // success!
        }

        KLOG_WARNING("failed after %d attempts!\n", attempts);
        return;
}

static void counterRejam(void)
{
        WriteDualPortRAM(DP_Command, Command_Rejam);
}

/* convenience function to read current unix time into a struct timeval32 */
static void do_gettimeofday_tv32(struct timeval32* tv32)
{
        struct timeval tv;
        do_gettimeofday(&tv);
        tv32->tv_sec = tv.tv_sec;
        tv32->tv_usec = tv.tv_usec;
}

/**
 * Read time from the registers.
 * Set offset to 0 to read main clock.
 * Set offset to 0x10 to read time of external pulse.
 *
 * Data are stored in BCD form as 4 byte niblets, containing ones, tens or
 * hundreds value for the respective time fields.
 *
 * Called by:
 *     get_irig_time(&ti);
 *         ISR
 *     ioctl
 *         user space
 *     irig_clock_gettime(struct timespec* tp)
 *         other modules
 *
 * Since reading the Usec1_Nsec100 value latches the other digits,
 * all these calls hold a spin_lock before calling getTimeFields().
 */
static void getTimeFields(struct irigTime *ti, int offset)
{
        unsigned char us0ns2, us2us1, ms1ms0, sec0ms2, min0sec1, hour0min1;
        unsigned char day0hour1, day2day1, year1year0;

        /* reading the Usec1_Nsec100 value latches all other digits */
        us0ns2 = inb(board.addr + offset + Usec1_Nsec100_Port); //0x0f
        us2us1 = inb(board.addr + offset + Usec100_Usec10_Port);        //0x0e
        ms1ms0 = inb(board.addr + offset + Msec10_Msec1_Port);  //0x0d
        sec0ms2 = inb(board.addr + offset + Sec1_Msec100_Port); //0x0c
        min0sec1 = inb(board.addr + offset + Min1_Sec10_Port);  //0x0b
        hour0min1 = inb(board.addr + offset + Hr1_Min10_Port);  //0x0a
        day0hour1 = inb(board.addr + offset + Day1_Hr10_Port);  //0x09
        day2day1 = inb(board.addr + offset + Day100_Day10_Port);        //0x08
        year1year0 = inb(board.addr + offset + Year10_Year1_Port);      //0x07

        /*
         * Time code inputs do not contain year information.
         * The 10s and 1s digits of year must be initialized by setting
         * DP_Year10_Year (as done in setYear). Otherwise the year defaults to 0.
         *
         * The year field does rollover correctly at the end of the year.
         * Test1:
         * Set major to 1999 Dec 31 23:59 (yday=365, non-leap year)
         * rolled over from year=99, yday=365, to year=0, yday=1
         * Test2:
         * Set major to 2004 Dec 31 23:59 (yday=366, leap)
         * rolled over from year=4, yday=366, to year=5, yday=1
         */

        ti->year = (year1year0 >> 4) * 10 + (year1year0 & 0x0f);

        /* After cold start the year field is not set, and it
         * takes some time before the setYear to DPR takes effect.
         * I saw values of 165 for the year during this time.
         */
        if (board.lastStatus & DP_Extd_Sts_NoYear) {
                // KLOG_DEBUG("fixing year=%d to %d\n", ti->year, StaticYear);
                ti->year = StaticYear;
        }
        // This has a Y2K problem, but who cares - it was written in 2004 and
        // it's for a real-time data system!
        else
                ti->year += (StaticYear / 100) * 100;

        ti->yday = ((day2day1 >> 4) * 100) + ((day2day1 & 0x0f) * 10) +
            (day0hour1 >> 4);
        ti->hour = (day0hour1 & 0x0f) * 10 + (hour0min1 >> 4);
        ti->min = (hour0min1 & 0x0f) * 10 + (min0sec1 >> 4);
        ti->sec = (min0sec1 & 0x0f) * 10 + (sec0ms2 >> 4);
        ti->msec = ((sec0ms2 & 0x0f) * 100) + ((ms1ms0 >> 4) * 10) +
            (ms1ms0 & 0x0f);
        ti->usec =
            ((us2us1 >> 4) * 100) + ((us2us1 & 0x0f) * 10) + (us0ns2 >> 4);
        ti->nsec = (us0ns2 & 0x0f) * 100;
}

/**
 * Read sub-second time fields from the card, return microseconds.
 * May be useful for watching-the-clock when debugging.
 */
int getTimeUsec()
{
        unsigned char us0ns2, us2us1, ms1ms0, sec0ms2;
        int usec;

        /* reading the Usec1_Nsec100 value latches all other digits */
        us0ns2 = inb(board.addr + Usec1_Nsec100_Port);
        us2us1 = inb(board.addr + Usec100_Usec10_Port);
        ms1ms0 = inb(board.addr + Msec10_Msec1_Port);
        sec0ms2 = inb(board.addr + Sec1_Msec100_Port);

        usec = (((sec0ms2 & 0x0f) * 100) + ((ms1ms0 >> 4) * 10) +
                (ms1ms0 & 0x0f)) * USECS_PER_MSEC +
            ((us2us1 >> 4) * 100) + ((us2us1 & 0x0f) * 10) + (us0ns2 >> 4);
        return usec;
}

/**
 * Get main clock.
 */
static void get_irig_time(struct irigTime *ti)
{
        getTimeFields(ti, 0);
#ifdef DEBUG
        {
                int td, hr, mn, sc;
                // unsigned char status = inb(board.addr + Status_Port);
                dsm_sample_time_t tt = GET_TMSEC_CLOCK;
                struct timespec ts;
                irigTotimespec(ti, &ts);
                // clock difference
                td = (ts.tv_sec % SECS_PER_DAY) * TMSECS_PER_SEC +
                    ts.tv_nsec / NSECS_PER_TMSEC - tt;
                hr = (tt / 3600 / TMSECS_PER_SEC);
                tt %= (3600 * TMSECS_PER_SEC);
                mn = (tt / 60 / TMSECS_PER_SEC);
                tt %= (60 * TMSECS_PER_SEC);
                sc = tt / TMSECS_PER_SEC;
                tt %= TMSECS_PER_SEC;
                KLOG_DEBUG("%04d %03d %02d:%02d:%02d.%03d %03d %03d, "
                           "clk=%02d:%02d:%02d.%04d, diff=%d tmsec, estat=0x%x, state=%d\n",
                           ti->year, ti->yday, ti->hour, ti->min, ti->sec,
                           ti->msec, ti->usec, ti->nsec, hr, mn, sc,
                           (int) tt, td, board.lastStatus,
                           board.clockState);
        }
#endif
}

/**
 * Break a struct timeval32 into the fields of a struct irigTime.
 * This uses some code from glibc/time routines.
 */
static void timespecToirig(const struct timespec *ts, struct irigTime *ti)
{
        int days, rem, y;
        unsigned int t = ts->tv_sec;

        days = t / SECS_PER_DAY;
        rem = t % SECS_PER_DAY;
        ti->hour = rem / SECS_PER_HOUR;
        rem %= SECS_PER_HOUR;
        ti->min = rem / 60;
        ti->sec = rem % 60;
        y = 1970;

        while (days < 0 || days >= (my_isleap(y) ? 366 : 365)) {
                /* Guess a corrected year, assuming 365 days per year.  */
                int yg = y + days / 365 - (days % 365 < 0);

                /* Adjust DAYS and Y to match the guessed year.  */
                days -= ((yg - y) * 365 + LEAPS_THRU_END_OF(yg - 1)
                         - LEAPS_THRU_END_OF(y - 1));
                y = yg;
        }
        ti->year = y;
        ti->yday = days + 1;    // irig uses 1-366, unix 0-365

        rem = ts->tv_nsec;
        ti->msec = rem / NSECS_PER_MSEC;
        rem %= NSECS_PER_MSEC;
        ti->usec = rem / NSECS_PER_USEC;
        rem %= NSECS_PER_USEC;
        ti->nsec = rem;
}

static void timeval32Toirig(const struct timeval32 *tv, struct irigTime *ti)
{
        struct timespec ts;
        ts.tv_sec = tv->tv_sec;
        ts.tv_nsec = tv->tv_usec * NSECS_PER_USEC;
        timespecToirig(&ts, ti);
}

static void timevalToirig(const struct timeval *tv, struct irigTime *ti)
{
        struct timespec ts;
        ts.tv_sec = tv->tv_sec;
        ts.tv_nsec = tv->tv_usec * NSECS_PER_USEC;
        timespecToirig(&ts, ti);
}

/**
 * Convert a struct irigTime into a struct timespec.
 */
static void irigTotimespec(const struct irigTime *ti, struct timespec *ts)
{
        int y = ti->year;
        int nleap = LEAPS_THRU_END_OF(y - 1) - LEAPS_THRU_END_OF(1969);

        ts->tv_nsec =
            ti->msec * NSECS_PER_MSEC + ti->usec * NSECS_PER_USEC +
            ti->nsec;

        ts->tv_sec = (y - 1970) * 365 * SECS_PER_DAY +
            (nleap + ti->yday - 1) * SECS_PER_DAY +
            ti->hour * 3600 + ti->min * 60 + ti->sec;
}

/**
 * Convert a struct irigTime into a struct timeval32
 */
static void irigTotimeval32(const struct irigTime *ti, struct timeval32* tv)
{
        int y = ti->year;
        int nleap = LEAPS_THRU_END_OF(y - 1) - LEAPS_THRU_END_OF(1969);

        tv->tv_usec =
            ti->msec * USECS_PER_MSEC + ti->usec +
            (ti->nsec + NSECS_PER_USEC / 2) / NSECS_PER_USEC;

        tv->tv_sec = (y - 1970) * 365 * SECS_PER_DAY +
            (nleap + ti->yday - 1) * SECS_PER_DAY +
            ti->hour * 3600 + ti->min * 60 + ti->sec;

        if (tv->tv_usec > USECS_PER_SEC)
                KLOG_INFO("sec=%d,usec=%d\n",tv->tv_sec,tv->tv_usec);
}

/* convenience function to read current irig time into a struct timeval32 */
static void get_irig_time_tv32(struct timeval32* tv32)
{
        struct irigTime ti;
        get_irig_time(&ti);
        irigTotimeval32(&ti,tv32);
}

/**
 * This function is available for use by external modules.
 * Note that it disables interrupts.
 */
void irig_clock_gettime(struct timespec *tp)
{
        struct irigTime it;
        unsigned long flags;

        spin_lock_irqsave(&board.lock, flags);
        get_irig_time(&it);
        spin_unlock_irqrestore(&board.lock, flags);

        irigTotimespec(&it, tp);
}

/* this function is available for external use */
int get_msec_clock_resolution()
{
        return MSEC_PER_INTERRUPT;
}

// /**
//  * Get external event time.
//  */
// static void 
// getExtEventTime(struct irigTime* ti) {
//     return getTimeFields(ti, 0x10);
// }

/**
 * set the year fields in Dual Port RAM.
 */
static void setYear(int val)
{
        StaticYear = val;
#ifdef DEBUG
        KLOG_DEBUG("setYear=%d\n", val);
#endif
        WriteDualPortRAM(DP_Year1000_Year100,
                         ((val / 1000) << 4) + ((val % 1000) / 100));
        val %= 100;
        WriteDualPortRAM(DP_Year10_Year1, ((val / 10) << 4) + (val % 10));

        WriteDualPortRAM(DP_Command, Command_Set_Years);
}

/**
 * The major time consists of the day-of-year, hour, minute
 * and second fields.  Ideally they are set via the time-code
 * input, but this function can be used if there is no time-code.
 *
 * The sub-second values are determined from the PPS input,
 * and I see no way to change them if there is no PPS or time-code.
 */
static int setMajorTime(struct irigTime *ti)
{
        int val;

#ifdef DEBUG
        // unsigned char status = inb(board.addr + Status_Port);
        KLOG_DEBUG("setMajor=%04d %03d %02d:%02d:%02d.%03d %03d %03d, "
                   "estat=0x%x, state=%d\n",
                   ti->year, ti->yday, ti->hour, ti->min, ti->sec,
                   ti->msec, ti->usec, ti->nsec, board.lastStatus,
                   board.clockState);
#endif
        /* The year fields in Dual Port RAM are not technically
         * part of the major time, but we'll set them too.  */
        setYear(ti->year);

        val = ti->yday;
        WriteDualPortRAM(DP_Major_Time_d100, val / 100);
        val %= 100;
        WriteDualPortRAM(DP_Major_Time_d10d1,
                         ((val / 10) << 4) + (val % 10));

        val = ti->hour;
        WriteDualPortRAM(DP_Major_Time_h10h1,
                         ((val / 10) << 4) + (val % 10));

        val = ti->min;
        WriteDualPortRAM(DP_Major_Time_m10m1,
                         ((val / 10) << 4) + (val % 10));

        val = ti->sec;
        WriteDualPortRAM(DP_Major_Time_s10s1,
                         ((val / 10) << 4) + (val % 10));

        WriteDualPortRAM(DP_Command, Command_Set_Major);

        return 0;
}

/**
 * Set the software clock and the tasklet loop counter based on a clock
 * value in a timeval struct.
 *
 * This function is called by the bottom half tasklet if clockAction==RESET_COUNTERS.
 *
 * Note: we are seeing sub-millisecond agreement between the IRIG
 * register clocks and the system clock (conditioned by NTP),
 * with a GPS-conditioned timeserver providing the IRIG and NTP references.
 *
 * This function sets TMsecClockTicker from the timeval.
 *
 */
static int setSoftTickers(struct timeval32 *tv,int round)
{
        int counter, newClock;

        newClock = (tv->tv_sec % SECS_PER_DAY) * TMSECS_PER_SEC +
            tv->tv_usec / USECS_PER_TMSEC;
        if (round) newClock += TMSEC_PER_INTERRUPT / 2;
        newClock -= newClock % TMSEC_PER_INTERRUPT;
        newClock %= TMSECS_PER_DAY;

        board.TMsecClockTicker = newClock;

        counter = newClock / TMSEC_PER_SOFT_TIC;
        counter %= MAX_TASKLET_COUNTER;

        atomic_set(&board.pending100Hz, 0);

        return counter;
}

/**
 * Try to determine if our software clock and client callbacks are keeping 
 * up with real-time.
 *
 * Other pc104 cards are receiving clock signals from this pc104sg card.
 * If they want to do polling, or other periodic things, they
 * register with this module for their callbacks to be called at a
 * given rate by the 100Hz tasklet.  We want to avoid missing a
 * callback if at all possible.
 *
 * It seems to happen, especially on vulcans, that a CPU card can miss
 * one or more IRIG interrupts. When that happens, then
 * one or more schedules of the bottom half tasklet are missed, which
 * means that the software clock and the callbacks get behind.
 *
 * This function compares the difference between the newClock value
 * which was calculated from either the irig or unix clock, and
 * currClock, which is the current software clock we provide to
 * other modules.  The difference is calculated as a number of
 * 0.01 second delta-Ts.
 *
 * This dT check is performed once a second.
 *
 * If the clock difference is within limits of
 *      (IRIG_MAX_DT_DIFF / scale : IRIG_MIN_DT_DIFF / scale)
 * then the pendingH100Hz counter is adjusted by the difference, so that
 * the tasklet will be either scheduled more or less rapidly than 100 Hz
 * for a period.
 *
 * If the difference is negative, it is likely that a previous positive
 * result was in error, or that spurious interrupts occurred. The latter
 * doesn't seem to happen.
 *
 * The hope is that in either case the software clock and callback
 * loop counter get back in sync with the clock signals that were
 * sent from the IRIG card to the other cards.
 *
 * If the difference is outside those limits, then set clockAction is set
 * to RESET_COUNTERS, so that the tasklet will reset the loop counter and the
 * software clock the next time it is scheduled, and also call the resync
 * callbacks of the clients, so that they could possibly resync their
 * processing.
 *
 * If the out-of-sync condition is caused by missed interrupts,
 * or a temporary glitch in the irig clock, where it later recovers
 * to be in sync with the output signals, this method should fix
 * the problem.
 *
 * Since we don't have any other details about the clock signals
 * that were sent to other cards, we only have the value of the irig
 * clock and unix clock to try to re-synchronize.
 *
 * Another side issue arises when we are running without an IRIG
 * source. This should only happen when bench testing.
 * The pc104sg card still generates signals and interrupts,
 * but they are based on an internal oscillator, which slowly drifts
 * relative to the unix clock. In that case is is harder to determine
 * what to do if a disagreement dTs is seen.
 */
static void checkSoftTicker(int newClock, int currClock,int scale, int notify)
{
        int ndt;
        unsigned long flags;

        newClock -= newClock % TMSEC_PER_INTERRUPT;
        newClock %= TMSECS_PER_DAY;

        // how many deltaTs did we jump forward or backward.
        ndt = (newClock - currClock) / TMSEC_PER_INTERRUPT;

        // rollover of either counter
        if (ndt > TMSECS_PER_DAY / TMSEC_PER_SOFT_TIC / 2)
                ndt -= TMSECS_PER_DAY / TMSEC_PER_SOFT_TIC;
        else if (ndt < -TMSECS_PER_DAY / TMSEC_PER_SOFT_TIC / 2)
                ndt += TMSECS_PER_DAY / TMSEC_PER_SOFT_TIC;

        if (ndt > IRIG_MAX_DT_DIFF / scale || ndt < IRIG_MIN_DT_DIFF / scale) {
                KLOG_WARNING
                    ("software clock out by %d dt, clock state=%s, resetting counters, #resets=%d\n",
                     ndt, clockStateString(),board.status.softwareClockResets);

                spin_lock_irqsave(&board.lock, flags);
#ifdef SUPPORT_USER_OVERRIDE
                if (board.clockState != USER_OVERRIDE)
#endif
                        board.clockAction = RESET_COUNTERS;      // reset counter on next interrupt
                spin_unlock_irqrestore(&board.lock, flags);
        }
        else {
                spin_lock_irqsave(&board.lock, flags);
                board.status.slews[ndt - IRIG_MIN_DT_DIFF]++;
                /* ask for more or fewer 100 Hz callbacks */
                atomic_add(ndt,&board.pending100Hz);
                spin_unlock_irqrestore(&board.lock, flags);
        }
}

/**
 * Invoke the callback functions for a given rate.
 */
static inline void doCallbacklist(int rate)
{
        struct list_head *list = board.CallbackLists + rate;
        struct list_head *ptr;
        struct irig_callback *cbentry;

        for (ptr = list->next; ptr != list; ptr = ptr->next) {
                cbentry = list_entry(ptr, struct irig_callback, list);
                cbentry->callback(cbentry->privateData);
        }
}

/**
 * Invoke all resync callback functions.
 */
static void doResyncCallbacks(void)
{
        int rate;
        for (rate = 0; rate < IRIG_NUM_RATES; rate++) {
                struct list_head *list = board.CallbackLists + rate;
                struct list_head *ptr;
                struct irig_callback *cbentry;
                for (ptr = list->next; ptr != list; ptr = ptr->next) {
                        cbentry = list_entry(ptr, struct irig_callback, list);
                        if (cbentry->resyncCallback)
                                cbentry->resyncCallback(cbentry->privateData);
                }
        }
}

/**
 * Tasklet called 100 times/second in softare interrupt context.
 * This tasklet maintains the software clock and performs requested
 * regular callbacks.
 */
static void pc104sg_bh_100Hz(unsigned long dev)
{
        unsigned long flags;
        int nloop;
        int ic;
        int count100Hz = board.count100Hz;

        for (nloop = 0; ; nloop++,count100Hz++) {
                if (count100Hz == MAX_TASKLET_COUNTER) count100Hz = 0;

                spin_lock_irqsave(&board.lock, flags);

                /* check if there is anything to do */
                if (atomic_read(&board.pending100Hz) <= 0) {
                        spin_unlock_irqrestore(&board.lock, flags);
                        break;
                }

                atomic_dec(&board.pending100Hz);

#if defined(SUPPORT_USER_SET) || defined(SUPPORT_USER_OVERRIDE)
                switch (board.clockAction) {
#ifdef SUPPORT_USER_OVERRIDE
                case USER_OVERRIDE_REQUESTED:
                        count100Hz = setSoftTickers(&board.userClock,1);
                        board.clockState = USER_OVERRIDE;
                        break;
#endif
#ifdef SUPPORT_USER_SET
                case USER_SET_REQUESTED:
                        // has requested to set the clock, and we
                        // have no time sync, then set the clock counters
                        // to the user clock, and then run normally.
                        if ((board.lastStatus & DP_Extd_Sts_Nosync)) {
                                count100Hz = setSoftTickers(&board.userClock,1);
                                board.clockACTION = NO_ACTION;
                        }
                        // set to irig clock since we have time sync
                        else board.clockAction = RESET_COUNTERS;
                        break;
#endif
                default:
                        break;
                }
#endif

                /* fix the clock and loop counter if requested */
                if (unlikely(board.clockAction == RESET_COUNTERS) && board.resetSnapshotDone) {
                        // If current sync is OK and was OK over last second
                        if (!(board.statusOr & (CLOCK_SYNC_NOT_OK | CLOCK_STATUS_NOSYNC)) &&
                            !(board.resetSnapshot.statusOr & (CLOCK_SYNC_NOT_OK | CLOCK_STATUS_NOSYNC))) {
                                // have IRIG sync. Set clock counters based on time registers
                                count100Hz = setSoftTickers(&board.resetSnapshot.irig_time,0);
                                board.clockState = SYNCD_SET;
                        }
                        else {
                                count100Hz = setSoftTickers(&board.resetSnapshot.unix_time,1);
                                board.clockState = UNSYNCD_SET;
                        }
                        if (board.notifyClients == NOTIFY_CLIENTS)
                                doResyncCallbacks();
                        board.notifyClients = NO_NOTIFY;
                        board.resetSnapshotDone = 0;
                        board.clockAction = NO_ACTION;
                        board.status.softwareClockResets++;
                        atomic_set(&board.pending100Hz,0);
                }
                else {
                        board.TMsecClockTicker += TMSEC_PER_INTERRUPT;
                        board.TMsecClockTicker %= TMSECS_PER_DAY;
                }

                /*
                 * This little double clock provides a clock that can be
                 * read by external modules without needing a mutex.
                 */
                TMsecClock[board.WriteClock] = board.TMsecClockTicker;

                /* see comment about memory barrier in setSoftwareTickers */
                smp_wmb();
                ic = ReadClock;
                /* prior to this line TMsecClock[ReadClock=0] is  OK to read */
                ReadClock = board.WriteClock;
                /* now TMsecClock[ReadClock=1] is still OK to read. */
                board.WriteClock = ic;

                /* Near the end of the second, ask for next clock 
                 * snapshot to be taken by ISR. 
                 * If nloop is 0 then it appears that we're keeping up with
                 * the interrupts.
                 */
                if (!board.askedSnapShot && (count100Hz % 100) > 96 && nloop == 0) {
                        board.askedSnapShot = 1;
                        board.doSnapShot = 1;
                }

                spin_unlock_irqrestore(&board.lock, flags);

                /* perform any pending add/remove callback requests. */
                spin_lock(&board.cblist_lock);
                handlePendingCallbacks();
                spin_unlock(&board.cblist_lock);


                /* perform 100Hz processing... */
                doCallbacklist(IRIG_100_HZ);

                if ((count100Hz % 2))
                        goto _5;

                /* perform 50Hz processing... */
                doCallbacklist(IRIG_50_HZ);

                if ((count100Hz % 4))
                        goto _5;

                /* perform 25Hz processing... */
                doCallbacklist(IRIG_25_HZ);

_5:
                if ((count100Hz % 5))
                        continue;

                /* perform 20Hz processing... */
                doCallbacklist(IRIG_20_HZ);

                if ((count100Hz % 10))
                        goto _25;

                /* perform 10Hz processing... */
                doCallbacklist(IRIG_10_HZ);

                if ((count100Hz % 20))
                        goto _25;

                /* perform  5Hz processing... */
                doCallbacklist(IRIG_5_HZ);

_25:
                if ((count100Hz % 25))
                        continue;

                /* perform  4Hz processing... */
                doCallbacklist(IRIG_4_HZ);

                if ((count100Hz % 50))
                        continue;

                /* perform  2Hz processing... */
                doCallbacklist(IRIG_2_HZ);

                if ((count100Hz % 100))
                        continue;

                /* perform 1Hz processing... */
                doCallbacklist(IRIG_1_HZ);

                /* schedule asking for another snapshot */
                board.askedSnapShot = 0;

                if ((count100Hz % 1000))
                        continue;

                /* perform  0.1 Hz processing... */
                doCallbacklist(IRIG_0_1_HZ);
        }
        board.count100Hz = count100Hz;
}

/*
 * This function is registered to be called every second.
 * It is called from software interrupt context.
 *
 */
static void oneHzFunction(void *ptr)
{
        unsigned long flags;

        struct timeval32 ti;
        struct timeval32 tu;
        int currClock;
        unsigned char statusOr;
        unsigned char lastStatus;
        int doSnapShot;

        int newClock;

        struct irig_device* dev;
        struct dsm_clock_sample_2 *osamp;

        /* copy the snapshot */
        spin_lock_irqsave(&board.lock, flags);

        doSnapShot = board.doSnapShot;

        tu = board.snapshot.unix_time;
        ti = board.snapshot.irig_time;
        currClock = board.snapshot.clock_time;
        statusOr = board.snapshot.statusOr;
        lastStatus = board.lastStatus;

        spin_unlock_irqrestore(&board.lock, flags);

        /* check if snapshot was taken */
        if (!doSnapShot
#ifdef SUPPORT_USER_OVERRIDE
                        && board.clockState != USER_OVERRIDE
#endif
                ) {
                int syncDiff = 0;
                if (board.lastSyncTime > 0)
                        syncDiff = GET_TMSEC_CLOCK - board.lastSyncTime;
                if (syncDiff > TMSECS_PER_DAY / 2)
                        syncDiff -= TMSECS_PER_DAY;
                else if (syncDiff < -TMSECS_PER_DAY / 2)
                        syncDiff += TMSECS_PER_DAY;

                if (!(statusOr & (CLOCK_SYNC_NOT_OK | CLOCK_STATUS_NOSYNC))) {
                        if (syncDiff > MAX_TMSEC_SINCE_LAST_SYNC) {
                                /* first sync after a long time of no sync */
                                newClock = (ti.tv_sec % SECS_PER_DAY) * TMSECS_PER_SEC +
                                    ti.tv_usec / USECS_PER_TMSEC;
                                /* use 1/2 the tick tolerances */
                                checkSoftTicker(newClock,currClock,2,NOTIFY_CLIENTS);
                        }
                        else if (board.clockState == SYNCD_SET) {
                                /* good situation, sync'd */
                                newClock = (ti.tv_sec % SECS_PER_DAY) * TMSECS_PER_SEC +
                                    ti.tv_usec / USECS_PER_TMSEC;
                                checkSoftTicker(newClock,currClock,1,NOTIFY_CLIENTS);
                        }
                        else {
                                /* sync'd over last second, but counters were last set
                                 * when unsync'd */
                                board.clockAction = RESET_COUNTERS;
                                board.notifyClients = NOTIFY_CLIENTS;
                        }
                        board.lastSyncTime = GET_TMSEC_CLOCK;
                }
                else {
                        if (syncDiff > MAX_TMSEC_SINCE_LAST_SYNC) {
                                /* some unsync over last second, and a long time since
                                 * consistently sync'd */
                                if (board.clockState == UNSYNCD_SET) {
                                        /* check against unix clock */
                                        newClock = (tu.tv_sec % SECS_PER_DAY) * TMSECS_PER_SEC +
                                            tu.tv_usec / USECS_PER_TMSEC;
                                        checkSoftTicker(newClock,currClock,1,NOTIFY_CLIENTS);
                                }
                                else {
                                        /* not currently sync'd, and a long time since
                                         * last sync but counters were last set
                                         * while sync'd.
                                         */
                                        board.clockAction = RESET_COUNTERS;
                                        board.notifyClients = NOTIFY_CLIENTS;
                                }
                        }
                        else {
                                /* some unsync over last second, but not a long time
                                 * since last full second of sync */
                                if (!(lastStatus & (CLOCK_SYNC_NOT_OK | CLOCK_STATUS_NOSYNC))) {
                                        /* currently sync'd, check against irig time */
                                        newClock = (ti.tv_sec % SECS_PER_DAY) * TMSECS_PER_SEC +
                                            ti.tv_usec / USECS_PER_TMSEC;
                                }
                                else {
                                        /* currently not sync'd, check against unix time */
                                        newClock = (tu.tv_sec % SECS_PER_DAY) * TMSECS_PER_SEC +
                                            tu.tv_usec / USECS_PER_TMSEC;
                                }
                                checkSoftTicker(newClock,currClock,1,NO_NOTIFY);
                        }
                }
        }

        /* if device is open, send the snapshot as a sample. */
        spin_lock(&board.dev_lock);
        dev = board.dev;
        if (!dev) {
                spin_unlock(&board.dev_lock);
                return;
        }

        dev->seqnum++;      // will rollover

        osamp = (struct dsm_clock_sample_2 *)
            GET_HEAD(dev->samples, PC104SG_SAMPLE_QUEUE_SIZE);
        if (!osamp) {           // no output sample available
                if (!(dev->skippedSamples++ % 10))
                    KLOG_WARNING("%s: skippedSamples=%d\n",
                                 board.deviceName, dev->skippedSamples);
                spin_unlock(&board.dev_lock);
                return;
        }
        
        /*
         * Use the current value of the ticker for this sample timetag, not the
         * ticker value that was saved in the clock snapshot.
         */
        osamp->timetag = GET_TMSEC_CLOCK / TMSECS_PER_MSEC;
        // osamp->timetag = currClock / TMSECS_PER_MSEC;

        osamp->length = 2 * sizeof(osamp->data.irigt) + 4;

        /*
         * The irig and unix times will not be exactly on the second,
         * since the snapshot was requested near the end, but not
         * exactly at the end, of the previous second.
         */

        /* snapshot irig time. Convert to little endian */
        osamp->data.irigt.tv_sec = cpu_to_le32(ti.tv_sec);
        osamp->data.irigt.tv_usec = cpu_to_le32(ti.tv_usec);

        /* snapshot unix system time. Convert to little endian */
        osamp->data.unixt.tv_sec = cpu_to_le32(tu.tv_sec);
        osamp->data.unixt.tv_usec = cpu_to_le32(tu.tv_usec);

        osamp->data.status = statusOr;
        osamp->data.seqnum = dev->seqnum;
        osamp->data.clockAdjusts = board.status.softwareClockResets;
        osamp->data.syncToggles = board.status.syncToggles;
        INCREMENT_HEAD(dev->samples, PC104SG_SAMPLE_QUEUE_SIZE);
        wake_up_interruptible(&dev->rwaitq);
        spin_unlock(&board.dev_lock);
}

static inline void requestExtendedStatus(void)
{
        /* 
         * Finish read of extended status from dual-port RAM and submit the next
         * request for extended status.
         */
        if (board.DP_RamExtStatusRequested) {
                GetRequestedDualPortRAM(&board.status.extendedStatus);
                board.DP_RamExtStatusRequested = 0;
        }

        /* send next request */
        if (board.DP_RamExtStatusEnabled) {
                RequestDualPortRAM(DP_Extd_Sts);
                board.DP_RamExtStatusRequested = 1;
        }
}

/*
 * Handle heartbeat interrupts.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
static irqreturn_t pc104sg_isr(int irq, void *callbackPtr)
#else
static irqreturn_t
pc104sg_isr(int irq, void *callbackPtr, struct pt_regs *regs)
#endif
{
        unsigned char status = inb(board.addr + Status_Port);
        irqreturn_t ret = IRQ_NONE;

        if ((status & Heartbeat) && (board.IntMask & Heartbeat_Int_Enb)) {

                resetHeartBeatLatch();

                ret = IRQ_HANDLED;

                spin_lock(&board.lock);

                requestExtendedStatus();

                /* Bits in extended status:
                 * bit 0:  0=clock sync'd to PPS or time code. This means
                 *           the sub-second fields are OK.
                 *         1=clock not sync'd.
                 * bit 1:  0=time code inputs OK (day,hr,min,sec fields OK)
                 *         1=time code inputs not readable. In this case
                 *           the pc104sg keeps incrementing its own clock
                 *           starting from whatever was set in the major time fields.
                 * bit 2:  0=PPS inputs OK
                 *         1=PPS inputs not readable
                 * bit 3:  Major time not set since jam
                 * bit 4:  Year not set.
                 */

                /* Count the number of sync changes */
                if ((board.lastStatus & DP_Extd_Sts_Nosync) != (board.status.extendedStatus & DP_Extd_Sts_Nosync))
                {
                        board.status.syncToggles++;
                }
                board.lastStatus = board.status.extendedStatus;

                // add bit from status register, though its probably identical to
                // CLOCK_STATUS_NOSYNC bit in extended status
                if (!(status & Sync_OK)) board.lastStatus |= CLOCK_SYNC_NOT_OK;

                board.statusOr |= board.lastStatus;

                if (!(status & Sync_OK)) board.statusOr |= CLOCK_SYNC_NOT_OK;

                if (unlikely(board.doSnapShot) ||
                                unlikely(board.clockAction == RESET_COUNTERS)) {
                        struct timeval32 itv32;             
                        struct timeval32 utv32;
                        do_gettimeofday_tv32(&utv32);
                        get_irig_time_tv32(&itv32);

                        if (board.doSnapShot) {
                                board.snapshot.clock_time = board.TMsecClockTicker;
                                board.snapshot.irig_time = itv32;
                                board.snapshot.unix_time = utv32;
                                board.snapshot.statusOr = board.statusOr;
                                board.statusOr = 0;
                                board.doSnapShot = 0;
                        }
                        if (board.clockAction == RESET_COUNTERS) {
                                board.resetSnapshot.clock_time = board.TMsecClockTicker;
                                board.resetSnapshot.irig_time = itv32;
                                board.resetSnapshot.unix_time = utv32;
                                board.resetSnapshot.statusOr = board.statusOr;
                                board.resetSnapshotDone = 1;
                                // if tasklet has gotten behind, flush its schedule queue.
                                atomic_set(&board.pending100Hz,0);
                        }
                }

                atomic_inc(&board.pending100Hz);

                spin_unlock(&board.lock);

                /*
                 * schedule the bottom half tasklet.
                 */
                tasklet_hi_schedule(&board.tasklet100Hz);
        }
#ifdef CHECK_EXT_EVENT
        if ((status & Ext_Ready) && (board.IntMask & Ext_Ready_Int_Enb)) {
                ret = IRQ_HANDLED;
                struct irigTime ti;
                getExtEventTime(&ti);
                KLOG_DEBUG
                    ("ext event=%04d %03d %02d:%02d:%02d.%03d %03d %03d, "
                     "stat=0x%x, state=%d\n", ti.year, ti.yday, ti.hour,
                     ti.min, ti.sec, ti.msec, ti.usec, ti.nsec,
                     board.lastStatus, board.clockState);
        }
#endif
        return ret;
}


static int pc104sg_open(struct inode *inode, struct file *filp)
{
        int result = 0;
        struct irig_device *dev = board.dev;

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        spin_lock_bh(&board.dev_lock);

        if (atomic_read(&board.num_opened) == 0) {
                BUG_ON(board.dev);      /* board.dev should be NULL */
                board.dev = dev =
                    (struct irig_device *) kmalloc(sizeof(struct irig_device),
                                         GFP_KERNEL);
                if (!dev) {
                        spin_unlock_bh(&board.dev_lock);
                        return -ENOMEM;
                }
                memset(dev, 0, sizeof(struct irig_device));

                init_waitqueue_head(&dev->rwaitq);
                result = realloc_dsm_circ_buf(&dev->samples,
                                   sizeof(struct dsm_clock_data_2),
                                   PC104SG_SAMPLE_QUEUE_SIZE);
                if (result) {
                        kfree(dev);
                        spin_unlock_bh(&board.dev_lock);
                        return result;
                }
        }
        else BUG_ON(!board.dev);        /* board.dev should be non-NULL */

        atomic_inc(&board.num_opened);
        spin_unlock_bh(&board.dev_lock);

        filp->private_data = dev;
        return result;
}

/* device close */
static int pc104sg_release(struct inode *inode, struct file *filp)
{
        struct irig_device *dev = (struct irig_device *) filp->private_data;

        spin_lock_bh(&board.dev_lock);

        /* decrements and tests. If value is 0, returns true. */
        if (atomic_dec_and_test(&board.num_opened)) {
                free_dsm_circ_buf(&dev->samples);
                kfree(dev);
                board.dev = 0;
        }
        spin_unlock_bh(&board.dev_lock);
        return 0;
}

/*
 * Implementation of poll fops.
 */
static unsigned int pc104sg_poll(struct file *filp, poll_table * wait)
{
        struct irig_device *dev = (struct irig_device *) filp->private_data;
        unsigned int mask = 0;

        poll_wait(filp, &dev->rwaitq, wait);

        if (sample_remains(&dev->read_state) ||
            GET_TAIL(dev->samples,dev->samples.size))
                mask |= POLLIN | POLLRDNORM;    /* readable */
        return mask;
}

/*
 * User-side read function
 */
static ssize_t
pc104sg_read(struct file *filp, char __user * buf, size_t count,
             loff_t * f_pos)
{
        struct irig_device *dev = (struct irig_device *) filp->private_data;

        return nidas_circbuf_read(filp, buf, count,
                                  &dev->samples, &dev->read_state,
                                  &dev->rwaitq);
}

static long
pc104sg_ioctl(struct file *filp, unsigned int cmd,unsigned long arg)
{
        void __user *userptr = (void __user *) arg;
        int len = _IOC_SIZE(cmd);
        int ret = -EINVAL;
        struct irigTime ti;
        struct timeval32 tv;
        unsigned long flags;

#ifdef DEBUG
        KLOG_DEBUG("cmd=0x%x\n", cmd);
#endif

        /*
         * Make sure the ioctl command is one of ours
         */
        if (_IOC_TYPE(cmd) != IRIG_IOC_MAGIC)
                return -EINVAL;

        /*
         * Verify read or write access to the user arg, if necessary
         */
        if ((_IOC_DIR(cmd) & _IOC_READ) &&
            !access_ok(VERIFY_WRITE, userptr, len))
                return -EFAULT;

        if ((_IOC_DIR(cmd) & _IOC_WRITE) &&
            !access_ok(VERIFY_READ, userptr, len))
                return -EFAULT;


        ret = -EFAULT;

        switch (cmd) {
        case IRIG_GET_STATUS:
                if (len != sizeof(board.status))
                        break;
                ret =
                    copy_to_user(userptr, &board.status,len) ? -EFAULT : len;
                break;
        case IRIG_GET_CLOCK:
                if (len != sizeof(tv))
                        break;
                spin_lock_irqsave(&board.lock, flags);
                get_irig_time(&ti);
                spin_unlock_irqrestore(&board.lock, flags);
                irigTotimeval32(&ti, &tv);
                ret =
                    copy_to_user(userptr, &tv, sizeof(tv)) ? -EFAULT : len;
                break;
        case IRIG_SET_CLOCK:
                if (len != sizeof(tv))
                        break;

                ret =
                    copy_from_user(&tv, userptr, sizeof(tv)) ? -EFAULT : len;
                if (ret < 0) break;

                spin_lock_irqsave(&board.lock, flags);
                board.DP_RamExtStatusEnabled = 0;
                board.DP_RamExtStatusRequested = 0;
                spin_unlock_irqrestore(&board.lock, flags);

                timeval32Toirig(&tv, &ti);

                if (board.lastStatus & (DP_Extd_Sts_NoMajT | DP_Extd_Sts_Nocode))
                        setMajorTime(&ti);
                else
                        setYear(ti.year);

                spin_lock_irqsave(&board.lock, flags);
                board.DP_RamExtStatusEnabled = 1;
                spin_unlock_irqrestore(&board.lock, flags);

                board.clockAction = RESET_COUNTERS;
                break;
#ifdef SUPPORT_USER_OVERRIDE
        case IRIG_OVERRIDE_CLOCK:
                if (len != sizeof(board.userClock))
                        break;

                spin_lock_irqsave(&board.lock, flags);
                ret =
                    copy_from_user(&board.userClock, userptr,
                                   sizeof(board.userClock)) ? -EFAULT : len;
                if (ret < 0) {
                        spin_unlock_irqrestore(&board.lock, flags);
                        break;
                }
                board.DP_RamExtStatusEnabled = 0;
                board.DP_RamExtStatusRequested = 0;
                spin_unlock_irqrestore(&board.lock, flags);

                timeval32Toirig(&board.userClock, &ti);

                if (board.lastStatus & DP_Extd_Sts_Nosync)
                        setMajorTime(&ti);
                else
                        setYear(ti.year);

                spin_lock_irqsave(&board.lock, flags);
                board.DP_RamExtStatusEnabled = 1;
                spin_unlock_irqrestore(&board.lock, flags);

                board.clockState = USER_OVERRIDE_REQUESTED;
                ret = len;
                break;
#endif
        default:
                KLOG_WARNING
                    ("%s: Unrecognized ioctl %d (number %d, size %d)\n",
                     board.deviceName, cmd, _IOC_NR(cmd), _IOC_SIZE(cmd));
                ret = -EINVAL;
                break;
        }

        return ret;
}

static struct file_operations pc104sg_fops = {
        .owner = THIS_MODULE,
        .read = pc104sg_read,
        .poll = pc104sg_poll,
        .open = pc104sg_open,
        .unlocked_ioctl = pc104sg_ioctl,
        .release = pc104sg_release,
        .llseek  = no_llseek,
};


/* -- MODULE ---------------------------------------------------------- */
static void __exit pc104sg_cleanup(void)
{

        disableAllInts();

        if (board.irq)
                free_irq(board.irq, &board);

        if (board.oneHzCallback) unregister_irig_callback(board.oneHzCallback);

        /* free up our pool of callbacks */
        free_callbacks();

        tasklet_disable(&board.tasklet100Hz);

        cdev_del(&board.pc104sg_cdev);

        if (MAJOR(board.pc104sg_device) != 0)
            unregister_chrdev_region(board.pc104sg_device, 1);

        /* free up the I/O region and remove /proc entry */
        if (board.addr)
                release_region(board.addr, PC104SG_IOPORT_WIDTH);
        KLOG_NOTICE("done\n");
}

/* -- MODULE ---------------------------------------------------------- */

/* module initialization */
static int __init pc104sg_init(void)
{
        int i;
        int errval = 0;
        unsigned int addr;
        int irq;
        struct timeval unix_timeval;
        struct irigTime irig_time;             

#ifndef SVNREVISION
#define SVNREVISION "unknown"
#endif
        KLOG_NOTICE("version: %s\n", SVNREVISION);

        // zero out board structure
        memset(&board,0,sizeof(board));

        /* device name for info messages */
        sprintf(board.deviceName, "/dev/irig%d", 0);

        board.pc104sg_device = MKDEV(0, 0);

        spin_lock_init(&board.lock);

        tasklet_init(&board.tasklet100Hz, pc104sg_bh_100Hz,(unsigned long)&board);

        spin_lock_init(&board.dev_lock);

        atomic_set(&board.num_opened,0);

        board.IntMask = 0x1f;

        atomic_set(&board.pending100Hz, 0);

        board.count100Hz = 0;

        for (i = 0; i < IRIG_NUM_RATES; i++) {
                INIT_LIST_HEAD(board.CallbackLists + i);
        }

        INIT_LIST_HEAD(&board.CallbackPool);

        spin_lock_init(&board.cblist_lock);

        INIT_LIST_HEAD(&board.pendingAdds);

        atomic_set(&board.nPendingCallbackChanges, 0);

        init_waitqueue_head(&board.callbackWaitQ);

        /* initialize clock counters that external modules grab */
        ReadClock = 0;
        board.WriteClock = 1;
        TMsecClock[ReadClock] = 0;
        TMsecClock[board.WriteClock] = 0;

        board.TMsecClockTicker = 0;

        board.lastSyncTime = -1;

        board.lastStatus =
            DP_Extd_Sts_Nosync | DP_Extd_Sts_Nocode |
            DP_Extd_Sts_NoPPS | DP_Extd_Sts_NoMajT | DP_Extd_Sts_NoYear;

        board.status.extendedStatus = board.lastStatus;

        board.DP_RamExtStatusEnabled = 1;
        board.DP_RamExtStatusRequested = 0;

        board.doSnapShot = 0;
        board.askedSnapShot = 0;

        /* Set the software clock and loop counters */
        board.clockState = UNSYNCD_SET;
        board.clockAction = RESET_COUNTERS;
        board.notifyClients = NO_NOTIFY;

        /* check for module parameters */
        addr = (unsigned long) IoPort + SYSTEM_ISA_IOPORT_BASE;

        errval = -EBUSY;
        /* Grab the region so that no one else tries to probe our ioports. */
        if (!request_region(addr, PC104SG_IOPORT_WIDTH,driver_name))
                goto err0;

        board.addr = addr;

        /* shutoff pc104sg interrupts just in case */
        disableAllInts();

        /* create our pool of callback entries */
        errval = -ENOMEM;
        for (i = 0; i < CALLBACK_POOL_SIZE; i++) {
                struct irig_callback *cbentry =
                    (struct irig_callback *)
                    kmalloc(sizeof(struct irig_callback),
                            GFP_KERNEL);
                if (!cbentry)
                        goto err0;
                list_add(&cbentry->list, &board.CallbackPool);
        }

        /*
         * Allow for ISA interrupts to be remapped for some processors
         */
        irq = GET_SYSTEM_ISA_IRQ(Irq);
        if (irq != Irq) {
                KLOG_NOTICE
                    ("ISA IRQ %d remapped to IRQ %d for this processor\n",
                     Irq, irq);
        }

        errval =
            request_irq(irq, pc104sg_isr, IRQF_SHARED, "PC104-SG IRIG",
                        &board);
        if (errval < 0) {
                /* failed... */
                KLOG_ERR("could not allocate IRQ %d\n", Irq);
                goto err0;
        }
        board.irq = irq;

        /* 
         * IRIG-B is the default, but we'll set it anyway 
         */
        setTimeCodeInputSelect(DP_CodeSelect_IRIGB);
        setPrimarySyncReference(0);     // 0=PPS, 1=timecode

        /*
         * Set the major time from the unix clock.
         */
        do_gettimeofday(&unix_timeval);
        timevalToirig(&unix_timeval, &irig_time);
        setMajorTime(&irig_time);


        /*
         * Set the internal heart-beat and rate2 to be in phase with
         * the PPS/time_code reference
         */
        setHeartBeatOutput(INTERRUPT_RATE);
        setRate2Output(A2DClockFreq);
        counterRejam();

        /*
         * Initialize the first request for extended status from dual-port
         * RAM.  The value will be read and successive requests will be 
         * issued by the interrupt service routine.
         */
        if (board.DP_RamExtStatusEnabled) {
                RequestDualPortRAM(DP_Extd_Sts);
                board.DP_RamExtStatusRequested = 1;
        }

        /* start interrupts */
        enableHeartBeatInt();

        /*
         * Register the 1 Hz callback
         */
        board.oneHzCallback =
            register_irig_callback(oneHzFunction,0,IRIG_1_HZ, 0, &errval);
        if (!board.oneHzCallback) {
                KLOG_ERR("%s: Error registering callback\n",
                         board.deviceName);
                goto err0;
        }

        /*
         * Initialize and add the user-visible device
         */
        if ((errval =
             alloc_chrdev_region(&board.pc104sg_device, 0, 1,
                                 driver_name)) < 0) {
                KLOG_ERR
                    ("Error %d allocating device major number for 'irig'\n",
                     -errval);
                goto err0;
        }
#ifdef DEBUG
        KLOG_DEBUG("Got major device number %d for 'irig'\n",
                    MAJOR(board.pc104sg_device));
#endif
        cdev_init(&board.pc104sg_cdev, &pc104sg_fops);
        if ((errval =
             cdev_add(&board.pc104sg_cdev, board.pc104sg_device, 1)) < 0) {
                KLOG_ERR("cdev_add() for PC104SG failed!\n");
                goto err0;
        }
        return 0;

err0:
        if (board.oneHzCallback) unregister_irig_callback(board.oneHzCallback);

        /* free up our pool of callbacks */
        free_callbacks();

        cdev_del(&board.pc104sg_cdev);

        if (MAJOR(board.pc104sg_device) != 0)
            unregister_chrdev_region(board.pc104sg_device, 1);

        tasklet_disable(&board.tasklet100Hz);

        disableAllInts();

        if (board.irq)
                free_irq(board.irq, &board);

        /* free up the I/O region */
        if (board.addr)
                release_region(board.addr, PC104SG_IOPORT_WIDTH);

        return errval;
}

module_init(pc104sg_init);
module_exit(pc104sg_cleanup);
