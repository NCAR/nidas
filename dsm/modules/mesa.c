/* mesa.c

   RTLinux module for interfacing the Mesa Electronics
   4I34(M) Anything I/O FPGA card.

   Original Author: Mike Spowart

   Copyright by the National Center for Atmospheric Research

   Revisions:

*/

/* RTLinux includes...  */
#include <rtl.h>
#include <rtl_posixio.h>
#include <stdio.h>
#include <unistd.h>

/* Linux module includes... */
#include <linux/ioport.h>
#include <bits/posix1_lim.h>

#include <mesa.h>
#include <irigclock.h>

RTLINUX_MODULE(mesa);
MODULE_AUTHOR("Mike Spowart");
MODULE_DESCRIPTION("4I34 driver for RTLinux");

/* Define IOCTLs */
static struct ioctlCmd ioctlcmds[] = {
  { MESA_LOAD,    _IOC_SIZE(MESA_LOAD   ) },
  { COUNTERS_SET, _IOC_SIZE(COUNTERS_SET) },
  { RADAR_SET,    _IOC_SIZE(RADAR_SET   ) },
  { PMS260X_SET,  _IOC_SIZE(PMS260X_SET ) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);
static struct ioctlHandle* ioctlhandle = 0;

static int counter_channels = 0;
static int radar_channels = 0;
enum flag {TRUE, FALSE};
enum flag prog_done = FALSE;

/* Set the base address of the Mesa 4I34 card */
volatile unsigned int phys_membase;
static volatile unsigned int baseadd = MESA_BASE;
MODULE_PARM(baseadd, "1l");
MODULE_PARM_DESC(baseadd, "ISA memory base (default 0xf7000220)");

/* global variables */
extern int ptog;  // this variable is toggled every second

#define err(format, arg...) \
     printk(KERN_ERR  "%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

/* File pointers to data and command FIFOs */
static int fd_mesa_load;
static int fd_mesa_counter[N_COUNTERS][2];
static int fd_mesa_radar[N_RADARS][2];
static struct rtl_sigaction cmndAct[N_MESA];

/* -- IRIG CALLBACKS--------------------------------------------------- */
void read_counter(void* channel)
{
  short chn = (int) channel;
  unsigned int counts;
  int read_address_offset;

  if (chn == 1)
    read_address_offset = COUNT0_READ_OFFSET;
  else
    read_address_offset = COUNT1_READ_OFFSET;

  /* read from the counter channel */
  if ((counts = readw(MESA_BASE + read_address_offset)) == 0)
    rtl_printf("NO COUNTS\n");

  /* write the counts to the user's FIFO */
  write(fd_mesa_counter[chn][ptog], &counts, sizeof(unsigned int));
}
/*************************************************************************/
void read_radar(void* channel)
{
  short chn = (int) channel;
  unsigned int altitude;

  /* read from the radar channel */
  if ((altitude = readw(MESA_BASE + RADAR_READ_OFFSET)) <= 0)
    rtl_printf("BAD ALTITUDE\n");

  /* write the altitude to the user's FIFO */
  write(fd_mesa_radar[chn][ptog], &altitude, sizeof(unsigned int));

}
/************************************************************************/
int load_start()
{
  int count;
  unsigned char config, status;

  config = M_4I34CFGCSOFF | M_4I34CFGINITASSERT |
          M_4I34CFGWRITEDISABLE | M_4I34LEDOFF;
  outb(config,MESA_BASE + R_4I34CONTROL);
  status = inb(MESA_BASE + R_4I34STATUS);

  /* Note that if we see DONE at the start of programming, it's most likely due
     to an attempt to access the 4I34 at the wrong I/O location. */
  if (status & M_4I34PROGDUN) return -EFAULT;

  config = M_4I34CFGCSON | M_4I34CFGINITDEASSERT |
           M_4I34CFGWRITEENABLE | M_4I34LEDON;
  outb(config,MESA_BASE + R_4I34CONTROL);

  /* Delay 100 uS. */
  for(count=0; count <= 1000; count++)
  {
    status = inb(MESA_BASE + R_4I34STATUS);
  }
  return 0;
}
/************************************************************************/
void load_program(int sig, rtl_siginfo_t *siginfo, void *v)
{
  static char buf[1024];
  static unsigned int len;
  unsigned char *bptr;

  /* read the FIFO into a buffer */
  len += read(siginfo->rtl_si_fd, &buf[len], SSIZE_MAX);

  if(len == filesize){                /* Now program the FPGA */
    bptr = buf;
    do {
      outb(*bptr++, MESA_BASE + R_4I34DATA);
    } while(--len != 0);
    prog_done = TRUE;
    len = 0;
  }
}
/************************************************************************/
int load_finish()
{
  int ret= 0, count, waitcount;
  unsigned char config;
  enum flag success;

  config = M_4I34CFGCSOFF | M_4I34CFGINITDEASSERT |
           M_4I34CFGWRITEDISABLE | M_4I34LEDON;
  outb(config,MESA_BASE + R_4I34CONTROL);

  /* Wait for Done bit set */
  success = FALSE;
  for(waitcount = PROGWAITLOOPCOUNT; waitcount != 0; --waitcount)
  {
    if(inb((MESA_BASE + R_4I34STATUS) & M_4I34PROGDUN)){
      success = TRUE;
      continue;
    }
  }
  if(success) {
    rtl_printf("FPGA programming done\n");
    ret = 0;

    /* Indicate end of programming by turning off 4I34 led */
    config = M_4I34CFGCSOFF | M_4I34CFGINITDEASSERT |
             M_4I34CFGWRITEDISABLE | M_4I34LEDOFF;
    outb(config,MESA_BASE + R_4I34CONTROL);

    /* Now send out extra configuration completion clocks */
    for(count = 24; count != 0; --count) {
      outb(0xFF, MESA_BASE + R_4I34DATA);
    }
  }
  else {
    rtl_printf("FPGA programming not successful\n");
/*    cleanup_module();  */
    ret = -1;
  }
  return ret;
}
/************************************************************************/
/* -- IOCTL CALLBACK -------------------------------------------------- */
/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 */
static int mesa_ioctl(int cmd, int board, int port, void *buf, size_t len)
{
  int j, tog, ret = len;
  char devstr[30];
  struct radar_set* radar_ptr; 
  struct counters_set* counter_ptr; 

  switch (cmd)
  {
    case GET_NUM_PORTS:		/* user get */
      err("GET_NUM_PORTS");
      *(int *) buf = 4;
      break;

    case MESA_LOAD:
      filesize = (int) buf;
      if(load_start() != 0)
      {
        /* Now wait for the FPGA to be programmed */
        while(!prog_done);
        prog_done = FALSE;

        ret = load_finish();
      }
      else
        ret = -1;
      break;

    case COUNTERS_SET:
      /* create and open toggled (2 each) counter data FIFOs */
      counter_ptr = (struct counters_set*) buf;
      counter_channels = counter_ptr->channel;
      for (tog=0; tog < 2; tog++)
      {
        for (j=0; j < counter_channels; j++)
        {
          sprintf(devstr, "/dev/mesa_counter_%d_read_%d", j, tog);
          mkfifo( devstr, 0666 );
          fd_mesa_counter[j][tog] = open( devstr, O_NONBLOCK | O_WRONLY );
        }
      }

      /* register poll routine with the IRIG driver */
      register_irig_callback(&read_counter, counter_ptr->rate, (void *)counter_channels);
      ret = 0;        
      break;

    case RADAR_SET:
      radar_ptr = (struct radar_set*) buf;
      radar_channels = radar_ptr->channel;
      /* create and open toggled radar data FIFOs */
      for (tog=0; tog < 2; tog++)
      {
        for (j=0; j < radar_channels; j++)
        {
          sprintf(devstr, "/dev/mesa_radar_%d_read_%d", j, tog);
          mkfifo( devstr, 0666 );
          fd_mesa_radar[j][tog] = open( devstr, O_NONBLOCK | O_WRONLY );
        }
      }
            
      /* register poll routine with the IRIG driver */
      register_irig_callback(&read_radar, radar_ptr->rate, (void *)radar_channels);
      ret = 0;        
      break;

    case PMS260X_SET:
      ret = 0;        
      break;
   
    default:
      ret = -1;        
      break;
  }
  return ret;
}
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  char devstr[30];
  int  j, tog;

  /* Close my ioctl FIFO, deregister my mesa_ioctl function */
  closeIoctlFIFO(ioctlhandle);

  /* for each Mesa counter... */
  if(counter_channels > 0)
  {
    /* close and unlink its toggled output FIFOs */
    for (tog=0; tog < 2; tog++)
    {
      for(j=0; j < counter_channels; j++)
      {
        sprintf(devstr, "/dev/mesa_counter_%d_read_%d", j, tog);
        close( fd_mesa_counter[j][tog] );
        unlink( devstr );
      }
    }
  }

  /* Radar port... */
  if(radar_channels > 0)
  {
    /* close and unlink its input FIFO */
    for (tog=0; tog < 2; tog++)
    {
      for(j=0; j < radar_channels; j++)
      {
        sprintf(devstr, "/dev/mesa_radar_%d_read_%d", j, tog);
        close( fd_mesa_radar[j][tog] );
        unlink( devstr );
      }
    }
  }
  /* Free up the ISA memory region */
  release_region(baseadd, MESA_REGION_SIZE);

  rtl_printf("(%s) cleanup_module\n\n", __FILE__);
}
/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  char devstr[30];
  int chn;

  rtl_printf("(%s) init_module: compiled on %s at %s\n\n",
             __FILE__, __DATE__, __TIME__);

  /* open up ioctl FIFO, register mesa_ioctl function */
  ioctlhandle = openIoctlFIFO("mesa", BOARD_NUM, mesa_ioctl,
                              nioctlcmds, ioctlcmds);
  if (!ioctlhandle) return -EIO;

  for (chn=0; chn < BOARD_NUM; chn++)
  {

    /* Make 4I34 board Ioctl Fifo... */
    sprintf(devstr, "/dev/mesa_program_board");
    mkfifo( devstr, 0666 );
    fd_mesa_load = open( devstr, O_NONBLOCK | O_RDONLY );

    /* create FIFO handler */
    cmndAct[chn].sa_sigaction = load_program;
    cmndAct[chn].sa_fd        = fd_mesa_load;
    cmndAct[chn].sa_flags     = RTL_SA_RDONLY | RTL_SA_SIGINFO;
    rtl_sigaction( RTL_SIGPOLL, &cmndAct[chn], NULL );
  }

  /* reserve the ISA memory region */
  if (!request_region(baseadd, MESA_REGION_SIZE, "mesa"))
  {
    err("%s: couldn't allocate I/O range %x - %x", __FILE__, baseadd,
        baseadd + MESA_REGION_SIZE - 1);
    cleanup_module();
    return -EBUSY;
  }
  rtl_printf("(%s) init_module: loaded\n\n", __FILE__);
  return 0; /* success */
}
