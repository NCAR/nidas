/* pc104sg.c

   RTLinux pc104sg driver for the ISA bus based jxi2 pc104-SG card.

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

     $LastChangedRevision: $
         $LastChangedDate: $
           $LastChangedBy: $
                 $HeadURL: $
*/


#include <pthread.h>
#include <semaphore.h>
#include <rtl_core.h>
#include <unistd.h>
#include <rtl_time.h>
#include <rtl.h>

#include <linux/ioport.h>
#include <linux/list.h>

#include <dsm_viper.h>
#include <pc104sg.h>
#include <irigclock.h>
#include <rtl_isa_irq.h>

RTLINUX_MODULE(pc104sg);

#define INTERRUPT_RATE 1000
	
/* TODO - hz100_cnt needs to be initialized to the zero'th second read from
   the pc104sg card...  the current implementation arbitrarily sets it to zero
   at the first occurance of this ISR. */

static int hz100_cnt = 0;

unsigned long msecClock[2] = { 0, 0};
unsigned char readClock = 0;
static unsigned char writeClock = 1;
static unsigned long msecClockTicker;

/* module prameters (can be passed in via command line) */
static unsigned int irq = 10;
static int ioport = 0x2a0;
static unsigned int m_rate[2] = {INTERRUPT_RATE, 50000};

MODULE_PARM(irq, "i");
MODULE_PARM(ioport, "i");
MODULE_PARM(m_rate, "1-2i");

MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("RTLinux ISA pc104-SG jxi2 Driver");

/* Actual physical address of this card. Set in init_module */
static unsigned int isa_address = 0;

/* The 100 hz thread */
static pthread_t     pc104sgThread = 0;
static sem_t         threadsem;

#define CALL_BACK_POOL_SIZE 32	/* number of callbacks we can support */

struct irigCallback {
  struct list_head list;
  irig_callback_t* callback;
  void* privateData;
};

static struct list_head callbacklists[IRIG_NUM_RATES];

static struct list_head callbackpool;

static pthread_mutex_t cblistmutex = PTHREAD_MUTEX_INITIALIZER;


/*
 * add a callback entry to the list of callbacks for the given rate.
 */
int register_irig_callback(irig_callback_t* callback, enum irigClockRates rate,
                            void* privateData)
{
    struct list_head *ptr;
    struct irigCallback* cbentry;

    /* We could do a kmalloc of the entry, but that would require
     * that this function be called at module init time.
     * We want it to be call-able at any time, so we
     * kmalloc a pool of entries at this module's init time,
     * and grab an entry here.
     */

    pthread_mutex_lock(&cblistmutex);

    ptr = callbackpool.next;
    if (ptr == &callbackpool) {		/* none left */
	pthread_mutex_unlock(&cblistmutex);
        return -ENOMEM;
    }

    cbentry = list_entry(ptr,struct irigCallback, list);
    list_del(&cbentry->list);

    cbentry->callback = callback;
    cbentry->privateData = privateData;

    list_add(&cbentry->list,callbacklists + rate);
    pthread_mutex_unlock(&cblistmutex);

    return 0;
}

void unregister_irig_callback(irig_callback_t* callback, enum irigClockRates rate)
{
    pthread_mutex_lock(&cblistmutex);

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

    pthread_mutex_unlock(&cblistmutex);
}

static void free_callbacks()
{
    int i;

    struct list_head *ptr;
    struct list_head *newptr;
    struct irigCallback *cbentry;

    pthread_mutex_lock(&cblistmutex);

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
	kfree(cbentry);
    }

    pthread_mutex_unlock(&cblistmutex);
}

/* -- Utility --------------------------------------------------------- */
void inline enableHeartBeatInt (void)
{
    /* enable heart beat interrupt (disables others) */
    outb(Heartbeat_Int_Enb + Reset_Heartbeat,
  	isa_address+Status_Port);
}

/* -- Utility --------------------------------------------------------- */
void disableHeartBeatInt (void)
{
  /* disble all interrupts */
  outb(Heartbeat_Int_Enb + Reset_Heartbeat,
  	isa_address+Status_Port);
}

/* -- Utility --------------------------------------------------------- */
static int Read_Dual_Port_RAM (unsigned int addr,unsigned int* val)
{
  int i;
  unsigned char status;

  /* clear the respose flag */
  inb(isa_address+Dual_Port_Data_Port);

  /* specify dual port address */
  outb(addr, isa_address+Dual_Port_Address_Port);

  wmb();

  /* wait for PC104 to acknowledge */
  i = 0;
  do {
    unsigned long jwait = jiffies + 1;
    while (jiffies < jwait) schedule();
    status = inb(isa_address+Extended_Status_Port);
  } while(i++ < 10 && !(status &  Response_Ready));

  /* check for a time out on the response... */
  if (!(status &  Response_Ready)) {
    rtl_printf("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
    return -1;
  }

  /* return read DP_Control value */
  *val = inb(isa_address+Dual_Port_Data_Port);
  return 0;
}

/* -- Utility --------------------------------------------------------- */
static int Set_Dual_Port_RAM (unsigned char addr, unsigned char value)
{
  int i;
  unsigned char status;

  /* clear the respose flag */
  inb(isa_address+Dual_Port_Data_Port);

  /* specify dual port address */
  outb(addr, isa_address+Dual_Port_Address_Port);

  wmb();

  /* wait for PC104 to acknowledge */
  i = 0;
  do {
    unsigned long jwait = jiffies + 1;
    while (jiffies < jwait) schedule();
    status = inb(isa_address+Extended_Status_Port);
    // rtl_printf("wait1 jwait=%d,jiffies=%d status=%x\n",jwait,jiffies,status);
  } while(i++ < 10 && !(status &  Response_Ready));

  /* check for a time out on the response... */
  if (!(status &  Response_Ready)) {
    rtl_printf("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
    return -1;
  }

  /* clear the respose flag */
  inb(isa_address+Dual_Port_Data_Port);

  /* write new value to DP RAM */
  outb(value, isa_address+Dual_Port_Data_Port);

  wmb();

  i = 0;
  do {
    unsigned long jwait = jiffies + 1;
    while (jiffies < jwait) schedule();
    status = inb(isa_address+Extended_Status_Port);
    // rtl_printf("wait2 jwait=%d,jiffies=%d status=%x\n",jwait,jiffies,status);
  } while(i++ < 10 && !(status &  Response_Ready));

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

/* -- Utility --------------------------------------------------------- */
void setHeartBeatOutput (int rate)
{
  int divide;
  unsigned char lsb, msb;

  divide = 3000000 / rate;
  lsb = (char)(divide & 0xff);
  msb = (char)((divide & 0xff00)>>8);

  Set_Dual_Port_RAM (DP_Ctr1_ctl,
  	DP_Ctr1_ctl_sel + DP_ctl_rw + DP_ctl_mode3 + DP_ctl_bin);
  Set_Dual_Port_RAM (DP_Ctr1_msb, msb);
  Set_Dual_Port_RAM (DP_Ctr1_lsb, lsb);
  Set_Dual_Port_RAM (DP_Command, Command_Rejam);
}

/* -- Utility --------------------------------------------------------- */
void setRate2Output (int rate)
{
  int divide;
  unsigned char lsb, msb;

  divide = 5000000 / rate;
  lsb = (char)(divide & 0xff);
  msb = (char)((divide & 0xff00)>>8);
  Set_Dual_Port_RAM (DP_Ctr2_ctl,
  	DP_Ctr2_ctl_sel + DP_Ctr1_ctl_sel + DP_ctl_rw + DP_ctl_mode3 + DP_ctl_bin);
  Set_Dual_Port_RAM (DP_Ctr2_msb, msb);
  Set_Dual_Port_RAM (DP_Ctr2_lsb, lsb);
  Set_Dual_Port_RAM (DP_Command, Command_Rejam);
}


int getTimeFields(struct irigTime* tp)
{
  /* Reads the time from the interface. */
  unsigned char us0,us1us0,ms1ms0,sec0ms2,min0sec1,hour0min1,day0hour1,
  	day2day1,year10year1;

  /* LATCH ALL DIGITS NOW */
  us0       = inb(isa_address+Usec1_Nsec100_Port);	//f
  rmb();
  us1us0    = inb(isa_address+Usec100_Usec10_Port);	//e
  ms1ms0    = inb(isa_address+Msec10_Msec1_Port);	//d
  sec0ms2   = inb(isa_address+Sec1_Msec100_Port);	//c   
  min0sec1  = inb(isa_address+Min1_Sec10_Port);		//b
  hour0min1 = inb(isa_address+Hr1_Min10_Port);		//a
  day0hour1 = inb(isa_address+Day1_Hr10_Port);		//9
  day2day1  = inb(isa_address+Day100_Day10_Port);	//8
  year10year1  = inb(isa_address+Year10_Year1_Port);	//8

  tp->year = (year10year1 >> 4) * 10 + (year10year1 & 0x0f);
  tp->day = ((day2day1 >> 4) * 100) +
         ((day2day1 & 0x0f) * 10) +
         (day0hour1 >> 4);

  tp->hour = (day0hour1 & 0x0f) * 10 + (hour0min1) / 16;

  tp->min = (hour0min1 & 0x0f) * 10 + min0sec1  / 16;

  tp->sec =     (min0sec1 & 0x0f) * 10 + sec0ms2   / 16;

  tp->msec = ((sec0ms2 & 0x0f) * 100) + ((ms1ms0 / 16) * 10) + (ms1ms0 & 0x0f);

  tp->usec = (((us1us0 & 0xf0) / 16) * 100) +
         ( (us1us0 & 0x0f) * 10) +
         ((us0     & 0xf0) / 16);

  tp->nsec = (us0 & 0x0f);
  return 0;
}

int getExtTimeFields(struct irigTime* tp)
{
  /* Reads the time from the interface. */
  unsigned char us0,us1us0,ms1ms0,sec0ms2,min0sec1,hour0min1,day0hour1,
  	day2day1,year10year1;

  /* LATCH ALL DIGITS NOW */
  us0       = inb(isa_address+Ext_Usec1_Nsec100_Port);	//0x1f
  rmb();
  us1us0    = inb(isa_address+Ext_Usec100_Usec10_Port);	//0x1e
  ms1ms0    = inb(isa_address+Ext_Msec10_Msec1_Port);	//0x1d
  sec0ms2   = inb(isa_address+Ext_Sec1_Msec100_Port);	//0x1c   
  min0sec1  = inb(isa_address+Ext_Min1_Sec10_Port);	//0x1b
  hour0min1 = inb(isa_address+Ext_Hr1_Min10_Port);	//0x1a
  day0hour1 = inb(isa_address+Ext_Day1_Hr10_Port);	//0x19
  day2day1  = inb(isa_address+Ext_Day100_Day10_Port);	//0x18
  year10year1  = inb(isa_address+Ext_Year10_Year1_Port);//0x17

  // According to pc104sg manual: time code inputs do not
  // contain year information.  So, it appears that this
  // year would only be incremented on a year rollover.
  tp->year = (year10year1 >> 4) * 10 + (year10year1 & 0x0f);
  tp->day = (((day2day1 & 0xf0) / 16) * 100) +
         ( (day2day1 & 0x0f) * 10) +
         ((day0hour1 & 0xf0) / 16);

  tp->hour = (day0hour1 & 0x0f) * 10 + (hour0min1) / 16;

  tp->min = (hour0min1 & 0x0f) * 10 + min0sec1  / 16;

  tp->sec =     (min0sec1 & 0x0f) * 10 + sec0ms2   / 16;

  tp->msec = ((sec0ms2 & 0x0f) * 100) + ((ms1ms0 / 16) * 10) + (ms1ms0 & 0x0f);

  tp->usec = (((us1us0 & 0xf0) / 16) * 100) +
         ( (us1us0 & 0x0f) * 10) +
         ((us0     & 0xf0) / 16);

  tp->nsec = (us0 & 0x0f);

  return 0;
}


int setMajorTime(struct irigTime* it)
{

    /* the year fields in Dual Port RAM are not technically
     * part of the major time, but we'll set them too.
     */
    int val = it->year;
    Set_Dual_Port_RAM (DP_Year1000_Year100,
	((val / 1000) << 4) + ((val % 1000) / 100));
    val %= 100;
    Set_Dual_Port_RAM (DP_Year10_Year1,((val / 10) << 4) + (val % 10));

    val = it->day;
    Set_Dual_Port_RAM (DP_Major_Time_d100, val / 100);
    val %= 100;
    Set_Dual_Port_RAM (DP_Major_Time_d10d1, ((val / 10) << 4) + (val % 10));

    val = it->hour;
    Set_Dual_Port_RAM (DP_Major_Time_h10h1, ((val / 10) << 4) + (val % 10));

    val = it->min;
    Set_Dual_Port_RAM (DP_Major_Time_m10m1, ((val / 10) << 4)+ (val % 10));

    val = it->sec;
    Set_Dual_Port_RAM (DP_Major_Time_s10s1, ((val / 10) << 4) + (val % 10));

    // Set_Dual_Port_RAM (DP_Major_Time_d100d10, 0x03);
    // Set_Dual_Port_RAM (DP_Major_Time_d10d1, 0x65);
    // Set_Dual_Port_RAM (DP_Major_Time_h10h1, 0x23);
    // Set_Dual_Port_RAM (DP_Major_Time_m10m1, 0x59);
    // Set_Dual_Port_RAM (DP_Major_Time_s10s1, 0x50);
    // Set_Dual_Port_RAM (DP_Command, Command_Set_Major); /* Set 365 23:59:50 */
    return 0;
}

static void doCallbacklist(struct list_head* list) {
    struct list_head *ptr;
    struct irigCallback *cbentry;

    for (ptr = list->next; ptr != list; ptr = ptr->next) {
	cbentry = list_entry(ptr,struct irigCallback, list);
	cbentry->callback(cbentry->privateData);
    }
}

static void *pc104sg_100hz_thread (void *param)
{
    /* semaphore timeout in nanoseconds */
    unsigned long nsec_deltat = NSECS_PER_SEC / 100;
    struct timespec tspec;
    int timeouts = 0;

    nsec_deltat += (nsec_deltat * 3) / 8;	/* add 3/8ths more */
    rtl_printf("nsec_deltat = %d\n",nsec_deltat);

    clock_gettime(CLOCK_REALTIME,&tspec);
    // long timeout on first semaphore
    tspec.tv_sec += 1;

    for (;;) {

	/* wait for the pc104sg_100hz_isr to signal us */
	// timespec_add_ns(&tspec, nsec_deltat);
	tspec.tv_nsec += nsec_deltat;
	if (tspec.tv_nsec >= NSECS_PER_SEC) {
	    tspec.tv_sec++;
	    tspec.tv_nsec -= NSECS_PER_SEC;
	}

	/* If we get a timeout on the semaphore, then we've missed
	 * an irig interrupt.  Report the status and re-enable.
	 */
	if (sem_timedwait(&threadsem,&tspec) < 0 &&
		rtl_errno == RTL_ETIMEDOUT) {
	    unsigned char status = inb(isa_address+Status_Port) & Heartbeat;
	    if (!(timeouts++ % 10))
		rtl_printf("semaphore timeout #%d heartbeat=0x%x\n",
			timeouts, status);
	    enableHeartBeatInt();
	}
	clock_gettime(CLOCK_REALTIME,&tspec);


	/* this macro creates a block, which is terminated by
	 * pthread_cleanup_pop */
	pthread_cleanup_push((void(*)(void*))rtl_pthread_mutex_unlock,
		(void*)&cblistmutex);

	pthread_mutex_lock(&cblistmutex);
    
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

cleanup_pop:
	pthread_cleanup_pop(1);
    }
}

void debugCallback(void* privateData) {
    rtl_printf("1Hz beat...\n");
    struct irigTime major;
    getExtTimeFields(&major);
    rtl_printf("year=%d,day=%d,hour=%d,min=%d,sec=%d,msec=%d,usec=%d,nsec=%d\n",
      major.year, major.day, major.hour, major.min, major.sec,
      major.msec, major.usec, major.nsec);
    getTimeFields(&major);
    rtl_printf("year=%d,day=%d,hour=%d,min=%d,sec=%d,msec=%d,usec=%d,nsec=%d\n",
      major.year, major.day, major.hour, major.min, major.sec,
      major.msec, major.usec, major.nsec);
}
/* -- Linux ----------------------------------------------------------- */
unsigned int pc104sg_100hz_isr (unsigned int irq, void* callbackPtr, struct rtl_frame *regs)
{
  // rtl_printf("pc104sg_100hz_isr\n");

  /*
   * Do any interrupt context handling that must be done right away.
   * This code is not running in a Linux thread but in an
   * "interrupt context".
   */
  /* TODO - sample pulse counters here at the highest rate... */

  if (!(inb(isa_address+Status_Port) & Heartbeat)) {
      rtl_printf("no heartbeat\n");
  }
  else {
      /*
       * Wake up the thread so it can handle any processing for this irq
       * that can be done in a Linux thread and doesn't need to be done in
       * an "interrupt context".
       */

      /*
       * This little double clock ensures that msecClock[readClock]
       * is valid for at least a millisecond after reading the value of
       * readClock, even if this code is pre-emptive.
       *
       * This interrupt occurs a 1000hz.  If somehow a bogged down
       * piece of code read the value of readClock, and then
       * didn't get around to reading msecClock[readClock] until
       * more than a millisecond later then it could read a
       * half-written value, but that ain't gunna happen.
       */

#if INTERRUPT_RATE == 1000
      if (++msecClockTicker == MSEC_IN_DAY) msecClockTicker = 0;
#else
      msecClockTicker += 1000 / INTERRUPT_RATE;
      if (msecClockTicker >= MSEC_IN_DAY) msecClockTicker = 0;
#endif

      msecClock[writeClock] = msecClockTicker;
      unsigned char c = readClock;
      /* prior to this line msecClock[readClock=0] is  OK to read */
      readClock = writeClock;
      /* now msecClock[readClock=1] is still OK to read. We're assuming
       * that the byte write of readClock = writeClock is atomic.
       */
      writeClock = c;

      if (!(msecClockTicker % 10)) {
#if INTERRUPT_RATE >= 100
	  if (++hz100_cnt == 100) hz100_cnt = 0;
#else
	  hz100_cnt += 100 / INTERRUPT_RATE;
	  if (hz100_cnt == 100) hz100_cnt = 0;
#endif
	  sem_post( &threadsem );
      }
      /* re-enable interrupt */
      enableHeartBeatInt();
  }
  return 0;
}

/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
    /* stop generating pc104sg interrupts */
    disableHeartBeatInt();

    rtl_free_isa_irq(irq);

    /* cancel the thread */
    if (pc104sgThread) {
        pthread_cancel( pc104sgThread );
	pthread_join( pc104sgThread, NULL );
    }

    /* free up our pool of callbacks */
    free_callbacks();

    /* free up the I/O region and remove /proc entry */
    if (isa_address) {
	/* stop generating pc104sg interrupts */
	disableHeartBeatInt();
    	release_region(isa_address, PC104SG_IOPORT_WIDTH);
    }

    sem_destroy(&threadsem);

    rtl_printf("(%s) %s:\t done\n\n", __FILE__, __FUNCTION__);

}

/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
    int i;
    int errval = 0;
    unsigned int addr;

    rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
	     __FILE__, __FUNCTION__, __DATE__, __TIME__);

    INIT_LIST_HEAD(&callbackpool);
    for (i = 0; i < IRIG_NUM_RATES; i++)
	INIT_LIST_HEAD(callbacklists+i);

    /* initialize the semaphore to the 100 hz thread */
    sem_init (&threadsem, /* which semaphore */
	    1,                /* usable between processes? Usu. 1 */
	    0);               /* initial value */


    /* check for module parameters */
    addr = (unsigned int)ioport + SYSTEM_ISA_IOPORT_BASE;

    /* Grab the region so that no one else tries to probe our ioports. */
    if ((errval = check_region(addr, PC104SG_IOPORT_WIDTH)) < 0) goto err0;
    request_region(addr, PC104SG_IOPORT_WIDTH, "pc104sg");
    isa_address = addr;

    readClock = 0;
    writeClock = 0;
    msecClock[readClock] = 0;
    msecClock[writeClock] = 0;

    /* stop generating pc104sg interrupts */
    disableHeartBeatInt();

    struct irigTime major;
    major.year = 2004;
    major.day = 1;
    major.hour = 0;
    major.min = 0;
    major.sec = 0;
    major.msec = 0;
    major.usec = 0;
    major.nsec = 0;

    setMajorTime(&major);

    /* create our pool of callback entries */
    errval = -ENOMEM;
    for (i = 0; i < CALL_BACK_POOL_SIZE; i++) {
	struct irigCallback* cbentry =
	    (struct irigCallback*) kmalloc(sizeof(struct irigCallback), GFP_KERNEL);
	if (!cbentry) goto err0;
	list_add(&cbentry->list,&callbackpool);
    }

    /* install a handler for the interrupt */
    rtl_printf("(%s) %s:\t rtl_request_isa_irq( %d, pc104sg_100hz_isr )\n",
	     __FILE__, __FUNCTION__, irq);

    /* create the 100 Hz thread */
    if ((errval = pthread_create(
    	&pc104sgThread, NULL, pc104sg_100hz_thread, (void *)0))) {
        errval = -errval;
	goto err0;
    }

    if ((errval = rtl_request_isa_irq(irq, pc104sg_100hz_isr,0 )) < 0 ) {
	/* failed... */
	rtl_printf("(%s) %s:\t could not allocate IRQ %d\n",
		   __FILE__, __FUNCTION__, irq);
	goto err0;
    }

    /* activate the pc104sg-B board */
    setHeartBeatOutput(INTERRUPT_RATE);
    rtl_printf("(%s) %s:\t setHeartBeatOutput(%d) done\n", __FILE__, __FUNCTION__, m_rate[0]);
    // setRate2Output(m_rate[1]);
    // rtl_printf("(%s) %s:\t setRate2Output(%d) done\n",     __FILE__, __FUNCTION__, m_rate[1]);
    enableHeartBeatInt();
    rtl_printf("(%s) %s:\t enableHeartBeatInt  done\n",    __FILE__, __FUNCTION__);

    /*
    if (register_irig_callback(debugCallback, IRIG_1_HZ,0)) {
	rtl_printf("register_irig_callback failed\n");
	return err;
    }
    */

    return 0;

err0:

    /* cancel the thread */
    if (pc104sgThread) {
        pthread_cancel( pc104sgThread );
	pthread_join( pc104sgThread, NULL );
    }

    /* free up our pool of callbacks */
    free_callbacks();

    /* free up the I/O region and remove /proc entry */
    if (isa_address) {
	/* stop generating pc104sg interrupts */
	disableHeartBeatInt();
    	release_region(isa_address, PC104SG_IOPORT_WIDTH);
    }

    sem_destroy(&threadsem);
    return errval;
}

