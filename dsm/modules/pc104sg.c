/* pc104sg.c

   Time-stamp: <Thu 26-Aug-2004 06:48:04 pm>

   RTLinux pc104sg driver for the ISA bus based jxi2 pc104-SG card.

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

*/

/* RTLinux module includes...  */
#include <rtl.h>

#include <rtl_posixio.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/list.h>

#include <pc104sg.h>
#include <rtl_isa_irq.h>

#include <irigclock.h>



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
static unsigned int m_rate[2] = {1000, 50000};

MODULE_PARM(irq, "i");
MODULE_PARM(ioport, "i");
MODULE_PARM(m_rate, "1-2i");

RTLINUX_MODULE(pc104sg);
MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("RTLinux ISA pc104-SG jxi2 Driver");

#define ISA_IOPORT_OFFSET 0xf7000000

/* Actual physical address of this card. Set in init_module */
static unsigned int isa_address;

/* The 100 hz thread */
pthread_t     pc104sgThread;
sem_t         anIrigSemaphore;	/* change this to a condition variable */


struct irigCallback {
  irig_callback_t* callback;
  void* privateData;
  struct list_head list;
};

static struct list_head callbacklists[IRIG_NUM_RATES];

static pthread_mutex_t cblistmutex = PTHREAD_MUTEX_INITIALIZER;

void register_irig_callback(irig_callback_t* callback, enum irigClockRates rate,
	void* privateData)
{

    struct irigCallback* cb = 
	(struct irigCallback*) kmalloc(sizeof(struct irigCallback), GFP_KERNEL);
    cb->callback = callback;
    cb->privateData = privateData;

    pthread_mutex_lock(&cblistmutex);
    list_add(&cb->list,callbacklists+(int)rate);
    pthread_mutex_unlock(&cblistmutex);
}

void unregister_irig_callback(irig_callback_t* callback, enum irigClockRates rate)
{
    pthread_mutex_lock(&cblistmutex);

    struct list_head *ptr;
    struct irigCallback *entry;
    for (ptr = callbacklists[rate].next; ptr != callbacklists+rate;
    	ptr = ptr->next) {
	entry = list_entry(ptr,struct irigCallback, list);
	if (entry->callback == callback) {
	    list_del(&entry->list);
	    kfree(entry);
	    break;
	}
    }

    pthread_mutex_unlock(&cblistmutex);
}

static void release_callbacks()
{
    int i;

    struct list_head *ptr;
    struct list_head *newptr;
    struct irigCallback *entry;

    pthread_mutex_lock(&cblistmutex);

    for (i = 0; i < IRIG_NUM_RATES; i++) {
	for (ptr = callbacklists[i].next;
		ptr != callbacklists+i; ptr = newptr) {
	    newptr = ptr->next;
	    entry = list_entry(ptr,struct irigCallback, list);
	    list_del(&entry->list);
	    kfree(entry);
	}
    }

    pthread_mutex_unlock(&cblistmutex);
}

/* -- Utility --------------------------------------------------------- */
void enableHeartBeatInt (void)
{
    /* writing 0 to bits 0-4 causes resets or simulated external events */

    /* enable heart beat interrupt (disables others) */
    outb(Heartbeat_Int_Enb + Reset_Heartbeat,
  	isa_address+Status_Port);
    // rtl_printf("enableHeartBeatInt, status=0x%x\n",
    // 	inb(isa_address+Status_Port));
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
    rtl_printf("wait1 jwait=%d,jiffies=%d status=%x\n",jwait,jiffies,status);
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
    struct irigCallback *entry;
    for (ptr = list->next; ptr != list; ptr = ptr->next) {
	entry = list_entry(ptr,struct irigCallback, list);
	entry->callback(entry->privateData);
    }
}

static void *pc104sg_100hz_thread (void *t)
{
    pthread_mutex_lock(&cblistmutex);

    for (;;) {
	pthread_mutex_unlock(&cblistmutex);

	/* wait for the pc104sg_100hz_isr to signal us */
	sem_wait (&anIrigSemaphore);

	pthread_mutex_lock(&cblistmutex);

	/* perform 100Hz processing... */
	doCallbacklist(callbacklists + IRIG_100_HZ);

	if ((hz100_cnt %   2)) continue;

	/* perform 50Hz processing... */
	doCallbacklist(callbacklists + IRIG_50_HZ);

	if ((hz100_cnt %   4)) continue;

	/* perform 25Hz processing... */
	doCallbacklist(callbacklists + IRIG_25_HZ);

	if ((hz100_cnt %  10)) continue;

	/* perform 10Hz processing... */
	doCallbacklist(callbacklists + IRIG_10_HZ);

	if ((hz100_cnt %  20)) continue;

	/* perform  5Hz processing... */
	doCallbacklist(callbacklists + IRIG_5_HZ);

	if ((hz100_cnt % 100)) continue;

	/* perform  1Hz processing... */
	doCallbacklist(callbacklists + IRIG_1_HZ);

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
unsigned int pc104sg_100hz_isr (unsigned int irq, struct rtl_frame *regs)
{
  // rtl_printf("pc104sg_100hz_isr\n");

  /*
   * Do any interrupt context handling that must be done right away.
   * This code is not running in a Linux thread but in an
   * "interrupt context".
   */
  /* TODO - sample pulse counters here at the highest rate... */

  if (!inb(isa_address+Status_Port) & Heartbeat) {
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

      if (++msecClockTicker == MSEC_IN_DAY) msecClockTicker = 0;

      msecClock[writeClock] = msecClockTicker;
      unsigned char c = readClock;
      /* prior to this line msecClock[readClock=0] is  OK to read */
      readClock = writeClock;
      /* now msecClock[readClock=1] is still OK to read. We're assuming
       * that the byte write of readClock = writeClock is atomic.
       */
      writeClock = c;

      if (!(msecClockTicker % 10)) {
	  if (++hz100_cnt == 100)
	    hz100_cnt = 0;
	  sem_post( &anIrigSemaphore );
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
  pthread_cancel( pc104sgThread );
  pthread_join( pc104sgThread, NULL );

  release_callbacks();

  /* free up the I/O region and remove /proc entry */
  release_region(isa_address, PC104SG_IOPORT_WIDTH);

  rtl_printf("(%s) %s:\t done\n\n", __FILE__, __FUNCTION__);
}

/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
    int i;
    int err;
    rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
	     __FILE__, __FUNCTION__, __DATE__, __TIME__);

    /* check for module parameters */
    isa_address = (unsigned int)ioport + ISA_IOPORT_OFFSET;
    rtl_printf("(%s) %s:\t addr = %x width = %x\n", __FILE__, __FUNCTION__,
	isa_address, PC104SG_IOPORT_WIDTH);

    /* Grab the region so that no one else tries to probe our ioports. */
    if ((err = check_region(isa_address, PC104SG_IOPORT_WIDTH)) < 0) return err;
    rtl_printf("(%s) %s: calling request_region \n", __FILE__, __FUNCTION__);
    request_region(isa_address, PC104SG_IOPORT_WIDTH, "pc104sg");
    rtl_printf("(%s) %s:\t request_region done\n", __FILE__, __FUNCTION__);


    /* initialize the semaphore to the 100 hz thread */
    sem_init (&anIrigSemaphore, /* which semaphore */
	    1,                /* usable between processes? Usu. 1 */
	    0);               /* initial value */

    rtl_printf("(%s) %s:\t sem_init done\n", __FILE__, __FUNCTION__);

    /* create the 100 Hz thread */
    pthread_create( &pc104sgThread, NULL, pc104sg_100hz_thread, (void *)0 );
    rtl_printf("(%s) %s:\t pthread_create done\n", __FILE__, __FUNCTION__);

    /* stop generating pc104sg interrupts */
    rtl_printf("(%s) %s:\t disableHeartBeatInt()\n", __FILE__, __FUNCTION__);
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


    /* install a handler for the interrupt */
    rtl_printf("(%s) %s:\t rtl_request_isa_irq( %d, pc104sg_100hz_isr )\n",
	     __FILE__, __FUNCTION__, irq);

    if ( rtl_request_isa_irq(irq, pc104sg_100hz_isr ) < 0 )
    {
    /* failed... */
    cleanup_module();
    rtl_printf("(%s) %s:\t could not allocate IRQ %d\n",
	       __FILE__, __FUNCTION__, irq);
    return -EIO;
    }

    /* activate the pc104sg-B board */
    setHeartBeatOutput(1000);
    rtl_printf("(%s) %s:\t setHeartBeatOutput(%d) done\n", __FILE__, __FUNCTION__, m_rate[0]);
    // setRate2Output(m_rate[1]);
    // rtl_printf("(%s) %s:\t setRate2Output(%d) done\n",     __FILE__, __FUNCTION__, m_rate[1]);
    enableHeartBeatInt();
    rtl_printf("(%s) %s:\t enableHeartBeatInt  done\n",    __FILE__, __FUNCTION__);

    #ifdef BOZO
    #endif
    rtl_printf("(%s) %s:\t loaded\n", __FILE__, __FUNCTION__);

    readClock = 0;
    writeClock = 0;
    msecClock[readClock] = 0;
    msecClock[writeClock] = 0;

    for (i = 0; i < IRIG_NUM_RATES; i++)
	INIT_LIST_HEAD(callbacklists+i);

    register_irig_callback(debugCallback, IRIG_1_HZ,0);
    return 0;
}
