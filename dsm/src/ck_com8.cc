/* ck_com8.cc

   Time-stamp: <Thu 26-Aug-2004 06:48:03 pm>

   Test program to print out messages received from the toggle
   FIFO buffers for the 8 port serial card.

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

*/

// Linux include files.
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <bits/posix1_lim.h>

// serial driver includes
#include <com8.h>

int main()
{
  printf("(%s) %s:\t compiled on %s at %s\n",
	 __FILE__, __FUNCTION__, __DATE__, __TIME__);


  char devstr[30];
  int fdSerial[8][2];  // file pointers

  // open all of the serial ports paired FIFO's
  for (int ii=1; ii<9; ii++)
  {
    for (int tog=0; tog<2; tog++)
    {
      sprintf(devstr, "/dev/com8_data_%d_%d", ii, tog);
      fdSerial[ii][tog] = open(devstr, O_RDONLY);

      if (fdSerial[ii][tog] < 0)
      {
	printf("(%s) %s:\t failed to open '%s'\n",
	       __FILE__, __FUNCTION__, devstr);
	return 0;
      }
    }
  }

  struct message
  {
    char *str;
    int len;
  };
  message buffer[8][2];

  // Note: fd_set is a 1024 bit mask.
  fd_set readfds;

  // initialize the message buffers
  for (int ii=1; ii<9; ii++)
    for (int tog=0; tog<2; tog++)
    { 
      buffer[ii][tog].str = "";
      buffer[ii][tog].len = 0;
    }

  // Main loop for gathering strings from the serial ports
  while (1)
  {
    // zero the readfds bitmask
    FD_ZERO(&readfds);

    // set the fd's to read data from ALL ports
    for (int ii=1; ii<9; ii++)
      for (int tog=0; tog<2; tog++)
	FD_SET(fdSerial[ii][tog], &readfds);

    // The select command waits for inbound FIFO data for ALL ports
    select(8*2+1, &readfds, NULL, NULL, NULL);

    for (int ii=1; ii<9; ii++)
      for (int tog=0; tog<2; tog++)

	// check to see if there is data on this FIFO
	if (FD_ISSET(fdSerial[ii][tog], &readfds))
	{
	  // read 'n' characters from the FIFO.
	  buffer[ii][tog].len +=
	    read(fdSerial[ii][tog],
		 &(buffer[ii][tog].str[buffer[ii][tog].len]),
		 SSIZE_MAX);
	  
	  // check if the message for this FIFO is null terminated
	  if (buffer[ii][tog].str[buffer[ii][tog].len] == '\0')
	  {
	    // print the full message recieved from this FIFO
	    sprintf(devstr, "/dev/com8_data_%d_%d", ii, tog);
	    printf("%s > %s\n", devstr, buffer[ii][tog].str);
	    buffer[ii][tog].len = 0;
	  }
	}
  }
  return 0;
}
