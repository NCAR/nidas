/* mesa.c

   RTLinux module for interfacing the Mesa Electronics
   4I34(M) Anything I/O FPGA card.

   Original Author: Mike Spowart

   Copyright by the National Center for Atmospheric Research

   Implementation notes:

   Revisions:

     $LastChangedRevision$
         $LastChangedDate: $
           $LastChangedBy$
                 $HeadURL: $
*/

/* RTLinux includes...  */
#include <rtl.h>
#include <rtl_posixio.h>
#include <stdio.h>
#include <unistd.h>

/* Linux module includes... */
#include <linux/init.h>
#include <linux/ioport.h>
#include <bits/posix1_lim.h>

#include <mesa.h>
#include <irigclock.h>

RTLINUX_MODULE(mesa);

#define err(format, arg...) \
     rtl_printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

/* Define IOCTLs */
static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS, _IOC_SIZE(GET_NUM_PORTS) },
  { COUNTERS_SET,  _IOC_SIZE(COUNTERS_SET ) },
  { RADAR_SET,     _IOC_SIZE(RADAR_SET    ) },
  { PMS260X_SET,   _IOC_SIZE(PMS260X_SET  ) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);
static struct ioctlHandle* ioctlhandle = 0;

static int counter_channels = 0;
static int radar_channels = 0;
enum flag {TRUE, FALSE};

/* Set the base address of the Mesa 4I34 card */
volatile unsigned long phys_membase;
static volatile unsigned long baseadd = MESA_BASE;
MODULE_PARM(baseadd, "1l");
MODULE_PARM_DESC(baseadd, "ISA memory base (default 0xf7000220)");

/* global variables */

/* File pointers to data and command FIFOs */
static int fd_mesa_load;
static int fd_mesa_counter[N_COUNTERS];
static int fd_mesa_radar[N_RADARS];
static struct sigaction cmndAct[N_MESA];

/* -- IRIG CALLBACKS--------------------------------------------------- */
void read_counter(void* channel)
{
  struct dsm_mesa_sample sample;
  short chn = (int) channel;
  int read_address_offset;

  if (chn == 1)
    read_address_offset = COUNT0_READ_OFFSET;
  else
    read_address_offset = COUNT1_READ_OFFSET;

  /* read from the counter channel */
  if ((sample.data = readw(MESA_BASE + read_address_offset)) == 0)
    rtl_printf("NO COUNTS\n");

  /* write the counts to the user's FIFO */
  sample.timetag = GET_MSEC_CLOCK;
  write(fd_mesa_counter[chn], &sample, sizeof(sample));
}
/*************************************************************************/
void read_radar(void* channel)
{
  short chn = (int) channel;
  struct dsm_mesa_sample sample;

  /* read from the radar channel */
  if ((sample.data = readw(MESA_BASE + RADAR_READ_OFFSET)) <= 0)
    rtl_printf("BAD ALTITUDE\n");

  /* write the altitude to the user's FIFO */
  sample.timetag = GET_MSEC_CLOCK;
  write(fd_mesa_radar[chn], &sample, sizeof(sample));

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
  char buf[MAX_BUFFER];
  unsigned long len;
  static unsigned long total = 0;
  unsigned char *bptr;

  /* read the FIFO into a buffer */
  do {
    len = read(siginfo->rtl_si_fd, &buf[total], MAX_BUFFER);
    total += len;
  }while(buf[total] != EOF && !feof(siginfo->rtl_si_fd));

  /* Now program the FPGA */
  bptr = buf;
  do {
    outb(*bptr++, MESA_BASE + R_4I34DATA);
  } while(--total != 0);

  load_finish();
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
  int j, ret = len;
  char devstr[30];
  struct radar_set* radar_ptr; 
  struct counters_set* counter_ptr; 

  switch (cmd)
  {
    case GET_NUM_PORTS:		/* user get */
      err("GET_NUM_PORTS");
      *(int *) buf = N_PORTS;
      break;

    case COUNTERS_SET:
      err("COUNTERS_SET");
      /* create and open counter data FIFOs */
      counter_ptr = (struct counters_set*) buf;
      counter_channels = counter_ptr->channel;
      for (j=0; j < counter_channels; j++)
      {
        sprintf(devstr, "/dev/mesa_in_%d", j);
        fd_mesa_counter[j] = open( devstr, O_NONBLOCK | O_WRONLY );
      }

      /* register poll routine with the IRIG driver */
      register_irig_callback(&read_counter, counter_ptr->rate, 
                            (void *)counter_channels);
      break;

    case RADAR_SET:
      err("RADAR_SET");
      radar_ptr = (struct radar_set*) buf;
      radar_channels = radar_ptr->channel;
      /* create and open radar data FIFOs */
      for (j=counter_channels; j < counter_channels + radar_channels; j++)
      {
        sprintf(devstr, "/dev/mesa_in_%d", j);
        fd_mesa_radar[j-counter_channels] = open( devstr, O_NONBLOCK | O_WRONLY );
      }
            
      /* register poll routine with the IRIG driver */
      register_irig_callback(&read_radar, radar_ptr->rate, 
                            (void *)radar_channels);
      break;

    case PMS260X_SET:
      break;
   
    default:
      ret = -EIO;        
      break;
  }
  return ret;
}
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  char devstr[30];
  int  j;

  /* Close my ioctl FIFO, deregister my mesa_ioctl function */
  closeIoctlFIFO(ioctlhandle);

  /* for each Mesa counter... */
  if(counter_channels > 0)
  {
    /* close and unlink its output FIFOs */
    for(j=0; j < counter_channels; j++)
    {
      sprintf(devstr, "/dev/mesa_in_%d", j);
      if (fd_mesa_counter[j]) {
	err("calling close( fd_mesa_counter[%d] )", j);
	close( fd_mesa_counter[j] );
      }
      unlink( devstr );
    }
  }

  /* Radar port... */
  if(radar_channels > 0)
  {
    /* close and unlink its input FIFO */
    for(j=counter_channels; j < counter_channels + radar_channels; j++)
    {
      sprintf(devstr, "/dev/mesa_in_%d", j);
      if (fd_mesa_radar[j-counter_channels]) {
	err("calling close( fd_mesa_radar[%d] )", j-counter_channels);
	close( fd_mesa_radar[j-counter_channels] );
      }
      unlink( devstr );
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

  for (chn=0; chn<N_PORTS; chn++)
  {
    /* create its output FIFO */
    sprintf( devstr, "/dev/mesa_in_%d", chn );
    mkfifo( devstr, 0666 );
  }

  for (chn=0; chn < BOARD_NUM+1; chn++)
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
//    err("%s: couldn't allocate I/O range %x - %x", __FILE__, baseadd,
//        baseadd + MESA_REGION_SIZE - 1);
    cleanup_module();
    return -EBUSY;
  }
  rtl_printf("(%s) init_module: loaded\n\n", __FILE__);
  return 0; //success
}
MODULE_AUTHOR("Mike Spowart <spowart@ucar.edu>");
MODULE_DESCRIPTION("Mesa ISA driver for RTLinux");
  
