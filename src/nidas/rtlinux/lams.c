/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    RTLinux LAMS driver for the ADS3 DSM.

 ********************************************************************
*/

#define __RTCORE_POLLUTED_APP__

#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_pthread.h>
#include <rtl_semaphore.h>
#include <rtl_core.h>
#include <rtl_unistd.h>
#include <rtl_time.h>
#include <rtl_posixio.h>
#include <rtl_stdio.h>
#include <linux/ioport.h>

#include <nidas/rtlinux/dsmlog.h>
#include <nidas/rtlinux/dsm_viper.h>
#include <nidas/rtlinux/rtl_isa_irq.h>
#include <nidas/rtlinux/irigclock.h>
#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/rtlinux/dsm_version.h>

#include <linux/kernel.h>
#include <linux/termios.h>
#include <nidas/rtlinux/lams.h>

RTLINUX_MODULE(lams);

MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("LAMS ISA driver for RTLinux");

// module parameters (can be passed in via command line)
static int irq = 4;
static int ioport = 0x220;
MODULE_PARM(irq, "i");
MODULE_PARM_DESC(irq, "ISA irq setting (default 4)");

MODULE_PARM(ioport, "i");
MODULE_PARM_DESC(ioport, "ISA memory base (default 0x220)");

volatile unsigned long baseAddr;
static const char* devprefix = "lams";
static struct ioctlHandle* ioctlHandle = 0;
static rtl_sem_t threadSem;
static rtl_pthread_t lamsThread = 0;
static int fd_lams_data = 0;
static struct lamsPort lams;

static struct ioctlCmd ioctlcmds[] = {
   { GET_NUM_PORTS,  _IOC_SIZE(GET_NUM_PORTS) },
   { LAMS_SET_CHN,   _IOC_SIZE(LAMS_SET_CHN)  },
   { AIR_SPEED,      _IOC_SIZE(AIR_SPEED)     },
};
static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);

// -- UTIL ---------------------------------------------------------------------
// TODO - share this with other modules
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
#if 0
static void read_radar(void * channel)
{
  MESA_SIXTEEN_BIT_SAMPLE sample;
  struct MESA_Board * brd = boardInfo;

  // read from the radar channel
  sample.sampleID = ID_RADAR;
  sample.size = sizeof(dsm_sample_id_t) + sizeof(short) * brd->nRadars;
  sample.timetag = GET_MSEC_CLOCK;
  sample.data[0] = inw(brd->addr + RADAR_READ_OFFSET);
//DSMLOG_DEBUG("chn: %d  sample.data: %d\n", 0, sample.data[0]);

  // write the altitude to the user's FIFO
  rtl_write(brd->outfd, &sample, sample.size + sizeof(dsm_sample_length_t)
        + sizeof(dsm_sample_time_t));
}
#endif

// -- THREAD -------------------------------------------------------------------
static void *lams_thread (void * chan)
{
   int n;
   struct rtl_timespec timeout; // semaphore timeout in nanoseconds

   rtl_clock_gettime(RTL_CLOCK_REALTIME,&timeout);
   timeout.tv_sec = 0;
   timeout.tv_nsec = 0;

   for (;;) {
      timeout.tv_nsec += 300 * NSECS_PER_MSEC;
      if (timeout.tv_nsec >= NSECS_PER_SEC) {
         timeout.tv_sec++;
         timeout.tv_nsec -= NSECS_PER_SEC;
      }
      if (rtl_sem_timedwait(&threadSem, &timeout) < 0) {
         DSMLOG_DEBUG("thread timed out!\n");
         readw(baseAddr + FLAGS_OFFSET);
         // timed out!  flush the hardware FIFO
         for (n=0; n<MAX_BUFFER; n++)
            readw(baseAddr + DATA_OFFSET);
      } else {

         // TODO this is constant... set its value in ...::init()
         lams.size = MAX_BUFFER * sizeof(short);
         if (fd_lams_data) {
           DSMLOG_DEBUG("lams.size:    %d\n", lams.size);
           lams.timetag = GET_MSEC_CLOCK;
  if (rtl_write(fd_lams_data,&lams, SIZEOF_DSM_SAMPLE_HEADER + lams.size) < 0) {

              DSMLOG_ERR("error: write: %s. Closing\n",
                         rtl_strerror(rtl_errno));
              rtl_close(fd_lams_data);
              fd_lams_data = 0;
	   }
	}
      }
      if (rtl_errno == RTL_EINTR) return 0; // thread interrupted
   }
}
// -- INTERRUPT SERVICE ROUTINE ------------------------------------------------
static unsigned int lams_isr (unsigned int irq, void* callbackPtr,
                              struct rtl_frame *regs)
{
   static int nTattle, nGlyph;
   static unsigned long sum[MAX_BUFFER], max1[MAX_BUFFER], temp0[MAX_BUFFER];
   static unsigned long temp1[MAX_BUFFER], max;
   static int n, nAvg = 0;

   readw(baseAddr + RAM_CLEAR_OFFSET); //Clear Dual Port memory address counter

  for (n=3; n < MAX_BUFFER; n++) {
     temp0[n] = readw(baseAddr + PEAK_DATA_OFFSET);
     temp1[n] = readw(baseAddr + AVG_DATA_OFFSET);
  }
  for (n=0; n < 3; n++) {
     temp0[n] = readw(baseAddr + PEAK_DATA_OFFSET);
     temp1[n] = readw(baseAddr + AVG_DATA_OFFSET);
  }
  for (n=256; n < MAX_BUFFER; n++) {
     sum[n] += temp1[n];
     if(temp0[n] > max1[n-256]) {
       max1[n-256] = temp0[n];
     }
     if(temp0[n] > max) {
       max = temp0[n];
       nGlyph = n-256;
     }
   }
   if (n == MAX_BUFFER) {
     if (nAvg++ >= N_AVG) {
       nAvg = 0;
       for (n=0; n < MAX_BUFFER; n++) {
         if (n < MAX_BUFFER/2) {
           lams.data[n] = max1[n]*300;
         }
         else {
           lams.data[n] = sum[n]/N_AVG ;
         }
         sum[n] = 0;
       }

       rtl_sem_post( &threadSem );
       if (++nTattle == N_PEAK) {
         nTattle = 0;
         DSMLOG_DEBUG("(%d) lams.data: 0x%04x, max: 0x%04x\n",
                      nGlyph,lams.data[nGlyph], max);
         readw(baseAddr + PEAK_CLEAR_OFFSET); 
         for (n=0; n < MAX_BUFFER; n++) {
           max1[n] = 0;
         }
       }
     }
   }
   return 0;
}
// -- IOCTL --------------------------------------------------------------------
static int ioctlCallback(int cmd, int board, int chn,
                         void *buf, rtl_size_t len)
{
   int ret = len;
   unsigned int airspeed, lams_channels;
   struct lams_set* lams_ptr;
   char devstr[30];

   switch (cmd)
   {
      case GET_NUM_PORTS:
         DSMLOG_DEBUG("GET_NUM_PORTS\n");
         *(int *) buf = N_CHANNELS;
         break;

      case LAMS_SET_CHN:
         DSMLOG_DEBUG("LAMS_SET_CHN\n");
         lams_ptr = (struct lams_set*) buf;
         lams_channels = lams_ptr->channel;
         DSMLOG_DEBUG("LAMS_SET_CHN lams_channels=%d\n", lams_channels);

         // open the channel's data FIFO
         sprintf( devstr, "%s/lams_in_%d", getDevDir(), 0);
         DSMLOG_DEBUG("opening %s\n",devstr);
         fd_lams_data = rtl_open( devstr, RTL_O_NONBLOCK | RTL_O_WRONLY );
         if (fd_lams_data < 0) {
               DSMLOG_ERR("error: open %s: %s\n", devstr,
                          rtl_strerror(rtl_errno));
            fd_lams_data = 0;
            return -convert_rtl_errno(rtl_errno);
         }
   	 DSMLOG_DEBUG("LAMS_SET_CHN fd_lams_data: %x\n", fd_lams_data);
         break;
      
      case AIR_SPEED:
         DSMLOG_DEBUG("AIR_SPEED\n");
         airspeed = *(unsigned int*) buf;
//       writew(airspeed, baseAddr + AIR_SPEED_OFFSET);
         break;
	 	
      default:
         ret = -RTL_EIO;
         break;
   }
   return ret;
}


// -- CLEANUP MODULE -----------------------------------------------------------
void cleanup_module (void)
{
   DSMLOG_DEBUG("start\n");

   // close the RTL data fifo
   DSMLOG_DEBUG("closing fd_lams_data: %x\n", fd_lams_data);
   if (fd_lams_data)
      rtl_close( fd_lams_data );

   // destroy the RTL data fifo
   char devstr[30];
   sprintf( devstr, "%s/lams_in_%d", getDevDir(), 0);
   rtl_unlink( devstr );

   // undo what was done in reverse order upon cleanup
   rtl_free_isa_irq(irq);
   rtl_pthread_kill(lamsThread, SIGTERM);
   rtl_pthread_join(lamsThread, NULL);
   rtl_sem_destroy(&threadSem);
   release_region(baseAddr, REGION_SIZE);
   closeIoctlFIFO(ioctlHandle);

   DSMLOG_DEBUG("done\n");
}
// -- INIT MODULE --------------------------------------------------------------
int init_module (void)
{
   baseAddr = SYSTEM_ISA_IOPORT_BASE + ioport;
   DSMLOG_NOTICE("compiled on %s at %s by Ling\n", __DATE__, __TIME__);

   // open up ioctl FIFO, register ioctl function
   createFifo("_in_", BOARD_NUM);
   ioctlHandle = openIoctlFIFO(devprefix, BOARD_NUM, ioctlCallback,
                               nioctlcmds, ioctlcmds);
   if (!ioctlHandle) return -RTL_EIO;

   request_region(baseAddr, REGION_SIZE, "lams");
   int n;
   lams.timetag = 0;
   for (n=0; n<MAX_BUFFER; n++) lams.data[n] = 0;

   // initialize the semaphore to the thread
   if ( rtl_sem_init( &threadSem, 1, 0) < 0) {
      DSMLOG_ERR("rtl_sem_init failure: %s\n", rtl_strerror(rtl_errno));
      goto rtl_sem_init_err;
   }
   // create the thread to write data to the FIFO
   if (rtl_pthread_create( &lamsThread, NULL,
                           lams_thread, (void *)0)) {
      DSMLOG_ERR("rtl_pthread_create failure: %s\n", rtl_strerror(rtl_errno));
      goto rtl_pthread_create_err;
   }
   // activate interupt service routine
   if (rtl_request_isa_irq(irq, lams_isr, 0) < 0) {
      DSMLOG_ERR("rtl_request_isa_irq failure: %s\n", rtl_strerror(rtl_errno));
      goto rtl_request_isa_irq_err;
   }
   DSMLOG_DEBUG("done\n");
   return 0;

   // undo what was done in reverse order upon failure
   rtl_request_isa_irq_err: rtl_free_isa_irq(irq);
                            rtl_pthread_kill(lamsThread, SIGTERM);
                            rtl_pthread_join(lamsThread, NULL);
   rtl_pthread_create_err:  rtl_sem_destroy(&threadSem);
   rtl_sem_init_err:        release_region(baseAddr, REGION_SIZE);
                            closeIoctlFIFO(ioctlHandle);
   return -convert_rtl_errno(rtl_errno);
}
