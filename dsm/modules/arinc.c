/* cei420a.c

   RTLinux module for interfacing the ISA bus interfaced
   Condor Engineering's CEI-420A-42 ARINC card.

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research

   Implementation notes:

      I have tagged areas which need development with a 'TODO' comment.

   Revisions:

     $LastChangedRevision: $
         $LastChangedDate: $
           $LastChangedBy: $
                 $HeadURL: $
*/

/* RTLinux includes...  */
#include <rtl.h>
#include <rtl_posixio.h>
#include <rtl_stdio.h>
#include <rtl_unistd.h>

/* Linux module includes... */
#include <linux/init.h>
#include <linux/ioport.h>
#include <bits/posix1_lim.h>

/* API source code inclusion */
#include <api220.c>

/* DSM includes... */
#include <cei420a.h>
#include <irigclock.h>

RTLINUX_MODULE(arinc);

#define BOARD_NUM   0
#define err(format, arg...) \
     rtl_printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

/* Define IOCTLs */
static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS, _IOC_SIZE(GET_NUM_PORTS) },
  { ARINC_SET,     _IOC_SIZE(ARINC_SET    ) },
  { ARINC_GO,      _IOC_SIZE(ARINC_GO     ) },
  { ARINC_RESET,   _IOC_SIZE(ARINC_RESET  ) },
  { ARINC_STAT,    _IOC_SIZE(ARINC_STAT   ) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);
static struct ioctlHandle* ioctlhandle = 0;

/* Set the base address of the ARINC card */
volatile unsigned long phys_membase;
static volatile unsigned long basemem = 0x3c0c8000;
MODULE_PARM(basemem, "1l");
MODULE_PARM_DESC(basemem, "ISA memory base (default 0x3c0c800)");

/* global variables */
static unsigned char   cfg_rate[N_ARINC_RX][0377];
static unsigned char   overflow[N_ARINC_RX];
static unsigned char  underflow[N_ARINC_RX];
static unsigned long        lps[N_ARINC_RX];
static enum irigClockRates poll[N_ARINC_RX];

/* File pointers to data and command FIFOs */
static int              fd_arinc_rx[N_ARINC_RX];
static int              fd_arinc_tx[N_ARINC_TX];
static struct rtl_sigaction cmndAct[N_ARINC_TX];

/* -- UTILITY --------------------------------------------------------- */
static void error_exit(short board, short status)
{
    // display the error message
    err("Error while testing board %d", board);
    err("  Error reported:  \'%s\'",    ar_get_error(status));
    err("  Additional info: \'%s\'\n",  ar_get_error(ARS_LAST_ERROR));
}
/* -- IRIG CALLBACK --------------------------------------------------- */
void arinc_sweep(void* channel)
{
  struct dsm_arinc_sample sample;
  short chn = (int) channel;
  short status;

  /* read ARINC channel until it's empty or our buffer is full */
  status = ar_getwords(BOARD_NUM, chn, LPB, &(sample.length), sample.data);

  /* log possible buffer underflows */
  if (status == ARS_NODATA)
    underflow[chn]++;

  /* log possible buffer overflows */
  if (sample.length == LPB)
    overflow[chn]++;

  /* write this block of ARINC data to the output FIFO */
  if (sample.length > 0)
  {
    sample.timetag = GET_MSEC_CLOCK;
    rtl_write( fd_arinc_rx[chn], &sample, sample.length*4 );
  }
}
/* -- IOCTL CALLBACK -------------------------------------------------- */
/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 */
static int arinc_ioctl(int cmd, int board, int port, void *buf, rtl_size_t len)
{
  err("DEBUG cmd=%d, board=%d, port=%d, len=%d", cmd, board, port, len);

  int chn, lbl, ret = -RTL_EINVAL;
  short status;
  char devstr[30];

  switch (cmd)
  {
  case GET_NUM_PORTS:		/* user get */
    {
	err("GET_NUM_PORTS");
	*(int *) buf = N_ARINC_RX + N_ARINC_TX;
	ret = sizeof(int);
    }
    break;
  case ARINC_SET:
    {
      /* Store the rate for the channel's label. */
      struct arinc_set* clr = (struct arinc_set*) buf;
      cfg_rate[clr->channel][clr->label] = clr->rate;

      /* Measure the total labels per second for the given channel. */
      lps[clr->channel] += clr->rate;

      /* un-filter this label on this channel */
      status = ar_label_filter(BOARD_NUM, clr->channel, clr->label, ARU_FILTER_OFF);
      if (status != ARS_NORMAL)
        error_exit(BOARD_NUM, status);
    }
    break;

  case ARINC_GO:
    {
      for (chn=0; chn<N_ARINC_RX; chn++)
        if (lps[chn])
        {
          /* open its output FIFO */
          sprintf( devstr, "/dev/arinc_in_%d", chn );
          fd_arinc_rx[chn] = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_WRONLY );

          /* Round up to the next highest poll rate based upon the
           * buffering capacity of the channel. */
          if ((poll[chn] = irigClockRateToEnum(lps[chn]/LPB)) == IRIG_NUM_RATES)
            return -RTL_EINVAL;
            
          /* register poll routine with the IRIG driver */
          register_irig_callback( &arinc_sweep, poll[chn], (void *)chn );
        }
      /* launch the board */
      status = ar_go(BOARD_NUM);
      if (status != ARS_NORMAL)
        error_exit(BOARD_NUM, status);
    }
    break;

  case ARINC_RESET:
    {
      /* Clear out the configuration array and unregister from IRIG. */
      for (chn=0; chn<N_ARINC_RX; chn++)
        if (lps[chn])
        {
          unregister_irig_callback(&arinc_sweep, poll[chn]);
          lps[chn] = 0;

          for (lbl=0; lbl<0377; lbl++)
            cfg_rate[chn][lbl] = 0;
        }
    }
    break;

  case ARINC_STAT:
    {
      /* Display the current configuration. */
      for (chn=0; chn<N_ARINC_RX; chn++)
        if (lps[chn])
        {
          rtl_printf("\n\n\nChannel %d's device generates a total of %d labels/sec\n", chn, lps[chn]);
          rtl_printf("\nChannel %d is polled at %dHz\n", chn, irigClockEnumToRate(poll[chn]));
          rtl_printf("\nChannel %d has  overflowed %d times\n", chn,  overflow[chn]);
          rtl_printf("\nChannel %d has underflowed %d times\n", chn, underflow[chn]);

          rtl_printf("\nchn labl rate\n-------------\n");
          for (lbl=0; lbl<0377; lbl++)
            if (cfg_rate[chn][lbl])
              rtl_printf("%2d  %04o %4d\n", chn, lbl, cfg_rate[chn][lbl]);
        }
        else
          rtl_printf("\n\n\nChannel %d is unused\n\n\n", chn);
    }
    break;
  }
  return ret;
}
/* -- RTLinux --------------------------------------------------------- */
void arinc_transmit(int sig, rtl_siginfo_t *siginfo, void *v)
{
  static char buf[N_ARINC_TX][1024];
  static int  len[N_ARINC_TX];
  int port, idx, lft;

  err("DEBUG Executing handler.");

  /* determine which port the file pointer is for */
  for (port=0; port < N_ARINC_TX; port++)
    if (fd_arinc_tx[port] == siginfo->si_fd)
      break;

  /* read the FIFO into a buffer until there is enough data to transmit */
  len[port] += rtl_read(siginfo->si_fd, &buf[port][len[port]], SSIZE_MAX);
  if (len[port] < 4)
    return;

  /* transmit the buffered 32-bit ARINC word(s) */
  for (idx=0; 4 <= len[port]; idx+=4, len[port]-=4)
    ar_putword(BOARD_NUM, port, 0xffff & (unsigned long)(buf[port][idx]) );

  /* save any partial data for the next transmission
   * by shifting the remaining buffered bytes up... */
  for (lft=0; lft < len[port]; lft++, idx++)
    buf[port][lft] = buf[port][idx];
}
/* -- MODULE ---------------------------------------------------------- */
static void __exit cei420a_cleanup(void) 
{
  char devstr[30];
  short chn;

  /* Close my ioctl FIFO, deregister my arinc_ioctl function */
  if (ioctlhandle)
    closeIoctlFIFO(ioctlhandle);

  /* for each ARINC receive port... */
  for (chn=0; chn<N_ARINC_RX; chn++)
    if (lps[chn])
    {
      /* close and unlink its output FIFO */
      sprintf( devstr, "/dev/arinc_in_%d", chn );
      rtl_close( fd_arinc_rx[chn] );
      rtl_unlink( devstr );
    }
  /* for each ARINC transmit port... */
  for (chn=0; chn<N_ARINC_TX; chn++)
  {
    /* close and unlink its input FIFO */
    sprintf(devstr, "/dev/arinc_out_%d", chn);
    rtl_close( fd_arinc_tx[chn] );
    rtl_unlink( devstr );
  }
  /* TODO - free up the ISA memory region */
  release_region(basemem, PAGE_SIZE);

  // close the board
  short status = ar_close(BOARD_NUM);
  if (status != ARS_NORMAL)
    error_exit(BOARD_NUM, status);

  err("done.\n");
}
/* -- UTILITY --------------------------------------------------------- */
static int scan_ceiisa( void )
{
  unsigned char value;
  unsigned int indx;
  int status = -RTL_ENODEV;

  char *boardID[] = {"Standard CEI-220",
                     "Standard CEI-420",
                     "Custom CEI-220 6-Wire",
                     "CEI-420-70J",
                     "CEI-420-XXJ",
                     "Obsolete",
                     "CEI-420A-42-A",
                     "CEI-420A-XXJ"};

  /* There are no a lot of tests that I can perform to see if it is a
   * CEI 420 card. The test I perform are:
   *   - configuration register 1 : looking at bits 4-7, bit 6 is set
   *                                (CEI-420-XXJ) or bit 4 and 5 are
   *                                set (CEI-420-70J)
   *   - configuration register 2 : value has to be 0x0
   *   - configuration register 3 : value of bits 4-7 has to be 0x0
   */

  value = readb(phys_membase + 0x808);
  value >>= 4;
  err("DEBUG value = 0x%X", value);
  if (value != 0x0)
  {
    /* passed register 1 test... */
    value = readb(phys_membase + 0x80A);
    value >>= 4;
    err("DEBUG value = 0x%X", value);
    if (value == 0x5)
      err("Obsolete CEI-420a");
    if (value == 0x0 || value == 0x1 || value == 0x2 ||
        value == 0x3 || value == 0x4 || value == 0x6 || value == 0x7)
    {
      /* passed register 2 test... */
      indx = value;
      value = readb(phys_membase + 0x80C);
      err("DEBUG value = 0x%X", value);
      if (value == 0x0)
      {
        /* passed register 3 test... */
        value = readb(phys_membase + 0x80E);
        value >>= 4;
        err("DEBUG value = 0x%X", value);
        if(value == 0)
        {
          /* passed register 4 test... */
          err("cei220/420 found.  Board = %s", boardID[indx]);
          err("found CEI-220/420 at 0x%lX", basemem);
          status = ARS_NORMAL;
        }
      }
    }
  }
  return status;
}
#if 0
/* -- UTILITY --------------------------------------------------------- */
int test_loader (void)
{
  volatile unsigned long handle;
  unsigned int   i;                // Loop index.
  unsigned int   bytes_written;    // Page byte wrap counter.
  unsigned int   current_page;     // Page counter.
  unsigned int   isError;          // Verification error counter.
  unsigned short mem_value;        // Value read back from memory.

  char last_error[200] = {"No additional information available"};


  handle = phys_membase;

  //----------------------------------------------------------------------
  //  Put the CEI220 board into native mode so the special               +
  //   registers may be accessed by this host.  This is the mode         +
  //   in which we will remain for the duration of the program.          +
  //  Use function calls to avoid complier optimization problems.        +
  //----------------------------------------------------------------------
  err("DEBUG readw(handle+0x00/2) = %X", readw(handle+0x00/2));
  err("DEBUG readw(handle+0x0e/2) = %X", readw(handle+0x0e/2));
  err("DEBUG readw(handle+0x02/2) = %X", readw(handle+0x02/2));
  err("DEBUG readw(handle+0x0c/2) = %X", readw(handle+0x0c/2));
  err("DEBUG readw(handle+0x04/2) = %X", readw(handle+0x04/2));
  err("DEBUG readw(handle+0x0a/2) = %X", readw(handle+0x0a/2));
  err("DEBUG readw(handle+0x06/2) = %X", readw(handle+0x06/2));
  err("DEBUG readw(handle+0x08/2) = %X", readw(handle+0x08/2));
  err("DEBUG readw(handle+0x00/2) = %X", readw(handle+0x00/2));

  //------------------------------------------------------------------------
  //  Stop and reset the i960 microcontroller, and clear the Xilinx.       +
  //------------------------------------------------------------------------
  // Clear the microcontroller run bit and reset the FPGA
  writew((uint16_t) 0x0000, handle+RESET_REG);
  writew((uint16_t) 0x0002, handle+RESET_REG);

  //--------------------------------------------------------------------
  //  Perform a self-test of 64 Kbytes of Dual-Port memory, leaving    +
  //   it set to all zeros when we are complete.  Write first pattern  +
  //   which consists of writing the word offset to each word...       +
  //--------------------------------------------------------------------
  bytes_written = 0;      // Start writing bytes at the board beginning,
  current_page =  0;      //  which is page zero.
  for ( i = 0; i <= 32767; i++ )
  {
    //  Update the RAM Page Register if needed.
    if ( (bytes_written & 0x07FF) == 0 )
    {
      writew((uint16_t) current_page, handle+RPR);
      current_page++;
    }
    writew((uint16_t) i, handle+(i & 0x3FF)*2);
/*  err("DEBUG wrote page: 0x%lX offset: 0x%lX value:0x%lX", */
/*             current_page, (i & 0x3FF), i); */
    bytes_written += 2;        // We have written 2 bytes to the board.
  }
  //--------------------------------------------------------------------
  //  Read back the first pattern, and write the second pattern (0).   +
  //--------------------------------------------------------------------
  bytes_written = 0;      // Start reading bytes at the board beginning,
  current_page =  0;      //  which is page zero.
  isError = 0;
  for ( i = 0; i <= 32767; i++ )
  {
    //  Update the RAM Page Register if needed.
    if ( (bytes_written & 0x07FF) == 0 )
    {
      writew((uint16_t) current_page, handle+RPR);
      current_page++;
    }
    mem_value = readw(handle+(i & 0x3FF)*2);

    err("DEBUG read  page: 0x%lX offset: 0x%lX value:0x%lX",
               current_page, (i & 0x3FF), mem_value);

    if ( (mem_value & 0xff) != (i & 0xff) )
    {
      sprintf(last_error,"CEI-220/420 Dual-port memory error1 at %4.4X page %X,"
              " is = %4.4X should = %4.4X",
              bytes_written, current_page-1, mem_value, i);
      isError++;
      if ( isError > 255 )
        break;            // Give it up if there is no board there...
    }
    writew((uint16_t) 0, handle+(i & 0x3FF)*2);
    bytes_written += 2;     // We have written 2 bytes to the board.
  }
  if ( isError )
    err("DEBUG %s", last_error);

  return 0;
}
#endif
/* -- MODULE ---------------------------------------------------------- */
static int __init cei420a_init(void)
{
  err("compiled on %s at %s", __DATE__, __TIME__);

  /* map ISA card memory into kernel memory */
  if ( (phys_membase = (unsigned long)ioremap(basemem, PAGE_SIZE)) <= 0 )
  {
    err("ioremap failed...\n");
    return -RTL_EIO;
  }
  err("DEBUG 0x%lX = ioremap(0x%lX, 0x%lX)", phys_membase, basemem, PAGE_SIZE);

  /* scan the ISA bus for the device */
  short status = scan_ceiisa();
  if (status != ARS_NORMAL) goto fail;

  /* load the board (the size and address are not used - must specify zero) */
//  test_loader();
  status = ar_loadslv(BOARD_NUM,0,0,0);
  if (status != ARS_NORMAL) goto fail;

  /* select buffered mode */
  status = ar_set_storage_mode(BOARD_NUM, ARU_BUFFERED);
  if (status != ARS_NORMAL) goto fail;

  /* initialize the slave */
  status = ar_init_slave(BOARD_NUM);
  if (status != ARS_NORMAL) goto fail;

  /* Display the board type */
  err("Board %s detected",ar_get_boardname(BOARD_NUM,NULL));

  /* Display the number of receivers and transmitters */
  err("Supporting %d transmitters and %d receivers.",
      ar_num_xchans(BOARD_NUM), ar_num_rchans(BOARD_NUM));

  // DEBUG - enable internal wrap
  status = ar_set_config(BOARD_NUM,ARU_INTERNAL_WRAP,AR_WRAP_ON);
  if (status != ARS_NORMAL) goto fail;

  /* initialize the label filter to filter out all labels on all channels */
  short chn;
  for (chn=0; chn<N_ARINC_RX; chn++)
  {
    status = ar_label_filter(BOARD_NUM, chn, ARU_ALL_LABELS, ARU_FILTER_ON);
    if (status != ARS_NORMAL) goto fail;
  }
  /* open up my ioctl FIFO, register my arinc_ioctl function */
  ioctlhandle = openIoctlFIFO("arinc", BOARD_NUM, arinc_ioctl,
                              nioctlcmds, ioctlcmds);
  if (!ioctlhandle) return -RTL_EIO;

  /* for each ARINC receive port... */
  char devstr[30];
  for (chn=0; chn<N_ARINC_RX; chn++)
  {
    /* create its output FIFO */
    sprintf( devstr, "/dev/arinc_in_%d", chn );

    // remove broken device file before making a new one
    rtl_unlink(devstr);
    if ( rtl_errno != -RTL_ENOENT ) return -rtl_errno;

    rtl_mkfifo( devstr, 0666 );
  }
  /* for each ARINC transmit port... */
  for (chn=0; chn<N_ARINC_TX; chn++)
  {
    /* create and open its input FIFO */
    sprintf(devstr, "/dev/arinc_out_%d", chn);

    // remove broken device file before making a new one
    rtl_unlink(devstr);
    if ( rtl_errno != -RTL_ENOENT ) return -rtl_errno;

    rtl_mkfifo( devstr, 0666 );
    fd_arinc_tx[chn] = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_RDONLY );

    /* create its real-time FIFO handler */
    cmndAct[chn].sa_sigaction = arinc_transmit;
    cmndAct[chn].sa_fd        = fd_arinc_tx[chn];
    cmndAct[chn].sa_flags     = RTL_SA_RDONLY | RTL_SA_SIGINFO;
    rtl_sigaction( RTL_SIGPOLL, &cmndAct[chn], NULL );
  }
  /* reserve the ISA memory region */
  if (!request_region(basemem, PAGE_SIZE, "arinc"))
  {
    err("couldn't allocate I/O range %x - %x\n", basemem,
        basemem + PAGE_SIZE - 1);
    return -RTL_EBUSY;
  }
  err("done.\n");
  return 0; /* success */

 fail:
  error_exit(BOARD_NUM, status);
  cei420a_cleanup();
  return -RTL_EIO;
}

module_init(cei420a_init);
module_exit(cei420a_cleanup);

MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("CEI420a ISA driver for RTLinux");
