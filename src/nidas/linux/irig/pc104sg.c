/* pc104sg.c

   pc104sg driver for the ISA bus based jxi2 pc104-SG card

   adapted from John Wasinger's RTLinux driver

   Copyright 2007 UCAR, NCAR, All Rights Reserved

   Revisions:

     $LastChangedRevision: 3648 $
     $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $
     $LastChangedBy: cjw $
     $HeadURL: http://svn.atd.ucar.edu/svn/nids/trunk/src/nidas/rtlinux/pc104sg.c $
*/

#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/unistd.h>
#include <linux/list.h>
#include <linux/syscalls.h>

#include <asm/io.h>
#include <asm/semaphore.h>

#include "dsmlog.h"
#include "pc104sg.h"
#include "irigclock.h"
#include "ioctl_fifo.h"
#include <nidas/rtlinux/dsm_version.h>

#define DEBUG 1

/* IRIG interrupt rate, in Hz */
static const int INTERRUPT_RATE = 100;

/* IRIG interrupt rate for A/D */
static const int A2DREF_RATE = 10000;

/* module parameters (can be passed in via command line) */
static unsigned int Irq = 10;
static int IoPort = 0x2a0;
static const unsigned long IRQ_DEVID = 0xf0f0f0f0;

module_param(Irq, int, 0);
module_param(IoPort, int, 0);
MODULE_PARM(Irq, "IRQ number");
MODULE_PARM(IoPort, "I/O base address");
// We need GPL licensing to use the kernel workqueue stuff.
MODULE_LICENSE("GPL");

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_DESCRIPTION("ISA pc104-SG jxi2 Driver");

static const int SYSTEM_ISA_IOPORT_BASE = 0;

/* Actual physical address of this card. Set in init_module */
static unsigned int ISA_Address;

/* the three interrupts on this card are enabled with bits
 * 5,6,7 of the status port: 5=heartbeat, 6=match,
 * 7=external-time-tag.
 * Writing a 0 to bits 0-4 causes other things, like board reset,
 * so we set those bits to 1 here in the interrupt mask.
 */
static unsigned char IntMask = 0x1f;

/**
 * Symbols with external scope, referenced via GET_MSEC_CLOCK
 * macro by other modules.
 */
unsigned long volatile MsecClock[2] = { 0, 0};
unsigned char volatile ReadClock = 0;

/**
 * local clock variables.
 */
static unsigned char volatile WriteClock = 1;

/**
 * The millisecond clock counter.
 */
static unsigned long volatile MsecClockTicker = 0;

/** number of milliseconds per interrupt */
#define MSEC_PER_INTERRUPT (MSECS_PER_SEC / INTERRUPT_RATE)

/** frequency (in Hz) to look into doing callbacks */
static const int CALLBACK_CHECK_RATE = 100;

/** number of milliseconds elapsed between thread signals */
#define MSEC_PER_CALLBACK_CHECK (MSECS_PER_SEC / CALLBACK_CHECK_RATE)

/*
 * Allow for counting up to 10 seconds, so that we can do 0.1hz callbacks.
 */
#define MAX_INTERRUPT_COUNTER (10 * INTERRUPT_RATE)

/*
 * The 100 Hz counter.
 */
static int volatile Hz100_Cnt = 0;

static struct timeval UserClock;

/**
 * Enumeration of the state of the clock.
 */
enum clock {
   CODED,                   // normal state, clock set from time code inputs
   RESET_COUNTERS,          // need to reset our counters from the clock
   USER_SET_REQUESTED,      // user has requested to set the clock via ioctl
   USER_SET,                // clock was set from user IRIG_SET_CLOCK ioctl
   USER_OVERRIDE_REQUESTED, // user has requested override of clock
   USER_OVERRIDE,           // clock has been overridden
};

/**
 * Current clock state.
 */
static unsigned char volatile ClockState=CODED;

/**
 * Value of extended status from dual port RAM.
 * Bits:
 * 0: 1=On-board clock has not been verified to be within
 *          DP_Syncthr in last 5 seconds
 * 1: 1=Input time code unreadable.
 * 2: 1=PPS pulses not 1 second apart
 * 3: 1=Major time has not been set since counter rejam
 * 4: 1=Year not set
 */
static unsigned char ExtendedStatus =
   DP_Extd_Sts_Nosync | DP_Extd_Sts_Nocode |
   DP_Extd_Sts_NoPPS | DP_Extd_Sts_NoMajT |
   DP_Extd_Sts_NoYear;

static unsigned char LastStatus =
   DP_Extd_Sts_Nosync | DP_Extd_Sts_Nocode |
   DP_Extd_Sts_NoPPS | DP_Extd_Sts_NoMajT |
   DP_Extd_Sts_NoYear;

static unsigned char SyncOK = 0;

/**
 * The year field in the pc104sg time registers
 * ranges from 0-99, so we keep track of the century.
 */
static int StaticYear;

/**
 * structure setup by ioctl FIFO registration.
 */
//XX static struct ioctlHandle* ioctlhandle = 0;

/**
 * User ioctls that we support.
 */
//XX static struct ioctlCmd ioctlcmds[] = {
//XX    { GET_NUM_PORTS, _IOC_SIZE(GET_NUM_PORTS) },
//XX    { IRIG_OPEN, _IOC_SIZE(IRIG_OPEN) },
//XX    { IRIG_CLOSE, _IOC_SIZE(IRIG_CLOSE) },
//XX    { IRIG_GET_STATUS, _IOC_SIZE(IRIG_GET_STATUS) },
//XX    { IRIG_GET_CLOCK, _IOC_SIZE(IRIG_GET_CLOCK) },
//XX    { IRIG_SET_CLOCK, _IOC_SIZE(IRIG_SET_CLOCK) },
//XX    { IRIG_SET_CLOCK, _IOC_SIZE(IRIG_SET_CLOCK) },
//XX    { IRIG_OVERRIDE_CLOCK, _IOC_SIZE(IRIG_OVERRIDE_CLOCK) },
//XX };
//XX 
//XX static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);

static char* DevPrefix = "irig";

/**
 * Structure of device information used when device is opened.
 */
static struct irig_port* PortDev = 0;

static spinlock_t DP_RamLock = SPIN_LOCK_UNLOCKED;
static int DP_RamExtStatusEnabled = 1;
static int DP_RamExtStatusRequested = 0;

/*
 * pc104sg_100Hz_task() is the function to be called for each 100Hz
 * interrupt from the card.  The data value passed is INTERRUPT_100HZ if
 * it's called because of an actual interrupt from the IRIG card, or
 * TIMEOUT_100HZ if we timed out waiting for the interrupt.
 */
static void pc104sg_task_100Hz(unsigned long ul_trigger);
typedef enum 
{
    TASK_INTERRUPT_TRIGGER,	// got interrupt from the IRIG
    TASK_TIMEOUT_TRIGGER	// timed out waiting for interrupt
} task_100Hz_trigger;

/*
 * Here's the tasklet to be scheduled when we receive the 100Hz
 * interrupt.
 */
DECLARE_TASKLET(Tasklet100HzInterrupt, pc104sg_task_100Hz, 
		TASK_INTERRUPT_TRIGGER);

/*
 * 100Hz interrupt timeout timer.
 * We wait up to 11/8 * the expected interrupt interval before timing out.
 */
#define INTERRUPT_TIMEOUT_LENGTH ((HZ / INTERRUPT_RATE) * 11 / 8);
static struct timer_list Timeout100Hz_Timer;

#define CALL_BACK_POOL_SIZE 32  /* number of callbacks we can support */

/** macros borrowed from glibc/time functions */
#define SECS_PER_HOUR   (60 * 60)

#ifndef SECS_PER_DAY
#define SECS_PER_DAY    (SECS_PER_HOUR * 24)
#endif

/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
#define my_isleap(year) \
   ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

#define DIV(a, b) ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y) (DIV (y, 4) - DIV (y, 100) + DIV (y, 400))


/**
 * Entry in a callback list.
 */
struct irigCallback {
      struct list_head list;
      irig_callback_t* callback;
      void* privateData;
};

static struct list_head CallbackLists[IRIG_NUM_RATES];

static struct list_head CallbackPool;

static DECLARE_MUTEX(CbListMutex);

/**
 * Module function that allows other modules to register their callback
 * function to be called at the given rate.  register_irig_callback
 * can be called at (almost) anytime, not just at module init time.
 * The only time that register/unregister_irig_callback cannot be
 * called is from within a callback function itself.
 * A callback function cannot do either register_irig_callback
 * or unregister_irig_callback, otherwise you'll get a deadlock on
 * CbListMutex.
 *
 */
int register_irig_callback(irig_callback_t* callback, enum irigClockRates rate,
                           void* privateData)
{
   struct list_head *ptr;
   struct irigCallback* cbentry;

   /* We could do a gpos_malloc of the entry, but that would require
    * that this function be called at module init time.
    * We want it to be call-able at any time, so we
    * gpos_malloc a pool of entries at this module's init time,
    * and grab an entry here.
    */
   down(&CbListMutex);

   ptr = CallbackPool.next;
   if (ptr == &CallbackPool) {          /* none left */
      up(&CbListMutex);
      return -ENOMEM;
   }

   cbentry = list_entry(ptr, struct irigCallback, list);
   list_del(&cbentry->list);

   cbentry->callback = callback;
   cbentry->privateData = privateData;

   list_add(&cbentry->list, CallbackLists + rate);

   up(&CbListMutex);

   return 0;
}

/**
 * Modules call this function to un-register their callbacks.
 * Note: this cannot be called from within a callback function
 * itself - a callback function cannot register/unregister itself, or
 * any other callback function.  If you try it you will get
 * a deadlock on the CbListMutex.
 */
void unregister_irig_callback(irig_callback_t* callback,
                              enum irigClockRates rate, void* privateData)
{
   struct list_head *ptr;
   struct irigCallback *cbentry;

   down(&CbListMutex);

   for (ptr = CallbackLists[rate].next; ptr != CallbackLists + rate;
        ptr = ptr->next) {
      cbentry = list_entry(ptr, struct irigCallback, list);
      if (cbentry->callback == callback &&
          (cbentry->privateData == privateData || privateData == 0)) {
         /* remove it from the list for the rate, and add to the pool. */
         list_del(&cbentry->list);
         list_add(&cbentry->list, &CallbackPool);
         break;
      }
   }

   up(&CbListMutex);
}

/**
 * Cleanup function that un-registers all callbacks.
 */
static void free_callbacks(void)
{
   int i;

   struct list_head *ptr;
   struct irigCallback *cbentry;

   down(&CbListMutex);

   for (i = 0; i < IRIG_NUM_RATES; i++) {
      for (ptr = CallbackLists[i].next;
           ptr != CallbackLists + i; ptr = CallbackLists[i].next) {
         cbentry = list_entry(ptr, struct irigCallback, list);
         /* remove it from the list for the rate, and add to the pool. */
         list_del(&cbentry->list);
         list_add(&cbentry->list, &CallbackPool);
      }
   }

   for (ptr = CallbackPool.next; ptr != &CallbackPool;
        ptr = CallbackPool.next) {
      cbentry = list_entry(ptr, struct irigCallback, list);
      list_del(&cbentry->list);
      kfree(cbentry);
   }

   up(&CbListMutex);
}

/**
 * After receiving a heartbeat interrupt, one must reset
 * the heart beat flag in order to receive further interrupts.
 */
static void inline ackHeartBeatInt (void)
{
   /* reset heart beat flag, write a 0 to bit 4, leave others alone */
   outb(IntMask & ~Heartbeat, ISA_Address + Status_Port);
   {
      unsigned char status = inb(ISA_Address + Status_Port);
      DSMLOG_NOTICE("status = 0x%x\n", status);
   }
}

/**
 * Enable heart beat interrupts
 */
static void enableHeartBeatInt (void)
{
   IntMask |= Heartbeat_Int_Enb;
#ifdef DEBUG
   DSMLOG_DEBUG("IntMask=0x%x\n", IntMask);
#endif
   ackHeartBeatInt();   // reset flag too to avoid immediate interrupt
}

/**
 * Disable heart beat interrupts
 */
static void disableHeartBeatInt (void)
{
   IntMask &= ~Heartbeat_Int_Enb;
   outb(IntMask, ISA_Address + Status_Port);
}

/**
 * After receiving a match interrupt, one must reset
 * the match flag in order to receive further interrupts.
 */
static void inline ackMatchInt (void)
{
   /* reset match flag, write a 0 to bit 3, leave others alone */
   outb(IntMask & 0xf7, ISA_Address + Status_Port);
}

/**
 * Enable external time tag interrupt. These are caused by
 * a TTL input on a pin, and allows one to tag external
 * events.  This may be useful for synchronization tests of DSMs.
 */
static void enableExternEventInt (void)
{
   IntMask |= Ext_Ready_Int_Enb;
   outb(IntMask, ISA_Address + Status_Port);
}

/**
 * Disable external time tag interrupt.
 */
static void disableExternEventInt (void)
{
   IntMask &= ~Ext_Ready_Int_Enb;
   outb(IntMask, ISA_Address + Status_Port);
}

static void disableAllInts (void)
{
   /* disable all interrupts */
   IntMask = 0x1f;
#ifdef DEBUG
   DSMLOG_DEBUG("IntMask=0x%x\n", IntMask);
#endif
   outb(IntMask, ISA_Address + Status_Port);
}

/**
 * Read dual port RAM.
 * @param isRT    Set to 1 if this is called from a real-time thread.
 * in which case this function uses udelay().
 * Use isRT=0 if called from the ioctl callback (which is not a
 * real-time thread). In this case this function uses
 * a jiffy schedule method to delay.
 */
static int Read_Dual_Port_RAM (unsigned char addr, unsigned char* val, 
			       int isRT)
{
   int i;
   unsigned char status;
   unsigned long delay_usec = 10; // 10 microsecond wait

   /* clear the response */
   inb(ISA_Address + Dual_Port_Data_Port);

   /* specify dual port address */
   outb(addr, ISA_Address + Dual_Port_Address_Port);

   wmb();

   /* wait for PC104 to acknowledge.
    * On a viper @ 200MHz, without a nanosleep or jiffy wait,
    * this took about 32 loops to see the expected status.
    * With a 1 usec sleep, it loops about 4 or 6 times.
    * Changing it to 4 usec didn't change anything - must
    * be below the resolution of the clock. Changing it
    * to 10 usec resulted in a loop count of 1.
    */
   i = 0;
   do {
      if (isRT) 
	 udelay(delay_usec);
      else {
         unsigned long j = jiffies + 1;
         while (jiffies < j) schedule();
      }
      status = inb(ISA_Address + Extended_Status_Port);
   } while(i++ < 10 && !(status &  Response_Ready));
#ifdef DEBUG
   if (i > 1) DSMLOG_DEBUG("Read_Dual_Port_RAM, i=%d\n", i);
#endif

   /* check for a time out on the response... */
   if (!(status &  Response_Ready)) {
      DSMLOG_WARNING("timed out...\n");
      return -1;
   }

   /* return read DP_Control value */
   *val = inb(ISA_Address + Dual_Port_Data_Port);
   return 0;
}

/**
 * Make a request to DP ram
 */
static inline void Req_Dual_Port_RAM(unsigned char addr)
{
   /* clear the response */
   inb(ISA_Address + Dual_Port_Data_Port);

   /* specify dual port address */
   outb(addr, ISA_Address + Dual_Port_Address_Port);
}

/**
 * Get requested value from DP ram. It must be ready.
 */
static inline void Get_Dual_Port_RAM(unsigned char* val)
{
   static int ntimeouts = 0;
   unsigned char status;
   status = inb(ISA_Address + Extended_Status_Port);

   /* check for a time out on the response... */
   if (!(status & Response_Ready)) {
      if (!(ntimeouts++ % 100))
         DSMLOG_WARNING("timed out\n");
      return;
   }

   /* return read DP_Control value */
   *val = inb(ISA_Address + Dual_Port_Data_Port);
}

/**
 * Set a value in dual port RAM.
 * @param isRT    Set to 1 if this is called from a real-time thread.
 * in which case this function uses udelay().
 */
static int Set_Dual_Port_RAM (unsigned char addr, unsigned char value, 
			      int isRT)
{
   int i;
   unsigned char status;
   unsigned long delay_usec = 20; // 20 microsecond wait

   /* clear the response */
   inb(ISA_Address + Dual_Port_Data_Port);

   /* specify dual port address */
   outb(addr, ISA_Address + Dual_Port_Address_Port);

   wmb();

   /* wait for PC104 to acknowledge */
   /* On a 200Mhz viper, this took about 32 loops to
    * see the expected status.
    */
   i = 0;
   do {
      if (isRT)
	 udelay(delay_usec);
      else {
         unsigned long j = jiffies + 1;
         while (jiffies < j) schedule();
      }
      status = inb(ISA_Address + Extended_Status_Port);
   } while(i++ < 10 && !(status &  Response_Ready));
#ifdef DEBUG
   if (i > 3) DSMLOG_DEBUG("Set_Dual_Port_RAM 1, i=%d\n", i);
#endif

   /* check for a time out on the response... */
   if (!(status &  Response_Ready)) {
      DSMLOG_WARNING("timed out...\n");
      return -1;
   }

   /* clear the response */
   inb(ISA_Address + Dual_Port_Data_Port);

   /* write new value to DP RAM */
   outb(value, ISA_Address + Dual_Port_Data_Port);

   wmb();

   i = 0;
   do {
      if (isRT)
	 udelay(delay_usec);
      else {
         unsigned long j = jiffies + 1;
         while (jiffies < j) schedule();
      }
      status = inb(ISA_Address + Extended_Status_Port);
   } while(i++ < 10 && !(status &  Response_Ready));
#ifdef DEBUG
   if (i > 3) DSMLOG_DEBUG("Set_Dual_Port_RAM 2, i=%d\n", i);
#endif

   /* check for a time out on the response... */
   if (!(status &  Response_Ready)) {
      DSMLOG_WARNING("timed out...\n");
      return -1;
   }

   /* check that the written value matches */
   if (inb(ISA_Address + Dual_Port_Data_Port) != value) {
      DSMLOG_WARNING("no match on read-back\n");
      return -1;
   }

   /* success */
   return 0;
}


/* This controls COUNTER 1 on the PC104SG card
 * Since it calls Set_Dual_Port_RAM it may only be called from
 * a real-time thread.
 */
static void setHeartBeatOutput (int rate, int isRT)
{
   int divide;
   unsigned char lsb, msb;

   divide = 3000000 / rate;

   lsb = (char)(divide & 0xff);
   msb = (char)((divide & 0xff00)>>8);

   Set_Dual_Port_RAM (DP_Ctr1_ctl,
                      DP_Ctr1_ctl_sel | DP_ctl_rw | DP_ctl_mode3 | DP_ctl_bin, 
		      isRT);
   Set_Dual_Port_RAM (DP_Ctr1_lsb, lsb, isRT);
   Set_Dual_Port_RAM (DP_Ctr1_msb, msb, isRT);

   /* We'll wait until rate2 is set and then do a rejam. */
   // Set_Dual_Port_RAM (DP_Command, Command_Set_Ctr1, isRT);
}

/**
 * Set the primary time reference.
 * @param val 0=PPS is primary time reference, 1=time code is primary
 * Since it calls Set_Dual_Port_RAM it may only be called from
 * a real-time thread.
 */
static void setPrimarySyncReference(unsigned char val, int isRT)
{
   unsigned char control0;
   Read_Dual_Port_RAM (DP_Control0, &control0, isRT);

   if (val) control0 |= DP_Control0_CodePriority;
   else control0 &= ~DP_Control0_CodePriority;

#ifdef DEBUG
   DSMLOG_DEBUG("setting DP_Control0 to 0x%x\n", control0);
#endif
   Set_Dual_Port_RAM (DP_Control0, control0, isRT);
}

static void setTimeCodeInputSelect(unsigned char val, int isRT)
{
   Set_Dual_Port_RAM(DP_CodeSelect, val, isRT);
}

static void getTimeCodeInputSelect(unsigned char *val, int isRT)
{
   Read_Dual_Port_RAM(DP_CodeSelect, val, isRT);
}

/* -- Utility --------------------------------------------------------- */

/* This controls COUNTER 0 on the PC104SG card
 * Since it calls Set_Dual_Port_RAM it may only be called from
 * a real-time thread.
 */
void setRate2Output (int rate, int isRT)
{
   int divide;
   unsigned char lsb, msb;

   divide = 3000000 / rate;

   lsb = (char)(divide & 0xff);
   msb = (char)((divide & 0xff00)>>8);
   Set_Dual_Port_RAM (DP_Ctr0_ctl,
                      DP_Ctr0_ctl_sel | DP_ctl_rw | DP_ctl_mode3 | DP_ctl_bin, 
		      isRT);
   Set_Dual_Port_RAM (DP_Ctr0_lsb, lsb, isRT);
   Set_Dual_Port_RAM (DP_Ctr0_msb, msb, isRT);
}

static void counterRejam(int isRT)
{
   Set_Dual_Port_RAM(DP_Command, Command_Rejam, isRT);
}

/**
 * Break a struct timeval into the fields of a struct irigTime.
 * This uses some code from glibc/time routines.
 */
static void timespec2irig (const struct timespec* ts, struct irigTime* ti)
{
   long int days, rem, y;
   unsigned long int t = ts->tv_sec;

   days = t / SECS_PER_DAY;
   rem = t % SECS_PER_DAY;
   ti->hour = rem / SECS_PER_HOUR;
   rem %= SECS_PER_HOUR;
   ti->min = rem / 60;
   ti->sec = rem % 60;
   y = 1970;

   while (days < 0 || days >= (my_isleap (y) ? 366 : 365))
   {
      /* Guess a corrected year, assuming 365 days per year.  */
      long int yg = y + days / 365 - (days % 365 < 0);

      /* Adjust DAYS and Y to match the guessed year.  */
      days -= ((yg - y) * 365
               + LEAPS_THRU_END_OF (yg - 1)
               - LEAPS_THRU_END_OF (y - 1));
      y = yg;
   }
   ti->year = y;
   ti->yday = days + 1; // irig uses 1-366, unix 0-365

   rem = ts->tv_nsec;
   ti->msec = rem / NSECS_PER_MSEC;
   rem %= NSECS_PER_MSEC;
   ti->usec = rem / NSECS_PER_USEC;
   rem %= NSECS_PER_USEC;
   ti->nsec = rem;
}

static void timeval2irig (const struct timeval* tv, struct irigTime* ti)
{
   struct timespec ts;
   ts.tv_sec = tv->tv_sec;
   ts.tv_nsec = tv->tv_usec * NSECS_PER_USEC;
   timespec2irig(&ts, ti);
}
/**
 * Convert a struct irigTime into a struct timeval.
 */
static void irig2timespec(const struct irigTime* ti, struct timespec* ts)
{
   int y = ti->year;
   int nleap =  LEAPS_THRU_END_OF(y-1) - LEAPS_THRU_END_OF(1969);

   ts->tv_nsec = ti->msec * NSECS_PER_MSEC + ti->usec * NSECS_PER_USEC + 
       ti->nsec;

   ts->tv_sec = (y - 1970) * 365 * SECS_PER_DAY +
      (nleap + ti->yday - 1) * SECS_PER_DAY +
      ti->hour * 3600 + ti->min * 60 + ti->sec;
}

static void irig2timeval(const struct irigTime* ti, struct timeval* tv)
{
   struct timespec ts;
   irig2timespec(ti, &ts);
   tv->tv_sec = ts.tv_sec;
   tv->tv_usec = (ts.tv_nsec + NSECS_PER_USEC/2) / NSECS_PER_USEC;
}

/**
 * Read a time from the card.
 * Set offset to 0 to read main clock.
 * Set offset to 0x10 to read time of external pulse.
 *
 * Data are stored in BCD form as 4 byte niblets,
 * containing a 1x,10x or 100x value for the respective
 * time fields.
 */
static void getTimeFields(struct irigTime* ti, int offset)
{
   unsigned char us0ns2, us2us1, ms1ms0, sec0ms2, min0sec1, hour0min1;
   unsigned char day0hour1, day2day1, year10year1;

   /* reading the Usec1_Nsec100 value latches all other digits */
   us0ns2    = inb(ISA_Address + offset + Usec1_Nsec100_Port);   //0x0f
   us2us1    = inb(ISA_Address + offset + Usec100_Usec10_Port);  //0x0e
   ms1ms0    = inb(ISA_Address + offset + Msec10_Msec1_Port);    //0x0d
   sec0ms2   = inb(ISA_Address + offset + Sec1_Msec100_Port);    //0x0c
   min0sec1  = inb(ISA_Address + offset + Min1_Sec10_Port);      //0x0b
   hour0min1 = inb(ISA_Address + offset + Hr1_Min10_Port);       //0x0a
   day0hour1 = inb(ISA_Address + offset + Day1_Hr10_Port);       //0x09
   day2day1  = inb(ISA_Address + offset + Day100_Day10_Port);    //0x08
   year10year1  = inb(ISA_Address + offset + Year10_Year1_Port); //0x07

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

   ti->year = (year10year1 / 16) * 10 + (year10year1 & 0x0f);
   /*
     DSMLOG_DEBUG("getTimeFields, year=%d, century=%d\n",
     ti->year, (StaticYear/100)*100);
   */

   /* After cold start the year field is not set, and it
    * takes some time before the setYear to DPR takes effect.
    * I saw values of 165 for the year during this time.
    */
   if (ExtendedStatus & DP_Extd_Sts_NoYear) {
      // DSMLOG_DEBUG("fixing year=%d to %d\n", ti->year, StaticYear);
      ti->year = StaticYear;
   }
   // This has a Y2K problem, but who cares - it was written in 2004 and
   // it's for a real-time data system!
   else ti->year += (StaticYear/100) * 100;

   ti->yday = ((day2day1 / 16) * 100) + ((day2day1 & 0x0f) * 10) +
      day0hour1 / 16;
   ti->hour = (day0hour1 & 0x0f) * 10 + (hour0min1) / 16;
   ti->min = (hour0min1 & 0x0f) * 10 + min0sec1 / 16;
   ti->sec = (min0sec1 & 0x0f) * 10 + sec0ms2 / 16;
   ti->msec = ((sec0ms2 & 0x0f) * 100) + ((ms1ms0 / 16) * 10) +
      (ms1ms0 & 0x0f);
   ti->usec = ((us2us1 / 16) * 100) + ((us2us1 & 0x0f) * 10) + us0ns2 / 16;
   ti->nsec = (us0ns2 & 0x0f) * 100;
}

/**
 * Read sub-second time fields from the card, return microseconds.
 * May be useful for watching-the-clock when debugging.
 */
long getTimeUsec()
{
   unsigned char us0ns2, us2us1, ms1ms0, sec0ms2;
   long usec;

   /* reading the Usec1_Nsec100 value latches all other digits */
   us0ns2    = inb(ISA_Address + Usec1_Nsec100_Port);
   us2us1    = inb(ISA_Address + Usec100_Usec10_Port);
   ms1ms0    = inb(ISA_Address + Msec10_Msec1_Port);
   sec0ms2   = inb(ISA_Address + Sec1_Msec100_Port);

   usec = (((sec0ms2 & 0x0f) * 100) + ((ms1ms0 / 16) * 10) +
	   (ms1ms0 & 0x0f)) * USECS_PER_MSEC +
       ((us2us1 / 16) * 100) + ((us2us1 & 0x0f) * 10) + us0ns2 / 16;
   return usec;
}

/**
 * Get main clock.
 */
static void getCurrentTime(struct irigTime* ti)
{
   getTimeFields(ti, 0);
#ifdef DEBUG
   {
      int td, hr, mn, sc;
      // unsigned char status = inb(ISA_Address + Status_Port);
      dsm_sample_time_t tt = GET_MSEC_CLOCK;
      struct timespec ts;
      irig2timespec(ti, &ts);
      // clock difference
      td = (ts.tv_sec % SECS_PER_DAY) * MSECS_PER_SEC +
	  ts.tv_nsec / NSECS_PER_MSEC - tt;
      hr = (tt / 3600 / MSECS_PER_SEC);
      tt %= (3600 * MSECS_PER_SEC);
      mn = (tt / 60 / MSECS_PER_SEC);
      tt %= (60 * MSECS_PER_SEC);
      sc = tt / MSECS_PER_SEC;
      tt %= MSECS_PER_SEC;
      DSMLOG_DEBUG("%04d %03d %02d:%02d:%02d.%03d %03d %03d, clk=%02d:%02d:%02d.%03d, diff=%d, estat=0x%x, state=%d\n",
		   ti->year, ti->yday, ti->hour, ti->min, ti->sec, ti->msec, 
		   ti->usec, ti->nsec, hr, mn, sc, (int)tt, td, 
		   ExtendedStatus, ClockState);
   }
#endif
}

/* this function is available for external use */
void irig_clock_gettime(struct timespec* tp)
{
   struct irigTime it;
   getTimeFields(&it, 0);
   irig2timespec(&it, tp);
}

/* this function is available for external use */
int get_msec_clock_resolution()
{
   return MSEC_PER_INTERRUPT;
}

/**
 * Get external event time.
 */
static void getExtEventTime(struct irigTime* ti) {
   return getTimeFields(ti, 0x10);
}

/**
 * set the year fields in Dual Port RAM.
 * May only be called from a real-time thread.
 */
static void setYear(int val, int isRT)
{
   StaticYear = val;
#ifdef DEBUG
   DSMLOG_DEBUG("setYear=%d\n", val);
#endif
   Set_Dual_Port_RAM(DP_Year1000_Year100,
                     ((val / 1000) << 4) + ((val % 1000) / 100), isRT);
   val %= 100;
   Set_Dual_Port_RAM(DP_Year10_Year1, ((val / 10) << 4) + (val % 10), isRT);

   Set_Dual_Port_RAM (DP_Command, Command_Set_Years, isRT);
}

/**
 * The major time consists of the day-of-year, hour, minute
 * and second fields.  Ideally they are set via the time-code
 * input, but this function can be used if there is no time-code.
 *
 * The sub-second values are determined from the PPS input,
 * and I see no ways to change them if there is no PPS or time-code.
 * This may only be called from a real-time thread.
 */
static int setMajorTime(struct irigTime* ti, int isRT)
{
   int val;

#ifdef DEBUG
   // unsigned char status = inb(ISA_Address + Status_Port);
   DSMLOG_DEBUG("setMajor=%04d %03d %02d:%02d:%02d.%03d %03d %03d, estat=0x%x, state=%d\n",
                ti->year, ti->yday, ti->hour, ti->min, ti->sec, ti->msec, 
		ti->usec, ti->nsec, ExtendedStatus, ClockState);
#endif
   /* The year fields in Dual Port RAM are not technically
    * part of the major time, but we'll set them too.  */
   setYear(ti->year, isRT);

   val = ti->yday;
   Set_Dual_Port_RAM(DP_Major_Time_d100, val / 100, isRT);
   val %= 100;
   Set_Dual_Port_RAM(DP_Major_Time_d10d1, ((val / 10) << 4) + (val % 10), 
		     isRT);

   val = ti->hour;
   Set_Dual_Port_RAM(DP_Major_Time_h10h1, ((val / 10) << 4) + (val % 10), 
		     isRT);

   val = ti->min;
   Set_Dual_Port_RAM(DP_Major_Time_m10m1, ((val / 10) << 4) + (val % 10), 
		     isRT);

   val = ti->sec;
   Set_Dual_Port_RAM(DP_Major_Time_s10s1, ((val / 10) << 4) + (val % 10), 
		     isRT);

   Set_Dual_Port_RAM (DP_Command, Command_Set_Major, isRT);

   return 0;
}

/**
 * Increment our clock by a number of ticks.
 */
static void inline increment_clock(int tick)
{
   unsigned char c;
    
   MsecClockTicker += tick;
   MsecClockTicker %= MSECS_PER_DAY;

   /*
    * This little double clock provides a clock that can be
    * read by external modules without needing a mutex.
    * It ensures that MsecClock[ReadClock]
    * is valid for at least an interrupt period after reading the
    * value of ReadClock, even if this code is pre-emptive.
    *
    * This clock is incremented at the interrupt rate.
    * If somehow a bogged down piece of code reads the value of
    * ReadClock, and then didn't get around to reading
    * MsecClock[ReadClock] until more than an interrupt period
    * later then it could read a half-written value, but that
    * ain't gunna happen.
    */
   MsecClock[WriteClock] = MsecClockTicker;
   c = ReadClock;
   /* prior to this line MsecClock[ReadClock=0] is  OK to read */
   ReadClock = WriteClock;
   /* now MsecClock[ReadClock=1] is still OK to read. We're assuming
    * that the byte write of ReadClock is atomic.
    */
   WriteClock = c;
}

static inline void increment_hz100_cnt(void)
{
   if (++Hz100_Cnt == MAX_INTERRUPT_COUNTER) Hz100_Cnt = 0;
}

/**
 * Set the clock and 100 hz counter based on the time in a time val struct.
 */
static void setCounters(struct timeval* tv)
{
#ifdef DEBUG
   int td = (tv->tv_sec % SECS_PER_DAY) * MSECS_PER_SEC +
      tv->tv_usec / USECS_PER_MSEC - MsecClockTicker;
#endif

   MsecClockTicker =
      (tv->tv_sec % SECS_PER_DAY) * MSECS_PER_SEC +
      (tv->tv_usec + USECS_PER_MSEC/2) / USECS_PER_MSEC;
   MsecClockTicker -= MsecClockTicker % MSEC_PER_INTERRUPT;
   MsecClockTicker %= MSECS_PER_DAY;

   Hz100_Cnt = MsecClockTicker / MSEC_PER_CALLBACK_CHECK;
   if (!(MsecClockTicker % MSEC_PER_CALLBACK_CHECK)
       && (Hz100_Cnt-- == 0)) Hz100_Cnt = MAX_INTERRUPT_COUNTER - 1;
   Hz100_Cnt %= MAX_INTERRUPT_COUNTER;

#ifdef DEBUG
   DSMLOG_DEBUG("tv=%d.%06d, MsecClockTicker=%lu, td=%d, Hz100_Cnt=%d\n",
                (int)tv->tv_sec, (int)tv->tv_usec, MsecClockTicker, td, 
		Hz100_Cnt);
#endif

}

/**
 * Update the clock counters to the current time.
 */
static inline void setCountersToClock(void)
{
   // reset counters to clock
   struct irigTime ti;
   struct timeval tv;
   getCurrentTime(&ti);
   irig2timeval(&ti, &tv);
   setCounters(&tv);
}

/**
 * Invoke the callback functions for a given rate.
 */
static inline void doCallbacklist(struct list_head* list)
{
   struct list_head *ptr;
   struct irigCallback *cbentry;

   for (ptr = list->next; ptr != list; ptr = ptr->next) {
      cbentry = list_entry(ptr, struct irigCallback, list);
      cbentry->callback(cbentry->privateData);
   }
}

/* /\** */
/*  * This is the thread that runs forever waiting */
/*  * on semaphores from the interrupt service routine. */
/*  *\/ */
/* static void  */
/* pc104sg_100hz_thread(void *param) */
/* { */
/*    int isRT = 1; */
/*    unsigned long nsec_deltat; */
/*    long timeout; */
/*    int ntimeouts = 0; */
/*    int msecs_since_last_timeout = 0; */

/*    /\* */
/*     * Interrupts are not enabled at this point */
/*     * until the call to enableHeartBeatInt() below. */
/*     *\/ */
/*    /\* */
/*     * First initialize some dual port ram settings that */
/*     * can't be done at init time. */
/*     *\/ */
/*    /\* IRIG-B is the default, but we'll set it anyway *\/ */
/*    setTimeCodeInputSelect(DP_CodeSelect_IRIGB, isRT); */

/* #ifdef DEBUG */
/*    { */
/*       unsigned char timecode; */
/*       getTimeCodeInputSelect(&timecode, isRT); */
/*       DSMLOG_DEBUG("timecode=0x%x\n", timecode); */
/*    } */
/* #endif */
/*    setPrimarySyncReference(0, isRT);     // 0=PPS, 1=timecode */

/*    setHeartBeatOutput(INTERRUPT_RATE, isRT); */
/* #ifdef DEBUG */
/*    DSMLOG_DEBUG("setHeartBeatOutput(%d) done\n", INTERRUPT_RATE); */
/* #endif */

/*    setRate2Output(A2DREF_RATE, isRT); */
/* #ifdef DEBUG */
/*    DSMLOG_DEBUG("setRate2Output(%d) done\n", A2DREF_RATE); */
/* #endif */

/*    /\* */
/*     * Set the internal heart-beat and rate2 to be in phase with */
/*     * the PPS/time_code reference */
/*     *\/ */
/*    counterRejam(isRT); */

/*    ClockState = RESET_COUNTERS; */

/*    /\* initial request of extended status from DPR *\/ */
/*    Req_Dual_Port_RAM(DP_Extd_Sts); */

/*    /\* start interrupts *\/ */
/*    enableHeartBeatInt(); */
/* #ifdef DEBUG */
/*    DSMLOG_DEBUG("enableHeartBeatInt  done\n"); */
/* #endif */

/*    /\* event timeout in nanoseconds *\/ */
/*    nsec_deltat = MSEC_PER_CALLBACK_CHECK * NSECS_PER_MSEC; */
/*    nsec_deltat += (nsec_deltat * 3) / 8;        /\* add 3/8ths more *\/ */
/*    // DSMLOG_DEBUG("nsec_deltat = %d\n", nsec_deltat); */

/*    /\* event timeout in jiffies *\/ */
/*    timeout = nsec_deltat / TICK_NSEC; */
/*    timeout = 20; // 2 seconds, just for now */
/*    DSMLOG_NOTICE("nsec_deltat: %lu, timeout: %lu, TICK_NSEC: %lu\n", */
/* 		 nsec_deltat, timeout, TICK_NSEC); */
   
/* //    struct timespec irigts; */
/* //    dsm_sample_time_t   tstart = 0, tend = 0, tduty, tduty_max=0; */

/*    while(1) { */
/*       long timeRemaining; */

/*       /\* wait for the pc104sg_isr to signal us */
/*        * If we time out while waiting, then we've missed */
/*        * an irig interrupt.  Report the status and re-enable. */
/*        *\/ */
/*       DSMLOG_NOTICE("entering event wait\n"); */
/*       timeRemaining = wait_event_interruptible_timeout(wq_100Hz, */
/* 						       (Flag_100Hz != 0), */
/* 						       timeout); */
/*       DSMLOG_NOTICE("done waiting with %lu remaining, flag=%d\n", */
/* 		    timeRemaining, Flag_100Hz); */
/*       // If we timed out while waiting, then assume we missed an */
/*       // interrupt */
/*       if (Flag_100Hz == 0) { // timed out while waiting */
/* 	 if (!(ntimeouts++ % 1)) { */
/* 	     DSMLOG_NOTICE( */
/* 		 "thread semaphore timeout #%d, %d msecs since last timeout\n", */
/* 		 ntimeouts, msecs_since_last_timeout); */
/* 	 } */
/* 	 ackHeartBeatInt(); */
/* 	 msecs_since_last_timeout = 0; */

/*          /\* */
/* 	  * If clock is not overidden and we have time codes, then */
/* 	  * set counters to the clock */
/* 	  *\/ */
/*          if (ClockState == CODED) { */
/* 	     ClockState = RESET_COUNTERS; */
/* 	 } */
/* 	 /\* */
/* 	  * Otherwise, increment the clock and counter ourselves. */
/* 	  * it is presumably safe to increment since interrupts */
/* 	  * aren't happening!  This shouldn't violate the policy of */
/* 	  * MsecClock[ReadClock], which is that it */
/* 	  * isn't updated more often than once an interrupt. */
/*           * See the comments in increment_clock. */
/* 	  *\/ */
/* 	 else { */
/* 	    DSMLOG_NOTICE("incrementing clock\n"); */
/* 	    increment_clock(MSEC_PER_CALLBACK_CHECK); */
/* 	    increment_hz100_cnt(); */
/* 	 } */

/*       } */
/*       // If the time remaining is not zero, then we were interrupted */
/*       else if (timeRemaining != 0) { */
/*          DSMLOG_NOTICE("thread interrupted\n"); */
/*          break; */
/*       } */
/*       // Otherwise, success! */
/*       else { */
/* 	  Flag_100Hz = 0; */
/* 	  msecs_since_last_timeout += MSEC_PER_CALLBACK_CHECK; */
/*       } */

/*       // lock the callback list */
/*       down(&CbListMutex); */

/*       /\* perform 100Hz processing... *\/ */
/*       doCallbacklist(CallbackLists + IRIG_100_HZ); */

/*       if ((Hz100_Cnt %   2)) goto _5; */

/*       /\* perform 50Hz processing... *\/ */
/*       doCallbacklist(CallbackLists + IRIG_50_HZ); */

/*       if ((Hz100_Cnt %   4)) goto _5; */

/*       /\* perform 25Hz processing... *\/ */
/*       doCallbacklist(CallbackLists + IRIG_25_HZ); */

/* _5:   if ((Hz100_Cnt %   5)) goto cleanup; */

/*       /\* perform 20Hz processing... *\/ */
/*       doCallbacklist(CallbackLists + IRIG_20_HZ); */

/*       if ((Hz100_Cnt %  10)) goto _25; */

/*       /\* perform 10Hz processing... *\/ */
/*       doCallbacklist(CallbackLists + IRIG_10_HZ); */

/*       if ((Hz100_Cnt %  20)) goto _25; */

/*       /\* perform  5Hz processing... *\/ */
/*       doCallbacklist(CallbackLists + IRIG_5_HZ); */

/* _25:  if ((Hz100_Cnt %  25)) goto cleanup; */

/*       /\* perform  4Hz processing... *\/ */
/*       doCallbacklist(CallbackLists + IRIG_4_HZ); */

/*       if ((Hz100_Cnt %  50)) goto cleanup; */

/*       /\* perform  2Hz processing... *\/ */
/*       doCallbacklist(CallbackLists + IRIG_2_HZ); */

/*       if ((Hz100_Cnt % 100)) goto cleanup; */

/* #ifdef DEBUG */
/*       DSMLOG_DEBUG("Hz100_Cnt=%d, GET_MSEC_CLOCK=%lu\n", */
/*                    Hz100_Cnt, GET_MSEC_CLOCK); */
/* #endif */
/*       /\* perform  1Hz processing... *\/ */
/*       doCallbacklist(CallbackLists + IRIG_1_HZ); */

/*       if ((Hz100_Cnt % 1000)) goto cleanup; */

/*       /\* perform  0.1 Hz processing... *\/ */
/*       doCallbacklist(CallbackLists + IRIG_0_1_HZ); */

/* //    irig_clock_gettime(&irigts); */
/* //    tend = (irigts.tv_sec % SECS_PER_DAY) * NSECS_PER_SEC + irigts.tv_nsec; */
/* //    tduty = tend - tstart; */
/* //    if (tduty_max < tduty) tduty_max = tduty; */
/* //    DSMLOG_DEBUG("JDW   tend: %12u ns\n", tend); */
/* //    DSMLOG_DEBUG("JDW tstart: %12u ns\n", tstart); */
/* //    DSMLOG_DEBUG("JDW  tduty: %12u ns    tduty_max: %u ns\n", tduty, tduty_max); */
/* cleanup: */
/*       up(&CbListMutex); */
/*    } */
/* #ifdef DEBUG */
/*    DSMLOG_DEBUG("run method exiting!\n"); */
/* #endif */
/*    /\* nothing *\/; */
/* //XX   return (void*) status; */
/* } */




static void 
pc104sg_task_100Hz(unsigned long ul_trigger)
{
   task_100Hz_trigger trigger = (task_100Hz_trigger)ul_trigger;
   static int consecutive_timeouts = 0;
   static int initialized = 0;
   int isRT = 1;

//   Timeout100Hz_Timer.expires = jiffies + INTERRUPT_TIMEOUT_LENGTH;
   Timeout100Hz_Timer.expires = jiffies + 5 * HZ; // 5 seconds, for now
   add_timer(&Timeout100Hz_Timer);

   /*
    * On the first call here, we just initialize, setting
    * some stuff that can't be done at init time.
    */
   if (! initialized) 
   {
       /* IRIG-B is the default, but we'll set it anyway */
       setTimeCodeInputSelect(DP_CodeSelect_IRIGB, isRT);
       setPrimarySyncReference(0, isRT);     // 0=PPS, 1=timecode
       /*
	* Set the internal heart-beat and rate2 to be in phase with
	* the PPS/time_code reference
	*/
       setHeartBeatOutput(INTERRUPT_RATE, isRT);
       setRate2Output(A2DREF_RATE, isRT);
       counterRejam(isRT);

       ClockState = RESET_COUNTERS;

       /* initial request of extended status from DPR */
       Req_Dual_Port_RAM(DP_Extd_Sts);

       /* start interrupts */
       enableHeartBeatInt();

       initialized = 1;

       return;
   }
   
   {
       unsigned char x9, xa, xb, xc;
       int hours, minutes, seconds, tenths;
       unsigned char status = inb(ISA_Address + Status_Port);
       SyncOK = status & Sync_OK;
       DSMLOG_NOTICE("status is 0x%x (sync %s OK)\n", status, 
		     SyncOK ? "is" : "is not");

       inb(ISA_Address + 0xf);
       x9 = inb(ISA_Address + 0x9);
       xa = inb(ISA_Address + 0xa);
       xb = inb(ISA_Address + 0xb);
       xc = inb(ISA_Address + 0xc);

       hours = (x9 & 0xf) * 10 + (xa >> 4);
       minutes = (xa & 0xf) * 10 + (xb >> 4);
       seconds = (xb & 0xf) * 10 + (xc >> 4);
       tenths = xc & 0xf;
//       DSMLOG_NOTICE("time is %02d:%02d:%02d.%1d\n", hours, minutes,
//		     seconds, tenths);
   }

   /*
    * Do some stuff depending on whether we were triggered by a real
    * IRIG interrupt or by a timeout while waiting.
    */
   switch (trigger) 
   {
     case TASK_TIMEOUT_TRIGGER:
       consecutive_timeouts++;
       if (!(consecutive_timeouts % 100)) 
	  DSMLOG_NOTICE("%d consecutive timeouts\n", consecutive_timeouts);
	   
       ackHeartBeatInt();

       /*
	* If clock is not overidden and we have time codes, then
	* set counters to the clock
	*/
       if (ClockState == CODED) {
	  ClockState = RESET_COUNTERS;
       }
       /*
	* Otherwise, increment the clock and counter ourselves.
	* it is presumably safe to increment since interrupts
	* aren't happening!  This shouldn't violate the policy of
	* MsecClock[ReadClock], which is that it
	* isn't updated more often than once an interrupt.
	* See the comments in increment_clock.
	*/
       else {
	  increment_clock(MSEC_PER_CALLBACK_CHECK);
	  increment_hz100_cnt();
       }
       break;
     case TASK_INTERRUPT_TRIGGER:
       consecutive_timeouts = 0;
       break;
     default:
       DSMLOG_ERR("unknown trigger %d\n", trigger);
       return;
   }
       
   // lock the callback list
   down(&CbListMutex);

   /* perform 100Hz processing... */
   doCallbacklist(CallbackLists + IRIG_100_HZ);

   if ((Hz100_Cnt %   2)) goto _5;

   /* perform 50Hz processing... */
   doCallbacklist(CallbackLists + IRIG_50_HZ);

   if ((Hz100_Cnt %   4)) goto _5;

   /* perform 25Hz processing... */
   doCallbacklist(CallbackLists + IRIG_25_HZ);

_5:
   if ((Hz100_Cnt %   5)) goto cleanup;

   /* perform 20Hz processing... */
   doCallbacklist(CallbackLists + IRIG_20_HZ);

   if ((Hz100_Cnt %  10)) goto _25;

   /* perform 10Hz processing... */
   doCallbacklist(CallbackLists + IRIG_10_HZ);

   if ((Hz100_Cnt %  20)) goto _25;

   /* perform  5Hz processing... */
   doCallbacklist(CallbackLists + IRIG_5_HZ);

_25:
   if ((Hz100_Cnt %  25)) goto cleanup;

   /* perform  4Hz processing... */
   doCallbacklist(CallbackLists + IRIG_4_HZ);

   if ((Hz100_Cnt %  50)) goto cleanup;

   /* perform  2Hz processing... */
   doCallbacklist(CallbackLists + IRIG_2_HZ);

   if ((Hz100_Cnt % 100)) goto cleanup;

#ifdef DEBUG
//   DSMLOG_DEBUG("Hz100_Cnt=%d, GET_MSEC_CLOCK=%lu\n",
//		Hz100_Cnt, GET_MSEC_CLOCK);
#endif
   /* perform  1Hz processing... */
   doCallbacklist(CallbackLists + IRIG_1_HZ);

   if ((Hz100_Cnt % 1000)) goto cleanup;

   /* perform  0.1 Hz processing... */
   doCallbacklist(CallbackLists + IRIG_0_1_HZ);

cleanup:
   up(&CbListMutex);
}

/*
 * Check the extended status byte.
 * If clock status has changed adjust our clock counters.
 * This function is called by the interrupt service routine
 * and so we don't have to worry about simultaneous access
 * when changing the clock counters.
 */
static inline void checkExtStatus(void)
{

   spin_lock(&DP_RamLock);

   /* finish read of extended status from DPR.
    * We split the read of DP_Extd_Sts into two parts.
    * Send the request, and then at the next
    * interrupt get the value. This avoids having
    * to do sleeps or busy waits for it to be ready.
    * The only gotcha is that other requests
    * can't be sent in the meantime.
    *
    * Infrequently the user sends ioctl's which also access
    * DP RAM. The spin_locks are used to avoid simultaneous access.
    */

   if (DP_RamExtStatusRequested) {
      Get_Dual_Port_RAM(&ExtendedStatus);
      DP_RamExtStatusRequested = 0;
   }

   /* send next request */
   if (DP_RamExtStatusEnabled) {
      Req_Dual_Port_RAM(DP_Extd_Sts);
      DP_RamExtStatusRequested = 1;
   }
   spin_unlock(&DP_RamLock);

   switch (ClockState) {
      case USER_OVERRIDE_REQUESTED:
         setCounters(&UserClock);
         ClockState = USER_OVERRIDE;
         break;
      case USER_SET_REQUESTED:
         // has requested to set the clock, and we
         // have no time code: then set the clock counters
         // by the user clock
         if ((LastStatus & DP_Extd_Sts_Nocode) &&
             (ExtendedStatus & DP_Extd_Sts_Nocode)) {
            setCounters(&UserClock);
            ClockState = USER_SET;
         }
         // ignore request since we have time code
         else ClockState = CODED;
         break;
      case USER_SET:
         if (!(LastStatus & DP_Extd_Sts_Nocode) &&
             !(ExtendedStatus & DP_Extd_Sts_Nocode)) {
            // have good clock again, set counters back to coded clock
            ClockState = RESET_COUNTERS;
         }
         break;
      case RESET_COUNTERS:
      case USER_OVERRIDE:
      case CODED:
         break;
   }
   /* At this point ClockState is either
    * CODED: we're going with whatever the hardware clock says
    * RESET_COUNTERS: need to reset our counters to hardware clock
    * USER_OVERRIDE: user has overridden the clock
    *           In this case the hardware clock doesn't
    *           match our own counters.
    * USER_SET: the clock has been set from an ioctl with setMajorTime(),
    *          and time code input is missing.  Since the
    *           timecode is missing, the counters will be within
    *          a second of the hardware clock.
    */

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
    */
   // transition from no sync to sync, reset the counters
   if (ClockState == CODED &&
       (LastStatus & DP_Extd_Sts_Nosync) &&
       !(ExtendedStatus & DP_Extd_Sts_Nosync))
      ClockState = RESET_COUNTERS;

   if (ClockState == RESET_COUNTERS) {
      setCountersToClock();
      ClockState = CODED;
   }
   LastStatus = ExtendedStatus;
}

/*
 * pc104sg interrupt function.
 */
static irqreturn_t 
pc104sg_isr (int irq, void* callbackPtr, struct pt_regs *regs)
{
   unsigned char status = inb(ISA_Address + Status_Port);
   SyncOK = status & Sync_OK;

   DSMLOG_DEBUG("status: %d, IntMask: %d", status, IntMask);
   if ((status & Heartbeat) && (IntMask & Heartbeat_Int_Enb)) {

      /* acknowledge interrupt (essential!) */
      ackHeartBeatInt();

      increment_clock(MSEC_PER_INTERRUPT);

      checkExtStatus();

      /*
       * On 10 millisecond intervals execute the interrupt tasklet so it
       * can perform the callbacks at the various rates.
       */
      if (!(MsecClockTicker % MSEC_PER_CALLBACK_CHECK)) {
	 increment_hz100_cnt();
	 /*
	  * Cancel the existing timeout for the 100Hz task, then schedule
	  * the task to execute.
	  */
	 del_timer_sync(&Timeout100Hz_Timer);
	 tasklet_schedule(&Tasklet100HzInterrupt);
      }

   }
#ifdef CHECK_EXT_EVENT
   if ((status & Ext_Ready) && (IntMask & Ext_Ready_Int_Enb)) {
      struct irigTime ti;
      getExtEventTime(&ti);
      DSMLOG_DEBUG("ext event=%04d %03d %02d:%02d:%02d.%03d %03d %03d, stat=0x%x, state=%d\n",
                   ti.year, ti.yday, ti.hour, ti.min, ti.sec, ti.msec, ti.usec,
		   ti.nsec, ExtendedStatus, ClockState);
   }
#endif
   return 0;
}

static int close_port(struct irig_port* port)
{
   if (port->inFifoFd >= 0) {
      int fd = port->inFifoFd;
      port->inFifoFd = -1;
#ifdef DEBUG
      DSMLOG_DEBUG("closing %s\n", port->inFifoName);
#endif
      sys_close(fd);
   }

   return 0;
}


static int open_port(struct irig_port* port)
{
   int retval;
   if ((retval = close_port(port))) return retval;

   /* user opens port for read, so we open it for writing. */
#ifdef DEBUG
   DSMLOG_DEBUG("opening %s\n", port->inFifoName);
#endif
   port->inFifoFd = sys_open(port->inFifoName, O_NONBLOCK | O_WRONLY, 00700);
   if (port->inFifoFd < 0) {
       DSMLOG_ERR("error %d opening %s\n", errno, port->inFifoName);
      return -errno;
   }

// #define DO_FTRUNCATE
#ifdef DO_FTRUNCATE
   {
      int size = 4096;
      if (ftruncate(port->inFifoFd, size) < 0) {
	  DSMLOG_ERR("error (%d): ftruncate'ing %s to %d\n", errno,
		     port->inFifoName, size);
	  return -errno;
      }
   }
   
#endif

   return retval;
}

/*
 * This function is registered to be called
 * every second.  If the user has opened the irig device,
 * it gets the current clock value and writes it,
 * along with the extended status as a sample to the FIFO.
 */
static void portCallback(void* privateData)
{
   struct irig_port* dev = (struct irig_port*) privateData;
   struct irigTime ti;

   dsm_sample_time_t tt = GET_MSEC_CLOCK;
   getCurrentTime(&ti);

   // check clock sanity
   if (ClockState == CODED || ClockState == USER_SET) {
      struct timeval tv;
      int td;

      irig2timeval(&ti, &tv);
      // clock difference
      td = (tv.tv_sec % SECS_PER_DAY) * MSECS_PER_SEC +
	  tv.tv_usec / USECS_PER_MSEC - tt;
      /* If not within 3 milliseconds, ask to reset counters.
       * Since this is being called as a 1 Hz callback some
       * time may have elapsed since the 100 Hz interrupt.
       */
      if (abs(td) > 3) {
         ClockState = RESET_COUNTERS;
#ifdef DEBUG
         if (dev->inFifoFd >= 0)
            DSMLOG_DEBUG("tv=%d.%06d, tt=%d, td=%d, status=0x%x\n",
                         (int)tv.tv_sec, (int)tv.tv_usec, (int)tt, td, 
			 ExtendedStatus);
#endif
      }
   }

   if (dev->inFifoFd >= 0) {
      ssize_t wlen;

      dev->samp.timetag = tt;
      dev->samp.length = sizeof(dev->samp.data.tval) +
         sizeof(dev->samp.data.status);

      irig2timeval(&ti, &dev->samp.data.tval);
      dev->samp.data.status = ExtendedStatus;
      if (!SyncOK) dev->samp.data.status |= CLOCK_SYNC_NOT_OK;

#ifdef DEBUG
      DSMLOG_DEBUG("tv_secs=%d, tv_usecs=%d status=0x%x\n",
                   (int)dev->samp.data.tval.tv_sec, 
		   (int)dev->samp.data.tval.tv_usec,
                   dev->samp.data.status);
#endif

      wlen = sys_write(dev->inFifoFd, (char*)&(dev->samp),
		       SIZEOF_DSM_SAMPLE_HEADER + dev->samp.length);
      if (wlen < 0) {
	 DSMLOG_ERR("error (%d) writing %s. Closing\n", wlen,
                    dev->inFifoName);
         close_port(dev);
      }
   }

#ifdef DEBUG_XXX
   // unsigned char status = inb(ISA_Address + Status_Port);
   dsm_sample_time_t tt = GET_MSEC_CLOCK;
   int hr = (tt / 3600 / MSECS_PER_SEC);
   tt %= (3600 * MSECS_PER_SEC);
   int mn = (tt / 60 / MSECS_PER_SEC);
   tt %= (60 * MSECS_PER_SEC);
   int sc = tt / MSECS_PER_SEC;
   tt %= MSECS_PER_SEC;
   DSMLOG_DEBUG("%04d %03d %02d:%02d:%02d.%03d %03d %03d, clk=%02d:%02d:%02d.%03d, estat=0x%x, state=%d\n",
                ti.year, ti.yday, ti.hour, ti.min, ti.sec, ti.msec, ti.usec, 
		ti.nsec, hr, mn, sc, tt, ExtendedStatus, ClockState);
#endif

}

/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.  This is not being executed from a real-time thread.
 */
static int ioctlCallback(int cmd, int board, int portNum,
                         void *buf, size_t len)
{
   int retval = -EINVAL;
   int isRT = 0;
#ifdef DEBUG
   DSMLOG_DEBUG("ioctlCallback, cmd=0x%x board=%d, portNum=%d\n",
                cmd, board, portNum);
#endif

   /* only one board and one port supported by this module */
   if (board != 0) return retval;
   if (portNum != 0) return retval;

   switch (cmd) {
      case GET_NUM_PORTS:         /* user get */
         *(int *) buf = 1;
         retval = sizeof(int);
         break;
      case IRIG_OPEN:           /* open port */
#ifdef DEBUG
         DSMLOG_DEBUG("IRIG_OPEN\n");
#endif
         retval = open_port(PortDev);
         break;
      case IRIG_CLOSE:          /* close port */
#ifdef DEBUG
         DSMLOG_DEBUG("IRIG_CLOSE\n");
#endif
         retval = close_port(PortDev);
         break;
      case IRIG_GET_STATUS:
         *((unsigned char*)buf) = ExtendedStatus;
         retval = 1;
         break;
      case IRIG_GET_CLOCK:
      {
         struct irigTime ti;
         struct timeval tv;
         if (len != sizeof(tv)) break;
         getCurrentTime(&ti);
         irig2timeval(&ti, &tv);
         memcpy(buf, &tv, sizeof(tv));
         retval = len;
      }
      break;
      case IRIG_SET_CLOCK:
      {
         struct irigTime ti;
         unsigned long flags;
         if (len != sizeof(UserClock)) break;
         memcpy(&UserClock, buf, sizeof(UserClock));

         timeval2irig(&UserClock, &ti);

         spin_lock_irqsave(&DP_RamLock, flags);
         DP_RamExtStatusEnabled = 0;
         DP_RamExtStatusRequested = 0;
         spin_unlock_irqrestore(&DP_RamLock, flags);

         if (ExtendedStatus & DP_Extd_Sts_Nocode) setMajorTime(&ti, isRT);
         else setYear(ti.year, isRT);

         spin_lock_irqsave(&DP_RamLock, flags);
         DP_RamExtStatusEnabled = 1;
         spin_unlock_irqrestore(&DP_RamLock, flags);

         ClockState = USER_SET_REQUESTED;
         retval = len;
      }
      break;
      case IRIG_OVERRIDE_CLOCK:
      {
         struct irigTime ti;
         unsigned long flags;
         if (len != sizeof(UserClock)) break;
         memcpy(&UserClock, buf, sizeof(UserClock));
         timeval2irig(&UserClock, &ti);

         spin_lock_irqsave(&DP_RamLock, flags);
         DP_RamExtStatusEnabled = 0;
         DP_RamExtStatusRequested = 0;
         spin_unlock_irqrestore(&DP_RamLock, flags);

         if (ExtendedStatus & DP_Extd_Sts_Nocode) setMajorTime(&ti, isRT);
         else setYear(ti.year, isRT);

         spin_lock_irqsave(&DP_RamLock, flags);
         DP_RamExtStatusEnabled = 1;
         spin_unlock_irqrestore(&DP_RamLock, flags);

         ClockState = USER_OVERRIDE_REQUESTED;
         retval = len;
      }
      break;
      default:
         break;
   }
   return retval;
}
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
#ifdef DEBUG
   DSMLOG_DEBUG("cleaning up\n");
#endif

#ifdef DEBUG
   DSMLOG_NOTICE("free_callbacks\n");
#endif
   /* free up our pool of callbacks */
   free_callbacks();

#ifdef DEBUG
   DSMLOG_NOTICE("disableAllInts\n");
#endif
   disableAllInts();

   del_timer_sync(&Timeout100Hz_Timer);
   tasklet_kill(&Tasklet100HzInterrupt);

#ifdef DEBUG
   DSMLOG_NOTICE("free_isa_irq\n");
#endif
   free_irq(Irq, NULL);

   if (PortDev) {
#ifdef DEBUG
      DSMLOG_NOTICE("close_port\n");
#endif
      close_port(PortDev);
      if (PortDev->inFifoName) {
//         sys_unlink(PortDev->inFifoName);
         kfree(PortDev->inFifoName);
      }
      kfree(PortDev);
   }
//XX #ifdef DEBUG
//XX    DSMLOG_NOTICE("closeIoctlFIFO\n");
//XX #endif
//XX    if (ioctlhandle) closeIoctlFIFO(ioctlhandle);

   /* free up the I/O region and remove /proc entry */
#ifdef DEBUG
   DSMLOG_NOTICE("release_region\n");
#endif
   if (ISA_Address)
      release_region(ISA_Address, PC104SG_IOPORT_WIDTH);

   DSMLOG_NOTICE("done\n");

}

/* -- MODULE ---------------------------------------------------------- */
/* activate the pc104sg-B board */

int init_module (void)
{
   int i;
   int errval = 0;
   int irq_requested = 0;

   // DSM_VERSION_STRING is found in dsm_version.h
   DSMLOG_NOTICE("version: %s\n", DSM_VERSION_STRING);

   INIT_LIST_HEAD(&CallbackPool);
   for (i = 0; i < IRIG_NUM_RATES; i++)
      INIT_LIST_HEAD(CallbackLists + i);

   /* check for module parameters */
   ISA_Address = (unsigned int)IoPort + SYSTEM_ISA_IOPORT_BASE;

   errval = -EBUSY;
   /* Grab the region so that no one else tries to probe our ioports. */
   if (! request_region(ISA_Address, PC104SG_IOPORT_WIDTH, "pc104sg"))
      goto err0;

   /* initialize clock counters that external modules grab */
   ReadClock = 0;
   WriteClock = 0;
   MsecClock[ReadClock] = 0;
   MsecClock[WriteClock] = 0;

   /* shutoff pc104sg interrupts just in case */
   disableAllInts();

   PortDev = kmalloc(sizeof(struct irig_port), GFP_KERNEL);
   if (!PortDev) goto err0;
   PortDev->inFifoFd = -1;
//   PortDev->inFifoName = makeDevName(DevPrefix, "_in_", 0);
   PortDev->inFifoName = (char*)kmalloc(64, GFP_KERNEL);
   sprintf(PortDev->inFifoName, "/tmp/foodev");
   if (!PortDev->inFifoName) goto err0;

#ifdef DEBUG
   DSMLOG_DEBUG("creating %s\n", PortDev->inFifoName);
#endif

   // remove broken device file before making a new one
//XX    if ((errval = sys_unlink(PortDev->inFifoName)) < 0)
//XX    {
//XX       if (errval != -ENOENT)
//XX          goto err0;
//XX       else
//XX 	 errval = 0;
//XX    }

//XX    if (mkfifo(PortDev->inFifoName, 0666) < 0) {
//XX       errval = -errno;
//XX       DSMLOG_WARNING("mkfifo %s failed, errval=%d\n",
//XX                      PortDev->inFifoName, errval);
//XX       goto err0;
//XX    }
//XX
//XX    /* setup our device */
//XX    if (!(ioctlhandle = openIoctlFIFO(DevPrefix, 0, ioctlCallback, nioctlcmds,
//XX 				     ioctlcmds))) {
//XX       errval = -EINVAL;
//XX       goto err0;
//XX    }

   /* create our pool of callback entries */
   errval = -ENOMEM;
   for (i = 0; i < CALL_BACK_POOL_SIZE; i++) {
      struct irigCallback* cbentry =
	  (struct irigCallback*) kmalloc(sizeof(struct irigCallback),
					 GFP_KERNEL);
      if (!cbentry) goto err0;
      list_add(&cbentry->list, &CallbackPool);
   }

   errval = request_irq(Irq, pc104sg_isr, SA_SHIRQ, "PC104-SG IRIG", 
			(void*)IRQ_DEVID);
   if (errval < 0) {
      /* failed... */
      DSMLOG_WARNING("could not allocate IRQ %d\n", Irq);
      goto err0;
   }
   else 
   {
#ifdef DEBUG
       DSMLOG_DEBUG("got IRQ %d\n", Irq);
#endif
   }
   
       
   irq_requested = 1;
   /*
    * Force an immediate timeout to call our pc104sg_task_100Hz, so that it
    * can initialize itself.
    */
   init_timer(&Timeout100Hz_Timer);
   Timeout100Hz_Timer.function = pc104sg_task_100Hz;
   Timeout100Hz_Timer.expires = jiffies + 2;
   Timeout100Hz_Timer.data = TASK_TIMEOUT_TRIGGER;
   DSMLOG_NOTICE("adding first timer\n");
   add_timer(&Timeout100Hz_Timer);
   DSMLOG_NOTICE("first timer added\n");

   if ((errval = register_irig_callback(portCallback, IRIG_1_HZ, PortDev)) < 0)
      goto err0;

   return 0;

  err0:

   del_timer_sync(&Timeout100Hz_Timer);
   
   /* free up our pool of callbacks */
   free_callbacks();

   disableAllInts();

   if (irq_requested) free_irq(Irq, (void*)IRQ_DEVID);

//   if (ioctlhandle) closeIoctlFIFO(ioctlhandle);

   if (PortDev) {
      if (PortDev->inFifoName) {
//         sys_unlink(PortDev->inFifoName);
         kfree(PortDev->inFifoName);
      }
      kfree(PortDev);
   }

   /* free up the I/O region and remove /proc entry */
   if (ISA_Address)
      release_region(ISA_Address, PC104SG_IOPORT_WIDTH);

   return errval;
}
