/* ck_lams.cc

   Test program to print out messages received from the 
   FIFO buffers for the LAMS DSP card.

   Original Author: Mike Spowart

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

     $LastChangedRevision$
         $LastChangedDate$
           $LastChangedBy$
                 $HeadURL$
*/

// Linux include files.
#include <fcntl.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/select.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <bits/posix1_lim.h> // SSIZE_MAX
#include <signal.h>          // sigaction
#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/core/RTL_IODevice.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/rtlinux/lams.h>

using namespace std;
using namespace nidas::core;
namespace n_u = nidas::util;

#define DATA_ONLY

#define err(format, arg...) \
     printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

#if defined(__arm__)

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

int running = 1;

class LamsSensor : public DSMSensor
{
public:
  IODevice* buildIODevice() throw(n_u::IOException)
  {
    return new RTL_IODevice();
  }
  SampleScanner* buildSampleScanner()
  {
    return new SampleScanner();
  }
};
/* -------------------------------------------------------------------- */
void sigAction(int sig, siginfo_t* siginfo, void* vptr)
{
  cerr << "received signal " << strsignal(sig) << "(" << sig << ")" <<
    " si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
    " si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
    " si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
  
  switch(sig) {
  case SIGHUP:
  case SIGTERM:
  case SIGINT:
    running = 0;
    err("running: %d", running);
    break;
  default:
    break;
  }
}
/* -------------------------------------------------------------------- */
int main(int argc, char** argv)
{
  err("ARM version - compiled on %s at %s", __DATE__, __TIME__);
  err("sizeof(lamsPort): %d\n", sizeof(lamsPort));

  string ofName;
  if (argc < 2)
    ofName = string("/tmp/lams.bin");
  else
    ofName = string(argv[1]);

  // set up a sigaction to respond to ctrl-C
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset,SIGHUP);
  sigaddset(&sigset,SIGTERM);
  sigaddset(&sigset,SIGINT);
  sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);

  struct sigaction act;
  sigemptyset(&sigset);
  act.sa_mask = sigset;
  act.sa_flags = SA_SIGINFO;
  act.sa_sigaction = sigAction;
  sigaction(SIGHUP,&act,(struct sigaction *)0);
  sigaction(SIGINT,&act,(struct sigaction *)0);
  sigaction(SIGTERM,&act,(struct sigaction *)0);

  // create the board sensor
  LamsSensor sensor_in_0;
  sensor_in_0.setDeviceName("/dev/lams0");

  // Open up the disk for writing lams data
  err("creating: %s", ofName.c_str());
  int ofPtr = creat(ofName.c_str(), 0666);
  if (ofPtr < 0) {
    err("failed to open '%s' (%s)", ofName.c_str(), strerror(errno));
    goto failed;
  }
  // open up the lams sensor
  err("opening: /dev/lams0");
  try {
    sensor_in_0.open(O_RDONLY);
    err("sensor_in_0.open(O_RDONLY) success!");
  }
  catch (n_u::IOException& ioe) {
    err("%s",ioe.what());
    goto failed;
  }
  int fd_lams_data;
  fd_lams_data = sensor_in_0.getReadFd();
  err("sensor_in_0.getReadFd() = 0x%x", fd_lams_data);

  //Send the Air Speed
  unsigned int air_speed;
  air_speed = 0;
  err("send AIR_SPEED");
  err("AIR_SPEED: 0x%x", AIR_SPEED);
  err("air_speed: %d",   air_speed);
  sensor_in_0.ioctl(AIR_SPEED, &air_speed, sizeof(air_speed));

  // Set the lams.   
  struct lams_set set_lams;
  set_lams.channel = 1;
  err("send LAMS_SET_CHN");
  sensor_in_0.ioctl(LAMS_SET_CHN, &set_lams, sizeof(set_lams));

  // Note: fd_set is a 1024 bit mask.
  fd_set readfds;

  // Main loop for gathering data from the lams channels
  int len, rlen, status, nsel;
  len = 0;
//unsigned int databuf[sizeof(set_lams)*2];
  char databuf[sizeof(lamsPort)];
  struct lamsPort* data; data = (lamsPort*) &databuf;

  int nfds;
  err("entering main loop...");
//int skip;  skip = 0;
//int glyph; glyph = 0;
  while (running) {
    nfds = 0;

    // zero the readfds bitmask
    FD_ZERO(&readfds);

    // set the fd's to read data from ALL ports
    FD_SET(fd_lams_data, &readfds);
    nfds = max (nfds, fd_lams_data);

    // The select command waits for inbound FIFO data for ALL ports
    nsel = select(nfds+1, &readfds, NULL, NULL, 0);
    if (nsel < 0) // select error
      err("select error");

    // check to see if there is data on this FIFO
    if (FD_ISSET(fd_lams_data, &readfds)) {
      errno = 0;
      rlen = read(fd_lams_data, &databuf[len], sizeof(lamsPort));
      if (rlen < 0) {
        err("failed to read (%s), fd_lams_data: %x", strerror(errno), fd_lams_data);
        goto failed;
      }
      len += rlen;
//    if (skip++ == 9) {
//      skip=0;
//      if (glyph++ == 9) glyph=0;
//      err("%d rlen: %d data->data[0]: %x", glyph, rlen, data->data[0]);
//    }
      if (len == sizeof(lamsPort)) {
        len = 0;
        errno = 0;
#ifdef DATA_ONLY
        status = write(ofPtr, &(data->data), sizeof(data->data)); 
#else
        status = write(ofPtr, &databuf, sizeof(lamsPort)); 
#endif
        if (status < 0) {
          err("failed to write (%s)", strerror(errno));
          goto failed;
        }
//      fsync(fd_lams_data);
        sync();
      }
    }
  }
failed:
  err("closing sensors...");

  if (ofPtr != -1) close(ofPtr);
  sensor_in_0.close();
//try {
//  sensor_in_0.ioctl(LAMS_STOP, (void *)NULL, 0);
//  sensor_in_0.close();
//}
//catch (n_u::IOException& ioe) {
//  err("%s",ioe.what());
//}
  err("sensors closed.");
  return 1;
}

#else

int main(int argc, char** argv)
{
  char readbuf[sizeof(lamsPort)];
  char linebuf[11+MAX_BUFFER*5];
  unsigned int n, nRead, nHead;

  struct lamsPort* data = (lamsPort*) &readbuf;

  err("X86 version - compiled on %s at %s", __DATE__, __TIME__);

  string ifName, ofName;
  if (argc < 2) {
    ifName = string("/tmp/lams.dat");
    ofName = string("/tmp/lams.hex");
  } else {
    ifName = string(argv[1]);
    ofName = string(argv[2]);
  }
  
  int ifPtr=-1, ofPtr=-1;
  ifPtr = open(ifName.c_str(), 0);
  if (ifPtr < 0) {
    err("failed to open '%s' (%s)", ifName.c_str(), strerror(errno));
    goto failed;
  }
  ofPtr = creat(ofName.c_str(), 0666);
  if (ofPtr < 0) {
    err("failed to create '%s' (%s)", ofName.c_str(), strerror(errno));
    goto failed;
  }

  do {
    nRead = read(ifPtr, &readbuf, sizeof(lamsPort));

    if ( nRead > 0 ) {

      nHead = 0;
      sprintf(&linebuf[nHead], "%08lx -", data->timetag); nHead+=10;
      for (n=0; n<MAX_BUFFER; n++)
        sprintf(&linebuf[nHead+n*5], " %04x", data->data[n]);
      sprintf(&linebuf[nHead+n*5], "\n");

      int status = write(ofPtr, &linebuf, strlen(linebuf)); 
      if (status < 0) {
        err("failed to write... errno: %d", errno);
        goto failed;
      }
    }
  } while ( nRead > 0 );

failed:
  if (ofPtr != -1) close(ofPtr);
  if (ifPtr != -1) close(ifPtr);
  return 0;
}
#endif
