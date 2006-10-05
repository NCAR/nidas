//#define FAKE        // activate a thread to generate fake data
//#define IRIGLESS    // run without the IRIG card
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $

    RTLinux LAMS driver for the ADS3 DSM.

    Much of this was taken from linux serial driver:
    	linux/drivers/char/serial.c

 ********************************************************************
*/

#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_pthread.h>
#include <rtl_core.h>
#include <rtl_unistd.h>
#include <rtl_time.h>
#include <rtl_posixio.h>
#include <rtl_stdio.h>
#include <linux/init.h>          // module_init, module_exit
#include <linux/ioport.h>

#include <nidas/rtlinux/dsmlog.h>
#include <nidas/rtlinux/dsm_viper.h>
#include <nidas/rtlinux/rtl_isa_irq.h>
#include <nidas/rtlinux/irigclock.h>
#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/rtlinux/dsm_version.h>


#include <linux/kernel.h>
#include <linux/termios.h>
#include <nidas/rtlinux/dsm_lams.h>

// #define SERIAL_DEBUG_AUTOCONF

RTLINUX_MODULE(dsm_lams);

/* Number of boards this module can support a one time */
#define MAX_NUM_BOARDS 3

/* Maximum number of ports on a board */
#define MAX_NUM_PORTS_PER_BOARD 1

/* Maximum number of ports on a board */
#define MAX_NUM_PORTS_TOTAL 3

/* how many lams cards are we supporting. Determined at init time
 * by checking known board types.
 */
static int numboards = 1;

/* type of board, from dsm_serial.h.  BOARD_UNKNOWN means board doesn't exist */
//static int brdtype[MAX_NUM_BOARDS] = { BOARD__LAMS, BOARD_UNKNOWN};
  static int brdtype = 1;

/* IRQs of each port */
static int irq_param[MAX_NUM_BOARDS] = { 4,0,0 };
MODULE_PARM(irq_param, "1-" __MODULE_STRING(MAX_NUM_BOARDS) "i");
MODULE_PARM_DESC(irq_param, "IRQ number");

MODULE_PARM(brdtype, "1-" __MODULE_STRING(MAX_NUM_BOARDS) "i");
MODULE_PARM_DESC(brdtype, "type of each board, see dsm_lams.h");

MODULE_AUTHOR("Mike Spowart <spowart@ucar.edu>");
MODULE_DESCRIPTION("LAMS ISA driver for RTLinux");

static struct lamsBoard *boardInfo = 0;

enum flag {TRUE, FALSE};

static const char* devprefix = "lams";
static char * fifoName;

// Define IOCTLs
static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,  _IOC_SIZE(GET_NUM_PORTS) },
  { AIR_SPEED,      _IOC_SIZE(AIR_SPEED)     },
  { LAMS_SET,       _IOC_SIZE(LAMS_SET)      },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);
unsigned int airspeed;
static int lams_channels;
static struct ioctlHandle* ioctlhandle = 0;
static char requested_region = 0;

void cleanup_module (void);

/* Set the base address of the LAMS card */
volatile unsigned long phys_membase;
volatile unsigned long baseadd = LAMS_BASE;
MODULE_PARM(baseadd, "1l");
MODULE_PARM_DESC(baseadd, "ISA memory base");

/* File pointers to data and command FIFOs */
static int fd_lams_data[N_CHANNELS];

/************************************************************************/
/* -- CREATE FIFO NAME ------------------------------------------------ */
static char * createFifo(char inName[], int chan)
{
  char * devName;

  // create its output FIFO
  devName = makeDevName(devprefix, inName, chan);
  DSMLOG_DEBUG("rtl_mkfifo %s\n", devName);

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

/************************************************************************/
// the sensor gathers 512 words of spectral data at the rate of 38900 spectra
// every 1/10th of a sec.
// The sensor's processor averages 256 wspectra
/* -- IRQ HANDLER ----------------------------------------------------- */
static unsigned int dsm_irq_handler(unsigned char chn,
                                    struct lamsPort* lams)
{
  static char glyph = 0;
  unsigned int dump, n, i, n5;
  unsigned int lams_flags;

#define FIFO_EMPTY               0x1
#define FIFO_HALF_FULL           0x2
#define FIFO_FULL                0x4

//while (readw(LAMS_BASE + FLAGS_OFFSET) == 0x6);

/*
  for (i=0; i<512; i++) {
    dump = readw(LAMS_BASE + DATA_OFFSET);
    if (dump == LAMS_PATTERN) {
//    n5++;
//    if (n5n1000++ > 1000) {
        DSMLOG_DEBUG("i=%03d n5=%03d\n", i, n5);
//      n5n1000=0;
//    }
    }
  }
*/
      // flush the hardware data FIFO
      n5 = 0;
      n = 0;
      lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
      while (! (lams_flags & FIFO_EMPTY) ) {  
        dump       = readw(LAMS_BASE + DATA_OFFSET);
        lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
        if (dump == LAMS_PATTERN) n5++;
        n++;
      }
      DSMLOG_DEBUG("%d n5=%3d flushed %4d words\n", glyph++, n5, n);
      if (glyph==4) glyph=0;

#if 0
  unsigned int count = 0;
  unsigned int dump, lams_flags = 0;
  static   int num_arrays = 0;
  unsigned long lidar_data[MAX_BUFFER];

  // discard all data up to the pattern
  while (1) {
    lidar_data[0] = readw(LAMS_BASE + DATA_OFFSET);
    if (lidar_data[0] == LAMS_PATTERN) break;
    lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
    if (lams_flags & FIFO_EMPTY) break; 
//  DSMLOG_DEBUG("data=0x%x\n",lams.data[0]);
  }
  if (lidar_data[0] != LAMS_PATTERN) return 1;
  if (lams_flags & FIFO_EMPTY) return 1; 

  for (count = 1; count <= 255; count++) {
    lidar_data[count] += (unsigned long) readw(LAMS_BASE + DATA_OFFSET);
  }
  if (++num_arrays == NUM_ARRAYS) {
    for(count = 1; count <= 255; count++){
      lams->data[count] = (unsigned int) (lidar_data[count] / NUM_ARRAYS);
      lidar_data[count] = 0;
    }
    num_arrays = 0;
  }
  lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
  if (lams_flags & FIFO_HALF_FULL){  
    for (count = 0; count <= 255; count++){
      dump = readw(LAMS_BASE + DATA_OFFSET);
    }
  }
#ifndef IRIGLESS
  lams->timetag = GET_MSEC_CLOCK;
#endif
  DSMLOG_DEBUG("\n",lams.data[0]);
  rtl_write(fd_lams_data[chn], &lams, sizeof(lams));
#endif // 0
  return 0;
}

static char irqBusy = 0;

/************************************************************************/
/* -- IRQ HANDLER ----------------------------------------------------- */
unsigned int dsm_lams_irq_handler(unsigned int irq,
	void* callbackptr, struct rtl_frame *regs)
{
  struct lamsPort* lams = 0;
  int retval = 0;

  irqBusy = 1;
  rtl_spin_lock(&lams->lock);
  retval = dsm_irq_handler(irq,lams);
  rtl_spin_unlock(&lams->lock);
  irqBusy = 0;

  if (retval) return retval;
  return 0;
}

#ifdef FAKE
/* -- LAMS DEBUG CALLBACK --------------------------------------------- */
#ifdef IRIGLESS
static rtl_pthread_t fakeThread = 0;
#endif
static void* fake_thread(void * chan)
{
  unsigned int lams_flags, dump, n, n5;
  static char glyph = 0;

#ifdef IRIGLESS
  while (1) {
  rtl_usleep(1000000);
#endif
  lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
  dump       = readw(LAMS_BASE + DATA_OFFSET);
  DSMLOG_DEBUG("%d lams_flags = 0x%04x   dump = 0x%04x\n", glyph++, lams_flags, dump);
  if (glyph==4) glyph=0;
/*
//if (lams_flags != 0x6)
//  DSMLOG_DEBUG("%d lams_flags = 0x%x\n", glyph++, lams_flags);
  if (irqBusy) return;

  // flush the hardware data FIFO
  n  = 0;
  n5 = 0;
//while ( (lams_flags != FIFO_EMPTY) ) {  
  while ( !(lams_flags & FIFO_EMPTY) ) {  
    lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
    dump       = readw(LAMS_BASE + DATA_OFFSET);
    n++;
    if (dump == 0x5555) n5++;
  }
  DSMLOG_DEBUG("%d lams_flags=0x%04x flushed %4d words n5=%d\n", glyph, lams_flags, n, n5);
*/
#ifdef IRIGLESS
  }
#endif
}
#endif // FAKE

/************************************************************************/
/* -- IOCTL CALLBACK -------------------------------------------------- */
/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 */
static int ioctlCallback(int cmd, int board, int chn, void *buf, rtl_size_t len)
{
  int ret = len;
  char devstr[30];
  struct lams_set* lams_ptr;
  unsigned int dump, lams_flags, n; 

  switch (cmd)
  {
    case GET_NUM_PORTS:         /* user get */
      DSMLOG_DEBUG("GET_NUM_PORTS\n");
      *(int *) buf = N_CHANNELS;
      break;

    case AIR_SPEED:              // send the air speed to the board
      DSMLOG_DEBUG("AIR_SPEED\n");
      airspeed = *(unsigned int*) buf;
//    writew(airspeed, LAMS_BASE + AIR_SPEED_OFFSET);
      break;

    case LAMS_SET:
      DSMLOG_DEBUG("LAMS_SET\n");
      lams_ptr = (struct lams_set*) buf;
      lams_channels = lams_ptr->channel;
      DSMLOG_DEBUG("LAMS_SET lams_channels=%d\n", lams_channels);

      /* open the channels data FIFO */
      for (n=0; n < lams_channels; n++)
      {
        sprintf( devstr, "%s/lams_in_%d", getDevDir(), n);
        if ((fd_lams_data[n] =
          rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_WRONLY )) < 0) {
          DSMLOG_ERR("error: open %s: %s\n", devstr,
                     rtl_strerror(rtl_errno));
          return -convert_rtl_errno(rtl_errno);
        }
        DSMLOG_DEBUG("LAMS_SET fd_lams_data[%d]=0x%x '%s'\n", n,
                     fd_lams_data[n], devstr);
      }
      // flush the hardware data FIFO
      DSMLOG_DEBUG("flushing the hardware data FIFO\n");
      for  (n=0; n<512; n++)
        readw(LAMS_BASE + DATA_OFFSET);
      n=0;
      lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
      while (! (lams_flags & FIFO_EMPTY) ) {  
        dump =       readw(LAMS_BASE + DATA_OFFSET);
        lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
        n++;
      }
      DSMLOG_DEBUG("the hardware data FIFO is flushed\n");
#ifndef FAKE
      // activate interupt service routine
      DSMLOG_DEBUG("activate interupt service routine.  irq=%d chn=%d\n", boardInfo[chn].irq, chn);
      if ((rtl_request_isa_irq(boardInfo[chn].irq,dsm_lams_irq_handler, boardInfo)) < 0) {
        rtl_free_isa_irq(boardInfo[chn].irq); 
        DSMLOG_DEBUG("rtl_free_isa_irq(%d) ok\n", boardInfo[chn].irq);
      }
#else
      DSMLOG_DEBUG("DON'T activate interupt service routine.\n");
#endif // FAKE
      // flush the hardware data FIFO
      DSMLOG_DEBUG("flushing the hardware data FIFO again\n");
      for  (n=0; n<512; n++)
        readw(LAMS_BASE + DATA_OFFSET);
      n=0;
      lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
      while (! (lams_flags & FIFO_EMPTY) ) {  
        dump =       readw(LAMS_BASE + DATA_OFFSET);
        lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
        n++;
      }
      DSMLOG_DEBUG("again the hardware data FIFO is flushed\n");
      break;

    default:
      ret = -RTL_EIO;
      break;
  }
  return ret;
}
/*
static struct rtl_file_operations rtl_dsm_ser_fops = {
    read:           rtl_dsm_lams_read,
    write:          rtl_dsm_lams_write,
    ioctl:          ioctlCallback,
    open:           rtl_dsm_lams_open,
    release:        rtl_dsm_lams_release,
    install_poll_handler: rtl_dsm_lams_poll_handler,
};
*/
/************************************************************************/
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  char devstr[30];
  int  chn;

#ifdef FAKE
  // stop the fake thread
#ifdef IRIGLESS
  DSMLOG_DEBUG("delete the irig-less thread that generates fake data");
  if (fakeThread) {
    if (rtl_pthread_kill( fakeThread,SIGTERM ) < 0)
      DSMLOG_ERR("rtl_pthread_kill failure: %s\n", rtl_strerror(rtl_errno));
    rtl_pthread_join( fakeThread, NULL );
  }
#else
  DSMLOG_DEBUG("delete the irig callback that generates fake data");
  unregister_irig_callback(&fake_thread, IRIG_1_HZ, 0);
#endif // IRIGLESS
#endif // FAKE

  /* close and unlink eack data port... */
  for(chn=0; chn < lams_channels; chn++)
  { 
    if (fd_lams_data[chn]) {
      rtl_close( fd_lams_data[chn] );
      DSMLOG_DEBUG("closed fd_lams_data[%d]\n", chn);
    }
  }

  /* destroy the data fifos */
  for (chn=0; chn<lams_channels; chn++)
  {
    sprintf( devstr, "%s/lams_in_%d", getDevDir(), chn);
    rtl_unlink( devstr );
  }
  /* Close my ioctl FIFO, deregister my lams_ioctl function */
  if (ioctlhandle)
    closeIoctlFIFO(ioctlhandle);

  /* Free up the ISA memory region */
  release_region(baseadd, LAMS_REGION_SIZE);

  DSMLOG_DEBUG("cleanup_module\n");
}
/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  int chn;
  int   error = -EINVAL;

  // DSM_VERSION_STRING is found in dsm_version.h
  DSMLOG_NOTICE("version: %s\n", DSM_VERSION_STRING);
  DSMLOG_NOTICE("compiled on %s at %s\n", __DATE__, __TIME__);

  boardInfo = rtl_gpos_malloc( numboards * sizeof(struct lamsBoard) );
  DSMLOG_NOTICE("boardInfo:      0x%x\n", boardInfo);
  if (!boardInfo) return -RTL_EIO;

  for (chn = 0; chn < numboards; chn++) {
      boardInfo[chn].type = brdtype;
      boardInfo[chn].addr = 0;
      boardInfo[chn].irq = irq_param[chn];
      boardInfo[chn].numports = 0;
      boardInfo[chn].int_mask = 0;
  }

  /* open up ioctl FIFO, register lams_ioctl function */
  DSMLOG_NOTICE("devprefix:      %s\n", devprefix);
  DSMLOG_NOTICE("ioctlCallback:  0x%x\n", ioctlCallback);
  DSMLOG_NOTICE("BOARD_NUM:      %d\n", BOARD_NUM);
  DSMLOG_NOTICE("nioctlcmds:     %d\n", nioctlcmds);
  DSMLOG_NOTICE("ioctlcmds:      0x%x\n", ioctlcmds);

  ioctlhandle = openIoctlFIFO(devprefix, BOARD_NUM, ioctlCallback,
                              nioctlcmds, ioctlcmds);
  DSMLOG_NOTICE("ioctlhandle:    0x%x\n", ioctlhandle);
  if (!ioctlhandle) return -RTL_EIO;

//  for (chn=0; chn<N_CHANNELS; chn++)
//  {
  chn = 0;

  // create its output FIFO
  fifoName = createFifo("_in_", BOARD_NUM);
  DSMLOG_NOTICE("fifoName:       %s\n", fifoName);
  if (fifoName == 0)
  {
    error = -convert_rtl_errno(rtl_errno);
    goto err;
  }
//  }
  /* reserve the ISA memory region */
  if (!request_region(baseadd, LAMS_REGION_SIZE, "lams"))
  {
    DSMLOG_DEBUG("%s: couldn't allocate I/O range %x - %x\n", __FILE__, baseadd,
        baseadd + LAMS_REGION_SIZE - 1);
    cleanup_module();
    return -RTL_EBUSY;
  }
  requested_region = 1;

#ifdef FAKE
  // DEBUG create an irig callback to generate fake data
#ifdef IRIGLESS
  DSMLOG_DEBUG("create an irig-less thread to generate fake data");
  if (rtl_pthread_create( &fakeThread, NULL, 
                          fake_thread, (void *)0)) {
    DSMLOG_ERR("rtl_pthread_create failure: %s\n", rtl_strerror(rtl_errno));
    return -convert_rtl_errno(rtl_errno);
  }
#else
  DSMLOG_DEBUG("create an irig callback to generate fake data");
  register_irig_callback(&fake_thread, IRIG_1_HZ, 0);
#endif
#endif // FAKE

  DSMLOG_DEBUG("complete.\n");
  return 0; //success

err:
  release_region(baseadd, LAMS_REGION_SIZE);
  if (ioctlhandle)
    closeIoctlFIFO(ioctlhandle);
  rtl_gpos_free(boardInfo);
  boardInfo = 0;
  return error;
}
