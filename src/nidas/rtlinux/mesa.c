/* mesa.c

   RTLinux module for interfacing the Mesa Electronics
   4I34(M) Anything I/O FPGA card.

   Original Author: Mike Spowart

   Copyright 2005 UCAR, NCAR, All Rights Reserved

   Implementation notes:

                $Revision$
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
#include <linux/sched.h>
//#include <bits/posix1_lim.h>

// DSM includes...
#include <nidas/rtlinux/dsmlog.h>
#include <nidas/rtlinux/dsm_version.h>
#include <nidas/rtlinux/dsm_viper.h>
#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/rtlinux/irigclock.h>

#include <nidas/rtlinux/mesa.h>

RTLINUX_MODULE(mesa);
MODULE_AUTHOR("Mike Spowart <spowart@ucar.edu>");
MODULE_DESCRIPTION("Mesa ISA driver for RTLinux");

/* number of Mesa boards in system (number of non-zero ioport values) */
static int numboards = 0;

static struct MESA_Board* boardInfo = 0;

static const char* devprefix = "mesa";

// Define IOCTLs
static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS, _IOC_SIZE(GET_NUM_PORTS) },
  { MESA_LOAD_START,_IOC_SIZE(MESA_LOAD_START) },
  { MESA_LOAD_BLOCK,_IOC_SIZE(MESA_LOAD_BLOCK) },
  { MESA_LOAD_DONE, _IOC_SIZE(MESA_LOAD_DONE) },
  { COUNTERS_SET,  _IOC_SIZE(COUNTERS_SET ) },
  { RADAR_SET,     _IOC_SIZE(RADAR_SET    ) },
  { PMS260X_SET,   _IOC_SIZE(PMS260X_SET  ) },
  { DIGITAL_IN_SET,_IOC_SIZE(DIGITAL_IN_SET) },
  { MESA_STOP,     _IOC_SIZE(MESA_STOP    ) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);


// Set the base address of the Mesa 4I34 card
static unsigned long ioport = MESA_BASE;

MODULE_PARM(ioport, "1l");
MODULE_PARM_DESC(ioport, "ISA memory base (default 0x220)");

enum flag {TRUE, FALSE};

void cleanup_module (void);

/* -- IRIG CALLBACK --------------------------------------------------- */
static void read_counter(void * channel)
{
  int	i, read_address_offset;
  MESA_SIXTEEN_BIT_SAMPLE sample;
  struct MESA_Board * brd = boardInfo;

  sample.size = sizeof(dsm_sample_id_t) + sizeof(short) * brd->nCounters;
  sample.sampleID = ID_COUNTERS;
  sample.timetag = GET_MSEC_CLOCK;

  for (i = 0; i < brd->nCounters; i++)
  {
    if (i == 0)
      read_address_offset = COUNT0_READ_OFFSET;
    else
      read_address_offset = COUNT1_READ_OFFSET;

    // read from the counter channel
    sample.data[i] = inw(brd->addr + read_address_offset);
//DSMLOG_DEBUG("chn: %d  sample.data: %d\n", i, sample.data[i]);
  }

  rtl_write(brd->outfd, &sample, sample.size + sizeof(dsm_sample_length_t)
	+ sizeof(dsm_sample_time_t));
}

/* -- IRIG CALLBACK --------------------------------------------------- */
static void read_radar(void * channel)
{
  MESA_SIXTEEN_BIT_SAMPLE sample;
  struct MESA_Board * brd = boardInfo;

  // read from the radar channel
  sample.sampleID = ID_RADAR;
  sample.size = sizeof(dsm_sample_id_t) + sizeof(short) * brd->nRadars;
  sample.timetag = GET_MSEC_CLOCK;
 
  unsigned short pdata = -1, count =0;
  sample.data[0] = inw(brd->addr + RADAR_READ_OFFSET);
  while (pdata != sample.data[0]) {
    pdata=sample.data[0];
    sample.data[0] = inw(brd->addr + RADAR_READ_OFFSET);
    count++;
  }
  DSMLOG_DEBUG("\nchn: %d  sample.data: %d loop_count: %d\n", channel, sample.data[0], count);

  // write the altitude to the user's FIFO
  rtl_write(brd->outfd, &sample, sample.size + sizeof(dsm_sample_length_t)
	+ sizeof(dsm_sample_time_t));
}

/* -- IRIG CALLBACK --------------------------------------------------- */
static void read_260x(void * channel)
{
  int	i;
  MESA_TWO_SIXTY_X_SAMPLE sample;
  struct MESA_Board * brd = boardInfo;

  int send_size = TWO_SIXTY_BINS+8+1+1;

  sample.size = sizeof(short) * send_size;
  sample.sampleID = ID_260X;
  sample.timetag = GET_MSEC_CLOCK;

  sample.strobes = inw(brd->addr + STROBES_OFFSET);
  sample.resets = inw(brd->addr + TWOSIXTY_RESETS_OFFSET);

  // read 260X histogram data
  for (i = 0; i < TWO_SIXTY_BINS; ++i)
  {
    sample.data[i] = inw(brd->addr + HISTOGRAM_READ_OFFSET);
  }

  (void)inw(brd->addr + HISTOGRAM_CLEAR_OFFSET);

  // read 260X housekeeping data
#ifdef HOUSE_260X
  for (i = 0; i < 8; ++i)
  {
    sample.house[i] = inw(brd->addr + HOUSE_READ_OFFSET);
    (void)inw(brd->addr + HOUSE_ADVANCE_OFFSET);
  }

  (void)inw(brd->addr + HOUSE_RESET_OFFSET);
#endif

  // write the data to the user's FIFO
  rtl_write(brd->outfd, &sample, sample.size + sizeof(dsm_sample_length_t)
	+ sizeof(dsm_sample_time_t));
}


/* -- UTILITY --------------------------------------------------------- */

static void outportbwswap(struct MESA_Board * brd, unsigned char thebyte)
{
  static unsigned char swaptab[256] =
  {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0,
    0x30, 0xB0, 0x70, 0xF0, 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
    0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8, 0x04, 0x84, 0x44, 0xC4,
    0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC,
    0x3C, 0xBC, 0x7C, 0xFC, 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
    0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2, 0x0A, 0x8A, 0x4A, 0xCA,
    0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6,
    0x36, 0xB6, 0x76, 0xF6, 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
    0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE, 0x01, 0x81, 0x41, 0xC1,
    0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9,
    0x39, 0xB9, 0x79, 0xF9, 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
    0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5, 0x0D, 0x8D, 0x4D, 0xCD,
    0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3,
    0x33, 0xB3, 0x73, 0xF3, 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
    0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB, 0x07, 0x87, 0x47, 0xC7,
    0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF,
    0x3F, 0xBF, 0x7F, 0xFF
  };

  outb(swaptab[thebyte], brd->addr + R_4I34DATA);
}

/*---------------------------------------------------------------------------*/
/* dsm/modules/mesa.c: load_start: outb(5) */
/* dsm/modules/mesa.c: load_start: inb = 220 */
/* dsm/modules/mesa.c: load_start: outb(10) */
static int load_start(struct MESA_Board * brd)
{
  unsigned char config, status;

  config = M_4I34CFGCSOFF | M_4I34CFGINITASSERT |
           M_4I34CFGWRITEDISABLE | M_4I34LEDOFF;

  outb(config, brd->addr + R_4I34CONTROL);
  DSMLOG_DEBUG("outb(%#x)\n", config);
  status = inb(brd->addr + R_4I34STATUS);
  DSMLOG_DEBUG("inb = %#x\n", status);

  /* Note that if we see DONE at the start of programming, it's most likely due
   * to an attempt to access the 4I34 at the wrong I/O location.
   */
  if (status & M_4I34PROGDUN)
  {
    DSMLOG_ERR("failed - attempted to access the 4I34 at the wrong I/O location?\n");
    return -ENODEV;
  }
  config = M_4I34CFGCSON | M_4I34CFGINITDEASSERT |
           M_4I34CFGWRITEENABLE | M_4I34LEDON;

  outb(config, brd->addr + R_4I34CONTROL);
  DSMLOG_DEBUG("outb(%#x)\n", config);

  // Multi task for 100 us
//   struct rtl_timespec ts;
//   rtl_clock_gettime(RTL_CLOCK_REALTIME, &ts);
//   rtl_timespec_add_ns(&ts, 100000);
//   rtl_clock_nanosleep(RTL_CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);

  /* Delay 100 uS. */
    status = inb(brd->addr + R_4I34STATUS);
    unsigned long j = jiffies + 1;
    while (time_before(jiffies,j)) schedule();
  DSMLOG_DEBUG("load_start done, status=%#x\n", status);

    brd->progNbytes = 0;

  return 0;
}

/* -- UTILITY --------------------------------------------------------- */
static int load_finish(struct MESA_Board * brd)
{
  int waitcount, count, ret = -EIO;
  unsigned char config;
  enum flag success;

  DSMLOG_DEBUG("load_finish, program nbytes=%d\n",brd->progNbytes);
  config = M_4I34CFGCSOFF | M_4I34CFGINITDEASSERT |
           M_4I34CFGWRITEDISABLE | M_4I34LEDON;
  outb(config, brd->addr + R_4I34CONTROL);

  // Wait for Done bit set
  success = FALSE;
  for (waitcount = 0; waitcount < 200; ++waitcount)
  {
    // this is not called from a real-time thread, so use jiffies
    // to delay
    unsigned char status;
    if ( (status = inb(brd->addr + R_4I34STATUS)) & M_4I34PROGDUN )
    {
      DSMLOG_INFO("waitcount: %d, status=%#x,donebit=%#x\n",
        waitcount,status,M_4I34PROGDUN);
      success = TRUE;
      break;
    }
    DSMLOG_INFO("waitcount: %d, status=%#x,donebit=%#x\n",
        waitcount,status,M_4I34PROGDUN);
    unsigned long j = jiffies + 1;	// 1/100 of a second.
    while (time_before(jiffies,j)) schedule();
  }

  if (success == TRUE)
  {
    DSMLOG_NOTICE("FPGA programming done.\n");

    // Indicate end of programming by turning off 4I34 led
    config = M_4I34CFGCSOFF | M_4I34CFGINITDEASSERT |
             M_4I34CFGWRITEDISABLE | M_4I34LEDOFF;
    outb(config,brd->addr + R_4I34CONTROL);

    // Now send out extra configuration completion clocks
    for (count = 24; count != 0; --count)
      outb(0xFF, brd->addr + R_4I34DATA);

     ret = 0;
  }
  else
    DSMLOG_ERR("FPGA programming not successful.\n");

  return ret;
}

/* -- SIGACTION ------------------------------------------------------- */
static int load_program(struct MESA_Board * brd, struct _prog * buf)
{
  int	i, ret = buf->len;
  unsigned char *bptr;

  // read the FIFO into a buffer and then program the FPGA
  brd->progNbytes += buf->len;

  // Now program the FPGA
  bptr = buf->buffer;
  for (i = 0; i < buf->len; i++)
    outportbwswap(brd, *bptr++);

  return ret;
}

/* -- UTILITY --------------------------------------------------------- */
static void close_ports( struct MESA_Board * brd )
{
  // close and unlink eack counter port...
  if (brd->outfd)
  {
    rtl_close( brd->outfd );
    DSMLOG_DEBUG("closed brd->outfd\n");
  }

  // unregister poll function from IRIG module
  if (brd->counter_rate != IRIG_NUM_RATES)
  {
    unregister_irig_callback(&read_counter, brd->counter_rate, 0);
    DSMLOG_DEBUG("unregistered read_counter() from IRIG.\n");
  }

  // unregister poll function from IRIG module
  if (brd->radar_rate != IRIG_NUM_RATES)
  {
    unregister_irig_callback(&read_radar, brd->radar_rate, 0);
    DSMLOG_DEBUG("unregistered read_radar() from IRIG.\n");
  }

  // unregister poll function from IRIG module
  if (brd->twoSixty_rate != IRIG_NUM_RATES)
  {
    unregister_irig_callback(&read_260x, brd->twoSixty_rate, 0);
    DSMLOG_DEBUG("unregistered read_260x() from IRIG.\n");
  }
}

/* -- IOCTL CALLBACK -------------------------------------------------- */
/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 */
static int ioctlCallback(int cmd, int board, int port, void *buf, rtl_size_t len)
{
  // return LINUX errnos here, not RTL_XXX errnos.
  int ret = -EINVAL;

  struct radar_set * radar_ptr;
  struct counters_set * counter_ptr;
  struct pms260x_set * twoSixty_ptr;
  struct digital_in * digital_in_ptr;

//#ifdef DEBUG
  DSMLOG_DEBUG("ioctlCallback cmd=%x board=%d port=%d len=%d\n",
        cmd,board,port,len);
//#endif

  // Sanity checks.
  if (!boardInfo || board >= numboards)
    return ret;

  struct MESA_Board * brd = boardInfo + board;

  if (brd->outfd < 0)
  {
    brd->outfd = rtl_open(brd->fifoName, RTL_O_NONBLOCK | RTL_O_WRONLY);
    if (brd->outfd < 0)
      return brd->outfd;
  }

  switch (cmd)
  {
    case GET_NUM_PORTS:
      DSMLOG_DEBUG("GET_NUM_PORTS\n");
      *(int *) buf = N_PORTS;
      ret = sizeof(int);
      break;

    case MESA_LOAD_START:
      DSMLOG_DEBUG("MESA_FPGA_START\n");
      ret = load_start(brd);
      break;

    case MESA_LOAD_BLOCK:
//      DSMLOG_DEBUG("MESA_FPGA_LOAD\n");
      ret = load_program(brd, (struct _prog *)buf);
      break;

    case MESA_LOAD_DONE:
      DSMLOG_DEBUG("MESA_FPGA_DONE\n");
      ret = load_finish(brd);
      break;

    case DIGITAL_IN_SET:
      DSMLOG_DEBUG("DIGITAL_IN not implmented.\n");
      digital_in_ptr = (struct digital_in *) buf;

      ret = len;
      break;

    case COUNTERS_SET:
      // create and open counter data FIFOs
      counter_ptr = (struct counters_set *) buf;
      DSMLOG_DEBUG("COUNTERS_SET rate=%d\n", counter_ptr->rate);
      // register poll routine with the IRIG driver
      brd->counter_rate = irigClockRateToEnum(counter_ptr->rate);
      register_irig_callback(&read_counter, brd->counter_rate, 0);
      brd->nCounters = counter_ptr->nChannels;
      ret = len;
      break;

    case RADAR_SET:
      // create and open radar data FIFOs
      radar_ptr = (struct radar_set *) buf;
      DSMLOG_DEBUG("RADAR_SET rate=%d\n", radar_ptr->rate);
      // register poll routine with the IRIG driver
      brd->radar_rate = irigClockRateToEnum(radar_ptr->rate);
      register_irig_callback(&read_radar, brd->radar_rate, 0);
      brd->nRadars = radar_ptr->nChannels;
      ret = len;
      break;

    case PMS260X_SET:
      // create and open 260X data FIFOs
      twoSixty_ptr = (struct pms260x_set *) buf;
      DSMLOG_DEBUG("260X_SET rate=%d\n", twoSixty_ptr->rate);
      // register poll routine with the IRIG driver
      brd->twoSixty_rate = irigClockRateToEnum(twoSixty_ptr->rate);
      register_irig_callback(&read_260x, brd->twoSixty_rate, 0);
      brd->n260X = twoSixty_ptr->nChannels;
      ret = len;
      break;

    case MESA_STOP:
      DSMLOG_DEBUG("MESA_STOP\n");
      close_ports(brd);
      ret = 0;
      break;

    default:
      DSMLOG_ERR("Unknown ioctl cmd.\n");
      break;
  }
  return ret;
}

/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  int	ib;

  for (ib = 0; ib < numboards; ib++)
  {
    struct MESA_Board* brd = boardInfo + ib;

    // close the data fifos
    close_ports(brd);

    if (brd->ioctlhandle)
      closeIoctlFIFO(brd->ioctlhandle);

    // close and remove RTL fifo
    if (brd->outfd >= 0)
      rtl_close(brd->outfd);
    if (brd->fifoName)
    {
      rtl_unlink(brd->fifoName);
      rtl_gpos_free(brd->fifoName);
    }

    if (brd->addr)
      release_region(brd->addr, MESA_REGION_SIZE);
  }

  DSMLOG_DEBUG("complete.\n");
}

static char * createFifo(char inName[], int chan)
{
  char * devName;

  // create its output FIFO
  devName = makeDevName(devprefix, inName, chan);

  // remove broken device file before making a new one
  if ((rtl_unlink(devName) < 0 && rtl_errno != RTL_ENOENT)
              || rtl_mkfifo(devName, 0666) < 0)
  {
    DSMLOG_ERR("error: unlink/mkfifo %s: %s\n",
                    devName, rtl_strerror(rtl_errno));
    return 0;
  }

  return devName;
}

/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  int	ib;
  int	error = -EINVAL;

  boardInfo = 0;

  // DSM_VERSION_STRING is found in dsm_version.h
  DSMLOG_NOTICE("version: %s\n", DSM_VERSION_STRING);
  DSMLOG_NOTICE("compiled on %s at %s\n", __DATE__, __TIME__);


  numboards = 1;
  error = -ENOMEM;
  boardInfo = rtl_gpos_malloc( numboards * sizeof(struct MESA_Board) );
  if (!boardInfo) goto err;

  /* initialize each AnythingIO board structure */
  for (ib = 0; ib < numboards; ib++)
  {
    struct MESA_Board * brd = boardInfo + ib;

    // initialize structure to zero, then initialize things
    // that are non-zero
    memset(brd, 0, sizeof(struct MESA_Board));

    brd->counter_rate = IRIG_NUM_RATES;
    brd->radar_rate = IRIG_NUM_RATES;
    brd->twoSixty_rate = IRIG_NUM_RATES;
    brd->outfd = -1;
  }

  struct MESA_Board * brd = 0;
  for (ib = 0; ib < numboards; ib++)
  {
    brd = boardInfo + ib;
    error = -EBUSY;
    unsigned long addr = SYSTEM_ISA_IOPORT_BASE + ioport;

    // Get the mapped board address
    if (check_region(addr, MESA_REGION_SIZE)) {
      DSMLOG_ERR("ioport at 0x%x already in use\n", addr);
      goto err;
    }

    // reserve the ISA memory region
    request_region(addr, MESA_REGION_SIZE, "mesa");
    brd->addr = addr;

    /* Open up my ioctl FIFOs, register my ioctlCallback function */
    error = -EIO;
    DSMLOG_NOTICE("devprefix:      %s\n",   devprefix);
    DSMLOG_NOTICE("ioctlCallback:  0x%x\n", ioctlCallback);
    DSMLOG_NOTICE("ib:             %d\n",   ib);
    DSMLOG_NOTICE("nioctlcmds:     %d\n",   nioctlcmds);
    DSMLOG_NOTICE("ioctlcmds:      0x%x\n", ioctlcmds);
    DSMLOG_NOTICE("ioport:         0x%x\n", ioport);

    brd->ioctlhandle = openIoctlFIFO(devprefix, ib, ioctlCallback,
                              nioctlcmds, ioctlcmds);
    DSMLOG_NOTICE("ioctlhandle:    0x%x\n", brd->ioctlhandle);
    if (!brd->ioctlhandle) goto err;

    // create its output FIFO
    brd->fifoName = createFifo("_in_", ib);
    DSMLOG_NOTICE("fifoName:       %s\n", brd->fifoName);
    if (brd->fifoName == 0)
    {
      error = -convert_rtl_errno(rtl_errno);
      goto err;
    }
  }

  DSMLOG_DEBUG("complete.\n");
  return 0;	// success

err:
  if (brd)
  {
    if (brd->addr)
      release_region(brd->addr, MESA_REGION_SIZE);
    if (brd->ioctlhandle)
      closeIoctlFIFO(brd->ioctlhandle);
  }
  rtl_gpos_free(boardInfo);
  boardInfo = 0;
  return error;
}
