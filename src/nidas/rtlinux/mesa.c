/* mesa.c

   RTLinux module for interfacing the Mesa Electronics
   4I34(M) Anything I/O FPGA card.

   Original Author: Mike Spowart

   Copyright 2005 UCAR, NCAR, All Rights Reserved

   Implementation notes:

   Revisions:

     $LastChangedRevision$
         $LastChangedDate$
           $LastChangedBy$
                 $HeadURL$
*/

// RTLinux includes...
#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_posixio.h>
#include <rtl_stdio.h>
#include <rtl_unistd.h>

// Linux module includes...
#include <linux/init.h>          // module_init, module_exit
#include <linux/ioport.h>
//#include <bits/posix1_lim.h>

// DSM includes...
#include <nidas/rtlinux/dsmlog.h>
#include <nidas/rtlinux/dsm_version.h>
#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/rtlinux/irigclock.h>

#include <nidas/rtlinux/mesa.h>

RTLINUX_MODULE(mesa);
MODULE_AUTHOR("Mike Spowart <spowart@ucar.edu>");
MODULE_DESCRIPTION("Mesa ISA driver for RTLinux");

void load_finish(void);

// Define IOCTLs
static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS, _IOC_SIZE(GET_NUM_PORTS) },
  { MESA_LOAD,     _IOC_SIZE(MESA_LOAD    ) },
  { COUNTERS_SET,  _IOC_SIZE(COUNTERS_SET ) },
  { RADAR_SET,     _IOC_SIZE(RADAR_SET    ) },
  { PMS260X_SET,   _IOC_SIZE(PMS260X_SET  ) },
  { MESA_STOP,     _IOC_SIZE(MESA_STOP    ) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);
static struct ioctlHandle* ioctlhandle = 0;

// Set the base address of the Mesa 4I34 card
volatile unsigned long phys_membase;
static volatile unsigned long baseadd = MESA_BASE;
MODULE_PARM(baseadd, "1l");
MODULE_PARM_DESC(baseadd, "ISA memory base (default 0xf7000220)");

// global variables
unsigned long filesize;
static int counter_channels = 0;
static int radar_channels   = 0;

static enum irigClockRates counter_rate = IRIG_NUM_RATES;
static enum irigClockRates radar_rate   = IRIG_NUM_RATES;

enum flag {TRUE, FALSE};

char requested_region = 0;

// File pointers to data and command FIFOs
static int fd_mesa_load;
static int fd_mesa_counter[N_COUNTERS];
static int fd_mesa_radar[N_RADARS];
static struct rtl_sigaction cmndAct;

void cleanup_module (void);

/* -- IRIG CALLBACK --------------------------------------------------- */
void read_counter(void * channel)
{
  MESA_SIXTEEN_BIT_SAMPLE sample;
  short	chn = (int) channel;
  int	i, read_address_offset;

  for (i = 0; i < chn; i++)
  {
    if (i == 0)
      read_address_offset = COUNT0_READ_OFFSET;
    else
      read_address_offset = COUNT1_READ_OFFSET;

    // read from the counter channel
    if ((sample.data = readw(MESA_BASE + read_address_offset)) == 0)
      DSMLOG_ERR("NO COUNTS!\n");

    sample.data = 111 * (i+1);
    DSMLOG_DEBUG("chn: %d  sample.data: %d\n", i, sample.data);

    // write the counts to the user's FIFO
    sample.timetag = GET_MSEC_CLOCK;
    rtl_write(fd_mesa_counter[i], &sample, sizeof(sample));
  }
}

/* -- IRIG CALLBACK --------------------------------------------------- */
void read_radar(void * channel)
{
  MESA_SIXTEEN_BIT_SAMPLE sample;
  short chn = (int) channel;

  // read from the radar channel
  if ((sample.data = readw(MESA_BASE + RADAR_READ_OFFSET)) <= 0)
    DSMLOG_ERR("BAD ALTITUDE!\n");

  sample.data = 333;
  DSMLOG_DEBUG("chn: %d  sample.data: %d", chn-1, sample.data);

  // write the altitude to the user's FIFO
  sample.timetag = GET_MSEC_CLOCK;
  rtl_write(fd_mesa_radar[chn-1], &sample, sizeof(sample));
}

/* -- UTILITY --------------------------------------------------------- */

/* dsm/modules/mesa.c: load_start: outb(5) */
/* dsm/modules/mesa.c: load_start: inb = 220 */
/* dsm/modules/mesa.c: load_start: outb(10) */
void load_start()
{
  int count;
  unsigned char config, status;

  config = M_4I34CFGCSOFF | M_4I34CFGINITASSERT |
           M_4I34CFGWRITEDISABLE | M_4I34LEDOFF;

  outb(config,MESA_BASE + R_4I34CONTROL);
  DSMLOG_DEBUG("outb(%d)\n", config);
  status = inb(MESA_BASE + R_4I34STATUS);
  DSMLOG_DEBUG("inb = %d\n", status);

  /* Note that if we see DONE at the start of programming, it's most likely due
   * to an attempt to access the 4I34 at the wrong I/O location.
   */
  if (status & M_4I34PROGDUN)
  {
    DSMLOG_ERR("failed - attempted to access the 4I34 at the wrong I/O location?\n");
    cleanup_module();
  }
  config = M_4I34CFGCSON | M_4I34CFGINITDEASSERT |
           M_4I34CFGWRITEENABLE | M_4I34LEDON;

  outb(config,MESA_BASE + R_4I34CONTROL);
  DSMLOG_DEBUG("outb(%d)\n", config);

  // Multi task for 100 us
//   struct rtl_timespec ts;
//   rtl_clock_gettime(RTL_CLOCK_REALTIME, &ts);
//   rtl_timespec_add_ns(&ts, 100000);
//   rtl_clock_nanosleep(RTL_CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);

  /* Delay 100 uS. */
  for (count = 0; count <= 1000; count++)
    status = inb(MESA_BASE + R_4I34STATUS);
}

/* -- SIGACTION ------------------------------------------------------- */
void load_program(int sig, rtl_siginfo_t *siginfo, void *v)
{
  int	i;
  char	buf[MAX_BUFFER];
  unsigned long len;
  static unsigned long total = 0;
  unsigned char *bptr;

  // start of load process
  if (total == 0)
      load_start();

  // ignore if not incoming data
  if (siginfo->si_fd != fd_mesa_load) {
    DSMLOG_ERR("ignored... unknown file pointer.\n");
    return;
  }
  if (siginfo->si_code != RTL_POLL_IN) {
    DSMLOG_ERR("ignored... not incoming data.\n");
    return;
  }

  // read the FIFO into a buffer and then program the FPGA
  len = rtl_read(siginfo->si_fd, &buf, MAX_BUFFER);
  total += len;

  // Now program the FPGA
  bptr = buf;
  for (i = 0; i < len; i++)
    outb(*bptr++, MESA_BASE + R_4I34DATA);

  // end of load process
  if (total == filesize)
    load_finish();
}

/* -- UTILITY --------------------------------------------------------- */
void load_finish()
{
  int waitcount, count;
  unsigned char config;
  enum flag success;

  DSMLOG_DEBUG("start");
  config = M_4I34CFGCSOFF | M_4I34CFGINITDEASSERT |
           M_4I34CFGWRITEDISABLE | M_4I34LEDON;
  outb(config, MESA_BASE + R_4I34CONTROL);
  DSMLOG_DEBUG("outb success");

  // Wait for Done bit set
  success = FALSE;
  for (waitcount = PROGWAITLOOPCOUNT; waitcount != 0; --waitcount)
  {
    if ( inb(MESA_BASE + R_4I34STATUS) & M_4I34PROGDUN )
    {
      DSMLOG_DEBUG("waitcount: %d", waitcount);
      success = TRUE;
      continue;
    }
  }

  if (success)
  {
    DSMLOG_NOTICE("FPGA programming done.\n");

    // Indicate end of programming by turning off 4I34 led
    config = M_4I34CFGCSOFF | M_4I34CFGINITDEASSERT |
             M_4I34CFGWRITEDISABLE | M_4I34LEDOFF;
    outb(config,MESA_BASE + R_4I34CONTROL);

    // Now send out extra configuration completion clocks
    for (count = 24; count != 0; --count)
      outb(0xFF, MESA_BASE + R_4I34DATA);
  }
  else
  {
    DSMLOG_ERR("FPGA programming not successful.\n");
    cleanup_module();
  }
}

/* -- UTILITY --------------------------------------------------------- */
void close_ports( void )
{
  int	chn;

  // close and unlink eack counter port...
  for (chn = 0; chn < counter_channels; chn++)
  {
    if (fd_mesa_counter[chn])
    {
      rtl_close( fd_mesa_counter[chn] );
      DSMLOG_DEBUG("closed fd_mesa_counter[%d]\n", chn);
    }
  }

  // unregister poll function from IRIG module
  if (counter_rate != IRIG_NUM_RATES)
  {
    unregister_irig_callback(&read_counter, counter_rate, counter_channels);
    DSMLOG_DEBUG("unregistered read_counter() from IRIG.\n");
  }

  // close and unlink eack radar port...
  for (chn = counter_channels; chn < counter_channels + radar_channels; chn++)
  {
    if (fd_mesa_radar[chn-counter_channels])
    {
      rtl_close( fd_mesa_radar[chn-counter_channels] );
      DSMLOG_DEBUG("closed fd_mesa_radar[%d]\n", chn-counter_channels);
    }
  }

  // unregister poll function from IRIG module
  if (radar_rate != IRIG_NUM_RATES)
  {
    unregister_irig_callback(&read_radar, radar_rate, radar_channels);
    DSMLOG_DEBUG("unregistered read_radar() from IRIG.\n");
  }
}

/* -- IOCTL CALLBACK -------------------------------------------------- */
/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 */
static int mesa_ioctl(int cmd, int board, int port, void *buf, rtl_size_t len)
{
  int j;
  char devstr[30];
  struct radar_set* radar_ptr;
  struct counters_set* counter_ptr;

  switch (cmd)
  {
    case GET_NUM_PORTS:
      *(int *) buf = N_PORTS;
      break;

    case MESA_LOAD:
      filesize = *(unsigned long*) buf;
      break;

    case COUNTERS_SET:
      // create and open counter data FIFOs
      counter_ptr = (struct counters_set*) buf;
      counter_channels = counter_ptr->channel;
      for (j=0; j < counter_channels; j++)
      {
        sprintf(devstr, "/dev/mesa_in_%d", j);
        fd_mesa_counter[j] = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_WRONLY );
        if (fd_mesa_counter[j] < 0)
          return fd_mesa_counter[j];
      }
      // register poll routine with the IRIG driver
      counter_rate = irigClockRateToEnum(counter_ptr->rate);
      register_irig_callback(&read_counter, counter_rate,
                             (void *)counter_channels);
      break;

    case RADAR_SET:
      // create and open radar data FIFOs
      radar_ptr = (struct radar_set*) buf;
      radar_channels = radar_ptr->channel;
      for (j=counter_channels; j < counter_channels + radar_channels; j++)
      {
        sprintf(devstr, "/dev/mesa_in_%d", j);
        fd_mesa_radar[j-counter_channels] = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_WRONLY );
        if (fd_mesa_radar[j-counter_channels] < 0)
          return fd_mesa_radar[j-counter_channels];
      }
      // register poll routine with the IRIG driver
      radar_rate = irigClockRateToEnum(radar_ptr->rate);
      register_irig_callback(&read_radar, radar_rate,
                             (void *)radar_channels);
      break;

    case PMS260X_SET:
      DSMLOG_ERROR("PMS260X_SET\n");
      break;

    case MESA_STOP:
      close_ports();
      break;

    default:
      DSMLOG_ERROR("unknown ioctl cmd.\n");
      break;
  }
  return len;
}

/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  int	chn;
  char	devstr[30];

  // close and destroy the mesa load fifo
  sprintf(devstr, "/dev/mesa_program_board");
  DSMLOG_DEBUG("closing '%s' @ 0x%x\n", devstr, fd_mesa_load);
  if (fd_mesa_load)
    rtl_close( fd_mesa_load );
  rtl_unlink( devstr );

  // close the data fifos
  close_ports();

  // destroy the data fifos
  for (chn = 0; chn < N_PORTS; chn++)
  {
    sprintf(devstr, "/dev/mesa_in_%d\n", chn);
    rtl_unlink( devstr );
  }

  // Close my ioctl FIFO, deregister my mesa_ioctl function
  if (ioctlhandle)
    closeIoctlFIFO(ioctlhandle);

  // free up the ISA memory region
  if ( requested_region )
  {
    release_region(baseadd, MESA_REGION_SIZE);
    DSMLOG_DEBUG("freed up the ISA memory region.\n");
  }

  DSMLOG_DEBUG("done.\n");
}

/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  int	chn;
  char	devstr[30];

  // DSM_VERSION_STRING is found in dsm_version.h
  DSMLOG_NOTICE("version: %s\n", DSM_VERSION_STRING);
  DSMLOG_NOTICE("compiled on %s at %s\n", __DATE__, __TIME__);

  // open up ioctl FIFO, register mesa_ioctl function
  ioctlhandle = openIoctlFIFO("mesa", BOARD_NUM, mesa_ioctl,
                              nioctlcmds, ioctlcmds);
  if (!ioctlhandle) return -RTL_EIO;

  for (chn = 0; chn < N_PORTS; chn++)
  {
    // create its output FIFO
    sprintf( devstr, "/dev/mesa_in_%d", chn );

    // remove broken device file before making a new one
    if (rtl_unlink(devstr) < 0)
      if ( rtl_errno != RTL_ENOENT )
        return -rtl_errno;

    DSMLOG_DEBUG("rtl_mkfifo( %s, 0666 );\n", devstr);
    rtl_mkfifo( devstr, 0666 );
  }

  // Make 4I34 board Ioctl Fifo...
  sprintf(devstr, "/dev/mesa_program_board");

  // remove broken device file before making a new one
  if (rtl_unlink(devstr) < 0)
    if ( rtl_errno != RTL_ENOENT )
      return -rtl_errno;

  DSMLOG_DEBUG("rtl_mkfifo( %s, 0666 );\n", devstr);
  rtl_mkfifo( devstr, 0666 );
  fd_mesa_load = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_RDONLY );
  DSMLOG_DEBUG("opened '%s' @ 0x%x\n", devstr, fd_mesa_load);

  // create FIFO handler
  cmndAct.sa_sigaction = load_program;
  cmndAct.sa_fd        = fd_mesa_load;
  cmndAct.sa_flags     = RTL_SA_RDONLY | RTL_SA_SIGINFO;
  if ( rtl_sigaction( RTL_SIGPOLL, &cmndAct, NULL ) != 0 )
  {
    DSMLOG_ERROR("Cannot create FIFO handler for %s.\n", devstr);
    cleanup_module();
    return -RTL_EIO;
  }

  // reserve the ISA memory region
  if (!request_region(baseadd, MESA_REGION_SIZE, "mesa"))
  {
    DSMLOG_ERROR("couldn't allocate I/O range %x - %x\n", baseadd,
        baseadd + MESA_REGION_SIZE - 1);
    cleanup_module();
    return -RTL_EBUSY;
  }

  requested_region = 1;
  DSMLOG_DEBUG("done.\n");
  return 0;	// success
}
