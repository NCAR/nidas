/* main.h

   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   New ADS 3 header file...

   Original Author: John Wasinger

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:   NOTE - this is a tempory include file until that
                is going to be used until the new ADS 3 system is
		developed to use the original /jnet/shared/include.h
		file!
*/

#define cfgFifo "/dev/cfgFifo"  // Module configuration FIFO

#define ANALOG_MAX_CHAN 10
#define SERIAL_MAX_CHAN 12

struct analogTable
{
  int port;
  int rate;
  int cal;
};

struct serialTable
{
  int  baud_rate;
  int  fptr;
  int  port;
  int  rate;
  char cmd[50];
  int  len;
};
