/* init_modules.h

   Time-stamp: <Tue 20-Jul-2004 03:17:38 pm>

   New ADS 3 header file...

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research 2004
 
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
