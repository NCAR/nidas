/* isa_irig.c
   Linux/RTLinux module for interfacing the ISA bus interfaced
   jxi2 IRIG-b pc104-SG card.

   Original Author: John Wasinger
   Copyright by the National Center for Atmospheric Research
 
   Revisions:

*/

#include <Pc104sg.h>

#define ISA_BASE 0xf70002a0
#define ioWidth  0x20


#ifdef __RTL__
  /* RTLinux module includes...  */
  #include <rtl.h>
  #include <rtl_posixio.h>
  #include <pthread.h>
  #include <stdio.h>
  #include <unistd.h>
  #include <semaphore.h>
  #include <linux/ioport.h>
  RTLINUX_MODULE(irig);
#else
  /* Linux module includes...  */
  #include <asm/io.h>        // inb, outb
  #include <linux/delay.h>
  #include <linux/fs.h>
  #include <linux/kernel.h>
  #include <linux/module.h>
  #include <linux/string.h>
#endif

#ifdef __RTL__
/* #define __ISA_DEMUX__ */
#define file_operations rtl_file_operations
#define print           rtl_printf
#else
#define print           printk
#endif


/* these are file pointers to the FIFOs to the user application */
#define MAX_UVHYG_INTFC         2       /* max num of UV hygrometers per dsm */

static int fp_irig;
static int fp_pr[MAX_UVHYG_INTFC][2], fp_pw[MAX_UVHYG_INTFC];
static int fp_jr[MAX_UVHYG_INTFC][2], fp_jw[MAX_UVHYG_INTFC];

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

MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
#ifdef __RTL__
MODULE_DESCRIPTION("RTLinux jxi2 IRIG-b pc104-SG Driver Module");
#else
MODULE_DESCRIPTION("Linux jxi2 IRIG-b pc104-SG Driver Module");
#endif

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
static void disableHeartBeatInt (void)
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
    print("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
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
    print("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
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
    print("(%s) %s:\t timed out...\n",    __FILE__, __FUNCTION__);
    return -1;
  }

  /* check that the written value matches */
  if (inb(reg_data_ptr) != value)
    return 10/0;

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


/* -- POSIX ----------------------------------------------------------- */
#ifdef __RTL__
static ssize_t readTime(struct rtl_file *filp, char *buf, size_t count, off_t *pos)
#else
static ssize_t readTime(char *buf, size_t count, off_t *pos)
#endif
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

  // 0.1111111 s * 1000 ms/s =
  // 111.1111 ms * 1000 us/s =
  // 111111.1 us * 1000 ns/s =

  // measured values will always be positive
  static int last = -1;
  static int spike = -1;
  int now, abs;

  if (count==1)
  {
    // get the current time
//  now = (msec*10000 + usec*10 + nsec);  // (111111.1 us) * 10
    now = (msec*1000 + usec);  // (111111 us)

    // one time "run-time" initialization
    if (last == -1)
    {
      print("%s | initial value is: %06d\n", buf, now);
      last = now;
    }
    print("%s", buf);

    // measure the greatest spike ammount
    abs = (now-last);
    if (abs<0)
      abs *= -1;

    if (spike < abs)
    {
      spike = abs;
      print(" |%d-%d|=%d spike: %06d", now, last, abs, spike);
    }
    print("\n");
  }
  //      xx xx xx xx xx xx xx xx xx
  //      366 01:30:24.000
/*   print("%02x %02x %02x %02x %02x %02x %02x %02x\n" */
/*         "%03d %02d:%02d:%02d.%03d%03d%d\n", */
/*         day2day1, day0hour1, hour0min1, min0sec1, sec0ms2, ms1ms0, us1us0, us0, */
/*         days, hours, minutes, secs, msec, usec, nsec); */

  return strlen(buf);
}

/* -- POSIX ----------------------------------------------------------- */
#ifdef __RTL__
static ssize_t setMajorTime(struct rtl_file *filp, const char *buf, size_t count, off_t *pos)
#else
static ssize_t setMajorTime(const char *buf, size_t count, off_t *pos)
#endif
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
static struct file_operations irig_fops = {
  read:         readTime,
  write:        setMajorTime
};

/* -- Linux ----------------------------------------------------------- */
/* The 1 PPS thread */
pthread_t     aThread;
sem_t         anIrigSemaphore;

static void *irig_1PPS_thread (void *t)
{
  int ptog = 0;
  int i;
  int timeStrLen;
  char timeStr[40];

  while ( 1 )
  {
    /* wait for the irig_100hz_isr to signal us */
    sem_wait (&anIrigSemaphore);

    /* TEST - print the current timestamp */
    timeStrLen = readTime(NULL, timeStr, 1, NULL);
/*     print("(%03d) %s\n", timeStrLen, timeStr); */

    /* TODO - traverse though an array of function pointers
     * that is set up from the parsed header build file.
     */
    for (i=0; i<MAX_UVHYG_INTFC; i++)
    {
      write(fp_pr[i][ptog], "5.01 5.02 5.03 5.04 5.05 ", 25);
      write(fp_jr[i][ptog], "0.1 0.2 0.3 0.4 1 -2 3 -4 0.5 ", 30);
      write(fp_jr[i][ptog], "1.1 1.2 1.3 1.4 1 -2 3 -4 0.5 ", 30);
      write(fp_jr[i][ptog], "2.1 2.2 2.3 2.4 1 -2 3 -4 0.5 ", 30);
      write(fp_jr[i][ptog], "3.1 3.2 3.3 3.4 1 -2 3 -4 0.5 ", 30);
      write(fp_jr[i][ptog], "4.1 4.2 4.3 4.4 1 -2 3 -4 0.5 ", 30);
      write(fp_jr[i][ptog], "5.1 5.2 5.3 5.4 1 -2 3 -4 0.5 ", 30);
      write(fp_jr[i][ptog], "6.1 6.2 6.3 6.4 1 -2 3 -4 0.5 ", 30);
      write(fp_jr[i][ptog], "7.1 7.2 7.3 7.4 1 -2 3 -4 0.5 ", 30);
      write(fp_jr[i][ptog], "8.1 8.2 8.3 8.4 1 -2 3 -4 0.5 ", 30);
      write(fp_jr[i][ptog], "9.1 9.2 9.3 9.4 1 -2 3 -4 0.5 ", 30);
    }
    
    /* toggle the user-space application's second buffer selection. */
    write(fp_irig, &ptog, sizeof(int));
    if (ptog) ptog = 0;
    else      ptog = 1;
  }
  return NULL;
}

/* -- Linux ----------------------------------------------------------- */
#ifdef __RTL__
unsigned int irig_100hz_isr (unsigned int irq, struct rtl_frame *regs)
#else
static void irig_100hz_isr (int irq, void *dev_id, struct pt_regs *regs)
#endif
{
  /* TODO - hz100_cnt needs to be initialized to the zero'th second read from
   * the IRIG card...  the current implementation arbitrarily sets it to zero
   * at the first occurance of this ISR.
   */
  static int hz100_cnt = 0;

#ifdef __RTL__
  /* pend this IRQ so that the general purpose OS gets it, too */
  /* This needed for ISA multiplexing. */
  rtl_global_pend_irq(irq);
#endif
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
  if (++hz100_cnt > 100-1)
  {
    sem_post( &anIrigSemaphore );
    hz100_cnt = 0;
  }

  /* re-enable interrupt */
  enableHeartBeatInt();
#ifdef __RTL__
  return 0;
#endif
}

/* -- MODULE ---------------------------------------------------------- */
void cleanup_module(void)
{
  /* stop generating IRIG interrupts */
  disableHeartBeatInt();

  /* cancel the thread */
  pthread_cancel( aThread );
  pthread_join( aThread, NULL );

  /* free up the I/O region and remove /proc entry */
  release_region(isa_address, ioWidth);
#ifdef __RTL__
#ifndef __ISA_DEMUX__ // skip this, we are using isa_demux.cc implementation...
  rtl_free_irq(VIPER_ISA_IRQ10);
#endif
#else
  free_irq(VIPER_ISA_IRQ10, NULL);
#endif

  /* ##################################################################### */
  /* ## begin fake instrument FIFO simulation...                        ## */
  /* ##################################################################### */
  int i, j;
  char devstr[30];
  for (i=0; i<MAX_UVHYG_INTFC; i++)
  {
    for (j=0; j<2; j++)
    {
      close(fp_pr[i][j]);
      close(fp_jr[i][j]);
    }
    close(fp_pw[i]);
    close(fp_jw[i]);
  }
  close(fp_irig);

  for (i=0; i<MAX_UVHYG_INTFC; i++)
  {
    for (j=0; j<2; j++)
    {
      sprintf(devstr, "/dev/dpres_%d_read_%d", i, j);
      unlink(devstr);
      sprintf(devstr, "/dev/jpltdl_%d_read_%d", i, j);
      unlink(devstr);
    }
    sprintf(devstr, "/dev/dpres_%d_write", i);
    unlink(devstr);
    sprintf(devstr, "/dev/jpltdl_%d_write", i);
    unlink(devstr);
  }
  sprintf(devstr, "/dev/irig");
  unlink(devstr);
  /* ##################################################################### */
  /* ## ..end fake instrument FIFO simulation.                          ## */
  /* ##################################################################### */

  print("(%s) %s:\t done\n\n", __FILE__, __FUNCTION__);
}

/* -- MODULE ---------------------------------------------------------- */
/* int main (void) */
int init_module (void)
{
  print("(%s) %s:\t compiled on %s at %s\n\n",
	__FILE__, __FUNCTION__, __DATE__, __TIME__);

  char devstr[30];
  int i, j;

  /* check for module parameters */
  if (m_addr) isa_address = (unsigned int)m_addr + 0xf7000000;
  print("(%s) %s:\t addr = %x width = %x\n", __FILE__, __FUNCTION__,
	isa_address, ioWidth);

  /* ##################################################################### */
  /* ## begin fake instrument FIFO simulation...                        ## */
  /* ##################################################################### */
  for (i=0; i<MAX_UVHYG_INTFC; i++)
  {
    for (j=0; j<2; j++)
    {
      sprintf(devstr, "/dev/dpres_%d_read_%d", i, j);
      if (mkfifo(devstr,0666)!=0)
        goto fail_mkfifo;
      if ( (fp_pr[i][j] = open(devstr,  O_NONBLOCK | O_WRONLY)) < 0)
        goto fail_open;

      sprintf(devstr, "/dev/jpltdl_%d_read_%d", i, j);
      if (mkfifo(devstr,0666)!=0)
        goto fail_mkfifo;
      if ( (fp_jr[i][j] = open(devstr,  O_NONBLOCK | O_WRONLY)) < 0)
        goto fail_open;
    }

    sprintf(devstr, "/dev/dpres_%d_write", i);
    if (mkfifo(devstr,0666)!=0)
      goto fail_mkfifo;
    if ( (fp_pw[i] = open(devstr, O_NONBLOCK | O_RDONLY)) < 0)
      goto fail_open;

    sprintf(devstr, "/dev/jpltdl_%d_write", i);
    if (mkfifo(devstr,0666)!=0)
      goto fail_mkfifo;
    if ( (fp_jw[i] = open(devstr, O_NONBLOCK | O_RDONLY)) < 0)
      goto fail_open;
  }
  sprintf(devstr, "/dev/irig");
  if (mkfifo(devstr,0666)!=0)
    goto fail_mkfifo;
  if ( (fp_irig = open(devstr,  O_NONBLOCK | O_WRONLY)) < 0)
    goto fail_open;
  /* ##################################################################### */
  /* ## ..end fake instrument FIFO simulation.                          ## */
  /* ##################################################################### */


#if 0 // TODO
  /* Wait for the init_module to load the header build configuration */
  print("(%s) %s:\t sem_wait...\n", __FILE__, __FUNCTION__);
  sem_wait (&anInitSemaphore);
#endif

  /* initialize the semaphore to the 1 PPS thread */
  sem_init (&anIrigSemaphore, /* which semaphore */
            1,                /* usable between processes? Usu. 1 */
            0);               /* initial value */
  print("(%s) %s:\t sem_init done\n", __FILE__, __FUNCTION__);

  /* create the 1 PPS thread */
  pthread_create( &aThread, NULL, irig_1PPS_thread, (void *)0 );
  print("(%s) %s:\t pthread_create done\n", __FILE__, __FUNCTION__);

  /* Grab the region so that no one else tries to probe our ioports. */
  request_region(isa_address, ioWidth, "IRIG-b");
  print("(%s) %s:\t request_region done\n", __FILE__, __FUNCTION__);

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
  print("(%s) %s:\t set_GPIO_IRQ_edge(%d,%d)\n",
        __FILE__, __FUNCTION__, IRQ_TO_GPIO(VIPER_ISA_IRQ10), GPIO_FALLING_EDGE);
  set_GPIO_IRQ_edge(IRQ_TO_GPIO(VIPER_ISA_IRQ10), GPIO_FALLING_EDGE);
#endif

  /* DEBUG test - see if we are talking to the IRIG card... */
#ifdef __RTL__
  setMajorTime(NULL, "asdf",0, NULL);
#else
  setMajorTime("asdf",0, NULL);
#endif
  int iii, jjj, timeStrLen;
  char timeStr[40];
  for (iii=0; iii<20; iii++)
  {
#ifdef __RTL__
    timeStrLen = readTime(NULL, timeStr, 0, NULL);
#else
    timeStrLen = readTime(timeStr, 0, NULL);
#endif
    print("(%03d) %s\n", timeStrLen, timeStr);
    for (jjj=0; jjj<10000000; jjj++);
  }
  /* END DEBUG test. */

  /* install a handler for the interrupt */
  print("(%s) %s:\t request_irq( %d, irig_100hz_isr )\n",
        __FILE__, __FUNCTION__, VIPER_ISA_IRQ10);
#ifdef __RTL__
#ifndef __ISA_DEMUX__ // skip this, we are using isa_demux.cc implementation...
  if ( rtl_request_irq( VIPER_ISA_IRQ10, irig_100hz_isr ) < 0 )
#else
  if (0)
#endif
#else
  if ( request_irq ( VIPER_ISA_IRQ10, &irig_100hz_isr,
                     SA_INTERRUPT, "IRIG-b", NULL ) != 0 )
#endif
  {
    /* failed... */
    cleanup_module();
    print("(%s) %s:\t could not allocate IRQ at #%d\n",
	  __FILE__, __FUNCTION__, VIPER_ISA_IRQ10);
    return -EIO;
  }

#ifndef __ISA_DEMUX__ // skip this, we are using isa_demux.cc implementation...
  /* activate the IRIG-B board */
  setHeartBeatOutput(m_rate[0]);
  print("(%s) %s:\t setHeartBeatOutput(%d) done\n", __FILE__, __FUNCTION__, m_rate[0]);
  setRate2Output(m_rate[1]);
  print("(%s) %s:\t setRate2Output(%d) done\n",     __FILE__, __FUNCTION__, m_rate[1]);
  enableHeartBeatInt();
  print("(%s) %s:\t enableHeartBeatInt  done\n",    __FILE__, __FUNCTION__);
#endif

#if 0 //TODO?
  /* create /dev/irig device file for POSIX file access */
  if ( rtl_register_dev("/dev/irig", &irig_fops, 0) )
  {
    print("(%s) %s:\t could not register device.\n", __FILE__, __FUNCTION__);
    return -EIO;
  }

  /* create /proc/irig file for shell diagnostics */
  misc_register (&irig_miscdev);
  create_proc_read_entry ("/dev/irig", 0, 0, rtc_read_proc, NULL);
#endif

  print("(%s) %s:\t loaded\n", __FILE__, __FUNCTION__);
  return 0;

  /* ##################################################################### */
  /* ## begin fake instrument FIFO simulation...                        ## */
  /* ##################################################################### */
 fail_open:
  print("(%s) %s:\t could not open '%s'\n", __FILE__, __FUNCTION__, devstr);
  unlink(devstr);

 fail_mkfifo:
  print("(%s) %s:\t could not create '%s'\n", __FILE__, __FUNCTION__, devstr);
  cleanup_module();
  return -EIO;
  /* ##################################################################### */
  /* ## ..end fake instrument FIFO simulation.                          ## */
  /* ##################################################################### */
}
