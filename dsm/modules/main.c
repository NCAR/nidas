/* main.c

   Time-stamp: <Fri 01-Apr-2005 10:24:55 am>

   RTLinux module that starts up the other modules based upon the
   configuration passed down to it from the 'src/dsmAsync.cc'
   application.

   The 'src/ck_main.cc' application is a test program that
   can be used to exercise this modules functionality.

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

*/

#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl_pthread.h>
#include <rtl_unistd.h>
#include <rtl_posixio.h>
#include <rtl_string.h>
#include <rtl_stdio.h>
#include <rtl_limits.h>
#include <sys/rtl_mman.h>

#include <linux/kmod.h>
#include <bits/posix1_lim.h>

#include <main.h>
#include <com8.h>
RTLINUX_MODULE(main);

// global declarations
int ptog = 0;
int module_loading = 0;
static int fdCfg;                  // file descriptor, command fifo
int nAnalogCfg=0, nSerialCfg=0;
struct analogTable analogCfg[ANALOG_MAX_CHAN];
struct serialTable serialCfg[SERIAL_MAX_CHAN];

// module title
MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("RTLinux 'initialize modules' module");


/* -- Utility --------------------------------------------------------- */
void start_modules ( void )
{
  rtl_printf("(%s) %s:\t Loading modules...\n", __FILE__, __FUNCTION__);
  char module_name[40];

  // Finished loading configuration structures
  // start up the kernel side of the system now...

  // TODO - use 'request_module()' to load in modules based upon
  // configuration.  (hint - see the book LDDv2 p. 306)

  // Load the ISA based 8 port serial card module
  if (nSerialCfg)
  {
    module_loading = 1;
    sprintf (module_name, "com8");
    if ( 0 > request_module(module_name) )
      {
	rtl_printf("(%s) %s:\t Unable to load module [%s].\n",
		   __FILE__, __FUNCTION__, module_name);
	return;
      }
    while ( module_loading ) // hang until module successfully loads
      udelay(1);
  }

  // Load the ISA based IRIG-b module
  module_loading = 1;
  sprintf (module_name, "irig");
  if ( 0 > request_module(module_name) )
  {
    rtl_printf("(%s) %s:\t Unable to load module [%s].\n",
	       __FILE__, __FUNCTION__, module_name);
    return;
  }
  while ( module_loading ) // hang until module successfully loads
    udelay(1);

  // Load the ISA de-multiplexer module after all ISA based
  // modules are loaded...
  module_loading = 1;
  sprintf (module_name, "demux");
  if ( 0 > request_module(module_name) )
  {
    rtl_printf("(%s) %s:\t Unable to load module [%s].\n",
	       __FILE__, __FUNCTION__, module_name);
    return;
  }
  while ( module_loading ) // hang until module successfully loads
    udelay(1);

  // DEBUG just print configuration results for now.
  int aa;
  for (aa=0; aa<nAnalogCfg; aa++)
    rtl_printf("(%s) %s:\t %02d Analog: port: %02d rate: %03d cal: %d \n",
	       __FILE__, __FUNCTION__, aa,
	       analogCfg[aa].port, analogCfg[aa].rate, analogCfg[aa].cal);

  for (aa=0; aa<nSerialCfg; aa++)
    rtl_printf("(%s) %s:\t %02d Serial: baud: %d port: %02d rate: %03d cmd: %s \n",
	       __FILE__, __FUNCTION__, serialCfg[aa].baud_rate, aa,
	       serialCfg[aa].port, serialCfg[aa].rate, serialCfg[aa].cmd);
}

/* -- RTLINUX --------------------------------------------------------- */
void handle_cfg (int sig, rtl_siginfo_t *siginfo, void *v)
{
  // ignore if not incoming data
  if (siginfo->si_fd != fdCfg)
  {
    rtl_printf("(%s) %s:\t ignored... unknown file pointer\n",
               __FILE__, __FUNCTION__);
    return;
  }
  if (siginfo->si_code != RTL_POLL_IN)
  {
    rtl_printf("(%s) %s:\t ignored... not incoming data\n",
               __FILE__, __FUNCTION__);
    return;
  }

  static enum
  {
    start,
    set_analog,
    set_serial
  } state = start;

  static char buf[2048];
  static int len = 0;
  int aa, bb;

  // This handler is re-entrant state machine function.
  switch (state)
  {
  case start:
    // read until string is null terminated...
    len += rtl_read(fdCfg, &buf[len], SSIZE_MAX);
    if (buf[strlen(buf)] != '\0')
      break;

    // The name of the structure from the FIFO determines which
    // structure is being sent.  This drives the state machine.
    if (!strcmp(buf,"RUN"))
    {
      start_modules();
      buf[0] = '\0';
      len = 0;
    }
    else if (!strcmp(buf,"ANALOG"))
      state = set_analog;
    else if (!strcmp(buf,"SERIAL"))
      state = set_serial;
    else
      rtl_printf("(%s) %s:\t Invalid structure name '%s'\n",
		 __FILE__, __FUNCTION__, buf);

    // shift data bytes following the strings null character up
    //  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
    //  A  N  A  L  O  G /0 xx xx xx xx xx xx xx xx xx
    for (aa=0, bb=strlen(buf)+1; bb<len; aa++, bb++)
      buf[aa] = buf[bb];
    len = aa;
    break;

  case set_analog:
    // read until structure is fully received...
    len += rtl_read(fdCfg, &buf[len], SSIZE_MAX);
    if (len < sizeof(struct analogTable))
      break;

    // copy buffer into the next analog configuration struct.
    if (nAnalogCfg < ANALOG_MAX_CHAN)
      memcpy(&analogCfg[nAnalogCfg++], buf, sizeof(struct analogTable));
    else
      rtl_printf("(%s) %s:\t analog configuration array is full!\n",
		 __FILE__, __FUNCTION__);
    state = start;
    len = 0;
    break;

  case set_serial:
    // read until structure is fully received...
    len += rtl_read(fdCfg, &buf[len], SSIZE_MAX);
    if (len < sizeof(struct serialTable))
      break;

    // copy buffer into the next serial configuration struct.
    if (nSerialCfg < SERIAL_MAX_CHAN)
      memcpy(&serialCfg[nSerialCfg++], buf, sizeof(struct serialTable));
    else
      rtl_printf("(%s) %s:\t serial configuration array is full!\n",
		 __FILE__, __FUNCTION__);
    state = start;
    len = 0;
    break;

  default:
    break;
  }
  return;
}

/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  rtl_printf("(%s) %s:\t exiting...\n", __FILE__, __FUNCTION__);
  rtl_close(fdCfg);
  rtl_unlink(cfgFifo);
}

/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  rtl_printf("(%s) %s:\t compiled on %s at %s\n",
             __FILE__, __FUNCTION__, __DATE__, __TIME__);

  // remove broken device file before making a new one
  if (rtl_unlink(cfgFifo) < 0)
    if ( rtl_errno != RTL_ENOENT ) return -rtl_errno;

  // create a fifo that receives data from user space
  if (rtl_mkfifo(cfgFifo, 0666)<0)
  {
    rtl_printf("(%s) %s:\t Cannot create %s\n",
               __FILE__, __FUNCTION__, cfgFifo);
    return -rtl_errno;
  }

  // open fifo
  fdCfg = rtl_open(cfgFifo, RTL_O_NONBLOCK | RTL_O_RDONLY);
  if (fdCfg < 0)
  {
    rtl_printf("(%s) %s:\t Cannot open %s\n",
               __FILE__, __FUNCTION__, cfgFifo);
    return -rtl_errno;
  }

  // create real-time fifo handler
  struct rtl_sigaction cfgSigAct;
  cfgSigAct.sa_sigaction = handle_cfg;
  cfgSigAct.sa_fd        = fdCfg;
  cfgSigAct.sa_flags     = RTL_SA_RDONLY | RTL_SA_SIGINFO;
  if ( rtl_sigaction( RTL_SIGPOLL, &cfgSigAct, NULL ) != 0 )
  {
    rtl_printf("(%s) %s:\t Cannot create FIFO handler for %s\n",
               __FILE__, __FUNCTION__, cfgFifo);
    cleanup_module();
    return -RTL_EIO;
  }

  rtl_printf("(%s) %s:\t loaded\n", __FILE__, __FUNCTION__);
  return 0;
}
