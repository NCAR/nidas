/* ck_mesa.cc

   Test program to print out messages received from the 
   FIFO buffers for the Mesa AnythingIO card.

   Original Author: Mike Spowart

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

     $LastChangedRevision$
         $LastChangedDate$
           $LastChangedBy$
                 $HeadURL$

*/

// Linux include files.
// #include <fcntl.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <bits/posix1_lim.h> // SSIZE_MAX
#include <csignal>          // sigaction

// Mesa driver includes
#include <nidas/core/DSMSensor.h>
#include <nidas/core/RTL_IODevice.h>

#include <nidas/rtlinux/mesa.h>

#define err(format, arg...) \
     printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

int running = 1;


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

class TestMesa : public DSMSensor
{
public:
  IODevice* buildIODevice() throw(n_u::IOException)
  {
    return new RTL_IODevice();
  }
  SampleScanner* buildSampleScanner()
  {
    return new DriverSampleScanner();
  }
};

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

  unsigned char buffer[MAX_BUFFER];

  char devstr[30];
  int status;
  int fd_mesa_counter[3];  // file pointers 
  int fd_mesa_radar[1];  // file pointers 
  int fdMesaFPGAfile;
  int fdMesaFPGAfifo;
  int counter_channels;
  int radar_channels;
  unsigned long len,total;
  unsigned long filesize;
  struct radar_set set_radar;
  struct counters_set set_counter;

  // create the board sensor
  TestMesa sensor_in_0;
  sensor_in_0.setDeviceName("/dev/mesa0");


  struct PulseCounters
  {
    int counts;
    int len;
  };
  PulseCounters counts_buf[2];

  struct RadarAlt
  {
    int altitude;
    int len;
  };
  RadarAlt altitude_buf[2];

  set_counter.nChannels = 2;
  set_radar.nChannels = 1;
  set_counter.rate = 1;
  set_radar.rate = 1;
  counter_channels = set_counter.nChannels;
  radar_channels = set_radar.nChannels;

  // Open up the FPGA program FIFO to the driver...
  sprintf(devstr, "/dev/mesa_program_board");
  err("opening '%s'", devstr);
  fdMesaFPGAfifo = open(devstr, O_NONBLOCK | O_WRONLY);
  err("fdMesaFPGAfifo = 0x%x", fdMesaFPGAfifo);

  // Open up the FPGA program drom disk...
  sprintf(devstr, "/tmp/code/firmware/mesa_fpga_file.bit");
  err("opening '%s'", devstr);
  fdMesaFPGAfile = open(devstr, O_RDONLY);

  // determine it's file size
  struct stat fstatus;
  status = fstat(fdMesaFPGAfile, &fstatus);
  if (status < 0)
  {
    err("failed to fstat... errno: %d", errno);
    goto close;
  }
  filesize = (unsigned long)fstatus.st_size;
  err("fdMesaFPGAfile = 0x%x  size: %lu", fdMesaFPGAfile, filesize);

  // open up the mesa sensor
  try {
    sensor_in_0.open(O_RDONLY);
  }
  catch (n_u::IOException& ioe) {
    err("%s",ioe.what());
    goto close;
  }

  //Send the Load FPGA Program ioctl
  sensor_in_0.ioctl(MESA_LOAD, &filesize, sizeof(unsigned long));
  total = 0;
  do {
    total += len = read(fdMesaFPGAfile, &buffer, MAX_BUFFER);
    status      = write(fdMesaFPGAfifo, &buffer, len); //sizeof(buffer));
    if (status < 0)
    {
      err("failed to write... errno: %d", errno);
      goto close;
    }
  }while ( total < filesize ); 
  close(fdMesaFPGAfile);
  close(fdMesaFPGAfifo);

  err(" opening sensors...");

  // open all of the counter FIFOs
  for (int ii=0; ii < counter_channels; ii++)
  {
    sprintf(devstr, "/dev/mesa_in_%d", ii);
    fd_mesa_counter[ii] = open(devstr, O_RDONLY);
    if (fd_mesa_counter[ii] < 0)
    {
      err("failed to open '%s'", devstr);
      return 0;
    }
    err("opened '%s' @ 0x%x", devstr, fd_mesa_counter[ii]);
  }
  // open the radar FIFO
  for (int ii=0; ii < radar_channels; ii++)
  {
    sprintf(devstr, "/dev/mesa_in_%d", ii + counter_channels);
    fd_mesa_radar[ii] = open(devstr, O_RDONLY);
    if (fd_mesa_radar[ii] < 0)
    {
      err("failed to open '%s'", devstr);
      return 0;
    }
    err("opened '%s' @ 0x%x", devstr, fd_mesa_radar[ii]);
  }

  // Note: fd_set is a 1024 bit mask.
  fd_set readfds;

  // initialize the data buffers
  for (int ii=0; ii < counter_channels; ii++)
  {
    counts_buf[ii].counts = 0;
    counts_buf[ii].len = 0;
  }
  for (int ii=0; ii < radar_channels; ii++)
  {
    altitude_buf[ii].altitude = 0;
    altitude_buf[ii].len = 0;
  }

  // Set the counters.   
  sensor_in_0.ioctl(COUNTERS_SET, &set_counter, sizeof(set_counter));

  // Set the radar.   
  sensor_in_0.ioctl(RADAR_SET, &set_radar, sizeof(set_radar));

  // Main loop for gathering data from the counters and radar
  while (running)
  {
    // zero the readfds bitmask
    FD_ZERO(&readfds);

    // set the fd's to read data from ALL ports
    for (int ii=0; ii < counter_channels; ii++)
      FD_SET(fd_mesa_counter[ii], &readfds);

    for (int ii=0; ii < radar_channels; ii++)
      FD_SET(fd_mesa_radar[ii], &readfds);

    // The select command waits for inbound FIFO data for ALL ports
    select((counter_channels + radar_channels)*2+1, &readfds, NULL, NULL, NULL);

    for (int ii=0; ii < counter_channels; ii++)
    {
      // check to see if there is data on this FIFO
      if (FD_ISSET(fd_mesa_counter[ii], &readfds))
      {
        // read 'n' integers from the FIFO.
        counts_buf[ii].len += read(fd_mesa_counter[ii],
                                   &counts_buf[ii].counts, SSIZE_MAX);
	  
        // print the full message recieved from this FIFO
//         printf("(%3d) %10ld: %3ld: %04lo: %u\n", len, sample.timetag, sample.length,
//            sample.data[0]&0x000000ff, (sample.data[0]&0xffffff00)>>8);

        sprintf(devstr, "/dev/mesa_counter_%d_in", ii);
        err("%s : %d\n", devstr, counts_buf[ii].counts);
        counts_buf[ii].len = 0;
      }
    }

    for (int ii=0; ii < radar_channels; ii++)
    {
      // check to see if there is data on this FIFO
      if (FD_ISSET(fd_mesa_radar[ii], &readfds))
      {
        // read 'n' characters from the FIFO.
        altitude_buf[ii].len += read(fd_mesa_radar[ii],
                                     &altitude_buf[ii].altitude, SSIZE_MAX);

        // print the full message recieved from this FIFO
        sprintf(devstr, "/dev/mesa_radar_%d_in", ii);
        err("%s : %d\n", devstr, altitude_buf[ii].altitude);
        altitude_buf[ii].len = 0;
      }
    }
  }

close:
  err(" closing sensors...");

  for (int ii=0; ii < counter_channels; ii++)
    close(fd_mesa_counter[ii]);

  for (int ii=0; ii < radar_channels; ii++)
    close(fd_mesa_radar[ii]);

  try {
    sensor_in_0.ioctl(MESA_STOP, (void *)NULL, 0);
    sensor_in_0.close();
  }
  catch (n_u::IOException& ioe) {
    err("%s",ioe.what());
  }
  err("sensors closed.");
  return 1;
}
