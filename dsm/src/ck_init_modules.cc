/* ck_init_modules.cc

   Time-stamp: <Tue 20-Jul-2004 03:18:17 pm>

   Test program to send configuration data to the init_modules
   RTLinux module..,

   Original Author: John Wasinger

   Copyright by the National Center for Atmospheric Research 2004
 
   Revisions:

*/

// Linux include files.
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// module driver includes
#include <init_modules.h>

int main()
{
  printf("(%s) %s:\t compiled on %s at %s\n\n",
	 __FILE__, __FUNCTION__, __DATE__, __TIME__);

  int fdCfg = open(cfgFifo, O_NONBLOCK | O_WRONLY);
  if (fdCfg < 0)
  {
    printf("(%s) %s:\t failed to open '%s'\n",
	   __FILE__, __FUNCTION__, cfgFifo);
    return 0;
  }

  analogTable analog;
  serialTable serial;

  for (int ii=0; ii<5; ii++)
  {
    analog.port = ii;
    analog.rate = ii*100;
    analog.cal  = ii * 11;
    write(fdCfg, "ANALOG\0", 7);
    write(fdCfg, &analog, sizeof(analog));
  }

  serial.baud_rate = 9600;
  serial.port      = 1;
  serial.rate      = 1000;
  sprintf(serial.cmd,"->hello<-");
  serial.len = strlen(serial.cmd);
  write(fdCfg, "SERIAL\0", 7);
  write(fdCfg, &serial, sizeof(serial));

  write(fdCfg, "UNKNOWN\0", 8);
  write(fdCfg, "RUN\0", 4);
  close(fdCfg);
  return 0;
}
