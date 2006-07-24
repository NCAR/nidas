/* ck_lams.cc

   Test program to print out messages received from the 
   FIFO buffers for the LAMS DSP card.

   Original Author: Mike Spowart

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

     $LastChangedRevision$
         $LastChangedDate: 2006-07-10 15:46:04 -0600 (Mon, 10 Jul 2006) $
           $LastChangedBy$
                 $HeadURL: http://svn/svn/nids/trunk/src/nidas/apps/ck_mesa.cc $

*/

// Linux include files.
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include <nidas/rtlinux/dsm_lams.h>

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

  err("compiled on %s at %s", __DATE__, __TIME__);

  char devstr[30];
  int status;
  int fd_lams_data[3];  // file pointers 
  int fdLamsAirspeedfifo;
  int fdLamsDatafile;
  int lams_channels = 1;
  int air_speed = 0;
  int len = 0;
  struct lams_set set_lams;

  // create the board sensor
  TestLams sensor_in_0;
  sensor_in_0.setDeviceName("/dev/lams0");

  LamsData data_buf[3];

  set_lams.channel = 1;
  //lams_channels = set_lams.channel;

  // Open up the Airspeed FIFO to the driver...
  sprintf(devstr, "/dev/lams_air_speed");
  err("opening '%s'", devstr);
  fdLamsAirspeedfifo = open(devstr, O_NONBLOCK | O_WRONLY);
  err("fdLamsAirspeedfifo = 0x%x", fdLamsAirspeedfifo);

  // Open up the disk for writing lams data
  sprintf(devstr, "/opt/lams_data.data");
  err("opening '%s'", devstr);
  fdLamsDatafile = open(devstr, O_NONBLOCK | O_WRONLY);
  err("fdLamsDatafile = 0x%x", fdLamsDatafile);


  // open up the mesa sensor
  try {
    sensor_in_0.open(O_RDONLY);
  }
  catch (n_u::IOException& ioe) {
    err("%s",ioe.what());
    goto close;
  }

  //Send the Load Air Speed ioctl
  sensor_in_0.ioctl(AIR_SPEED, &air_speed, sizeof(unsigned int));
  status      = write(fdLamsAirspeedfifo, &air_speed,1); 
  if (status < 0)
  {
    err("failed to write... errno: %d", errno);
    goto close;
  }
  close(fdLamsAirspeedfifo);

  err(" opening sensors...");

  // open all of the data FIFOs
  for (int ii=0; ii < lams_channels; ii++)
  {
    sprintf(devstr, "/dev/lams_in_%d", ii);
    fd_lams_data[ii] = open(devstr, O_RDONLY);
    if (fd_lams_data[ii] < 0)
    {
      err("failed to open '%s'", devstr);
      return 0;
    }
    err("opened '%s' @ 0x%x", devstr, fd_lams_data[ii]);
  }

  // Note: fd_set is a 1024 bit mask.
  fd_set readfds;

  // initialize the data buffers
  for (int ii=0; ii < lams_channels; ii++)
  {
    data_buf[ii].msec = 0;
    for (int j = 0; ii < MAX_BUFFER; j++)
    data_buf[ii].data[j] = 0;
  }

  // Set the lams.   
  sensor_in_0.ioctl(LAMS_SET, &set_lams, sizeof(set_lams));

  // Main loop for gathering data from the lams channels
  while (running)
  {
    // zero the readfds bitmask
    FD_ZERO(&readfds);

    // set the fd's to read data from ALL ports
    for (int ii=0; ii < lams_channels; ii++)
      FD_SET(fd_lams_data[ii], &readfds);

    // The select command waits for inbound FIFO data for ALL ports
    select((lams_channels)*2+1, &readfds, NULL, NULL, NULL);

    for (int ii=0; ii < lams_channels; ii++)
    {
      // check to see if there is data on this FIFO
      if (FD_ISSET(fd_lams_data[ii], &readfds))
      {
        // read '256' integers from the FIFO.
        for(int j = 0; j <= 255; j++){
          // check to see if there is data on this FIFO
          if (FD_ISSET(fd_lams_data[ii], &readfds))
          len += read(fd_lams_data[ii], &data_buf[ii].data[j], SSIZE_MAX);
          status = write(fdLamsDatafile, &data_buf[ii].data[j], 1); 
          if (status < 0)
          {
            err("failed to write... errno: %d", errno);
            goto close;
          }
	}  
      }
    }

    if (len != 256) 
    {
      sprintf(devstr, "len = %d", len);
      err("%s : %d\n", devstr, len);
    } 
    else 
    {
      len = 0;
      sprintf(devstr, "/dev/lams_data_%d_in", 0);
      err("%s : %d\n", devstr, data_buf[0].data[0]);
    }
 
 }

close:
  err(" closing sensors...");

  for (int ii=0; ii < lams_channels; ii++)
    close(fd_lams_data[ii]);
/*
  try {
    sensor_in_0.ioctl(LAMS_STOP, (void *)NULL, 0);
    sensor_in_0.close();
  }
  catch (n_u::IOException& ioe) {
    err("%s",ioe.what());
  }
  err("sensors closed.");
*/
  return 1;
}
