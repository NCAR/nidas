/* irig.c

   Time-stamp: <Thu 26-Aug-2004 06:48:04 pm>

   RTLinux IRIG-b driver for the ISA bus based jxi2 pc104-SG card.

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

*/

#include <Pc104sg.h>
#include <main.h>

#define ISA_BASE 0xf70002a0
#define ioWidth  0x20

/* RTLinux module includes...  */
#include <rtl.h>
#include <rtl_posixio.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>

#include <com8.h>

#define __ISA_DEMUX__ // skip these, we are using demux.cc implementation...

/* these are file pointers to the FIFOs to the user application */
static int fp_irig;

extern int ptog;  // this varible is toggled every second

/* TODO - hz100_cnt needs to be initialized to the zero'th second read from
   the IRIG card...  the current implementation arbitrarily sets it to zero
   at the first occurance of this ISR. */
static int hz100_cnt = 1;

/* set the address to default values */
volatile unsigned int isa_address = ISA_BASE;

/* these pointers are set in init_module... */
volatile unsigned int reg_data_ptr;
volatile unsigned int reg_address_ptr;
volatile unsigned int reg_status_ptr;
volatile unsigned int reg_ex_status_ptr;
volatile unsigned int usec_reg_ptr;
volatile unsigned int us1us0_reg_ptr;
volatile unsigned int ms1ms0_reg_ptr;
volatile unsigned int sec0ms2_reg_ptr;
volatile unsigned int min0sec1_reg_ptr;
volatile unsigned int hour0min1_reg_ptr;
volatile unsigned int day0hour1_reg_ptr;
volatile unsigned int day2day1_reg_ptr;

/* module prameters (can be passed in via command line) */
static unsigned int m_addr = 0;
static unsigned int m_rate[2] = {100, 50000};
MODULE_PARM(m_addr, "1l");
MODULE_PARM(m_rate, "1-2l");

RTLINUX_MODULE(irig);
MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("RTLinux ISA IRIG-b pc104-SG jxi2 Driver");

/* -- Utility --------------------------------------------------------- */
void enableHeartBeatInt (void)
{
/* bits 0-4 are write only
 * to reset heartbeat flag... 0x2F
 */
  /* clear interrupt */
  inb(reg_status_ptr);
  outb(0xEF, reg_status_ptr);

  /* enable interrupt */
  inb(reg_status_ptr);
  outb(0x2F, reg_status_ptr);
}

/* -- Utility --------------------------------------------------------- */
void disableHeartBeatInt (void)
{
/* bits 0-4 are write only
 * to reset heartbeat flag... 0x2F
 */
  /* clear interrupt */
  inb(reg_status_ptr);
  outb(0xEF, reg_status_ptr);
}

/* -- Utility --------------------------------------------------------- */
static int Read_Dual_Port_RAM (int addr)
{
  int i;

  /* clear the respose flag */
  inb(reg_data_ptr);

  /* specify dual port address */
  outb(addr, reg_address_ptr);

  /* wait for PC104 to acknowledge */
  for (i=0; (i<1000) && !(inb(reg_ex_status_ptr) & Response_Ready); i++)
    udelay(1);

  /* check for a time out on the response... */
  if (i>=1000) {
    rtl_printf("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
    return -1;
  }

  /* return read DP_Control value */
  return inb(reg_data_ptr);
}

/* -- Utility --------------------------------------------------------- */
static int Set_Dual_Port_RAM (int addr, int value)
{
  int i;

  /* clear the respose flag */
  inb(reg_data_ptr);

  /* specify dual port address */
  outb(addr, reg_address_ptr);

  /* wait for PC104 to acknowledge */
  for (i=0; (i<1000) && !(inb(reg_ex_status_ptr) & Response_Ready); i++)
    udelay(1);

  /* check for a time out on the response... */
  if (i>=1000) {
    rtl_printf("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
    return -1;
  }

  /* clear the respose flag */
  inb(reg_data_ptr);

  /* write new value to DP RAM */
  outb(value, reg_data_ptr);

  /* wait for PC104 to acknowledge */
  for (i=0; (i<1000) && !(inb(reg_ex_status_ptr) & Response_Ready); i++)
    udelay(1);

  /* check for a time out on the response... */
  if (i>=1000) {
    rtl_printf("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
    return -1;
  }

  /* check that the written value matches */
  if (inb(reg_data_ptr) != value)
    return -1;

  /* success */
  return 1;
}

/* -- Utility --------------------------------------------------------- */
void setHeartBeatOutput (int rate)
{
  int divide;
  char lsb, msb;

  divide = 3000000 / rate;
  lsb = (char)(divide & 0xff);
  msb = (char)((divide & 0xff00)>>8);
  Set_Dual_Port_RAM (DP_Ctr1_ctl, 0x76);
  Set_Dual_Port_RAM (DP_Ctr1_msb, msb);
  Set_Dual_Port_RAM (DP_Ctr1_lsb, lsb);
  Set_Dual_Port_RAM (DP_Command, Command_Rejam);
}

/* -- Utility --------------------------------------------------------- */
void setRate2Output (int rate)
{
  int divide;
  char lsb, msb;

  divide = 5000000 / rate;
  lsb = (char)(divide & 0xff);
  msb = (char)((divide & 0xff00)>>8);
  Set_Dual_Port_RAM (DP_Ctr2_ctl, 0xF6);
  Set_Dual_Port_RAM (DP_Ctr2_msb, msb);
  Set_Dual_Port_RAM (DP_Ctr2_lsb, lsb);
  Set_Dual_Port_RAM (DP_Command, Command_Rejam);
}


/* -- Utility --------------------------------------------------------- */
#if 0 // TODO - determine for to syncronize to the zero'th second...
void syncUP()
{
  us0       = inb(usec_reg_ptr);      //f   /* LATCH ALL DIGITS NOW */
  sec0ms2   = inb(sec0ms2_reg_ptr);   //c   
  min0sec1  = inb(min0sec1_reg_ptr);  //b
  secs =     (min0sec1 & 0x0f) * 10 + sec0ms2   / 16;
}
#endif
/* -- POSIX ----------------------------------------------------------- */
ssize_t readTime(struct rtl_file *filp, char *buf, size_t count, off_t *pos)
{
  /* Reads the time from the interface. */
  unsigned int us0,us1us0,ms1ms0,sec0ms2,min0sec1,hour0min1,day0hour1,day2day1;
  unsigned int days,hours,minutes,secs,msec,usec,nsec;

  us0       = inb(usec_reg_ptr);      //f   /* LATCH ALL DIGITS NOW */
  us1us0    = inb(us1us0_reg_ptr);    //e
  ms1ms0    = inb(ms1ms0_reg_ptr);    //d
  sec0ms2   = inb(sec0ms2_reg_ptr);   //c   
  min0sec1  = inb(min0sec1_reg_ptr);  //b
  hour0min1 = inb(hour0min1_reg_ptr); //a
  day0hour1 = inb(day0hour1_reg_ptr); //9
  day2day1  = inb(day2day1_reg_ptr);  //8

  days = (((day2day1 & 0xf0) / 16) * 100) +
         ( (day2day1 & 0x0f) * 10) +
         ((day0hour1 & 0xf0) / 16);

  hours = (day0hour1 & 0x0f) * 10 + (hour0min1) / 16;

  minutes = (hour0min1 & 0x0f) * 10 + min0sec1  / 16;

  secs =     (min0sec1 & 0x0f) * 10 + sec0ms2   / 16;

  msec = ((sec0ms2 & 0x0f) * 100) + ((ms1ms0 / 16) * 10) + (ms1ms0 & 0x0f);

  usec = (((us1us0 & 0xf0) / 16) * 100) +
         ( (us1us0 & 0x0f) * 10) +
         ((us0     & 0xf0) / 16);

  nsec = (us0 & 0x0f);

  //  8     9     a     b     c    d    e    f
  // 36 - 5 2 - 3 5 - 9 5 - 1 1 - 46 - 60 - 10
  // 36   5 2   3:5   9:5   1.1   46   60   10
  //                          m   mm   uu   un
  sprintf(buf, "%03d %02d:%02d:%02d.%03d%03d%d",
          days, hours, minutes, secs, msec, usec, nsec);

  // measured values will always be positive
  static int last = -1;
  static int spike = -1;
  int now, abs;

  if (count==1)
  {
    // get the current time's micro second reading
    now = (msec*1000 + usec);  // (111111 us)

    // one time "run-time" initialization
    if (last == -1)
    {
      rtl_printf("%s | initial value is: %06d\n", buf, now);
      last = now;
    }
    rtl_printf("%s", buf);

    // measure the greatest spike ammount
    abs = (now-last);
    if (abs<0)
      abs *= -1;

    if (spike < abs)
    {
      spike = abs;
      rtl_printf(" |%d-%d|=%d spike: %06d", now, last, abs, spike);
    }
    rtl_printf("\n");
  }
  //      xx xx xx xx xx xx xx xx xx
  //      366 01:30:24.000
/*   rtl_printf("%02x %02x %02x %02x %02x %02x %02x %02x\n" */
/* 	     "%03d %02d:%02d:%02d.%03d%03d%d\n", */
/* 	     day2day1, day0hour1, hour0min1, min0sec1, sec0ms2, ms1ms0, us1us0, us0, */
/* 	     days, hours, minutes, secs, msec, usec, nsec); */

  return strlen(buf);
}

/* -- POSIX ----------------------------------------------------------- */
ssize_t setMajorTime(struct rtl_file *filp, const char *buf, size_t count, off_t *pos)
{
  /* TODO - scanf the buf string and parse it out into the following
   * values:
   * int new_year, int new_month, int new_day,
   * int new_hour, int new_minute, int new_second
   *
   * TODO - import calendar conversion tools from Pc104sg.cc file...
   */
  
/*   /\* compute julian day *\/ */
/*   int *cal_ptr; */

/*   static int calendar[PC104SG_CALENDAR_SIZE] = */
/*          {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365}; */
/*   static int leap_calendar[PC104SG_CALENDAR_SIZE] = */
/*          {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}; */

/*   if (sg_year % PC104SG_LEAP_YEAR_MOD) { */
/*     cal_ptr = calendar; */
/*   } */
/*   else { */
/*     cal_ptr = leap_calendar; */
/*   } */
/*   sg_julian_day = *(cal_ptr + sg_month - 1) + sg_day; */

/*   Set_Dual_Port_RAM (DP_Major_Time_d100d10, sg_julian_day / 100); */
/*   Set_Dual_Port_RAM (DP_Major_Time_d10d1,sg_julian_day % 100); */
/*   Set_Dual_Port_RAM (DP_Major_Time_h10h1,new_hour); */
/*   Set_Dual_Port_RAM (DP_Major_Time_m10m1,new_minute); */
/*   Set_Dual_Port_RAM (DP_Major_Time_s10s1,new_second); */
/*   Set_Dual_Port_RAM (DP_Command,Command_Set_Major); */

  Set_Dual_Port_RAM (DP_Major_Time_d100d10, 0x03);
  Set_Dual_Port_RAM (DP_Major_Time_d10d1, 0x65);
  Set_Dual_Port_RAM (DP_Major_Time_h10h1, 0x23);
  Set_Dual_Port_RAM (DP_Major_Time_m10m1, 0x59);
  Set_Dual_Port_RAM (DP_Major_Time_s10s1, 0x50);
  Set_Dual_Port_RAM (DP_Command, Command_Set_Major); /* Set 365 23:59:50 */

  return 0;
}

/* -- POSIX ----------------------------------------------------------- */
/* These POSIX file access functions provide access
 * to this module from the user space. */
struct rtl_file_operations irig_fops = {
  read:         readTime,
  write:        setMajorTime
};

/* -- Linux ----------------------------------------------------------- */
/* The 100 hz thread */
pthread_t     aThread;
sem_t         anIrigSemaphore;

extern int module_loading;
extern int nSerialCfg;
extern struct serialTable serialCfg[];

static void *irig_100hz_thread (void *t)
{
  int timeStrLen;
  char timeStr[40];

  while ( 1 )
  {
    /* wait for the irig_100hz_isr to signal us */
    sem_wait (&anIrigSemaphore);

    /* TODO - traverse though an array of function pointers
     * that is set up from the parsed header build file.
     */
    /* transmit serial data requests at their configured rates... */
/*     int ii; */
/*     for (ii=0; ii<nSerialCfg; ii++) */
/*       if (!(hz100_cnt % (100/serialCfg[ii].rate))) */
/* 	write(serialCfg[ii].fptr, serialCfg[ii].cmd, serialCfg[ii].len); */

    /* perform 100Hz processing... */

    /* perform 50Hz processing... */
    if (!(hz100_cnt %   2))
    {
    }
    /* perform 25Hz processing... */
    if (!(hz100_cnt %   4))
    {
    }
    /* perform 10Hz processing... */
    if (!(hz100_cnt %  10))
    {
    }
    /* perform  5Hz processing... */
    if (!(hz100_cnt %  20))
    {
    }
    /* perform  1Hz processing... */
    if (!(hz100_cnt % 100))
    {
      rtl_printf("IRIG: serialCfg[0].fptr = 0x%lx\n", serialCfg[0].fptr);
      write(serialCfg[0].fptr, "hello\r\n", 8); //debug

/*       /\* TEST - print the current timestamp *\/ */
/*       timeStrLen = readTime(NULL, timeStr, 1, NULL); */

/*       /\* toggle the user-space application's second buffer selection. *\/ */
/*       write(fp_irig, &ptog, sizeof(int)); */
/*       if (ptog) ptog = 0; */
/*       else      ptog = 1; */
    }
  }
}

/* -- Linux ----------------------------------------------------------- */
unsigned int irig_100hz_isr (unsigned int irq, struct rtl_frame *regs)
{
  /* pend this IRQ so that the general purpose OS gets it, too */
  /* This needed for ISA multiplexing. */
  rtl_global_pend_irq(irq);

  /*
   * Do any interrupt context handling that must be done right away.
   * This code is not running in a Linux thread but in an
   * "interrupt context".
   */
  /* TODO - sample pulse counters here at the highest rate... */

  /*
   * Wake up the thread so it can handle any processing for this irq
   * that can be done in a Linux thread and doesn't need to be done in
   * an "interrupt context".
   */
  sem_post( &anIrigSemaphore );
  if (++hz100_cnt > 100)
    hz100_cnt = 1;

  /* re-enable interrupt */
  enableHeartBeatInt();
  return 0;
}

/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  /* stop generating IRIG interrupts */
  disableHeartBeatInt();

  /* cancel the thread */
  pthread_cancel( aThread );
  pthread_join( aThread, NULL );

  /* free up the I/O region and remove /proc entry */
  release_region(isa_address, ioWidth);
#ifndef __ISA_DEMUX__ // skip this, we are using demux.cc implementation...
  rtl_free_irq(VIPER_CPLD_IRQ);
#endif

  rtl_unregister_dev("/dev/irig");
  rtl_printf("(%s) %s:\t unregistered /dev/irig\n", __FILE__, __FUNCTION__);
  rtl_printf("(%s) %s:\t done\n\n", __FILE__, __FUNCTION__);
}

/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
	     __FILE__, __FUNCTION__, __DATE__, __TIME__);

  /* check for module parameters */
  if (m_addr) isa_address = (unsigned int)m_addr + 0xf7000000;
  rtl_printf("(%s) %s:\t addr = %x width = %x\n", __FILE__, __FUNCTION__,
	isa_address, ioWidth);

  /* initialize the semaphore to the 100 hz thread */
  sem_init (&anIrigSemaphore, /* which semaphore */
            1,                /* usable between processes? Usu. 1 */
            0);               /* initial value */
  rtl_printf("(%s) %s:\t sem_init done\n", __FILE__, __FUNCTION__);

  /* create the 100 Hz thread */
  pthread_create( &aThread, NULL, irig_100hz_thread, (void *)0 );
  rtl_printf("(%s) %s:\t pthread_create done\n", __FILE__, __FUNCTION__);

  /* Grab the region so that no one else tries to probe our ioports. */
  request_region(isa_address, ioWidth, "IRIG-b");
  rtl_printf("(%s) %s:\t request_region done\n", __FILE__, __FUNCTION__);

  /* set the pointers */
  reg_data_ptr =      isa_address+Dual_Port_Data_Port;
  reg_address_ptr =   isa_address+Dual_Port_Address_Port;
  reg_status_ptr =    isa_address+Status_Port;
  reg_ex_status_ptr = isa_address+Extended_Status_Port;
  usec_reg_ptr =      isa_address+Usec1_Nsec100_Port;
  us1us0_reg_ptr =    isa_address+Usec100_Usec10_Port;
  ms1ms0_reg_ptr =    isa_address+Msec10_Msec1_Port;
  sec0ms2_reg_ptr =   isa_address+Sec1_Msec100_Port;
  min0sec1_reg_ptr =  isa_address+Min1_Sec10_Port;
  hour0min1_reg_ptr = isa_address+Hr1_Min10_Port;
  day0hour1_reg_ptr = isa_address+Day1_Hr10_Port;
  day2day1_reg_ptr =  isa_address+Day100_Day10_Port;

#if 0 // TODO?
  /* Set hardware interrupt handler to trigger on the falling edge? */
  rtl_printf("(%s) %s:\t set_GPIO_IRQ_edge(%d,%d)\n",
	     __FILE__, __FUNCTION__, IRQ_TO_GPIO(VIPER_CPLD_IRQ), GPIO_FALLING_EDGE);
  set_GPIO_IRQ_edge(IRQ_TO_GPIO(VIPER_CPLD_IRQ), GPIO_FALLING_EDGE);
#endif

  /* stop generating IRIG interrupts */
  rtl_printf("(%s) %s:\t disableHeartBeatInt()\n", __FILE__, __FUNCTION__);
  disableHeartBeatInt();

  /* DEBUG test - see if we are talking to the IRIG card... */
  setMajorTime(NULL, "asdf",0, NULL);
  int iii, jjj, timeStrLen;
  char timeStr[40];
  for (iii=0; iii<3; iii++)
  {
    timeStrLen = readTime(NULL, timeStr, 0, NULL);
    rtl_printf("(%03d) %s\n", timeStrLen, timeStr);
    for (jjj=0; jjj<10000000; jjj++);
  }
  /* END DEBUG test. */

  /* install a handler for the interrupt */
  rtl_printf("(%s) %s:\t request_irq( %d, irig_100hz_isr )\n",
	     __FILE__, __FUNCTION__, VIPER_CPLD_IRQ);

#ifndef __ISA_DEMUX__ // skip this, we are using demux.cc implementation...
  if ( rtl_request_irq( VIPER_CPLD_IRQ, irig_100hz_isr ) < 0 )
  {
    /* failed... */
    cleanup_module();
    rtl_printf("(%s) %s:\t could not allocate IRQ at #%d\n",
	       __FILE__, __FUNCTION__, VIPER_CPLD_IRQ);
    return -EIO;
  }

  /* activate the IRIG-B board */
  setHeartBeatOutput(m_rate[0]);
  rtl_printf("(%s) %s:\t setHeartBeatOutput(%d) done\n", __FILE__, __FUNCTION__, m_rate[0]);
  setRate2Output(m_rate[1]);
  rtl_printf("(%s) %s:\t setRate2Output(%d) done\n",     __FILE__, __FUNCTION__, m_rate[1]);
  enableHeartBeatInt();
  rtl_printf("(%s) %s:\t enableHeartBeatInt  done\n",    __FILE__, __FUNCTION__);
#endif

  /* create /dev/irig device file for POSIX file access */
  if ( rtl_register_dev("/dev/irig", &irig_fops, 0) )
  {
    rtl_printf("(%s) %s:\t could not register device.\n", __FILE__, __FUNCTION__);
    return -EIO;
  }
  rtl_printf("(%s) %s:\t registered /dev/irig\n", __FILE__, __FUNCTION__);

#if 0 //TODO?
#error "not coded yet..."
  /* create /proc/irig file for shell diagnostics */
  misc_register (&irig_miscdev);
  create_proc_read_entry ("/dev/irig", 0, 0, rtc_read_proc, NULL);
#endif

  rtl_printf("(%s) %s:\t loaded\n", __FILE__, __FUNCTION__);
  module_loading = 0;
  return 0;
}
