/* arinc.c

   RTLinux module for interfacing the ISA bus interfaced
   Condor Engineering's CEI-420A-42 ARINC card.

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research

   Implementation notes:

      I have tagged areas which need development with a 'TODO' comment.

   Acronyms:

      LPS - Labels  Per Second
      LPB - Labels  Per Buffer
      BPS - Buffers Per Second

   Revisions:

     $LastChangedRevision: $
         $LastChangedDate: $
           $LastChangedBy: $
                 $HeadURL: $
*/

/* RTLinux includes...  */
#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_posixio.h>
#include <rtl_stdio.h>
#include <rtl_unistd.h>

/* Linux module includes... */
#include <linux/init.h>          // module_init, module_exit
#include <linux/ioport.h>

/* API source code inclusion */
#include <api220.c>

/* DSM includes... */
#include <arinc.h>
#include <irigclock.h>

RTLINUX_MODULE(arinc);

#define BOARD_NUM   0
#define err(format, arg...) \
     rtl_printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

/* Define IOCTLs */
static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,  _IOC_SIZE(GET_NUM_PORTS ) },
  { ARINC_SET,      _IOC_SIZE(ARINC_SET     ) },
  { ARINC_GO,       _IOC_SIZE(ARINC_GO      ) },
  { ARINC_RESET,    _IOC_SIZE(ARINC_RESET   ) },
  { ARINC_SIM_XMIT, _IOC_SIZE(ARINC_SIM_XMIT) },
  { ARINC_BIT,      _IOC_SIZE(ARINC_BIT     ) },
  { ARINC_STAT,     _IOC_SIZE(ARINC_STAT    ) },
  { ARINC_MEASURE,  _IOC_SIZE(ARINC_MEASURE ) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);
static struct ioctlHandle* ioctlhandle = 0;

/* Set the base address of the ARINC card */
void* phys_membase;
/* static unsigned long parity = AR_ODD; */
/* static unsigned long speed  = AR_HIGH; */
static volatile unsigned long basemem = 0x3c0d0000;
static enum irigClockRates sync_rate = IRIG_1_HZ;

/* MODULE_PARM(parity, "1l"); */
/* MODULE_PARM(speed,  "1l"); */
MODULE_PARM(basemem,   "1l");

/* MODULE_PARM_DESC(parity,  "ARINC 429 parity (default AR_ODD)"); */
/* MODULE_PARM_DESC(speed,   "ARINC 429 speed (default AR_HIGH)"); */
MODULE_PARM_DESC(basemem,   "ISA memory base (default 0x3c0d000)");

/* global variables */
char requested_region  = 0;
int  display_chn = -1;
struct recvHandle
{
  /* setup info... */
  char                fname[40];
  int                 fd;
  unsigned int        lps;   // Labels Per Second
  enum irigClockRates poll;
  char                sim_xmit;

  /* run-time info... */
  unsigned long       overflow;
  unsigned long       underflow;
  unsigned char       rate[0400];
  unsigned char       msg_id[0400];
  unsigned char       arcfgs[0400];
  unsigned int        nArcfg;
};
static struct recvHandle  chn_info[N_ARINC_RX];
static struct dsm_sample* sample = 0;

/* File pointers to data and command FIFOs */
static int              fd_arinc_tx[N_ARINC_TX];
static struct rtl_sigaction cmndAct[N_ARINC_TX];


/* -- UTILITY --------------------------------------------------------- */
static void error_exit(short board, short status)
{
  if (!status) return;

  // display the error message
  err("Error while testing board %d", board);
  err("  Error reported:  \'%s\'",    ar_get_error(status));
  err("  Additional info: \'%s\'\n",  ar_get_error(ARS_LAST_ERROR));
}
/* -- UTILITY --------------------------------------------------------- */
static short roundUpRate(short rate)
{
  if (rate==3)  return 4;
  if (rate==6)  return 7;
  if (rate==12) return 13;
  return rate;
}
/* -- UTILITY --------------------------------------------------------- */
static short rateToCeiClk(short rate)
{
  if (rate==3)  return 320;
  if (rate==6)  return 160;
  if (rate==12) return 80;
  return (1000/rate);
}
/* -- UTILITY --------------------------------------------------------- */
void diag_display(const int chn, const int nData, tt_data_t* data)
{
/*   struct recvHandle *hdl = &chn_info[chn]; */
/*   int xxx, yyy, zzz, iii, jjj; */
/*   char *glyph[] = {"\\","|","/","-"}; */

/*   static unsigned long       last_timetag; */
/*   static short               sum_sample_length; */
/*   static short               tally[0400]; */

/*   static int  rx_count=0, sum_of_tally=0; */
/*   static int  anim = 0; */

  int iii;
  err("length %3d", nData );
  for (iii=0; iii<nData; iii++)
    err("sample[%3d]: %8d %4o %#08x", iii, data[iii].time, data[iii].data & 0xff, data[iii].data & 0xffffff00 );

/*   for (iii=0; iii<nData; iii++) */
/*     tally[data[iii].data & 0377]++; */
/*   sum_sample_length += nData; */

/*   /\* move cursor to upper left corner before printing the table *\/ */
/*   rtl_printf( "%c[H", 27 ); */

/*   if (sample->timetag >= last_timetag + 1000) { */
/*     last_timetag = sample->timetag; */

/*     for (iii=0; iii<hdl->nArcfg; iii++) { */
/*       jjj = hdl->arcfgs[iii]; */

/*       if (iii % 8 == 0) rtl_printf("\n"); */
/*       rtl_printf(" %03o %3d/%3d |", jjj, tally[jjj], hdl->rate[jjj]); */

/*       sum_of_tally += tally[jjj]; */
/*       tally[jjj] = 0; */
/*     } */

/*     rx_count = ar_get_rx_count(BOARD_NUM, chn); */
/*     sum_sample_length = 0; */
/*     ar_clr_rx_count(BOARD_NUM, chn); */
/*   } */
/*   rtl_printf( "%c[H%c[6B", 27, 27 ); */
/*   rtl_printf("------------------------------------------------------------------------------------------------------------------------\n"); */
/*   rtl_printf("%s   chn: %d   poll: %3d Hz   LPS: %4d   sum: %4d   ar_get_rx_count: %4d             \n", */
/*              glyph[anim], chn, irigClockEnumToRate(hdl->poll), hdl->lps, sum_sample_length, rx_count); */
/*   xxx = sample->timetag; */
/*   yyy = GET_MSEC_CLOCK; */
/*   zzz = ar_get_timercntl(BOARD_NUM); */
/*   rtl_printf("sample->timetag:  %7d                                     \n", xxx); */
/*   rtl_printf("GET_MSEC_CLOCK:   %7d (%7d)                           \n", yyy, xxx-yyy); */
/*   rtl_printf("ar_get_timercntl: %7d (%7d)                           \n", zzz, yyy-zzz); */
/*   rtl_printf("-------------------------------------------------------------------------------------\n"); */
/*   if (++anim == 4) anim=0; */
}
/* -- IRIG CALLBACK --------------------------------------------------- */
/**
 * sync up the i960's internal clock to the IRIG time
 */
void arinc_timesync(void* junk)
{
//rtl_printf("%6d, %6d\n", GET_MSEC_CLOCK, ar_get_timercntl(BOARD_NUM));
  ar_set_timercnt(BOARD_NUM, GET_MSEC_CLOCK);
}
/* -- IRIG CALLBACK --------------------------------------------------- */
void arinc_sweep(void* channel)
{
  short status;
  int chn = (int) channel;
  struct recvHandle *hdl = &chn_info[chn];
  int nData;
  tt_data_t *data = (tt_data_t*)sample->data;

  /* set the sweep block's time tag */
  sample->timetag = GET_MSEC_CLOCK;

  /* read ARINC channel until it's empty or our buffer is full */
  status = ar_getwordst(BOARD_NUM, chn, LPB, &nData, data);

  /* log possible buffer underflows */
  if (status == ARS_NODATA) {
    hdl->underflow++;
    return;
  }
  /* display measurements of the labels as the arrive */
/*   if (display_chn == chn) */
/*     diag_display(chn, nData, data); */

  /* log possible buffer overflows */
  if (nData == LPB)
    hdl->overflow++;

  sample->length = nData * 8;

  /* write this block of ARINC data to the output FIFO */
  if (sample->length > 0)
    if (hdl->fd > -1)
      rtl_write(hdl->fd, sample, SIZEOF_DSM_SAMPLE_HEADER + sample->length);
}
/* -- IOCTL CALLBACK -------------------------------------------------- */
/**
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 */
static int arinc_ioctl(int cmd, int board, int chn, void *buf, rtl_size_t len)
{
  static int running = 0;

  struct recvHandle *hdl = &chn_info[chn];

  int lbl;
  short status;

  switch (cmd) {
  case GET_NUM_PORTS:

    *(int *) buf = N_ARINC_RX;
    break;

  case ARINC_SET:

    /* stop the board */
    if (running) {
      status = ar_reset(BOARD_NUM);
      if (status != ARS_NORMAL) goto ar_fail;
      running = 0;
    }
    /* store the rate for this channel's label */
    arcfg_t val = *(arcfg_t*) buf;
    if ( hdl->rate[val.label] )
      err( "duplicate label: %04o", val.label);
    hdl->rate[val.label] = val.rate;

    /* define a periodic message for the i960 to generate */
    if (hdl->sim_xmit) {
      status = ar_define_msg(BOARD_NUM, chn, rateToCeiClk(val.rate),
                             0, (long)val.label);
      if (status > 120) goto ar_fail;
      hdl->msg_id[val.label] = (unsigned char) status;
    }
    hdl->arcfgs[hdl->nArcfg++] = val.label;

    /* measure the total labels per second for the given channel */
    hdl->lps += roundUpRate(val.rate);

    /* round up to the next highest poll rate (bps+1) based upon the
     * buffering capacity of the channel */
    if ((hdl->poll = irigClockRateToEnum(hdl->lps/LPB+1)) == IRIG_NUM_RATES)
      return -RTL_EINVAL;

    /* un-filter this label on this channel */
    status = ar_label_filter(BOARD_NUM, chn, val.label, ARU_FILTER_OFF);
    if (status != ARS_NORMAL) goto ar_fail;
    break;

  case ARINC_GO:

    /* stop the board */
    if (running) {
      status = ar_reset(BOARD_NUM);
      if (status != ARS_NORMAL) goto ar_fail;
      running = 0;
    }
    if (hdl->lps) {
      /* open the channels data FIFO */
      hdl->fd = rtl_open( hdl->fname, RTL_O_NONBLOCK | RTL_O_WRONLY );
      if (hdl->fd < 0) return hdl->fd;

      /* increase the RTLinux FIFO to fit the large sample size writes */
/* dsm/modules/arinc.c: arinc_ioctl: calling ftruncate(hdl->fd, 2056) */
/*       err("calling ftruncate(hdl->fd, %d)", (SIZEOF_DSM_SAMPLE_HEADER + LPB*8) * 8); */
/*       status = ftruncate(hdl->fd, SIZEOF_DSM_SAMPLE_HEADER + LPB*8); */
/*       if (status) return status; */

      /* register a polling routine */
      register_irig_callback( &arinc_sweep, hdl->poll, (void *)chn );

      err("ARINC_GO: opened '%s' poll[%d] = %d Hz)", hdl->fname, chn, irigClockEnumToRate(hdl->poll));
    }
    /* launch the board */
    status = ar_go(BOARD_NUM);
    if (status != ARS_NORMAL) goto ar_fail;

    running = 1;
    err("ARINC_GO: board launched");
    break;

  case ARINC_RESET:

    running = 0;

    /* unregister poll recv routine with the IRIG driver */
    err("ARINC_RESET: unregister_irig_callback(&arinc_sweep, poll[%d] = %d Hz)",
        chn, irigClockEnumToRate(hdl->poll));
    unregister_irig_callback( &arinc_sweep, hdl->poll );

    /* close its output FIFO */
    if (hdl->fd > -1) {
      int temp_fd = hdl->fd;
      err("ARINC_RESET: chn: %d rtl_close(hdl->fd);", chn);
      hdl->fd = -1;
      rtl_close(temp_fd);
    }
    else
      err("ARINC_RESET: chn: %d skipping rtl_close(hdl->fd);", chn);

    /* clear out the channel's info structure */
    hdl->fd            = -1;
    hdl->lps           = 0;
    hdl->poll          = 0;
    hdl->overflow      = 0;
    hdl->underflow     = 0;
    hdl->sim_xmit      = 0;
    hdl->nArcfg        = 0;
    for (lbl=0; lbl<0400; lbl++) {
      hdl->rate[lbl]   = 0;
      hdl->msg_id[lbl] = 0;
      hdl->arcfgs[lbl] = 0;
    }

    /* filter out all labels on this channel */
    status = ar_label_filter(BOARD_NUM, chn, ARU_ALL_LABELS, ARU_FILTER_ON);
    if (status != ARS_NORMAL) goto ar_fail;

    /* stop the board */
    status = ar_reset(BOARD_NUM);
    if (status != ARS_NORMAL) goto ar_fail;

    break;

  case ARINC_SIM_XMIT:

    /* define a periodic messages for the i960 to generate */
    if (hdl->lps)
      return -RTL_EINVAL;

    if (chn > N_ARINC_TX - 1)
      return -RTL_EINVAL;

    hdl->sim_xmit = 1;
    break;

  case ARINC_BIT:

    if (running) return -RTL_EALREADY;

    if (hdl->lps) return -RTL_EALREADY;

    /* Perform a series of Built In Tests in the card. */
    short test_type = *(short*) buf;
    status = ar_execute_bit(BOARD_NUM, test_type);
    if (status != ARS_NORMAL) goto ar_fail;
    break;

  case ARINC_STAT:

    if (hdl->lps) {

      /* stop displaying measurements */
      display_chn = -1;

      /* move cursor to upper left corner and clear the screen */
      rtl_printf( "%c[2J%c[H", 27, 27 );

      /* Display the current configuration. */
      rtl_printf("Channel %d's device generates a total of %d labels/sec\n", chn, hdl->lps);
      rtl_printf("Channel %d is polled at %d Hz\n", chn, irigClockEnumToRate(hdl->poll));
      rtl_printf("Channel %d has  overflowed %d times\n", chn,  hdl->overflow);
      rtl_printf("Channel %d has underflowed %d times\n", chn, hdl->underflow);
      rtl_printf("\n");
      rtl_printf(" lble rate  | lble rate  | lble rate  | lble rate  | lble rate  | lble rate  | lble rate  | lble rate  \n");
      rtl_printf("------------+------------+------------+------------+------------+------------+------------+------------\n");
      for (lbl=0; lbl<0400; lbl+=8)
        rtl_printf(" %04o %3d %X | %04o %3d %X | %04o %3d %X | %04o %3d %X | %04o %3d %X | %04o %3d %X | %04o %3d %X | %04o %3d %X\n",
                   lbl+0, hdl->rate[lbl+0], ar_get_label_filter(BOARD_NUM, lbl+0),
                   lbl+1, hdl->rate[lbl+1], ar_get_label_filter(BOARD_NUM, lbl+1),
                   lbl+2, hdl->rate[lbl+2], ar_get_label_filter(BOARD_NUM, lbl+2),
                   lbl+3, hdl->rate[lbl+3], ar_get_label_filter(BOARD_NUM, lbl+3),
                   lbl+4, hdl->rate[lbl+4], ar_get_label_filter(BOARD_NUM, lbl+4),
                   lbl+5, hdl->rate[lbl+5], ar_get_label_filter(BOARD_NUM, lbl+5),
                   lbl+6, hdl->rate[lbl+6], ar_get_label_filter(BOARD_NUM, lbl+6),
                   lbl+7, hdl->rate[lbl+7], ar_get_label_filter(BOARD_NUM, lbl+7));
    }
    else
      rtl_printf("\n\nChannel %d is unused\n\n\n", chn);
    break;

  case ARINC_MEASURE:

    /* toggle or change the channel that we are displaying */
    if (display_chn == chn) display_chn = -1;
    else                    display_chn = chn;

    /* move cursor to upper left corner and clear the screen */
/*     rtl_printf( "%c[2J%c[H", 27, 27 ); */
    break;

  default:
    err("unknown ioctl cmd");
    break;
  }
  return len;

 ar_fail:
  error_exit(BOARD_NUM, status);
  return -RTL_EIO;
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
  len[port] += rtl_read(siginfo->si_fd, &buf[port][len[port]], RTL_SSIZE_MAX);
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
static void __exit arinc_cleanup(void)
{
  int chn;

  /* unregister a timesync routine */
  unregister_irig_callback( &arinc_timesync, sync_rate );

  /* for each ARINC receive port... */
  for (chn=0; chn<N_ARINC_RX; chn++) {
    struct recvHandle *hdl = &chn_info[chn];
    arinc_ioctl(ARINC_RESET, BOARD_NUM, chn, NULL, 0);
    rtl_unlink( hdl->fname );
  }
  /* for each ARINC transmit port... */
  char devstr[30];
  for (chn=0; chn<N_ARINC_TX; chn++) {
    sprintf(devstr, "/dev/arinc_out_%d", chn);
    rtl_close( fd_arinc_tx[chn] );
    rtl_unlink( devstr );
  }
  /* Close my ioctl FIFO, deregister my arinc_ioctl function */
  if (ioctlhandle)
    closeIoctlFIFO(ioctlhandle);

  /* free up the ISA memory region */
  if ( requested_region )
    release_region(basemem, PAGE_SIZE);

  /* free up the sample buffer */
  if (sample)
    rtl_gpos_free(sample);

  /* close the board */
  short status = ar_close(BOARD_NUM);
  if (status != ARS_NORMAL)
    error_exit(BOARD_NUM, status);

  /* unmap the DPRAM address */
  if (phys_membase)
    iounmap(phys_membase);

  err("done.\n");
}
/* -- UTILITY --------------------------------------------------------- */
/**
 * There are not a lot of tests to perform to see if it is a
 * CEI 420 card. The tests performed are:
 *   - configuration register 1 : looking at bits 4-7, bit 6 is set
 *                                (CEI-420-XXJ) or bit 4 and 5 are
 *                                set (CEI-420-70J)
 *   - configuration register 2 : value has to be 0x0
 *   - configuration register 3 : value of bits 4-7 has to be 0x0
 */
static int scan_ceiisa( void )
{
  unsigned char value;
  unsigned int indx;

  char *boardID[] = {"Standard CEI-220",
                     "Standard CEI-420",
                     "Custom CEI-220 6-Wire",
                     "CEI-420-70J",
                     "CEI-420-XXJ",
                     "Obsolete",
                     "CEI-420A-42-A",
                     "CEI-420A-XXJ"};

  value = readb(phys_membase + 0x808);
  value >>= 4;
  if (value != 0x0)
  {
    /* passed register 1 test... */
    value = readb(phys_membase + 0x80A);
    value >>= 4;
    if (value == 0x5)
      err("Obsolete CEI-420a");
    if (value == 0x0 || value == 0x1 || value == 0x2 ||
        value == 0x3 || value == 0x4 || value == 0x6 || value == 0x7)
    {
      /* passed register 2 test... */
      indx = value;
      value = readb(phys_membase + 0x80C);
      if (value == 0x0)
      {
        /* passed register 3 test... */
        value = readb(phys_membase + 0x80E);
        value >>= 4;
        if(value == 0)
        {
          /* passed register 4 test... */
          err("cei220/420 found.  Board = %s", boardID[indx]);
          err("found CEI-220/420 at 0x%lX", basemem);
          return 0;
        }
      }
    }
  }
  return -RTL_ENODEV;
}
/* -- MODULE ---------------------------------------------------------- */
static int __init arinc_init(void)
{
  err("compiled on %s at %s", __DATE__, __TIME__);
  int chn;

  /* map ISA card memory into kernel memory */
  phys_membase = ioremap(basemem, PAGE_SIZE);
  if (!phys_membase) {
    err("ioremap failed.\n");
    return -RTL_EIO;
  }
  /* scan the ISA bus for the device */
  short status = scan_ceiisa();
  if (status) goto fail;

  // obtain the API version string
  char api_version[150];
  ar_version(api_version);
  err("API Version %s", api_version);

  /* load the board (the size and address are not used - must specify zero) */
  status = ar_loadslv(BOARD_NUM,0,0,0);
  if (status != ARS_NORMAL) goto fail;

  /* initialize the slave */
  status = ar_init_slave(BOARD_NUM);
  if (status != ARS_NORMAL) goto fail;

  /* Display the board type */
  err("Board %s detected", ar_get_boardname(BOARD_NUM,NULL));
  err("Supporting %d transmitters and %d receivers.",
      ar_num_xchans(BOARD_NUM), ar_num_rchans(BOARD_NUM));

  /* select buffered mode */
  status = ar_set_storage_mode(BOARD_NUM, ARU_BUFFERED);
  if (status != ARS_NORMAL) goto fail;

  /* adjust the i960's internal clock rate */
  status = ar_set_timerrate(BOARD_NUM, 4*1000);
  if (status != ARS_NORMAL) goto fail;

  /* select high speed transmit */
  status = ar_set_config(BOARD_NUM, ARU_XMIT_RATE, AR_HIGH);
  if (status != ARS_NORMAL) goto fail;

  /* enable scheduled mode (clears the buffer of scheduled messages) */
  status = ar_msg_control(BOARD_NUM, AR_ON);
  if (status != ARS_NORMAL) goto fail;

  // disable internal wrap
  status = ar_set_config(BOARD_NUM, ARU_INTERNAL_WRAP, AR_WRAP_OFF);
  if (status != ARS_NORMAL) goto fail;

  /* instruct the board to time tag each label */
  status = ar_timetag_control(BOARD_NUM, ARU_ENABLE_TIMETAG);
  if (status != ARS_NORMAL) goto fail;

  /* prematurely launch the board (it will be reset and re-launched via ioctls) */
  status = ar_go(BOARD_NUM);
  if (status != ARS_NORMAL) goto fail;

  /* sync up the i960's internal clock to the IRIG time */
  ar_set_timercnt(BOARD_NUM, GET_MSEC_CLOCK);

  /* register a timesync routine */
  register_irig_callback( &arinc_timesync, sync_rate, (void *)0 );

  /* for each ARINC receive port... */
  for (chn=0; chn<N_ARINC_RX; chn++)
  {
/*     /\* set channel parity (TODO pass as a config item?) *\/ */
/*     status = ar_set_config(BOARD_NUM, ARU_RX_CH01_PARITY+chn, parity); */
/*     if (status != ARS_NORMAL) goto fail; */
/*     ck = ar_get_config(BOARD_NUM, ARU_RX_CH01_PARITY+chn); */
/*     err("parity = %d ck = %d", parity, ck); */

/*     /\* set channel speed (TODO pass as a config item?) *\/ */
/*     status = ar_set_config(BOARD_NUM, ARU_RX_CH01_BIT_RATE+chn, speed); */
/*     if (status != ARS_NORMAL) goto fail; */
/*     ck = ar_get_config(BOARD_NUM, ARU_RX_CH01_BIT_RATE+chn); */
/*     err("speed  = %d ck = %d", speed, ck); */

    // remove broken device file before making a new one
    struct recvHandle *hdl = &chn_info[chn];
    sprintf( hdl->fname, "/dev/arinc_in_%d", chn );

    status = rtl_unlink(hdl->fname);
    if ( status<0 && rtl_errno != RTL_ENOENT ) goto fail;

    status = rtl_mkfifo( hdl->fname, 0666 );
    if (status<0) goto fail;
  }

  /* for each ARINC transmit port... */
  char devstr[30];
  for (chn=0; chn<N_ARINC_TX; chn++)
  {
    // remove broken device file before making a new one
    sprintf(devstr, "/dev/arinc_out_%d", chn);

    status = rtl_unlink(devstr);
    if ( status<0 && rtl_errno != RTL_ENOENT ) goto fail;

    status = rtl_mkfifo( devstr, 0666 );
    if (status<0) goto fail;
    status = fd_arinc_tx[chn] = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_RDONLY );
    if (status<0) goto fail;

    /* create its real-time FIFO handler */
    rtl_memset(&cmndAct[chn],0,sizeof(cmndAct[chn]));
    cmndAct[chn].sa_sigaction = arinc_transmit;
    cmndAct[chn].sa_fd        = fd_arinc_tx[chn];
    cmndAct[chn].sa_flags     = RTL_SA_RDONLY | RTL_SA_SIGINFO;
    status = rtl_sigaction( RTL_SIGPOLL, &cmndAct[chn], NULL );
    if (status) goto fail;
  }

  /* open up my ioctl FIFO, register my arinc_ioctl function */
  ioctlhandle = openIoctlFIFO("arinc", BOARD_NUM, arinc_ioctl,
                              nioctlcmds, ioctlcmds);
  if (!ioctlhandle) { status = -RTL_EIO; goto fail; }

  /* reserve the ISA memory region */
  if (!request_region(basemem, PAGE_SIZE, "arinc"))
  {
    err("couldn't allocate I/O range %x - %x\n", basemem,
        basemem + PAGE_SIZE - 1);
    status = -RTL_EBUSY;
    goto fail;
  }
  requested_region = 1;

  /* reserve some RAM for the sample buffer */
  sample = rtl_gpos_malloc(SIZEOF_DSM_SAMPLE_HEADER + LPB*8);
  if (sample < 0) goto fail;

  err("done.\n");
  return 0; /* success */

 fail:
  arinc_cleanup();
  if (status==-1)
    return -rtl_errno;
  if (status<0)
    return status;

  /* ar_???() error codes are positive... */
  error_exit(BOARD_NUM, status);
  return -RTL_EIO;
}

module_init(arinc_init);
module_exit(arinc_cleanup);

MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("CEI420a ISA driver for RTLinux");
