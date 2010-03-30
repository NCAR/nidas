/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedRevision$
        $LastChangedDate$
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

#define RAM_CLEAR_OFFSET         0x00
#define PEAK_CLEAR_OFFSET        0x02
#define AVG_LSW_DATA_OFFSET      0x04
#define AVG_MSW_DATA_OFFSET      0x06
#define PEAK_DATA_OFFSET         0x08
#define TAS_BELOW_OFFSET         0x0A
#define TAS_ABOVE_OFFSET         0x0C

#define REGION_SIZE 0x10  // number of 1-byte registers
#define BOARD_NUM   0

volatile unsigned int baseAddr;
static const char* devprefix = "lams";
static struct ioctlHandle* ioctlHandle = 0;
static rtl_sem_t threadSem;
static rtl_pthread_t lamsThread = 0;
static int fd_lams_data = 0;
static struct lamsPort _lamsPort;
static struct lams_status status = {0};

static struct ioctlCmd ioctlcmds[] = {
   { GET_NUM_PORTS,       _IOC_SIZE(GET_NUM_PORTS) },
   { LAMS_SET_CHN,        _IOC_SIZE(LAMS_SET_CHN)  },
   { LAMS_N_AVG,          _IOC_SIZE(LAMS_N_AVG)         },
   { LAMS_N_PEAKS,        _IOC_SIZE(LAMS_N_PEAKS)       },
   { LAMS_TAS_BELOW,      _IOC_SIZE(LAMS_TAS_BELOW)     },
   { LAMS_TAS_ABOVE,      _IOC_SIZE(LAMS_TAS_ABOVE)     },
   { LAMS_GET_STATUS,     _IOC_SIZE(LAMS_GET_STATUS)   },
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

// -- UTIL ---------------------------------------------------------------------
static int openRTfifo()
{
   char devstr[30];
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
   return 0;
}

unsigned int channel = 0;
unsigned int nAVG   = 80;
unsigned int nPEAKS = 2000;

// -- THREAD -------------------------------------------------------------------
static void *lams_thread (void * chan)
{
   struct rtl_timespec timeout; // semaphore timeout in nanoseconds

   rtl_clock_gettime(RTL_CLOCK_REALTIME,&timeout);
   timeout.tv_sec = 0;
   timeout.tv_nsec = 0;

   for (;;) {
      timeout.tv_nsec += 600 * NSECS_PER_MSEC;
      if (timeout.tv_nsec >= NSECS_PER_SEC) {
         timeout.tv_sec++;
         timeout.tv_nsec -= NSECS_PER_SEC;
      }
//    if (rtl_sem_timedwait(&threadSem, &timeout) < 0) {
      if (rtl_sem_wait(&threadSem) < 0) {
         DSMLOG_DEBUG("thread timed out!\n");
         // timed out!  flush the hardware FIFO
      } else {

         if (fd_lams_data) {
           _lamsPort.timetag = GET_MSEC_CLOCK;
           if (rtl_write(fd_lams_data,&_lamsPort,
                         SIZEOF_DSM_SAMPLE_HEADER + _lamsPort.size) < 0) {
              DSMLOG_ERR("error: write: %s.\n",
                         rtl_strerror(rtl_errno));
	   }
	}
      }
      if (rtl_errno == RTL_EINTR) return 0; // thread interrupted
   }
}

static unsigned long	peak[MAX_BUFFER];
static unsigned long long	sum[MAX_BUFFER];

// -- INTERRUPT SERVICE ROUTINE ------------------------------------------------
static unsigned int lams_isr (unsigned int irq, void* callbackPtr,
                              struct rtl_frame *regs)
{
   unsigned long i, msw, lsw, apk, word;
   static unsigned int nTattle=0;
   static unsigned int nGlyph=0; 
   static unsigned int nAvg=0;
   static unsigned int nPeaks=0;

   if (++nPeaks >= nPEAKS) {
      inw(baseAddr + PEAK_CLEAR_OFFSET);
      memset((void *)peak, 0, sizeof(peak));
      nPeaks = 0;
   }

   // Clear Dual Port memory address counter
   inw(baseAddr + RAM_CLEAR_OFFSET);

   for (i = 0; i < MAX_BUFFER+4; i++) {
      lsw = inw(baseAddr + AVG_LSW_DATA_OFFSET);
      msw = inw(baseAddr + AVG_MSW_DATA_OFFSET);
      apk = inw(baseAddr + PEAK_DATA_OFFSET);

      if(i >= 3) {
        peak[i-3] = apk;
       if (peak[i-3] < apk) peak[i-4] = apk;
      }
//       if (peak[n] < apk) peak[n] = apk;
      
      if(i >= 5) {
        word = (msw << 16) + lsw;
        sum[i-4] += (unsigned long long)word;
      }
      sum[0] = sum[1];
   }

   if (++nTattle >= 1024) {
      nTattle = 0;
      if (++nGlyph == MAX_BUFFER) nGlyph = 0;
        DSMLOG_DEBUG("(%03d) avrg: 0x%08x   peak: 0x%04x\n",
                     nGlyph, _lamsPort.avrg[nGlyph], peak[nGlyph]);
   }

   if (++nAvg >= nAVG) {
      for (i = 0; i < MAX_BUFFER; i++) {
         _lamsPort.peak[i] = (unsigned short)peak[i];
         word = (unsigned long)(sum[i] / nAvg);
         _lamsPort.avrg[i] = word;
      }
      memset((void *)sum, 0, sizeof(sum));
      nAvg = 0;
      if (channel)
         rtl_sem_post( &threadSem );
   }

   return 0;
}

// -- IOCTL --------------------------------------------------------------------
 /*
  * Return: negative Linux errno (not RTLinux errnos), or 0=OK
  */
static int ioctlCallback(int cmd, int board, int chn,
                         void *buf, rtl_size_t len)
{
   int err, ret = -EINVAL;
   struct lams_set* lams_ptr;

   switch (cmd)
   {
      case GET_NUM_PORTS:
         DSMLOG_DEBUG("GET_NUM_PORTS\n");
         *(int *) buf = N_CHANNELS;
         ret = len;
         break;

      case LAMS_SET_CHN:
         DSMLOG_DEBUG("LAMS_SET_CHN\n");
         lams_ptr = (struct lams_set*) buf;
         channel = lams_ptr->channel;
         ret = len;
         DSMLOG_DEBUG("channel:       %d\n", channel);

         if (channel == 0) {
            if (fd_lams_data)
               rtl_close( fd_lams_data );
            break;
         }
         // open the channel's data FIFO
         err = openRTfifo();
         if (err) ret = err;
         break;
      
      case LAMS_TAS_BELOW:
         inw(baseAddr + TAS_BELOW_OFFSET);
         DSMLOG_DEBUG("TAS_BELOW\n");
         ret = len;
         break;

      case LAMS_TAS_ABOVE:
         inw(baseAddr + TAS_ABOVE_OFFSET);
         DSMLOG_DEBUG("TAS_ABOVE\n");
         ret = len;
         break;

      case LAMS_N_AVG:
         nAVG = *(unsigned int*) buf;
         DSMLOG_DEBUG("nAVG:          %d\n", nAVG);
         ret = len;
         break;

      case LAMS_N_PEAKS:
         nPEAKS = *(unsigned int*) buf;
         DSMLOG_DEBUG("nPEAKS:        %d\n", nPEAKS);
         ret = len;
         break;
      case LAMS_GET_STATUS:
         if (len != sizeof (status)) break;
         *((struct lams_status*) buf) = status;
         ret = len;
         break;
  
      default:
         break;
   }
   return ret;
}


// -- CLEANUP MODULE -----------------------------------------------------------
void cleanup_module (void)
{
   // undo what was done in reverse order upon cleanup
   rtl_free_isa_irq(irq);
   rtl_pthread_kill(lamsThread, SIGTERM);
   rtl_pthread_join(lamsThread, NULL);
   rtl_sem_destroy(&threadSem);
   release_region(baseAddr, REGION_SIZE);
   closeIoctlFIFO(ioctlHandle);

   // close the RTL data fifo
   DSMLOG_DEBUG("closing fd_lams_data: %x\n", fd_lams_data);
   if (fd_lams_data)
      rtl_close( fd_lams_data );

   // destroy the RTL data fifo
   char devstr[64];
   sprintf( devstr, "%s/lams_in_%d", getDevDir(), 0);
   rtl_unlink( devstr );

   DSMLOG_DEBUG("done\n");
}
// -- INIT MODULE --------------------------------------------------------------
int init_module (void)
{
   baseAddr = SYSTEM_ISA_IOPORT_BASE + ioport;
   DSMLOG_NOTICE("--------------------------------------------------\n");
   DSMLOG_NOTICE("compiled on %s at %s\n", __DATE__, __TIME__);
   DSMLOG_NOTICE("MAX_BUFFER:         %d\n", MAX_BUFFER);
   DSMLOG_NOTICE("sizeof(long long):  %d\n", sizeof(long long)); // 8
   DSMLOG_NOTICE("sizeof(long):       %d\n", sizeof(long));      // 4
   DSMLOG_NOTICE("sizeof(int):        %d\n", sizeof(int));       // 4
   DSMLOG_NOTICE("sizeof(short):      %d\n", sizeof(short));     // 2
   DSMLOG_NOTICE("sizeof(char):       %d\n", sizeof(char));      // 1
   DSMLOG_NOTICE("--------------------------------------------------\n");

   // open up ioctl FIFO, register ioctl function
   createFifo("_in_", BOARD_NUM);
   ioctlHandle = openIoctlFIFO(devprefix, BOARD_NUM, ioctlCallback,
                               nioctlcmds, ioctlcmds);
   if (!ioctlHandle) return -RTL_EIO;

   request_region(baseAddr, REGION_SIZE, "lams");
   _lamsPort.timetag = 0;
   memset((void *)&_lamsPort, 0, sizeof(_lamsPort));
   memset(peak, 0, sizeof(peak));

   // this is the size of the data portion only, NOT the timetag and size fields!
   _lamsPort.size = sizeof(_lamsPort.avrg) + sizeof(_lamsPort.peak);

   DSMLOG_DEBUG("// initialize the semaphore to the thread\n");
   // initialize the semaphore to the thread
   if ( rtl_sem_init( &threadSem, 1, 0) < 0) {
      DSMLOG_ERR("rtl_sem_init failure: %s\n", rtl_strerror(rtl_errno));
      goto rtl_sem_init_err;
   }
   DSMLOG_DEBUG("// create the thread to write data to the FIFO\n");
   // create the thread to write data to the FIFO
   if (rtl_pthread_create( &lamsThread, NULL,
                           lams_thread, (void *)0)) {
      DSMLOG_ERR("rtl_pthread_create failure: %s\n", rtl_strerror(rtl_errno));
      goto rtl_pthread_create_err;
   }
   DSMLOG_DEBUG("// activate interupt service routine\n");
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
