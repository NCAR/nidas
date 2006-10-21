/* ck_lams.cc

   Test program to print out messages received from the 
   FIFO buffers for the LAMS DSP card.

   Original Author: Mike Spowart

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

     $LastChangedRevision$
         $LastChangedDate: 2006-07-10 15:46:04 -0600 (Mon, 10 Jul 2006) $
           $LastChangedBy$
                 $HeadURL: http://svn/svn/nids/trunk/src/nidas/apps/ck_lams.cc $
*/

// Linux include files.
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

  string ofName;
  if (argc < 2)
    ofName = string("/mnt/tmp/lams.dat");
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
  int ofPtr = creat(ofName.c_str(), 0666);
  if (ofPtr < 0) {
    err("failed to open '%s' (%s)", ofName.c_str(), strerror(errno));
    goto failed;
  }
  // open up the lams sensor
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
  err("send LAMS_SET");
  sensor_in_0.ioctl(LAMS_SET, &set_lams, sizeof(set_lams));

  // Note: fd_set is a 1024 bit mask.
  fd_set readfds;

  // Main loop for gathering data from the lams channels
  int len, rlen, status, nsel;
  len = 0;
  unsigned int databuf[sizeof(set_lams)*2];

  int nfds;
  while (running)
  {
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
    if (FD_ISSET(fd_lams_data, &readfds))
    {
      len += rlen = read(fd_lams_data, &databuf, sizeof(lamsPort));
      status = write(ofPtr, &databuf, rlen); 
      
//    err("spectrum recv'd len=%d rlen=%d", len, rlen);
      if (status < 0) {
        err("failed to write... errno: %d", errno);
        goto failed;
      }
      // was a full record of spectrum read?
//    if (len == sizeof(lamsPort)) {
//      err("spectrum recv'd");
//      len = 0;
//    }
    }
 }
failed:
  err(" closing sensors...");

  if (ofPtr) close(ofPtr);
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
  err("X86 version - compiled on %s at %s", __DATE__, __TIME__);

  string ifName, ofName;
  if (argc < 2) {
    ifName = string("/tmp/lams.dat");
    ofName = string("/tmp/lams.hex");
  } else {
    ifName = string(argv[1]);
    ofName = string(argv[2]);
  }
  
  int ifPtr=0, ofPtr=0;
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
  char readbuf[sizeof(lamsPort)];
  char linebuf[11+MAX_BUFFER*5];
  unsigned int n, nRead, nHead;

  struct lamsPort* data = (lamsPort*) &readbuf;

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
  if (ofPtr) close(ofPtr);
  if (ifPtr) close(ifPtr);
  return 0;
}
#endif
