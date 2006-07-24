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
#include <linux/ioport.h>
#include <linux/termios.h>
#include <nidas/rtlinux/dsm_lams.h>

// #define SERIAL_DEBUG_AUTOCONF

RTLINUX_MODULE(dsm_lams);

//static char* devprefix = "dsmser";

/* Number of boards this module can support a one time */
#define MAX_NUM_BOARDS 3

/* Maximum number of ports on a board */
#define MAX_NUM_PORTS_PER_BOARD 1

/* Maximum number of ports on a board */
#define MAX_NUM_PORTS_TOTAL 3

#define err(format, arg...) \
     rtl_printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

/* how many lams cards are we supporting. Determined at init time
 * by checking known board types.
 */
static int numboards = 1;

/* type of board, from dsm_serial.h.  BOARD_UNKNOWN means board doesn't exist */
//static int brdtype[MAX_NUM_BOARDS] = { BOARD__LAMS, BOARD_UNKNOWN};
  static int brdtype = 1;
/* default ioport addresses */


/* IRQs of each port */
static int irq_param[MAX_NUM_BOARDS] = { 11,0,0 };
MODULE_PARM(irq_param, "1-" __MODULE_STRING(MAX_NUM_BOARDS) "i");
MODULE_PARM_DESC(irq_param, "IRQ number");

MODULE_PARM(brdtype, "1-" __MODULE_STRING(MAX_NUM_BOARDS) "i");
MODULE_PARM_DESC(brdtype, "type of each board, see dsm_lams.h");

MODULE_PARM(ioport, "1-" __MODULE_STRING(MAX_NUM_BOARDS) "i");
MODULE_PARM_DESC(ioport, "IOPORT address of each lams card");

static struct lamsBoard *boardInfo = 0;

unsigned int dsm_lams_irq_handler(unsigned int irq,
	void* callbackptr, struct rtl_frame *regs);

enum flag {TRUE, FALSE};

/* Define IOCTLs */
static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_BOARDS, _IOC_SIZE(GET_NUM_BOARDS) },
  { AIR_SPEED,      _IOC_SIZE(AIR_SPEED)      },
  { LAMS_SET,       _IOC_SIZE(LAMS_SET)       },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);
static int fd_lams_load_speed;
unsigned int airspeed;
static int lams_channels;
static struct ioctlHandle* ioctlhandle = 0;
static struct rtl_sigaction cmndAct[N_LAMS];
static char requested_region = 0;

void cleanup_module (void);

/* Set the base address of the LAMS card */
volatile unsigned long phys_membase;
static volatile unsigned long baseadd = LAMS_BASE;
MODULE_PARM(baseadd, "1l");
MODULE_PARM_DESC(baseadd, "ISA memory base (default 0xf7000220)");

/* File pointers to data and command FIFOs */
static int fd_lams_data[N_CHANNELS];

static unsigned int dsm_irq_handler(unsigned char chn, struct lamsPort* lams)
{
  unsigned int count = 0;
  unsigned int dump, lams_flags = 0;
  static int num_arrays = 0;
  unsigned int lidar_data[MAX_BUFFER];

  for (;;) {
    lidar_data[0] = readw(LAMS_BASE + DATA_OFFSET);
    if (lidar_data[0] == LAMS_PATTERN) break;
    lams_flags = readw(LAMS_BASE + FLAGS_OFFSET);
    if (lams_flags & FIFO_EMPTY) break; 
//#ifdef DEBUG
//    rtl_printf("data=0x%x\n",lams.data[0]);
//#endif
  }
  if (lidar_data[0] != LAMS_PATTERN) return 1;
  if (lams_flags & FIFO_EMPTY) return 1; 

  for (count = 1; count <= 255; count++){
    lidar_data[count++] += (unsigned int)readw(LAMS_BASE + DATA_OFFSET);
  }
  num_arrays += 1;
  if (num_arrays == NUM_ARRAYS) {
    for(count = 1; count <= 255; count++){
      lams->data[count] = lidar_data[count]/NUM_ARRAYS;
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
  lams->timetag = GET_MSEC_CLOCK;
  rtl_write(fd_lams_data[chn], &lams, sizeof(lams));
  return 0;
}

unsigned int dsm_lams_irq_handler(unsigned int irq,
	void* callbackptr, struct rtl_frame *regs)
{
  struct lamsPort* lams = 0;
  int retval = 0;

#ifdef DEBUG
  rtl_printf("dsm_irq_handler entered\n");
#endif

  rtl_spin_lock(&lams->lock);
  retval = dsm_irq_handler(irq,lams);
  rtl_spin_unlock(&lams->lock);

  if (retval) return retval;
  return 0;
}

/************************************************************************/
/* -- IOCTL CALLBACK -------------------------------------------------- */
/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 */
static int rtl_dsm_lams_ioctl(int cmd,int board,int chn,void *buf,
           rtl_size_t len)
{
  int ret = len;
  char devstr[30];
  int j;
  struct lams_set* lams_ptr;

  switch (cmd)
  {
    case GET_NUM_BOARDS:         /* user get */
      err("NUM_CHANNELS");
      *(int *) buf = N_CHANNELS;
      ret = 0;
      break;

    case AIR_SPEED:
      err("AIR_SPEED");
      airspeed = *(unsigned int*) buf;
      ret = 0;
      break;

    case LAMS_SET:
      err("LAMS_SET");
      /* create and open LAMS data FIFO */
      lams_ptr = (struct lams_set*) buf;
      lams_channels = lams_ptr->channel;
      for (j=0; j < lams_channels; j++)
      {
        sprintf(devstr, "/dev/lams_in_%d", j);
        fd_lams_data[j] = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_WRONLY );
        if (fd_lams_data[j] < 0)
          ret = fd_lams_data[j];
      }
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
    ioctl:          rtl_dsm_lams_ioctl,
    open:           rtl_dsm_lams_open,
    release:        rtl_dsm_lams_release,
    install_poll_handler: rtl_dsm_lams_poll_handler,
};
*/
/************************************************************************/
void load_air_speed(int sig, rtl_siginfo_t *siginfo, void *v)
{
  char buf;
  unsigned int speed;
//  unsigned char *bptr;

  /* read the FIFO into a buffer and then and then send to board */
  speed = rtl_read(siginfo->si_fd, &buf, 1);
//  bptr = &buf;
//  outb(*bptr, LAMS_BASE + AIR_SPEED_OFFSET);
  outb(buf, LAMS_BASE + AIR_SPEED_OFFSET);

  rtl_printf("Load airspeedl successful\n");
} 
/************************************************************************/
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  char devstr[30];
  int  chn;

  /* close and destroy the lams load airspeed fifo */
  sprintf(devstr, "/dev/lams_air_speed");
  err("closing '%s' @ 0x%x", devstr, fd_lams_load_speed);
  if (fd_lams_load_speed)
    rtl_close( fd_lams_load_speed );
  rtl_unlink( devstr );

  /* close and unlink eack data port... */
  for(chn=0; chn < lams_channels; chn++)
  { 
    if (fd_lams_data[chn]) {
      rtl_close( fd_lams_data[chn] );
      err("closed fd_lams_data[%d]", chn);
    }
  }

  /* destroy the data fifos */
  for (chn=0; chn<lams_channels; chn++)
  {
    sprintf(devstr, "/dev/lams_in_%d", chn);
    rtl_unlink( devstr );
  }
  /* Close my ioctl FIFO, deregister my lams_ioctl function */
  closeIoctlFIFO(ioctlhandle);

  /* Free up the ISA memory region */
  release_region(baseadd, LAMS_REGION_SIZE);

  rtl_printf("(%s) cleanup_module\n\n", __FILE__);
}
/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  char devstr[30];
  int chn;
  char boardirq;

  boardirq = irq_param[1];

  rtl_printf("(%s) init_module: compiled on %s at %s\n\n",
             __FILE__, __DATE__, __TIME__);

  boardInfo = rtl_gpos_malloc( numboards * sizeof(struct lamsBoard) );
  for (chn = 0; chn < numboards; chn++) {
      boardInfo[chn].type = brdtype;
      boardInfo[chn].addr = 0;
      boardInfo[chn].irq = 0;
      boardInfo[chn].numports = 0;
      boardInfo[chn].int_mask = 0;
  }

  /* open up ioctl FIFO, register lams_ioctl function */
  ioctlhandle = openIoctlFIFO("lams", BOARD_NUM, rtl_dsm_lams_ioctl,
                              nioctlcmds, ioctlcmds);
  if (!ioctlhandle) return -RTL_EIO;

//  for (chn=0; chn<N_CHANNELS; chn++)
//  {
  chn = 0;
    /* create its output FIFO */
  sprintf( devstr, "/dev/lams_in_%d", chn );
    // remove broken device file before making a new one
  rtl_unlink(devstr);
  if ( rtl_errno != -RTL_ENOENT ) return -rtl_errno;
  err("rtl_mkfifo( %s, 0666 );", devstr);
  rtl_mkfifo( devstr, 0666 );
  if ((rtl_request_isa_irq(boardirq,dsm_lams_irq_handler,
         boardInfo)) < 0)rtl_free_isa_irq(boardInfo[chn].irq); 
//  }

  /* Make LAMS board Ioctl Fifo... */
  sprintf(devstr, "/dev/lams_air_speed");
  // remove broken device file before making a new one
  rtl_unlink(devstr);
  if ( rtl_errno != -RTL_ENOENT ) return -rtl_errno;
  err("rtl_mkfifo( %s, 0666 );", devstr);
  rtl_mkfifo( devstr, 0666 );
  fd_lams_load_speed = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_RDONLY );
  err("opened '%s' @ 0x%x", devstr, fd_lams_load_speed);

  /* create FIFO handler */
  cmndAct[chn].sa_sigaction = load_air_speed;
  cmndAct[chn].sa_fd        = fd_lams_load_speed;
  cmndAct[chn].sa_flags     = RTL_SA_RDONLY | RTL_SA_SIGINFO;
  //rtl_sigaction( RTL_SIGPOLL, &cmndAct[chn], NULL );
  if ( rtl_sigaction( RTL_SIGPOLL, &cmndAct[chn], NULL ) != 0 )
  {
    err("Cannot create FIFO handler for %s", devstr);
    cleanup_module();
    return -RTL_EIO;
  }

  /* reserve the ISA memory region */
  if (!request_region(baseadd, LAMS_REGION_SIZE, "lams"))
  {
//    err("%s: couldn't allocate I/O range %x - %x", __FILE__, baseadd,
//        baseadd + LAMS_REGION_SIZE - 1);
    cleanup_module();
    return -RTL_EBUSY;
  }
  requested_region = 1;
  err("done.\n");

  return 0; //success
}
// MODULE_AUTHOR("Mike Spowart <spowart@ucar.edu>");
// MODULE_DESCRIPTION("LAMS ISA driver for RTLinux");

