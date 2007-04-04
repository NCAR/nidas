/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-02-01 05:36:37 -0700 (Thu, 01 Feb 2007) $

    $LastChangedRevision: 3653 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/apps/isff/set_pcmcom8.c $

    App to query and set options on a Diamond pcmcom8 serial IO card.
    Interacts via ioctls with the pcmcom8 kernel module.
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

#include <nidas/linux/serial/pcmcom8.h>

int printConfig(int fd,const char* devname) {
    struct pcmcom8_config config;
    int i;
    printf("current port config:\n");

    if (ioctl(fd,PCMCOM8_IOCGPORTCONFIG,&config) < 0) {
        fprintf(stderr,"ioctl PCMCOM8_IOCGPORTCONFIG: %s: %s\n",devname,strerror(errno));
        return -1;
    }
    // output in a form like options to setserial
    for (i = 0; i < 8; i++) printf("port %#x irq %d\n",
          config.ports[i].ioport,config.ports[i].irq);

#ifdef EEPROM_CONFIG_TOO
    if (ioctl(fd,PCMCOM8_IOCGEEPORTCONFIG,&config) < 0) {
        fprintf(stderr,"ioctl PCMCOM8_IOCGEEPORTCONFIG: %s: %s\n",devname,strerror(errno));
        return -1;
    }
    for (i = 0; i < 8; i++) printf("port %#x irq %d\n",
          config.ports[i].ioport,config.ports[i].irq);
#endif
    return 0;

}
int setConfig(int fd,const char* devname,int port0Addr,const int* irqs)
{
    int i;
    struct pcmcom8_config config;

    printf("setting ports:\n");
    for (i = 0; i < 8; i++) {
      config.ports[i].ioport = port0Addr + i * 8;
      config.ports[i].irq = irqs[i];
      printf("port %#x irq %d\n",
        config.ports[i].ioport,config.ports[i].irq);
    }

    /* We shouldn't need to set the RAM config. The last step of
     * setting the EEPROM config now requests that the RAM settings
     * be read from EEPROM.
     */
#ifdef DO_NORMAL_CONFIG_TOO
    if (ioctl(fd,PCMCOM8_IOCSPORTCONFIG,&config) < 0) {
        fprintf(stderr,"ioctl PCMCOM8_IOCSPORTCONFIG: %s: %s\n",devname,strerror(errno));
        return -1;
    }
#endif

    if (ioctl(fd,PCMCOM8_IOCSEEPORTCONFIG,&config) < 0) {
        fprintf(stderr,"ioctl PCMCOM8_IOCSEEPORTCONFIG: %s: %s\n",devname,strerror(errno));
        return -1;
    }
    if (ioctl(fd,PCMCOM8_IOCEECONFIGLOAD) < 0) {
        fprintf(stderr,"ioctl PCMCOM8_IOCEECONFIGLOAD: %s: %s\n",devname,strerror(errno));
        return -1;
    }
    if (ioctl(fd,PCMCOM8_IOCPORTENABLE) < 0) {
        fprintf(stderr,"ioctl PCMCOM8_IOCPORTENABLE: %s: %s\n",devname,strerror(errno));
        return -1;
    }
    return 0;
}

int getNumBoards(int fd,const char* devname)
{
    int nboards;
    if (ioctl(fd,PCMCOM8_IOCGNBOARD,&nboards) < 0) {
        fprintf(stderr,"ioctl PCMCOM8_IOCGNBOARD: %s: %s\n",devname,strerror(errno));
        return -1;
    }
    return nboards;
}

int getISABaseAddr(int fd,const char* devname,unsigned long* baseaddr)
{
    if (ioctl(fd,PCMCOM8_IOCGISABASE,baseaddr) < 0) {
        fprintf(stderr,"ioctl PCMCOM8_IOCGISABSE: %s: %s\n",devname,strerror(errno));
        return -1;
    }
    return 0;
}

void usage(const char* argv0)
{
    fprintf(stderr,"Usage: %s [-n] [-b] device [port0 irq ...]\n\n",argv0);
    fprintf(stderr,"\
-n: display the number of boards with acceptable EEPROM configuration\n\
-b: display the ISA base address on the system, in 0xhhhh form\n\
device: device name, /dev/pcmcom8_0, /dev/pcmcom8_1, etc\n\
port0: ioport address of serial port 0 on the board\n\
    the 8 ports will be configured at port0, port0+0x8,\n\
    port0+0x10, ..., up to port0+0x38\n\
irq: interrupt level to use for each port.\n\
    if less than 8 irqs are listed, the last will be repeated\n\
Examples:\n\
    # configure ports on first board at\n\
    # iports 0x100,0x108,0x110,0x118,...,0x138, all irq=3\n\
    %s /dev/pcmcom8_0 0x100 3\n\n\
    # query configuration of /dev/pcmcom8_1\n\
    %s /dev/pcmcom8_1\n\n\
    # query number of configured boards\n\
    %s -n /dev/pcmcom8_0\n\n",
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

    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "nb")) != -1) {
        switch (opt_char) {
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
        if (setConfig(fd,devname,port0Addr,irqs) < 0) return 1;

    if (printConfig(fd,devname) < 0) return 1;
    close(fd);
    return 0;
}
