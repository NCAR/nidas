/* ck_mesa.cc

   Test program to print out messages received from the toggle
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <bits/posix1_lim.h>
#include <iostream>
#include <iomanip>


// serial driver includes
#include <mesa.h>
#include <RTL_DSMSensor.h>

int main(int argc, char** argv)
{
  char devstr[30];
  unsigned char buffer[SSIZE_MAX];
  int fd_mesa_counter[3][2];  // file pointers 
  int fd_mesa_radar[1][2];  // file pointers 
  int fdMesaFPGA;    // pointer for the FPGA file
  int len, counter_channels;
  int radar_channels;
  struct radar_set* radar_ptr;
  struct counters_set* counter_ptr;
  struct mesa_load* load_ptr;

  struct PulseCounters
  {
    int counts;
    int len;
  };
  PulseCounters counts_buf[100][2];

  struct RadarAlt
  {
    int altitude;
    int len;
  };
  RadarAlt altitude_buf[100][2];


  counter_ptr->channel = 2;
  radar_ptr->channel = 1;
  counter_ptr->rate = 10;
  radar_ptr->rate = 10;
  counter_channels += counter_ptr->channel;
  radar_channels += radar_ptr->channel;

  RTL_DSMSensor sensor("/dev/mesa0");
  try {
    sensor.open(O_RDONLY);
  }
  catch (atdUtil::IOException& ioe) {
    std::cerr << ioe.what() << std::endl;
    return 1;
  }

  // open all of the counter paired FIFO's
  for (int ii=1; ii <= counter_channels; ii++)
  {
    for (int tog=0; tog<2; tog++)
    {
      sprintf(devstr, "/dev/mesa_counter_data_%d_%d", ii, tog);
      fd_mesa_counter[ii][tog] = open(devstr, O_RDONLY);

      if (fd_mesa_counter[ii][tog] < 0)
      {
	printf("(%s) %s:\t failed to open '%s'\n",
	       __FILE__, __FUNCTION__, devstr);
	return 0;
      }
    }
  }
  // open the radar paired FIFO's
  for (int ii=1; ii <= radar_channels; ii++)
  {
    for (int tog=0; tog<2; tog++)
    {
      sprintf(devstr, "/dev/mesa_radar_data_%d_%d", ii, tog);
      fd_mesa_radar[ii][tog] = open(devstr, O_RDONLY);

      if (fd_mesa_radar[ii][tog] < 0)
      {
        printf("(%s) %s:\t failed to open '%s'\n",
               __FILE__, __FUNCTION__, devstr);
        return 0;
      }
    }
  }

  sprintf(devstr, "/dev/mesa_fpga_file");
  fdMesaFPGA = open(devstr, O_RDONLY);
  len = read(fdMesaFPGA, &buffer, SSIZE_MAX);
  load_ptr->filesize = len;

  sensor.ioctl(COUNTERS_SET, (void *)NULL, 0);
  sensor.ioctl(RADAR_SET,    (void *)NULL, 0);
  sensor.ioctl(MESA_LOAD,    (void *)NULL, 0);


  // Note: fd_set is a 1024 bit mask.
  fd_set readfds;

  // initialize the data buffers
  for (int ii=1; ii < counter_channels; ii++)
    for (int tog=0; tog<2; tog++)
    {
      counts_buf[ii][tog].counts = 0;
      counts_buf[ii][tog].len = 0;
    }
  for (int ii=1; ii < radar_channels; ii++)
    for (int tog=0; tog<2; tog++)
    {
      altitude_buf[ii][tog].altitude = 0;
      altitude_buf[ii][tog].len = 0;
    }


  // Main loop for gathering data from the counters and radar
  while (1)
  {
    // zero the readfds bitmask
    FD_ZERO(&readfds);

    if(counter_channels > 0)
    {
      // set the fd's to read data from ALL ports
      for (int ii=1; ii <= counter_channels; ii++)
        for (int tog=0; tog<2; tog++)
          FD_SET(fd_mesa_counter[ii][tog], &readfds);
    }
    if(radar_channels > 0)
    {
      for (int ii=1; ii <= radar_channels; ii++)
        for (int tog=0; tog<2; tog++)
          FD_SET(fd_mesa_radar[ii][tog], &readfds);
    }
    // The select command waits for inbound FIFO data for ALL ports
    select((counter_channels + radar_channels)*2+1, &readfds, NULL, NULL, NULL);

    if(counter_channels > 0)
    {
      for (int ii=1; ii <= counter_channels; ii++)
      {
        for (int tog=0; tog<2; tog++)
        {
          // check to see if there is data on this FIFO
          if (FD_ISSET(fd_mesa_counter[ii][tog], &readfds))
          {
	    // read 'n' integers from the FIFO.
	    counts_buf[ii][tog].len += read(fd_mesa_counter[ii][tog],
                               &counts_buf[ii][tog].counts, SSIZE_MAX);
	  
	    // print the full message recieved from this FIFO
            sprintf(devstr, "/dev/mesa_counter_data_%d_%d", ii, tog);
            printf("%s : %d\n", devstr, counts_buf[ii][tog].counts);
            counts_buf[ii][tog].len = 0;
          }
        }
      }
    }

    if(radar_channels > 0)
    {
      for (int ii=1; ii <= radar_channels; ii++)
      {
        for (int tog=0; tog<2; tog++)
        {
          // check to see if there is data on this FIFO
          if (FD_ISSET(fd_mesa_radar[ii][tog], &readfds))
          {
            // read 'n' characters from the FIFO.
            altitude_buf[ii][tog].len += read(fd_mesa_radar[ii][tog],
                               &altitude_buf[ii][tog].altitude, SSIZE_MAX);

            // print the full message recieved from this FIFO
            sprintf(devstr, "/dev/mesa_radar_data_%d_%d", ii, tog);
            printf("%s > %s\n", devstr, altitude_buf[ii][tog].altitude);
            altitude_buf[ii][tog].len = 0;
          }
        }
      }
    }
  }
   return 0;
}
