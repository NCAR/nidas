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
static struct lamsPort _lamsPort;

static struct ioctlCmd ioctlcmds[] = {
   { GET_NUM_PORTS,  _IOC_SIZE(GET_NUM_PORTS) },
   { LAMS_SET_CHN,   _IOC_SIZE(LAMS_SET_CHN)  },
   { AIR_SPEED,      _IOC_SIZE(AIR_SPEED)     },
   { N_AVG,          _IOC_SIZE(N_AVG)         },
   { N_SKIP,         _IOC_SIZE(N_SKIP)        },
   { CALM,           _IOC_SIZE(CALM)          },
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

#define REGION_SIZE 0x10  // number of 1-byte registers
#define BOARD_NUM   0

unsigned int channel = 0;
unsigned int nAVG  = 8;
unsigned int nSKIP = 50;
unsigned int calm = 0;

// -- THREAD -------------------------------------------------------------------
static void *lams_thread (void * chan)
{
   struct rtl_timespec timeout; // semaphore timeout in nanoseconds

   rtl_clock_gettime(RTL_CLOCK_REALTIME,&timeout);
   timeout.tv_sec = 0;
   timeout.tv_nsec = 0;

   int fp=0;
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

         if (fp<10) DSMLOG_DEBUG(">>>>>>>>>>>>>>>>>>>>>>>>> first post: %d\n", fp++);
         // TODO this is constant... set its value in ...::init()
	 // is it the size of the data portion only, NOT the timetag and size fields!
         _lamsPort.size = sizeof(_lamsPort.data);

         if (fd_lams_data) {
           _lamsPort.timetag = GET_MSEC_CLOCK;
           if (rtl_write(fd_lams_data,&_lamsPort, SIZEOF_DSM_SAMPLE_HEADER + _lamsPort.size) < 0) {
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

static unsigned long long sum[MAX_BUFFER];
static unsigned long long calm_spectrum[MAX_BUFFER];

// -- INTERRUPT SERVICE ROUTINE ------------------------------------------------
static unsigned int lams_isr (unsigned int irq, void* callbackPtr,
                              struct rtl_frame *regs)
{
// static int fp=0;
// if (fp<10) DSMLOG_DEBUG("---------- lams_isr %d ----------\n", fp++);

   unsigned int msw, lsw;
   static int nTattle=0;
   static int nGlyph=0; 
   static int nAvg=0;
   static int nSkip=0;

   int n;
   int s=0;

   //Clear Dual Port memory address counter
   readw(baseAddr + RAM_CLEAR_OFFSET);

   for (n=3; n < MAX_BUFFER; n++) {
      msw = (short)readw(baseAddr + PEAK_DATA_OFFSET);
      lsw = (short)readw(baseAddr + AVG_DATA_OFFSET);
      if (channel)
         sum[s++] += (msw << 16) + lsw;
   }
   for (n=0; n < 3; n++) {
      msw = (short)readw(baseAddr + PEAK_DATA_OFFSET);
      lsw = (short)readw(baseAddr + AVG_DATA_OFFSET);
      if (channel)
         sum[s++] += (msw << 16) + lsw;
   }
   if (++nTattle == 1024) {
      nTattle = 0;
      if (++nGlyph == 256) nGlyph = 0;
         DSMLOG_DEBUG("(%03d) _lamsPort.data: 0x%04x\n",
                      nGlyph,_lamsPort.data[nGlyph]);
   }
   if (channel) {
      if (nAvg++ >= nAVG) {
         for (n=0; n < MAX_BUFFER; n++) {
            if (calm)
              _lamsPort.data[n] = calm_spectrum[n] = (long long)(sum[n] / nAvg);
            else
              if ( (long long)(sum[n] / nAvg) - calm_spectrum[n] > 500000000)
                _lamsPort.data[n] = 0;
              else
                _lamsPort.data[n] = (long long)(sum[n] / nAvg) - calm_spectrum[n];

            sum[n] = 0;
         }
         nAvg = 0;
      }
      if (nAvg == 0) {
         if (nSkip++ >= nSKIP) {
            rtl_sem_post( &threadSem );
            nSkip = 0;
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
   unsigned int airspeed = 0;
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
         channel = lams_ptr->channel;
         DSMLOG_DEBUG("channel:       %d\n", channel);

         if (channel == 0) {
            if (fd_lams_data)
               rtl_close( fd_lams_data );
            break;
         }
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
         airspeed = *(unsigned int*) buf;
//       writew(airspeed, baseAddr + AIR_SPEED_OFFSET);
         DSMLOG_DEBUG("airspeed:      %d\n", airspeed);
         break;

      case N_AVG:
         nAVG = *(unsigned int*) buf;
         DSMLOG_DEBUG("nAVG:          %d\n", nAVG);
         break;

      case N_SKIP:
         nSKIP = *(unsigned int*) buf;
         DSMLOG_DEBUG("nSKIP:         %d\n", nSKIP);
         break;

      case CALM:
         calm = *(int*) buf;
         DSMLOG_DEBUG("calm:          %d\n", calm);
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
   DSMLOG_NOTICE("--------------------------------------------------\n");
   DSMLOG_NOTICE("compiled on %s at %s\n", __DATE__, __TIME__);
   DSMLOG_NOTICE("MAX_BUFFER: %d\n", MAX_BUFFER);
   DSMLOG_NOTICE("sizeof(long long):  %d\n", sizeof(long long)); // 8
   DSMLOG_NOTICE("sizeof(long):       %d\n", sizeof(long));      // 4
   DSMLOG_NOTICE("sizeof(int):        %d\n", sizeof(int));       // 4
   DSMLOG_NOTICE("sizeof(short):      %d\n", sizeof(short));     // 2
   DSMLOG_NOTICE("sizeof(char):       %d\n", sizeof(char));      // 1
   DSMLOG_NOTICE("nAVG:               %d\n", nAVG);
   DSMLOG_NOTICE("nSKIP:              %d\n", nSKIP);
   DSMLOG_NOTICE("--------------------------------------------------\n");

   // open up ioctl FIFO, register ioctl function
   createFifo("_in_", BOARD_NUM);
   ioctlHandle = openIoctlFIFO(devprefix, BOARD_NUM, ioctlCallback,
                               nioctlcmds, ioctlcmds);
   if (!ioctlHandle) return -RTL_EIO;

   request_region(baseAddr, REGION_SIZE, "lams");
   int n;
   _lamsPort.timetag = 0;
   for (n=0; n<MAX_BUFFER; n++)
      _lamsPort.data[n] = calm_spectrum[n] = 0;

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
