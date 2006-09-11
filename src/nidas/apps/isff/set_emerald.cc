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

#include <nidas/linux/diamond/emerald/emerald.h>

int printConfig(int fd,const char* devname) {
  emerald_config config;
  int i;
  printf("current port config:\n");

  if (ioctl(fd,EMERALD_IOCGPORTCONFIG,&config) < 0) {
    fprintf(stderr,"ioctl EMERALD_IOCGPORTCONFIG: %s: %s\n",devname,strerror(errno));
    return -1;
  }
  // output in a form like options to setserial
  for (i = 0; i < 8; i++) printf("port 0x%x irq %d\n",
    	config.ports[i].ioport,config.ports[i].irq);

#ifdef EEPROM_CONFIG_TOO
  if (ioctl(fd,EMERALD_IOCGEEPORTCONFIG,&config) < 0) {
    fprintf(stderr,"ioctl EMERALD_IOCGEEPORTCONFIG: %s: %s\n",devname,strerror(errno));
    return -1;
  }
  for (i = 0; i < 8; i++) printf("port 0x%x irq %d\n",
    	config.ports[i].ioport,config.ports[i].irq);
#endif
  return 0;

}
int setConfig(int fd,const char* devname,int port0Addr,const int* irqs) {
  int i;
  emerald_config config;

  printf("setting ports:\n");
  for (i = 0; i < 8; i++) {
    config.ports[i].ioport = port0Addr + i * 8;
    config.ports[i].irq = irqs[i];
    printf("port 0x%x irq %d\n",
      config.ports[i].ioport,config.ports[i].irq);
  }

/* We shouldn't need to set the RAM config. The last step of
 * setting the EEPROM config now requests that the RAM settings
 * be read from EEPROM.
 */
#ifdef DO_NORMAL_CONFIG_TOO
  if (ioctl(fd,EMERALD_IOCSPORTCONFIG,&config) < 0) {
    fprintf(stderr,"ioctl EMERALD_IOCSPORTCONFIG: %s: %s\n",devname,strerror(errno));
    return -1;
  }
#endif

  if (ioctl(fd,EMERALD_IOCSEEPORTCONFIG,&config) < 0) {
    fprintf(stderr,"ioctl EMERALD_IOCSEEPORTCONFIG: %s: %s\n",devname,strerror(errno));
    return -1;
  }
  if (ioctl(fd,EMERALD_IOCEECONFIGLOAD) < 0) {
    fprintf(stderr,"ioctl EMERALD_IOCEECONFIGLOAD: %s: %s\n",devname,strerror(errno));
    return -1;
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

void usage(const char* argv0) {
  fprintf(stderr,"Usage: %s [-n] device [port0 irq ...]\n\n",argv0);
  fprintf(stderr,"\
-n: display the number of boards with acceptable EEPROM configuration\n\
device: device name, /dev/emerald0, /dev/emerald1, etc\n\
port0: ioport address of serial port 0 on the board\n\
    the 8 ports will be configured at port0, port0+0x8,\n\
    port0+0x10, ..., up to port0+0x38\n\
irq: interrupt level to use for each port.\n\
    if less than 8 irqs are listed, the last will be repeated\n\
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

  extern char *optarg;       /* set by getopt() */
  extern int optind;       /* "  "     "     */
  int opt_char;     /* option character */

  while ((opt_char = getopt(argc, argv, "n")) != -1) {
      switch (opt_char) {
      case 'n':
        getnumber = 1;
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
    if (sscanf(argv[optind++],"0x%x",&port0Addr) != 1) usage(argv[0]);

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

  if (port0Addr >= 0)
    if (setConfig(fd,devname,port0Addr,irqs) < 0) return 1;

  if (printConfig(fd,devname) < 0) return 1;
  close(fd);
  return 0;
}
