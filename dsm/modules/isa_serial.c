/* isa_serial.c

   RTLinux module for interfacing the ISA bus interfaced
   8 port serial card.

   Original Author: John Wasinger
   Copyright by the National Center for Atmospheric Research
 
   Implementation notes:

      I have tagged areas which need development with a 'TODO' comment.

   Revisions:

*/

/* RTLinux includes...  */
#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_posixio.h>
#include <rtl_stdio.h>
#include <rtl_unistd.h>

/* Linux module includes... */
#include <linux/ioport.h>

/* DSM includes... */
#define NUM_PORTS 8

RTLINUX_MODULE(isa_serial);
MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("8 port Serial Driver Module");

/* global variables */
extern int ptog;  // this varible is toggled every second

/* File pointers to data and command FIFOs */
static int fp_serial_data[NUM_PORTS][2];
static int fp_serial_cmnd[NUM_PORTS];
static struct rtl_sigaction cmndAct[NUM_PORTS]; // needed for the cmnd fifo handler

/* TODO - set the base address of the 8 port serial card */
volatile unsigned int isa_serial_8_addr = 0x00000;
#define ioWidth                           0x20

/* these pointers are set in init_module... */
/* TODO - declare pointers here */
//volatile unsigned int reg_something_ptr;
//volatile unsigned int reg_data_ptr;

/* -- DSM ------------------------------------------------------------- */
void isa_serial_poll (int fp, char* pollstr)
{
  /* note - this function will be added to an array of function pointers
   * that is called from irig_100hz_isr at variant rates based upon
   + configuration */
  int dev;

  /* REMOVE - n_of_8_serial_ports should be set during configuration */
  int n_of_8_serial_ports = NUM_PORTS;

  /* for each serial port... */
  for (dev=0; dev<n_of_8_serial_ports; dev++)
    if (fp==fp_serial_cmnd[dev])
    {
      /* TODO - write pollstr to device */

      return; // only one fp was specified
    }
}
/* -- RTLinux --------------------------------------------------------- */
unsigned int isa_serial_isr (unsigned int irq, struct rtl_frame *regs)
{
  char buf[1024];

  /* pend this IRQ so that the general purpose OS gets it, too */
  /* TEST - is this needed for ISA multiplexing? */
  rtl_global_pend_irq(irq);
  
  /* TODO - determine which serial port generated this
   * interrupt by reading the 8 port serial card's register */
  static int dev = -1;                            // REPLACE me
  if (++dev==8) dev=0;                            // REPLACE me
//value = inb(reg_something_ptr);                 // SUCH AS...
//outb(value, reg_something_ptr);                 // SUCH AS...

  /* TODO - read the serial port */
  sprintf(buf, "%d.01 %d.02 %d.03 %d.04 %d.05 ",   // REPLACE me
          dev, dev, dev, dev, dev);                // REPLACE me

  /* write the raw seial data to the dataFifo */
  rtl_write(fp_serial_data[dev][ptog], buf, strlen(buf));

  /* TODO - re-enable interrupt on serial card? */

  return 0;
}
/* -- RTLinux --------------------------------------------------------- */
void isa_serial_message(int sig, rtl_siginfo_t *siginfo, void *v)
{
  /* NOTE - this function is used to convey calibration type data
   * to a given instrument on a serial port.  This should not be used
   * on a periodic basis.
   */
  char buffer[1024];
  
  rtl_printf("(%s) Executing handler.\n", __FILE__);

  /* obtain command */
  buffer[ rtl_read(siginfo->si_fd, &buffer, 1024) ] = 0;

  /* take action */
  /* TODO - write buffer to the appropriate serial port */
}
/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
  char devstr[30];
  int tog, dev;

  /* REMOVE - n_of_8_serial_ports should be set during configuration */
  int n_of_8_serial_ports = NUM_PORTS;

  /* for each serial port... */
  for (dev=0; dev<n_of_8_serial_ports; dev++)
  {
    /* close and unlink its toggled output dataFIFOs */
    for (tog=0; tog<2; tog++)
    {
      sprintf(devstr, "/dev/isa_serial_%d_read_%d", dev, tog);
      rtl_close( fp_serial_data[tog][dev] );
      rtl_unlink( devstr );
    }
    /* does this port support asyncronous commands? */
    /* TODO - test configuration here... */
    if (1)
    {
      /* yes, close and unlink its input command fifo */
      sprintf(devstr, "/dev/isa_serial_%d_write", dev);
      rtl_close( fp_serial_cmnd[dev] );
      rtl_unlink( devstr );
    }
  }
  /* free up the I/O region and remove /proc entry */
  release_region (isa_serial_8_addr, ioWidth);
  rtl_free_irq (VIPER_CPLD_IRQ);

  rtl_printf("(%s) cleanup_module\n\n", __FILE__);
}

/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  rtl_printf("(%s) init_module: compiled on %s at %s\n\n",
             __FILE__, __DATE__, __TIME__);

  /* Wait for the init_module to load the header build configuration */
  /* TODO - create a semaphore that blocks in the init_module.c */
//rtl_printf("(%s) init_module: waiting for configuration...\n", __FILE__);
//sem_wait (&anInitSemaphore);

  char devstr[30];
  int tog, dev;

  /* REMOVE - n_of_8_serial_ports should be set during configuration */
  int n_of_8_serial_ports = NUM_PORTS;

  /* for each serial port... */
  for (dev=0; dev<n_of_8_serial_ports; dev++)
  {
    /* create and open its toggled output dataFIFOs */
    for (tog=0; tog<2; tog++)
    {
      sprintf(devstr, "/dev/isa_serial_%d_read_%d", dev, tog);

      // remove broken device file before making a new one
      if (rtl_unlink(devstr) < 0)
        if ( rtl_errno != RTL_ENOENT ) return -rtl_errno;

      rtl_mkfifo( devstr, 0666 );
      fp_serial_data[tog][dev] = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_WRONLY );
    }
    /* does this port support asyncronous commands? */
    /* TODO - test configuration here... */
    if (1)
    {
      /* yes, create and open its input command fifo */
      sprintf(devstr, "/dev/isa_serial_%d_write", dev);

      // remove broken device file before making a new one
      if (rtl_unlink(devstr) < 0)
        if ( rtl_errno != RTL_ENOENT ) return -rtl_errno;

      rtl_mkfifo( devstr, 0666 );
      fp_serial_cmnd[dev] = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_RDONLY );

      /* create realtime a fifo handler that passes an asycronous
       * string command to a given port */
      /* TEST - can we use the same handler funtion for each
       * cmndFifo? */
      cmndAct[dev].sa_sigaction = isa_serial_message;
      cmndAct[dev].sa_fd        = fp_serial_cmnd[dev];
      cmndAct[dev].sa_flags     = RTL_SA_RDONLY | RTL_SA_SIGINFO;
      rtl_sigaction( RTL_SIGPOLL, &cmndAct[dev], NULL );
    }
  }
  /* TODO - define address pointers here */
//  reg_something_ptr = isa_serial_8_addr+0x20;
//  reg_data_ptr      = isa_serial_8_addr+0x24;

  /* Grab the region so that no one else tries to probe our ioports. */
  request_region(isa_serial_8_addr, ioWidth, "8-port-serial");

  /* REMOVE - note that 'isa_serial_isr' is actually going
   * to be added to an array of function pointers that is used
   * by the demux.c module.  This module is needed because
   * FSMLab's RTLinux does not support Arcom's ISA multiplexing
   * code.  I have copied Arcom's code which originally did this
   * into that module. */
  /* install a handler for this IRQ */
  if ( rtl_request_irq( VIPER_CPLD_IRQ, &isa_serial_isr ) < 0 )
  {
    /* failed... */
    cleanup_module();
    rtl_printf("(%s) init_module: could not allocate IRQ at #%d\n",
	       __FILE__, VIPER_CPLD_IRQ);
    return -RTL_EIO;
  }
  rtl_printf("(%s) init_module: allocated IRQ at #%d\n",
	     __FILE__, VIPER_CPLD_IRQ);
  /* end - REMOVE */

  rtl_printf("(%s) init_module: loaded\n\n", __FILE__);
  return 0;
}
