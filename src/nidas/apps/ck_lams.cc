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
// Mesa driver includes
#include <nidas/core/DSMSensor.h>
#include <nidas/rtlinux/lams.h>

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

#define err(format, arg...) \
     printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

using namespace std;
using namespace nidas::core;
namespace n_u = nidas::util;

int running = 1;


class TestLams : public DSMSensor
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
  err("compiled on %s at %s", __DATE__, __TIME__);

  if (argc < 2) {
    fprintf (stderr, "Usage: %s outfile\n", argv[0]);
    return -EINVAL;
  }

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
  TestLams sensor_in_0;
  sensor_in_0.setDeviceName("/dev/lams0");

  // Open up the disk for writing lams data
  string ofName(argv[1]);
  int fdLamsDatafile;
  fdLamsDatafile = creat(ofName.c_str(), 0666);
  if (fdLamsDatafile < 0)
  {
    err("failed to open '%s' (%s)", ofName.c_str(), strerror(errno));
    goto failed;
  }
  err("file opened: fdLamsDatafile = 0x%x", fdLamsDatafile);

  err("open up the lams sensor");
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
  unsigned int databuf[MAX_BUFFER];

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
      len += rlen = read(fd_lams_data, &databuf, MAX_BUFFER);
      status = write(fdLamsDatafile, &databuf, rlen); 
//    err("spectra recv'd len=%d rlen=%d", len, rlen);
      if (status < 0)
      {
        err("failed to write... errno: %d", errno);
        goto failed;
      }
      // after reading '256' integers 
      if (len == 512) {
        err("spectra recv'd");
        len = 0;
      }
    }
 }
//goto failed; // SCANNING DEBUG EXIT
failed:
  err(" closing sensors...");

  if (fdLamsDatafile)
    close(fdLamsDatafile);
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
