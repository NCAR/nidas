/* ck_mesa.cc

   Test program to print out messages received from the 
   FIFO buffers for the Mesa AnythingIO card.

   Original Author: Mike Spowart

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

     $LastChangedRevision$
         $LastChangedDate: $
           $LastChangedBy$
                 $HeadURL: $

*/

// Linux include files.
// #include <fcntl.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <bits/posix1_lim.h>

// Mesa driver includes
#include <RTL_DSMSensor.h>
#include <mesa.h>

#define err(format, arg...) \
     printf("%s: %s: " format "\n",__FILE__, __FUNCTION__ , ## arg)

using namespace std;

int main(int argc, char** argv)
{
  std::cerr << __FILE__ << " " << __FUNCTION__ << "\n";
  flush(std::cerr);

  char devstr[30];
  unsigned char buffer[MAX_BUFFER];

  int fd_mesa_counter[3];  // file pointers 
  int fd_mesa_radar[1];  // file pointers 
  int fdMesaFPGA;    // pointer for the FPGA file
  int fdMesaFPGAfifo;
  int counter_channels;
  int radar_channels;
  unsigned long long_len;
  struct radar_set set_radar;
  struct counters_set set_counter;

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

  set_counter.channel = 2;
  set_radar.channel = 1;
  set_counter.rate = 10;
  set_radar.rate = 10;
  counter_channels = set_counter.channel;
  radar_channels = set_radar.channel;

  // Load the FPGA program...
  sprintf(devstr, "/dev/mesa_program_board");
  printf("(%s) %s:\t opening '%s'\n", __FILE__, __FUNCTION__, devstr);
  flush(std::cout);
  fdMesaFPGAfifo = open(devstr, O_RDONLY);

  // Open up the FPGA program drom disk...
  sprintf(devstr, "/opt/mesa_fpga_file.bit");
  printf("(%s) %s:\t opening '%s'\n", __FILE__, __FUNCTION__, devstr);
  flush(std::cout);
  fdMesaFPGA = open(devstr, O_RDONLY);
  do {
    long_len = read(fdMesaFPGA,&buffer,MAX_BUFFER);
    write(fdMesaFPGAfifo,&buffer,long_len);
  }while(buffer[long_len] != EOF && !feof((FILE*)"/opt/mesa_fpga_file.bit")); 

  std::cerr << __FILE__ << " opening sensors...\n"; flush(std::cerr);

  RTL_DSMSensor sensor_in_0("/dev/mesa0");
  try {
    sensor_in_0.open(O_RDONLY);
  }
  catch (atdUtil::IOException& ioe) {
    std::cerr << ioe.what() << std::endl;
    return 1;
  }

  // open all of the counter paired FIFO's
  for (int ii=1; ii <= counter_channels; ii++)
  {
    sprintf(devstr, "/dev/mesa_counter_in_%d", ii);
    fd_mesa_counter[ii] = open(devstr, O_RDONLY);

    if (fd_mesa_counter[ii] < 0)
    {
      printf("(%s) %s:\t failed to open '%s'\n",
            __FILE__, __FUNCTION__, devstr);
      return 0;
     }
  }
  // open the radar paired FIFO's
  for (int ii=1; ii <= radar_channels; ii++)
  {
    sprintf(devstr, "/dev/mesa_radar_in%d", ii);
    fd_mesa_radar[ii] = open(devstr, O_RDONLY);

    if (fd_mesa_radar[ii] < 0)
    {
      printf("(%s) %s:\t failed to open '%s'\n",
             __FILE__, __FUNCTION__, devstr);
      return 0;
    }
  }

  // Note: fd_set is a 1024 bit mask.
  fd_set readfds;

  // initialize the data buffers
  for (int ii=1; ii < counter_channels; ii++)
  {
    counts_buf[ii].counts = 0;
    counts_buf[ii].len = 0;
  }
  for (int ii=1; ii < radar_channels; ii++)
  {
    altitude_buf[ii].altitude = 0;
    altitude_buf[ii].len = 0;
  }

  // Set the counters.   
  std::cerr << __FILE__ << " sensor_in_0.ioctl(COUNTERS_SET, &set_counter, sizeof(struct counters_set))\n"; flush(std::cerr);
  sensor_in_0.ioctl(COUNTERS_SET, &set_counter, sizeof(set_counter));

  // Set the radar.   
  std::cerr << __FILE__ << " sensor_in_0.ioctl(COUNTERS_SET, &set_radar, sizeof(struct counters_set))\n"; flush(std::cerr);
  sensor_in_0.ioctl(RADAR_SET, &set_radar, sizeof(set_radar));

  // Main loop for gathering data from the counters and radar
  while (1)
  {
    // zero the readfds bitmask
    FD_ZERO(&readfds);

    if(counter_channels > 0)
    {
      // set the fd's to read data from ALL ports
      for (int ii=1; ii <= counter_channels; ii++)
        FD_SET(fd_mesa_counter[ii], &readfds);
    }
    if(radar_channels > 0)
    {
      for (int ii=1; ii <= radar_channels; ii++)
        FD_SET(fd_mesa_radar[ii], &readfds);
    }
    // The select command waits for inbound FIFO data for ALL ports
    select((counter_channels + radar_channels)*2+1, &readfds, NULL, NULL, NULL);

    if(counter_channels > 0)
    {
      for (int ii=1; ii <= counter_channels; ii++)
      {
        // check to see if there is data on this FIFO
        if (FD_ISSET(fd_mesa_counter[ii], &readfds))
        {
        // read 'n' integers from the FIFO.
          counts_buf[ii].len += read(fd_mesa_counter[ii],
                                     &counts_buf[ii].counts, SSIZE_MAX);
	  
        // print the full message recieved from this FIFO
          sprintf(devstr, "/dev/mesa_counter_%d_in", ii);
          printf("%s : %d\n", devstr, counts_buf[ii].counts);
          counts_buf[ii].len = 0;
        }
      }
    }

    if(radar_channels > 0)
    {
      for (int ii=1; ii <= radar_channels; ii++)
      {
        // check to see if there is data on this FIFO
        if (FD_ISSET(fd_mesa_radar[ii], &readfds))
        {
          // read 'n' characters from the FIFO.
          altitude_buf[ii].len += read(fd_mesa_radar[ii],
                                  &altitude_buf[ii].altitude, SSIZE_MAX);

          // print the full message recieved from this FIFO
          sprintf(devstr, "/dev/mesa_radar_%d_in", ii);
          printf("%s > %s\n", devstr, altitude_buf[ii].altitude);
          altitude_buf[ii].len = 0;
        }
      }
    }
  }
 close:
  std::cerr << __FILE__ << " closing sensors...\n";
  try {
    sensor_in_0.close();
  }
  catch (atdUtil::IOException& ioe) {
    std::cerr << ioe.what() << std::endl;
    return 1;
  }
  std::cerr << __FILE__ << " sensors closed.\n";
}
