/* pc104sg.c

   RTLinux pc104sg driver for the ISA bus based jxi2 pc104-SG card.

   Original Author: John Wasinger

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

     $LastChangedRevision: $
         $LastChangedDate: $
           $LastChangedBy: $
                 $HeadURL: $
*/

#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_pthread.h>
#include <rtl_semaphore.h>
#include <rtl_unistd.h>
#include <rtl_time.h>

#include <linux/ioport.h>
#include <linux/list.h>

#include <dsm_viper.h>
#include <pc104sg.h>
#include <irigclock.h>
#include <rtl_isa_irq.h>
#include <ioctl_fifo.h>

// #define DEBUG

RTLINUX_MODULE(pc104sg);

#define INTERRUPT_RATE 	100
#define A2DREF_RATE	10000

/* module parameters (can be passed in via command line) */
static unsigned int irq = 10;
static int ioport = 0x2a0;

MODULE_PARM(irq, "i");
MODULE_PARM(ioport, "i");

MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("RTLinux ISA pc104-SG jxi2 Driver");

/* Actual physical address of this card. Set in init_module */
static unsigned int isa_address = 0;

/* the three interrupts on this card are enabled with bits
 * 5,6,7 of the status port: 5=heartbeat, 6=match,
 * 7=external-time-tag. 
 * Writing a 0 to bits 0-4 causes other things, like board reset,
 * so we set those bits to 1 here in the interrupt mask.
 */
static unsigned char intmask = 0x1f;

/**
 * Symbols with external scope, referenced via GET_MSEC_CLOCK
 * macro by other modules.
 */
unsigned long msecClock[2] = { 0, 0};
unsigned char readClock = 0;

/**
 * local clock variables.
 */
static unsigned char writeClock = 1;

/**
 * The millisecond clock counter.
 */
static unsigned long msecClockTicker = 0;

/** number of milliseconds per interrupt */
#define MSEC_PER_INTRPT (1000 / INTERRUPT_RATE)

/** thread is signaled at 100hz */
#define THREAD_RATE_100HZ 100

/** number of milliseconds elapsed between thread signals */
#define MSEC_PER_THREAD_SIGNAL (1000 / THREAD_RATE_100HZ)

/*
 * we count up to 1000 centi-seconds = 10 seconds, so that
 * we can do 0.1hz callbacks.
 */
#define MAX_THREAD_COUNTER 1000

/* 
 * The 100 Hz counter.
 */
static int hz100_cnt = 0;

static struct rtl_timeval userClock;

/**
 * Enumeration of the state of the clock.
 */
enum clock {
    CODED,		// normal state, clock set from time code inputs
    RESET_COUNTERS,	// need to reset our counters from the clock
    USER_SET_REQUESTED,	// user has requested to set the clock via ioctl
    USER_SET,		// clock has been set from user IRIG_SET_CLOCK ioctl
    USER_OVERRIDE_REQUESTED,// user has requested override of clock
    USER_OVERRIDE,	// clock has been overridden
};

/**
 * Current clock state.
 */
static unsigned char clockState=CODED;

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
static unsigned char extendedStatus = 
    DP_Extd_Sts_Nosync | DP_Extd_Sts_Nocode |
    DP_Extd_Sts_NoPPS | DP_Extd_Sts_NoMajT |
    DP_Extd_Sts_NoYear;

static unsigned char lastStatus =
    DP_Extd_Sts_Nosync | DP_Extd_Sts_Nocode |
    DP_Extd_Sts_NoPPS | DP_Extd_Sts_NoMajT |
    DP_Extd_Sts_NoYear;

/**
 * The year field in the pc104sg time registers
 * ranges from 0-99, so we keep track of the century.
 */
static int staticYear;

/**
 * structure setup by ioctl FIFO registration.
 */
static struct ioctlHandle* ioctlhandle = 0;

/**
 * User ioctls that we support.
 */
static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,_IOC_SIZE(GET_NUM_PORTS) },
  { IRIG_OPEN, _IOC_SIZE(IRIG_OPEN) },
  { IRIG_CLOSE, _IOC_SIZE(IRIG_CLOSE) },
  { IRIG_GET_STATUS, _IOC_SIZE(IRIG_GET_STATUS) },
  { IRIG_GET_CLOCK, _IOC_SIZE(IRIG_GET_CLOCK) },
  { IRIG_SET_CLOCK, _IOC_SIZE(IRIG_SET_CLOCK) },
  { IRIG_SET_CLOCK, _IOC_SIZE(IRIG_SET_CLOCK) },
  { IRIG_OVERRIDE_CLOCK, _IOC_SIZE(IRIG_OVERRIDE_CLOCK) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);

static char* devprefix = "irig";

/**
 * Structure of device information used when device is opened.
 */
static struct irig_port* portDev = 0;

static spinlock_t dp_ram_lock = SPIN_LOCK_UNLOCKED;
static int dp_ram_ext_status_enabled = 1;
static int dp_ram_ext_status_requested = 0;

/**
 * The 100 hz thread
 */
static rtl_pthread_t     pc104sgThread = 0;

/**
 * Semaphore that the interrupt service routine uses to wake up
 * the thread.
 */
static rtl_sem_t         threadsem;

#define CALL_BACK_POOL_SIZE 32	/* number of callbacks we can support */

/** macros borrowed from glibc/time functions */
#define	SECS_PER_HOUR	(60 * 60)
#define	SECS_PER_DAY	(SECS_PER_HOUR * 24)

/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
#define my_isleap(year)	\
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

static struct list_head callbacklists[IRIG_NUM_RATES];

static struct list_head callbackpool;

static rtl_pthread_mutex_t cblistmutex = RTL_PTHREAD_MUTEX_INITIALIZER;

/**
 * Module function that allows other modules to register their callback
 * function to be called at the given rate.  register_irig_callback
 * can be called at (almost) anytime, not just at module init time.
 * The only time that register/unregister_irig_callback cannot be
 * called is from within a callback function itself.
 * A callback function cannot do either register_irig_callback
 * or unregister_irig_callback, otherwise you'll get a deadlock on
 * cblistmutex.
 *
 */
int register_irig_callback(irig_callback_t* callback, enum irigClockRates rate,
                            void* privateData)
{
    struct list_head *ptr;
    struct irigCallback* cbentry;

    /* We could do a rtl_gpos_malloc of the entry, but that would require
     * that this function be called at module init time.
     * We want it to be call-able at any time, so we
     * rtl_gpos_malloc a pool of entries at this module's init time,
     * and grab an entry here.
     */

    rtl_pthread_mutex_lock(&cblistmutex);

    ptr = callbackpool.next;
    if (ptr == &callbackpool) {		/* none left */
	rtl_pthread_mutex_unlock(&cblistmutex);
        return -RTL_ENOMEM;
    }

    cbentry = list_entry(ptr,struct irigCallback, list);
    list_del(&cbentry->list);

    cbentry->callback = callback;
    cbentry->privateData = privateData;

    list_add(&cbentry->list,callbacklists + rate);
    rtl_pthread_mutex_unlock(&cblistmutex);

    return 0;
}

/**
 * Modules call this function to un-register their callbacks.
 * Note: this cannot be called from within a callback function
 * itself - a callback function cannot register/unregister itself, or
 * any other callback function.  If you try it you will get
 * a deadlock on the cblistmutex.
 */
void unregister_irig_callback(irig_callback_t* callback,
	enum irigClockRates rate)
{
    rtl_pthread_mutex_lock(&cblistmutex);

    struct list_head *ptr;
    struct irigCallback *cbentry;
    for (ptr = callbacklists[rate].next; ptr != callbacklists+rate;
    	ptr = ptr->next) {
	cbentry = list_entry(ptr,struct irigCallback, list);
	if (cbentry->callback == callback) {
	    /* remove it from the list for the rate, and add to the pool. */
	    list_del(&cbentry->list);
	    list_add(&cbentry->list,&callbackpool);
	    break;
	}
    }

    rtl_pthread_mutex_unlock(&cblistmutex);
}

/**
 * Cleanup function that un-registers all callbacks.
 */
static void free_callbacks()
{
    int i;

    struct list_head *ptr;
    struct list_head *newptr;
    struct irigCallback *cbentry;

    rtl_pthread_mutex_lock(&cblistmutex);

    for (i = 0; i < IRIG_NUM_RATES; i++) {
	for (ptr = callbacklists[i].next;
		ptr != callbacklists+i; ptr = newptr) {
	    newptr = ptr->next;
	    cbentry = list_entry(ptr,struct irigCallback, list);
	    /* remove it from the list for the rate, and add to the pool. */
	    list_del(&cbentry->list);
	    list_add(&cbentry->list,&callbackpool);
	}
    }

    for (ptr = callbackpool.next; ptr != &callbackpool; ptr = ptr->next) {
	cbentry = list_entry(ptr,struct irigCallback, list);
	rtl_gpos_free(cbentry);
    }

    rtl_pthread_mutex_unlock(&cblistmutex);
}

/**
 * After receiving a heartbeat interrupt, one must reset
 * the heart beat flag in order to receive further interrupts.
 */
static void inline ackHeartBeatInt (void)
{
    /* reset heart beat flag, write a 0 to bit 4, leave others alone */
    outb(intmask & ~Heartbeat, isa_address+Status_Port);
}

/**
 * Enable heart beat interrupts
 */
static void enableHeartBeatInt (void)
{
    intmask |= Heartbeat_Int_Enb;
#ifdef DEBUG
    rtl_printf("intmask=0x%x\n",intmask);
#endif
    ackHeartBeatInt();	// reset flag too to avoid immediate interrupt
}

/**
 * Disable heart beat interrupts
 */
static void disableHeartBeatInt (void)
{
    intmask &= ~Heartbeat_Int_Enb;
    outb(intmask, isa_address+Status_Port);
}

/**
 * After receiving a match interrupt, one must reset
 * the match flag in order to receive further interrupts.
 */
static void inline ackMatchInt (void)
{
    /* reset match flag, write a 0 to bit 3, leave others alone */
    outb(intmask & 0xf7, isa_address+Status_Port);
}

/**
 * Enable external time tag interrupt. These are caused by
 * a TTL input on a pin, and allows one to tag external
 * events.  This may be useful for synchronization tests of DSMs.
 */
static void enableExternEventInt (void)
{
    intmask |= Ext_Ready_Int_Enb;
    outb(intmask, isa_address+Status_Port);
}

/**
 * Disable external time tag interrupt.
 */
static void disableExternEventInt (void)
{
    intmask &= ~Ext_Ready_Int_Enb;
    outb(intmask, isa_address+Status_Port);
}

static void disableAllInts (void)
{
    /* disable all interrupts */
    intmask = 0x1f;
#ifdef DEBUG
    rtl_printf("intmask=0x%x\n",intmask);
#endif
    outb(intmask,isa_address+Status_Port);
}

/**
 * Read dual port RAM.
 * @param isRT    Set to 1 if this is called from a real-time thread.
 *     in which case this function uses rtl_clock_nanosleep(RTL_CLOCK_REALTIME,...).
 *     rtl_clock_nanosleep should only be called from a real-time thread, not at
 *     init time and not at interrupt time.
 *     Use isRT=0 if called from the ioctl callback (which is not a
 *	real-time thread). In this case this function uses
 *	a jiffy schedule method to delay.
 */
static int Read_Dual_Port_RAM (unsigned char addr,unsigned char* val,int isRT)
{
    int i;
    unsigned char status;
    struct rtl_timespec tspec;
    tspec.tv_sec = 0;
    tspec.tv_nsec = 20000;

    /* clear the response */
    inb(isa_address+Dual_Port_Data_Port);

    /* specify dual port address */
    outb(addr, isa_address+Dual_Port_Address_Port);

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
	if (isRT) rtl_clock_nanosleep(RTL_CLOCK_REALTIME,0,&tspec,0);
	else {
	    unsigned long j = jiffies + 1;
	    while (jiffies < j) schedule();
	}
	status = inb(isa_address+Extended_Status_Port);
    } while(i++ < 10 && !(status &  Response_Ready));
#ifdef DEBUG
    if (i > 1) rtl_printf("Read_Dual_Port_RAM, i=%d\n",i);
#endif

    /* check for a time out on the response... */
    if (!(status &  Response_Ready)) {
	rtl_printf("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
	return -1;
    }

    /* return read DP_Control value */
    *val = inb(isa_address+Dual_Port_Data_Port);
    return 0;
}

/**
 * Make a request to DP ram
 */
static inline void Req_Dual_Port_RAM(unsigned char addr)
{
    /* clear the response */
    inb(isa_address+Dual_Port_Data_Port);

    /* specify dual port address */
    outb(addr, isa_address+Dual_Port_Address_Port);
}

/**
 * Get requested value from DP ram. It must be ready.
 */
static inline void Get_Dual_Port_RAM(unsigned char* val)
{
    static int ntimeouts = 0;
    unsigned char status;
    status = inb(isa_address+Extended_Status_Port);

    /* check for a time out on the response... */
    if (!(status & Response_Ready)) {
	if (!(ntimeouts++ % 100))
	    rtl_printf("(%s) %s: timed out\n", __FILE__, __FUNCTION__);
	return;
    }

    /* return read DP_Control value */
    *val = inb(isa_address+Dual_Port_Data_Port);
}

/**
 * Set a value in dual port RAM.
 * @param isRT    Set to 1 if this is called from a real-time thread.
 *     in which case this function uses rtl_clock_nanosleep(RTL_CLOCK_REALTIME,...).
 *     rtl_clock_nanosleep should only be called from a real-time thread, not at
 *     init time and not at interrupt time.
 */
static int Set_Dual_Port_RAM (unsigned char addr, unsigned char value,int isRT)
{
    int i;
    unsigned char status;
    struct rtl_timespec tspec;
    tspec.tv_sec = 0;
    tspec.tv_nsec = 20000;	// 20 microsecond wait

    /* clear the response */
    inb(isa_address+Dual_Port_Data_Port);

    /* specify dual port address */
    outb(addr, isa_address+Dual_Port_Address_Port);

    wmb();

    /* wait for PC104 to acknowledge */
    /* On a 200Mhz viper, this took about 32 loops to
     * see the expected status.
     */
    i = 0;
    do {
	if (isRT) rtl_clock_nanosleep(RTL_CLOCK_REALTIME,0,&tspec,0);
	else {
	    unsigned long j = jiffies + 1;
	    while (jiffies < j) schedule();
	}
	status = inb(isa_address+Extended_Status_Port);
    } while(i++ < 10 && !(status &  Response_Ready));
#ifdef DEBUG
    if (i > 3) rtl_printf("Set_Dual_Port_RAM 1, i=%d\n",i);
#endif

    /* check for a time out on the response... */
    if (!(status &  Response_Ready)) {
	rtl_printf("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
	return -1;
    }

    /* clear the response */
    inb(isa_address+Dual_Port_Data_Port);

    /* write new value to DP RAM */
    outb(value, isa_address+Dual_Port_Data_Port);

    wmb();

    i = 0;
    do {
	if (isRT) rtl_clock_nanosleep(RTL_CLOCK_REALTIME,0,&tspec,0);
	else {
	    unsigned long j = jiffies + 1;
	    while (jiffies < j) schedule();
	}
	status = inb(isa_address+Extended_Status_Port);
    } while(i++ < 10 && !(status &  Response_Ready));
#ifdef DEBUG
    if (i > 3) rtl_printf("Set_Dual_Port_RAM 2, i=%d\n",i);
#endif

    /* check for a time out on the response... */
    if (!(status &  Response_Ready)) {
	rtl_printf("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
	return -1;
    }

    /* check that the written value matches */
    if (inb(isa_address+Dual_Port_Data_Port) != value) {
	rtl_printf("(%s) %s:\t no match on read-back\n",    __FILE__, __FUNCTION__);
	return -1;
    }

    /* success */
    return 0;
}


/* This controls COUNTER 1 on the PC104SG card 
 * Since it calls Set_Dual_Port_RAM it may only be called from
 * a real-time thread.
 */
static void setHeartBeatOutput (int rate,int isRT)
{
    int divide;
    unsigned char lsb, msb;

    divide = 3000000 / rate;

    lsb = (char)(divide & 0xff);
    msb = (char)((divide & 0xff00)>>8);

    Set_Dual_Port_RAM (DP_Ctr1_ctl,
	DP_Ctr1_ctl_sel | DP_ctl_rw | DP_ctl_mode3 | DP_ctl_bin,isRT);
    Set_Dual_Port_RAM (DP_Ctr1_lsb, lsb,isRT);
    Set_Dual_Port_RAM (DP_Ctr1_msb, msb,isRT);

    /* We'll wait until rate2 is set and then do a rejam. */
    // Set_Dual_Port_RAM (DP_Command, Command_Set_Ctr1,isRT);
}

/**
 * Set the primary time reference.
 * @param val 0=PPS is primary time reference, 1=time code is primary
 * Since it calls Set_Dual_Port_RAM it may only be called from
 * a real-time thread.
 */
static void setPrimarySyncReference(unsigned char val,int isRT)
{
    unsigned char control0;
    Read_Dual_Port_RAM (DP_Control0, &control0,isRT);

    if (val) control0 |= DP_Control0_CodePriority;
    else control0 &= ~DP_Control0_CodePriority;

#ifdef DEBUG
    rtl_printf("setting DP_Control0 to 0x%x\n",control0);
#endif
    Set_Dual_Port_RAM (DP_Control0, control0,isRT);
}

static void setTimeCodeInputSelect(unsigned char val,int isRT)
{
    Set_Dual_Port_RAM(DP_CodeSelect,val,isRT);
}

static void getTimeCodeInputSelect(unsigned char *val,int isRT)
{
    Read_Dual_Port_RAM(DP_CodeSelect,val,isRT);
}

/* -- Utility --------------------------------------------------------- */

/* This controls COUNTER 0 on the PC104SG card
 * Since it calls Set_Dual_Port_RAM it may only be called from
 * a real-time thread.
*/
static void setRate2Output (int rate,int isRT)
{
    int divide;
    unsigned char lsb, msb;

    divide = 3000000 / rate;

    lsb = (char)(divide & 0xff);
    msb = (char)((divide & 0xff00)>>8);
    Set_Dual_Port_RAM (DP_Ctr0_ctl,
	DP_Ctr0_ctl_sel | DP_ctl_rw | DP_ctl_mode3 | DP_ctl_bin,isRT);
    Set_Dual_Port_RAM (DP_Ctr0_lsb, lsb,isRT);
    Set_Dual_Port_RAM (DP_Ctr0_msb, msb,isRT);
}

static void counterRejam(int isRT)
{
    Set_Dual_Port_RAM(DP_Command, Command_Rejam,isRT);
}

/**
 * Break a struct rtl_timeval into the fields of a struct irigTime.
 * This uses some code from glibc/time routines.
 */
static void timespec2irig (const struct rtl_timespec* ts, struct irigTime* ti)
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
    ti->yday = days + 1;	// irig uses 1-366, unix 0-365

    rem = ts->tv_nsec;
    ti->msec = rem / 1000000;
    rem %= 1000000;
    ti->usec = rem / 1000;
    rem %= 1000;
    ti->nsec = rem;
}

static void timeval2irig (const struct rtl_timeval* tv, struct irigTime* ti)
{
    struct rtl_timespec ts;
    ts.tv_sec = tv->tv_sec;
    ts.tv_nsec = tv->tv_usec * 1000;
    timespec2irig(&ts,ti);
}
/**
 * Convert a struct irigTime into a struct rtl_timeval.
 */
static void irig2timespec(const struct irigTime* ti,struct rtl_timespec* ts)
{
    ts->tv_nsec = ti->msec * 1000000 + ti->usec * 1000 + ti->nsec;

    int y = ti->year;
    int nleap =  LEAPS_THRU_END_OF(y-1) - LEAPS_THRU_END_OF(1969);

    ts->tv_sec = (y - 1970) * 365 * SECS_PER_DAY +
    	(nleap + ti->yday - 1) * SECS_PER_DAY +
		ti->hour * 3600 + ti->min * 60 + ti->sec;
}

static void irig2timeval(const struct irigTime* ti,struct rtl_timeval* tv)
{
    struct rtl_timespec ts;
    irig2timespec(ti,&ts);
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = (ts.tv_nsec + 500) / 1000;
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
static void getTimeFields(struct irigTime* ti,int offset)
{
    unsigned char us0ns2,us2us1,ms1ms0,sec0ms2,min0sec1,hour0min1,day0hour1,
	day2day1,year10year1;

    /* reading the Usec1_Nsec100 value latches all other digits */
    us0ns2    = inb(isa_address+offset+Usec1_Nsec100_Port);	//0x0f
    us2us1    = inb(isa_address+offset+Usec100_Usec10_Port);	//0x0e
    ms1ms0    = inb(isa_address+offset+Msec10_Msec1_Port);	//0x0d
    sec0ms2   = inb(isa_address+offset+Sec1_Msec100_Port);	//0x0c   
    min0sec1  = inb(isa_address+offset+Min1_Sec10_Port);	//0x0b
    hour0min1 = inb(isa_address+offset+Hr1_Min10_Port);		//0x0a
    day0hour1 = inb(isa_address+offset+Day1_Hr10_Port);		//0x09
    day2day1  = inb(isa_address+offset+Day100_Day10_Port);	//0x08
    year10year1  = inb(isa_address+offset+Year10_Year1_Port);	//0x07

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
    rtl_printf("getTimeFields, year=%d, century=%d\n",
    	ti->year,(staticYear/100)*100);
    */

    /* After cold start the year field is not set, and it
     * takes some time before the setYear to DPR takes effect.
     * I saw values of 165 for the year during this time.
     */
    if (extendedStatus & DP_Extd_Sts_NoYear) {
	rtl_printf("fixing year=%d to %d\n",ti->year,staticYear);
        ti->year = staticYear;
    }
    // This has a Y2K problem, but who cares - it was written in 2004 and
    // it's for a real-time data system!
    else ti->year += (staticYear/100) * 100;

    ti->yday = ((day2day1 / 16) * 100) + ( (day2day1 & 0x0f) * 10) +
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
 * Get main clock.
 */
static void getCurrentTime(struct irigTime* ti)
{
    getTimeFields(ti,0);
#ifdef DEBUG
    // unsigned char status = inb(isa_address+Status_Port);
    dsm_sample_time_t tt = GET_MSEC_CLOCK;
    struct rtl_timespec ts;
    irig2timespec(ti,&ts);
    // clock difference
    int td = (ts.tv_sec % 86400) * 1000 + ts.tv_nsec / 1000000 - tt;
    int hr = (tt / 3600000);
    tt %= 3600000;
    int mn = (tt / 60000);
    tt %= 60000;
    int sc = tt / 1000;
    tt %= 1000;
    rtl_printf("%04d %03d %02d:%02d:%02d.%03d %03d %03d, clk=%02d:%02d:%02d.%03d,diff=%d,estat=0x%x,state=%d\n",
	ti->year,ti->yday,ti->hour,ti->min,ti->sec,ti->msec,ti->usec,ti->nsec,
    	hr,mn,sc,tt,td,extendedStatus,clockState);
#endif
}

/**
 * Get external event time.
 */
static void getExtEventTime(struct irigTime* ti) {
    return getTimeFields(ti,0x10);
}

/**
 * set the year fields in Dual Port RAM.
 * May only be called from a real-time thread.
 */
static void setYear(int val,int isRT)
{
    staticYear = val;
#ifdef DEBUG
    rtl_printf("setYear=%d\n",val);
#endif
    Set_Dual_Port_RAM(DP_Year1000_Year100,
	((val / 1000) << 4) + ((val % 1000) / 100),isRT);
    val %= 100;
    Set_Dual_Port_RAM(DP_Year10_Year1,((val / 10) << 4) + (val % 10),isRT);

    Set_Dual_Port_RAM (DP_Command, Command_Set_Years,isRT);
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
static int setMajorTime(struct irigTime* ti,int isRT)
{

#ifdef DEBUG
    // unsigned char status = inb(isa_address+Status_Port);
    rtl_printf("setMajor=%04d %03d %02d:%02d:%02d.%03d %03d %03d, estat=0x%x,state=%d\n",
	ti->year,ti->yday,ti->hour,ti->min,ti->sec,ti->msec,ti->usec,ti->nsec,
    	extendedStatus,clockState);
#endif
    /* The year fields in Dual Port RAM are not technically
     * part of the major time, but we'll set them too.  */
    setYear(ti->year,isRT);

    int val;
    val = ti->yday;

    Set_Dual_Port_RAM(DP_Major_Time_d100, val / 100,isRT);
    val %= 100;
    Set_Dual_Port_RAM(DP_Major_Time_d10d1, ((val / 10) << 4) + (val % 10),isRT);

    val = ti->hour;
    Set_Dual_Port_RAM(DP_Major_Time_h10h1, ((val / 10) << 4) + (val % 10),isRT);

    val = ti->min;
    Set_Dual_Port_RAM(DP_Major_Time_m10m1, ((val / 10) << 4)+ (val % 10),isRT);

    val = ti->sec;
    Set_Dual_Port_RAM(DP_Major_Time_s10s1, ((val / 10) << 4) + (val % 10),isRT);

    Set_Dual_Port_RAM (DP_Command, Command_Set_Major,isRT);

    return 0;
}

/**
 * Increment our clock by a number of ticks.
 */
static void inline increment_clock(int tick) 
{
    msecClockTicker += tick;
    msecClockTicker %= MSECS_PER_DAY;

    /*
     * This little double clock provides a clock that can be
     * read by external modules without needing a mutex.
     * It ensures that msecClock[readClock]
     * is valid for at least an interrupt period after reading the
     * value of readClock, even if this code is pre-emptive.
     *
     * This clock is incremented at the interrupt rate.
     * If somehow a bogged down piece of code reads the value of
     * readClock, and then didn't get around to reading
     * msecClock[readClock] until more than an interrupt period
     * later then it could read a half-written value, but that
     * ain't gunna happen.
     */
    msecClock[writeClock] = msecClockTicker;
    unsigned char c = readClock;
    /* prior to this line msecClock[readClock=0] is  OK to read */
    readClock = writeClock;
    /* now msecClock[readClock=1] is still OK to read. We're assuming
     * that the byte write of readClock is atomic.
     */
    writeClock = c;
}

static inline void increment_hz100_cnt()
{
    if (++hz100_cnt == MAX_THREAD_COUNTER) hz100_cnt = 0;
}

/**
 * Set the clock and 100 hz counter based on the time in a time val struct.
 */
static void setCounters(struct rtl_timeval* tv)
{
    msecClockTicker =
	(tv->tv_sec % SECS_PER_DAY) * 1000 +
		(tv->tv_usec + 500) / 1000;
    msecClockTicker -= msecClockTicker % MSEC_PER_INTRPT;
    msecClockTicker %= MSECS_PER_DAY;

    hz100_cnt = msecClockTicker / MSEC_PER_THREAD_SIGNAL;
    hz100_cnt %= MAX_THREAD_COUNTER;

#ifdef DEBUG
    rtl_printf("tv_sec=%d tv_usec=%d, msecClockTicker=%d, hz100_cnt=%d\n",
    	tv->tv_sec,tv->tv_usec,msecClockTicker,hz100_cnt);
#endif

}

/**
 * Update the clock counters to the current time.
 */
static inline void setCountersToClock()
{
    // reset counters to clock
    struct irigTime ti;
    struct rtl_timeval tv;
    getCurrentTime(&ti);
    irig2timeval(&ti,&tv);
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
	cbentry = list_entry(ptr,struct irigCallback, list);
	cbentry->callback(cbentry->privateData);
    }
}

/**
 * This is the thread function that loops forever, waiting
 * on a semaphore from the interrupt service routine.
 */
static void *pc104sg_100hz_thread (void *param)
{
    int isRT = 1;
    /*
     * Interrupts are not enabled at this point
     * until the call to enableHeartBeatInt() below.
     */
    /*
     * First initialize some dual port ram settings that
     * can't be done at init time.
     */
    /* IRIG-B is the default, but we'll set it anyway */
    setTimeCodeInputSelect(DP_CodeSelect_IRIGB,isRT);

#ifdef DEBUG
    {
	unsigned char timecode;
	getTimeCodeInputSelect(&timecode,isRT);
	rtl_printf("timecode=0x%x\n",timecode);
    }
#endif
    setPrimarySyncReference(0,isRT);	// 0=PPS, 1=timecode

    setHeartBeatOutput(INTERRUPT_RATE,isRT);
#ifdef DEBUG
    rtl_printf("(%s) %s:\t setHeartBeatOutput(%d) done\n", __FILE__, __FUNCTION__, INTERRUPT_RATE);
#endif

    setRate2Output(A2DREF_RATE,isRT);
#ifdef DEBUG
    rtl_printf("(%s) %s:\t setRate2Output(%d) done\n",     __FILE__, __FUNCTION__, A2DREF_RATE);
#endif

    /*
     * Set the internal heart-beat and rate2 to be in phase with
     * the PPS/time_code reference
     */
    counterRejam(isRT);

    /*
     * Set the major time here until we are doing it from user
     * space.
     */
// #define SET_MAJOR
#ifdef SET_MAJOR

    struct rtl_timespec ts;
    // rtl_clock_gettime(RTL_CLOCK_GPOS,&tspec); // note - as of rtldk-2.2 RTL_CLOCK_GPOS is now defined...
    rtl_clock_gettime(RTL_CLOCK_REALTIME,&ts);	// 1970
    // ts.tv_sec = 1107129600;		// Jan 31 2005, 00:00
    ts.tv_sec = 1104537540;		// Dec 31 2004, 23:59:00 leap
    ts.tv_sec = 1104537540-86400;	// Dec 30 2004, 23:59:00 leap
    // ts.tv_sec = 946684740;		// Dec 31 1999, 23:59:00 noleap
    // ts.tv_sec = 852076740;		// Dec 31 1996, 23:59:00 leap
    // ts.tv_sec = 978307140;		// Dec 31 2000, 23:59:00 leap


    userClock.tv_sec = ts.tv_sec;
    userClock.tv_usec = ts.tv_nsec / 1000;
    clockState = USER_SET_REQUESTED;
#else
    clockState = RESET_COUNTERS;
#endif

    /* initial request of extended status from DPR */
    Req_Dual_Port_RAM(DP_Extd_Sts);

    /* start interrupts */
    enableHeartBeatInt();
#ifdef DEBUG
    rtl_printf("(%s) %s:\t enableHeartBeatInt  done\n",    __FILE__, __FUNCTION__);
#endif

    /* semaphore timeout in nanoseconds */
    unsigned long nsec_deltat = MSEC_PER_THREAD_SIGNAL * 1000000;
    struct rtl_timespec timeout;
    int ntimeouts = 0;
    int status = 0;
    int msecs_since_last_timeout = 0;

    nsec_deltat += (nsec_deltat * 3) / 8;	/* add 3/8ths more */
    // rtl_printf("nsec_deltat = %d\n",nsec_deltat);

    rtl_clock_gettime(RTL_CLOCK_REALTIME,&timeout);

    for (;;) {

	/* wait for the pc104sg_isr to signal us */
	// rtl_timespec_add_ns(&timeout, nsec_deltat);
	timeout.tv_nsec += nsec_deltat;
	if (timeout.tv_nsec >= NSECS_PER_SEC) {
	    timeout.tv_sec++;
	    timeout.tv_nsec -= NSECS_PER_SEC;
	}

	/* If we get a timeout on the semaphore, then we've missed
	 * an irig interrupt.  Report the status and re-enable.
	 */
	if (rtl_sem_timedwait(&threadsem,&timeout) < 0) {
	    if (rtl_errno == RTL_ETIMEDOUT) {
		// If clock is not overidden and we have time
		// codes, then set counters to the clock
		if (clockState == CODED) clockState = RESET_COUNTERS;
		else {
		    // increment the clock and counter ourselves.
		    // it is presumably safe to increment since
		    // interrupts aren't happening!
		    // This shouldn't violate the policy of
		    // msecClock[readClock], which is that it
		    // isn't updated more often than once an interrupt.
		    // See the comments in increment_clock.
		    increment_clock(MSEC_PER_THREAD_SIGNAL);
		    increment_hz100_cnt();
		}
		if (!(ntimeouts++ % 1)) {
		    rtl_printf("pc104sg thread semaphore timeout #%d, msecs since last timeout=%d\n",
			    ntimeouts, msecs_since_last_timeout);
		    rtl_printf("ackHeartBeatInt\n");
		    ackHeartBeatInt();
		}
		msecs_since_last_timeout = 0;
	    }
	    else if (rtl_errno == RTL_EINTR) {
	        rtl_printf("pc104sg thread interrupted\n");
		status = rtl_errno;
		break;
	    }
	    else {
	        rtl_printf("pc104sg thread error, error=%d\n",
			rtl_errno);
		status = rtl_errno;
		break;
	    }
	}
	else msecs_since_last_timeout += MSEC_PER_THREAD_SIGNAL;

	rtl_clock_gettime(RTL_CLOCK_REALTIME,&timeout);

	/* this macro creates a code block enclosed in {} brackets,
	 * which is terminated by rtl_pthread_cleanup_pop */
	rtl_pthread_cleanup_push((void(*)(void*))rtl_pthread_mutex_unlock,
		(void*)&cblistmutex);

	rtl_pthread_mutex_lock(&cblistmutex);
    
	/* perform 100Hz processing... */
	doCallbacklist(callbacklists + IRIG_100_HZ);

        if ((hz100_cnt %   2)) goto _5;

        /* perform 50Hz processing... */
        doCallbacklist(callbacklists + IRIG_50_HZ);

        if ((hz100_cnt %   4)) goto _5;

        /* perform 25Hz processing... */
        doCallbacklist(callbacklists + IRIG_25_HZ);

_5:     if ((hz100_cnt %   5)) goto cleanup_pop;

        /* perform 20Hz processing... */
        doCallbacklist(callbacklists + IRIG_20_HZ);

        if ((hz100_cnt %  10)) goto _25;

        /* perform 10Hz processing... */
        doCallbacklist(callbacklists + IRIG_10_HZ);

        if ((hz100_cnt %  20)) goto _25;

        /* perform  5Hz processing... */
        doCallbacklist(callbacklists + IRIG_5_HZ);

_25:    if ((hz100_cnt %  25)) goto cleanup_pop;

        /* perform  4Hz processing... */
        doCallbacklist(callbacklists + IRIG_4_HZ);

        if ((hz100_cnt %  50)) goto cleanup_pop;

        /* perform  2Hz processing... */
        doCallbacklist(callbacklists + IRIG_2_HZ);

        if ((hz100_cnt % 100)) goto cleanup_pop;

        /* perform  1Hz processing... */
        doCallbacklist(callbacklists + IRIG_1_HZ);

        if ((hz100_cnt % 1000)) goto cleanup_pop;

        /* perform  0.1 Hz processing... */
        doCallbacklist(callbacklists + IRIG_0_1_HZ);

cleanup_pop:
	rtl_pthread_cleanup_pop(1);


    }
    rtl_printf("pc104sg_100hz_thread run method exiting!\n");
    return (void*) status;
}

/*
 * Check the extended status byte.
 * If clock status has changed adjust our clock counters.
 * This function is called by the interrupt service routine
 * and so we don't have to worry about simultaneous access
 * when changing the clock counters.
 */
static inline void checkExtStatus()
{

    spin_lock(&dp_ram_lock);

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

    if (dp_ram_ext_status_requested) {
        Get_Dual_Port_RAM(&extendedStatus);
	dp_ram_ext_status_requested = 0;
    }

    /* send next request */
    if (dp_ram_ext_status_enabled) {
	Req_Dual_Port_RAM(DP_Extd_Sts);
	dp_ram_ext_status_requested = 1;
    }
    spin_unlock(&dp_ram_lock);

    switch (clockState) {
    case USER_OVERRIDE_REQUESTED:
	setCounters(&userClock);
	clockState = USER_OVERRIDE;
	break;
    case USER_SET_REQUESTED:
	// has requested to set the clock, and we
	// have no time code: then set the clock counters
	// by the user clock
        if ((lastStatus & DP_Extd_Sts_Nocode) &&
		(extendedStatus & DP_Extd_Sts_Nocode)) {
	    setCounters(&userClock);
	    clockState = USER_SET;
	}
	// ignore request since we have time code
	else clockState = CODED;
	break;
    case USER_SET:
        if (!(lastStatus & DP_Extd_Sts_Nocode) &&
		!(extendedStatus & DP_Extd_Sts_Nocode)) {
	    // have good clock again, set counters back to coded clock
	    clockState = RESET_COUNTERS;
	}
	break;
    case RESET_COUNTERS:
    case USER_OVERRIDE:
    case CODED:
	break;
    }
    /* At this point clockState is either
     * CODED: we're going with whatever the hardware clock says
     * RESET_COUNTERS: need to reset our counters to hardware clock
     * USER_OVERRIDE: user has overridden the clock
     *		In this case the hardware clock doesn't
     *		match our own counters.
     * USER_SET: the clock has been set from an ioctl with setMajorTime(),
     *          and time code input is missing.  Since the
     *		timecode is missing, the counters will be within
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
    if (clockState == CODED &&
	    (lastStatus & DP_Extd_Sts_Nosync) &&
	    !(extendedStatus & DP_Extd_Sts_Nosync))
	    	clockState = RESET_COUNTERS;

    if (clockState == RESET_COUNTERS) {
    	setCountersToClock();
	clockState = CODED;
    }
    lastStatus = extendedStatus;
}

/*
 * pc104sg interrupt function.
 */
static unsigned int pc104sg_isr (unsigned int irq, void* callbackPtr, struct rtl_frame *regs)
{
    unsigned char status = inb(isa_address+Status_Port);

    if ((status & Heartbeat) && (intmask & Heartbeat_Int_Enb)) {
	/* acknowledge interrupt */
	ackHeartBeatInt();

	increment_clock(MSEC_PER_INTRPT);

	checkExtStatus();

	/*
	 * On 10 millisecond intervals wake up the thread so
	 * it can perform the callbacks at the various rates.
	 */
	if (!(msecClockTicker % MSEC_PER_THREAD_SIGNAL)) {
	    increment_hz100_cnt();
	    rtl_sem_post( &threadsem );
	}

    }
#ifdef CHECK_EXT_EVENT
    if ((status & Ext_Ready) && (intmask & Ext_Ready_Int_Enb)) {
	struct irigTime ti;
	getExtEventTime(&ti);
	rtl_printf("ext event=%04d %03d %02d:%02d:%02d.%03d %03d %03d, stat=0x%x, state=%d\n",
	    ti.year, ti.yday, ti.hour, ti.min, ti.sec,ti.msec, ti.usec, ti.nsec,
	    extendedStatus,clockState);
    }
#endif
    return 0;
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
    if (clockState == CODED) {
	struct rtl_timespec ts;
	irig2timespec(&ti,&ts);
	// clock difference
	int td = (ts.tv_sec % 86400) * 1000 + ts.tv_nsec / 1000000 - tt;
	if (td != 0) clockState = RESET_COUNTERS;
    }

    if (dev->inFifoFd >= 0) {
	dev->samp.timetag = tt;
	dev->samp.length = sizeof(struct dsm_clock_data);

	irig2timeval(&ti,&dev->samp.data.tval);
	dev->samp.data.status = extendedStatus;

#ifdef DEBUG
	rtl_printf("tv_secs=%d, tv_usecs=%d status=0x%x\n",
		dev->samp.data.tval.tv_sec,dev->samp.data.tval.tv_usec,
		dev->samp.data.status);
#endif

	rtl_write(dev->inFifoFd,&dev->samp,
		SIZEOF_DSM_SAMPLE_HEADER + dev->samp.length);
    }

#ifdef DEBUG_XXX
    // unsigned char status = inb(isa_address+Status_Port);
    dsm_sample_time_t tt = GET_MSEC_CLOCK;
    int hr = (tt / 3600000);
    tt %= 3600000;
    int mn = (tt / 60000);
    tt %= 60000;
    int sc = tt / 1000;
    tt %= 1000;
    rtl_printf("%04d %03d %02d:%02d:%02d.%03d %03d %03d, clk=%02d:%02d:%02d.%03d, estat=0x%x,state=%d\n",
	ti.year,ti.yday,ti.hour,ti.min,ti.sec,ti.msec,ti.usec,ti.nsec,
    	hr,mn,sc,tt,extendedStatus,clockState);
#endif

}

static int close_port(struct irig_port* port)
{
    if (port->inFifoFd >= 0) {
	int fd = port->inFifoFd;
	port->inFifoFd = -1;
	rtl_printf("closing %s\n",port->inFifoName);
        rtl_close(fd);
    }
    return 0;
}
static int open_port(struct irig_port* port)
{
    int retval;
    if ((retval = close_port(port))) return retval;

    /* user opens port for read, so we open it for writing. */
    rtl_printf("opening %s\n",port->inFifoName);
    if ((port->inFifoFd = rtl_open(port->inFifoName, RTL_O_NONBLOCK | RTL_O_WRONLY)) < 0)
	return -rtl_errno;

    return retval;
}

/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.  This is not being executed from a real-time thread.
 */
static int ioctlCallback(int cmd, int board, int portNum,
        void *buf, rtl_size_t len)
{
    int retval = -RTL_EINVAL;
    int isRT = 0;
    rtl_printf("ioctlCallback, cmd=0x%x board=%d, portNum=%d\n",
    	cmd,board,portNum);

    /* only one board and one port supported by this module */
    if (board != 0) return retval;
    if (portNum != 0) return retval;
                                                                                
    switch (cmd) {
    case GET_NUM_PORTS:         /* user get */
        *(int *) buf = 1;
        retval = sizeof(int);
        break;
    case IRIG_OPEN:           /* open port */
        rtl_printf("IRIG_OPEN\n");
#ifdef DEBUG
#endif
        retval = open_port(portDev);
        break;
    case IRIG_CLOSE:          /* close port */
        rtl_printf("IRIG_CLOSE\n");
#ifdef DEBUG
#endif
        retval = close_port(portDev);
        break;
    case IRIG_GET_STATUS:
        *((unsigned char*)buf) = extendedStatus;
	retval = 1;
	break;
    case IRIG_GET_CLOCK:
	{
	    struct irigTime ti;
	    struct rtl_timeval tv;
	    if (len != sizeof(tv)) break;
	    getCurrentTime(&ti);
	    irig2timeval(&ti,&tv);
	    memcpy(buf,&tv,sizeof(tv));
	    retval = len;
	}
	break;
    case IRIG_SET_CLOCK:
	{
	    struct irigTime ti;
	    unsigned long flags;
	    if (len != sizeof(userClock)) break;
	    memcpy(&userClock,buf,sizeof(userClock));

	    timeval2irig(&userClock,&ti);

	    spin_lock_irqsave(&dp_ram_lock,flags);
	    dp_ram_ext_status_enabled = 0;
	    dp_ram_ext_status_requested = 0;
	    spin_unlock_irqrestore(&dp_ram_lock,flags);

	    if (extendedStatus & DP_Extd_Sts_Nocode) setMajorTime(&ti,isRT);
	    else setYear(ti.year,isRT);

	    spin_lock_irqsave(&dp_ram_lock,flags);
	    dp_ram_ext_status_enabled = 1;
	    spin_unlock_irqrestore(&dp_ram_lock,flags);

	    clockState = USER_SET_REQUESTED;
	    retval = len;
	}
	break;
    case IRIG_OVERRIDE_CLOCK:
	{
	    struct irigTime ti;
	    unsigned long flags;
	    if (len != sizeof(userClock)) break;
	    memcpy(&userClock,buf,sizeof(userClock));
	    timeval2irig(&userClock,&ti);

	    spin_lock_irqsave(&dp_ram_lock,flags);
	    dp_ram_ext_status_enabled = 0;
	    dp_ram_ext_status_requested = 0;
	    spin_unlock_irqrestore(&dp_ram_lock,flags);

	    if (extendedStatus & DP_Extd_Sts_Nocode) setMajorTime(&ti,isRT);
	    else setYear(ti.year,isRT);
	
	    spin_lock_irqsave(&dp_ram_lock,flags);
	    dp_ram_ext_status_enabled = 1;
	    spin_unlock_irqrestore(&dp_ram_lock,flags);

	    clockState = USER_OVERRIDE_REQUESTED;
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
    /* cancel the thread. Because this thread does a time-out wait
     * on a semaphore from the interrupt service routine,
     * we cancel it before disabling interrupts.
     */
    if (pc104sgThread) {
        if (rtl_pthread_kill( pc104sgThread,SIGTERM ) < 0)
	    rtl_printf("rtl_pthread_kill failure\n");
	rtl_pthread_join( pc104sgThread, NULL );
    }

    /* free up our pool of callbacks */
    free_callbacks();

    disableAllInts();

    rtl_free_isa_irq(irq);

    if (portDev) {
	close_port(portDev);
        if (portDev->inFifoName) {
	    rtl_unlink(portDev->inFifoName);
	    rtl_gpos_free(portDev->inFifoName);
	}
	rtl_gpos_free(portDev);
    }
    if (ioctlhandle) closeIoctlFIFO(ioctlhandle);

    /* free up the I/O region and remove /proc entry */
    if (isa_address)
    	release_region(isa_address, PC104SG_IOPORT_WIDTH);

    rtl_sem_destroy(&threadsem);

    rtl_printf("(%s) %s:\t done\n\n", __FILE__, __FUNCTION__);

}

/* -- MODULE ---------------------------------------------------------- */
/* activate the pc104sg-B board */
int init_module (void)
{
    int i;
    int errval = 0;
    unsigned int addr;
    int irq_requested = 0;

    rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
	     __FILE__, __FUNCTION__, __DATE__, __TIME__);

    INIT_LIST_HEAD(&callbackpool);
    for (i = 0; i < IRIG_NUM_RATES; i++)
	INIT_LIST_HEAD(callbacklists+i);

    /* initialize the semaphore to the 100 hz thread */
    rtl_sem_init (&threadsem, /* which semaphore */
	    1,                /* usable between processes? Usu. 1 */
	    0);               /* initial value */


    /* check for module parameters */
    addr = (unsigned int)ioport + SYSTEM_ISA_IOPORT_BASE;

    /* Grab the region so that no one else tries to probe our ioports. */
    if ((errval = check_region(addr, PC104SG_IOPORT_WIDTH)) < 0) goto err0;
    request_region(addr, PC104SG_IOPORT_WIDTH, "pc104sg");
    isa_address = addr;

    /* initialize clock counters that external modules grab */
    readClock = 0;
    writeClock = 0;
    msecClock[readClock] = 0;
    msecClock[writeClock] = 0;

    /* shutoff pc104sg interrupts just in case */
    disableAllInts();

    portDev = rtl_gpos_malloc( sizeof(struct irig_port) );
    if (!portDev) goto err0;
    portDev->inFifoFd = -1;
    portDev->inFifoName = makeDevName(devprefix,"_in_",0);
    if (!portDev->inFifoName) goto err0;

    rtl_printf("creating %s\n",portDev->inFifoName);

    // remove broken device file before making a new one
    if (rtl_unlink(portDev->inFifoName) < 0)
      if ( (errval = -rtl_errno) != -RTL_ENOENT ) goto err0;

    if (rtl_mkfifo(portDev->inFifoName, 0666) < 0) {
	errval = -rtl_errno;
	rtl_printf("rtl_mkfifo %s failed, errval=%d\n",
	    portDev->inFifoName,errval);
	if (errval != -RTL_EEXIST && errval != -EBADE) goto err0;
    }

    /* setup our device */
    if (!(ioctlhandle = openIoctlFIFO(devprefix,
	0,ioctlCallback,nioctlcmds,ioctlcmds))) {
	errval = -RTL_EINVAL;
	goto err0;
    }

    /* create our pool of callback entries */
    errval = -RTL_ENOMEM;
    for (i = 0; i < CALL_BACK_POOL_SIZE; i++) {
	struct irigCallback* cbentry =
	    (struct irigCallback*) rtl_gpos_malloc( sizeof(struct irigCallback) );
	if (!cbentry) goto err0;
	list_add(&cbentry->list,&callbackpool);
    }

    if ((errval = rtl_request_isa_irq(irq, pc104sg_isr,0 )) < 0 ) {
	/* failed... */
	rtl_printf("(%s) %s:\t could not allocate IRQ %d\n",
		   __FILE__, __FUNCTION__, irq);
	goto err0;
    }
    irq_requested = 1;

    /* start the 100 Hz thread */
    if ((errval = rtl_pthread_create(
    	&pc104sgThread, NULL, pc104sg_100hz_thread, (void *)0))) {
        errval = -errval;
	goto err0;
    }

    if ((errval = register_irig_callback(portCallback, IRIG_1_HZ,portDev))
    	< 0) goto err0;

    return 0;

err0:

    /* kill the thread */
    if (pc104sgThread) {
        rtl_pthread_kill( pc104sgThread,SIGTERM );
	rtl_pthread_join( pc104sgThread, NULL );
    }

    /* free up our pool of callbacks */
    free_callbacks();

    disableAllInts();

    if (irq_requested) rtl_free_isa_irq(irq);

    if (ioctlhandle) closeIoctlFIFO(ioctlhandle);

    if (portDev) {
        if (portDev->inFifoName) {
	    rtl_unlink(portDev->inFifoName);
	    rtl_gpos_free(portDev->inFifoName);
	}
	rtl_gpos_free(portDev);
    }

    /* free up the I/O region and remove /proc entry */
    if (isa_address)
    	release_region(isa_address, PC104SG_IOPORT_WIDTH);

    rtl_sem_destroy(&threadsem);
    return errval;
}

