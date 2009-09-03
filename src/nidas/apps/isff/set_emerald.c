/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    App to query and set options on a Diamond emerald serial IO card.
    Interacts via ioctls with the emerald kernel module.
 ********************************************************************
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

// #include "emerald.h"
#include <nidas/linux/diamond/emerald.h>

int printConfig(int fd,const char* devname,int eepromAccess) {
  emerald_config config;
  int i;
  printf("current port config:\n");

  if (eepromAccess) {
      if (ioctl(fd,EMERALD_IOCGEEPORTCONFIG,&config) < 0) {
        fprintf(stderr,"ioctl EMERALD_IOCGEEPORTCONFIG: %s: %s\n",devname,strerror(errno));
        return -1;
      }
  }
  else {
      if (ioctl(fd,EMERALD_IOCGPORTCONFIG,&config) < 0) {
        fprintf(stderr,"ioctl EMERALD_IOCGPORTCONFIG: %s: %s\n",devname,strerror(errno));
        return -1;
      }
  }

  // output in a form like options to setserial
  for (i = 0; i < 8; i++) printf("port %#x irq %d\n",
    	config.ports[i].ioport,config.ports[i].irq);
  return 0;
}

int setConfig(int fd,const char* devname,int port0Addr,const int* irqs,int eepromAccess) {
  int i;
  emerald_config config;

  printf("setting ports:\n");
  for (i = 0; i < 8; i++) {
    config.ports[i].ioport = port0Addr + i * 8;
    config.ports[i].irq = irqs[i];
    printf("port %#x irq %d\n",
      config.ports[i].ioport,config.ports[i].irq);
  }

  if (!eepromAccess) {
    if (ioctl(fd,EMERALD_IOCSPORTCONFIG,&config) < 0) {
      fprintf(stderr,"ioctl EMERALD_IOCSPORTCONFIG: %s: %s\n",devname,strerror(errno));
      return -1;
    }
  }
  else {
    if (ioctl(fd,EMERALD_IOCSEEPORTCONFIG,&config) < 0) {
      fprintf(stderr,"ioctl EMERALD_IOCSEEPORTCONFIG: %s: %s\n",devname,strerror(errno));
      return -1;
    }
    if (ioctl(fd,EMERALD_IOCEECONFIGLOAD) < 0) {
      fprintf(stderr,"ioctl EMERALD_IOCEECONFIGLOAD: %s: %s\n",devname,strerror(errno));
      return -1;
    }
  }
  if (ioctl(fd,EMERALD_IOCPORTENABLE) < 0) {
    fprintf(stderr,"ioctl EMERALD_IOCPORTENABLE: %s: %s\n",devname,strerror(errno));
    return -1;
  }
  return 0;
}

int getNumBoards(int fd,const char* devname) {
  int nboards;
  if (ioctl(fd,EMERALD_IOCGNBOARD,&nboards) < 0) {
    fprintf(stderr,"ioctl EMERALD_IOCGNBOARD: %s: %s\n",devname,strerror(errno));
    return -1;
  }
  return nboards;
}

int getISABaseAddr(int fd,const char* devname,unsigned long* baseaddr) {
  if (ioctl(fd,EMERALD_IOCGISABASE,baseaddr) < 0) {
    fprintf(stderr,"ioctl EMERALD_IOCGISABSE: %s: %s\n",devname,strerror(errno));
    return -1;
  }
  return 0;
}

void usage(const char* argv0) {
  fprintf(stderr,"Usage: %s [-n] [-b] device [port0 irq ...]\n\n",argv0);
  fprintf(stderr,"\
-n: display the number of boards with acceptable EEPROM configuration\n\
-b: display the ISA base address on the system, in 0xhhhh form\n\
-t: set values in temporary RAM, not EEPROM\n\
device: device name, /dev/emerald0, /dev/emerald1, etc\n\
port0: ioport address of serial port 0 on the board\n\
    the 8 ports will be configured at port0, port0+0x8,\n\
    port0+0x10, ..., up to port0+0x38\n\
irq: interrupt level to use for each port.\n\
    if less than 8 irqs are listed, the last will be repeated\n\
    ######################################################################\n\
    # WARNING: configuring serial ports on a system while the serial\n\
    # driver is actively accessing those same ports can cause a system crash.\n\
    # Make sure no process has the serial devices open, or if they are open\n\
    # make sure no data is being read or written.\n\
    ######################################################################\n\
Examples:\n\
    # configure ports on first board at\n\
    # iports 0x100,0x108,0x110,0x118,...,0x138, all irq=3\n\
    %s /dev/emerald0 0x100 3\n\n\
    # query configuration of /dev/emerald1\n\
    %s /dev/emerald1\n\n\
    # query number of configured boards\n\
    %s -n /dev/emerald0\n\n",
  	argv0,argv0,argv0);
  exit(1);
}


int main(int argc, char** argv) {

  int port0Addr = -1;
  int irqs[8];
  int fd;
  const char* devname;
  int getnumber = 0;
  int getbaseaddr = 0;
  int eepromAccess = 0;

  extern char *optarg;       /* set by getopt() */
  extern int optind;       /* "  "     "     */
  int opt_char;     /* option character */

  while ((opt_char = getopt(argc, argv, "enb")) != -1) {
      switch (opt_char) {
      case 'e':
        eepromAccess = 1;
        break;
      case 'n':
        getnumber = 1;
        break;
      case 'b':
        getbaseaddr = 1;
        break;
      case '?':
	usage(argv[0]);
        break;
      }
  }

  if (optind == argc) usage(argv[0]);
  devname = argv[optind++];

  if (optind < argc) {
    int i;
    if (sscanf(argv[optind++],"%x",&port0Addr) != 1) usage(argv[0]);

    if (optind == argc) usage(argv[0]);
    int nirq = 0;
    for ( ; optind < argc; optind++)
      if (sscanf(argv[optind],"%d",irqs + nirq++) != 1) usage(argv[0]);
    for (i = nirq; i < 8; i++) irqs[i] = irqs[nirq-1];
  }
  

  fd = open(devname,port0Addr < 0 ? O_RDONLY : O_RDWR);
  if (fd < 0) {
    fprintf(stderr,"open %s: %s\n",devname,strerror(errno));
    return 1;
  }
  if (getnumber) {
      int n = getNumBoards(fd,devname);
      fprintf(stdout,"%d\n",n);
      return 0;
  }
  if (getbaseaddr) {
      unsigned long addr;
      int n = getISABaseAddr(fd,devname,&addr);
      fprintf(stdout,"%#lx\n",addr);
      return n != 0;	// 0=OK if getISABaseAddr returns 0, else 1
  }

  if (port0Addr >= 0)
    if (setConfig(fd,devname,port0Addr,irqs,eepromAccess) < 0) return 1;

  if (printConfig(fd,devname,eepromAccess) < 0) return 1;
  close(fd);
  return 0;
}
