// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

int enablePorts(int fd,const char* devname) {
    if (ioctl(fd,EMERALD_IOCPORTENABLE) < 0) {
        fprintf(stderr,"ioctl EMERALD_IOCPORTENABLE: %s: %s\n",devname,strerror(errno));
        return -1;
    }
    return 0;
}

int getPortMode(int fd,const char* devname,int eepromAccess, int sport)
{
    emerald_mode prot;
    prot.port = sport;

    if (eepromAccess) {
        if (ioctl(fd,EMERALD_IOCG_EEMODE,&prot) < 0) {
            fprintf(stderr,"ioctl EMERALD_IOCG_EEMODE: %s: %s\n",devname,strerror(errno));
            return -1;
        }
    }
    else {
        if (ioctl(fd,EMERALD_IOCG_MODE,&prot) < 0) {
            fprintf(stderr,"ioctl EMERALD_IOCG_MODE: %s: %s\n",devname,strerror(errno));
            return -1;
        }
    }
    return prot.mode;
}

int setPortMode(int fd,const char* devname,int eepromAccess, int sport,int mode)
{
    emerald_mode prot;
    prot.port = sport;
    prot.mode = mode;

    if (eepromAccess) {
        if (ioctl(fd,EMERALD_IOCS_EEMODE,&prot) < 0) {
            fprintf(stderr,"ioctl EMERALD_IOCS_EEMODE: %s: %s\n",devname,strerror(errno));
            return -1;
        }
    }
    else {
        if (ioctl(fd,EMERALD_IOCS_MODE,&prot) < 0) {
            fprintf(stderr,"ioctl EMERALD_IOCS_MODE: %s: %s\n",devname,strerror(errno));
            return -1;
        }
    }
    return 0;
}


void usage(const char* argv0) {
    fprintf(stderr,"Usage: %s [-n] [-b] [-e] [-u] device [port0 irq ...]\n",argv0);
    fprintf(stderr,"       %s [-e] -p device serial_port [mode]\n\n",argv0);
    fprintf(stderr,"\
    -n: display the number of boards with acceptable EEPROM configuration\n\
    -b: display the ISA base address on the system, in 0xhhhh form\n\
    -e: set values in EEPROM, not temporary RAM\n\
    -u: up (enable) the ports\n\
    -p: get or set RS232/422/485 mode on a serial port\n\
        serial_port: serial port number, 0-7\n\
        mode: 0=RS232,1=RS422,2=RS485_ECHO, 3=RS485_NOECHO\n\
    device: device name, /dev/emerald0, /dev/emerald1, etc\n\
    port0: ioport address of serial port 0 on the board\n\
        the 8 ports will be configured and enabled at port0, port0+0x8,\n\
        port0+0x10, ..., up to port0+0x38\n\
    irq: interrupt level to use for each port.\n\
        if less than 8 irqs are listed, the last will be repeated\n\
######################################################################\n\
# WARNING: configuring and enabling serial ports on a system while the serial\n\
# driver is actively accessing those same ports can cause a system crash.\n\
# Make sure no process has the serial devices open, or if they are open\n\
# make sure no data is being read or written.\n\
######################################################################\n\
Examples:\n\
# query number of configured boards\n\
%s -n /dev/emerald0\n\
# query configuration of /dev/emerald1\n\
%s /dev/emerald1\n\n\
# enable /dev/emerald1 if the configuration is OK\n\
%s -u /dev/emerald1\n\
# configure and enable ports on first board at\n\
# iports 0x100,0x108,0x110,0x118,...,0x138, all irq=3\n\
%s /dev/emerald0 0x100 3\n\n",
        argv0,argv0,argv0,argv0);
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
    int enable = 0;
    int do_mode = 0;
    int sport = -1;
    int mode = -1;

    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "enbpu")) != -1) {
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
        case 'u':
            enable = 1;
            break;
        case 'p':
            do_mode = 1;
            break;
        case '?':
            usage(argv[0]);
            break;
        }
    }

    if (optind == argc) usage(argv[0]);
    devname = argv[optind++];

    if (do_mode) {
        if (optind == argc) usage(argv[0]);
        if (sscanf(argv[optind++],"%d",&sport) != 1) usage(argv[0]);

        if (optind < argc &&
            sscanf(argv[optind++],"%d",&mode) != 1) usage(argv[0]);
    }
    else if (optind < argc) {
        int i;
        if (sscanf(argv[optind++],"%x",&port0Addr) != 1) usage(argv[0]);

        if (optind == argc) usage(argv[0]);
        int nirq = 0;
        for ( ; optind < argc; optind++)
            if (sscanf(argv[optind],"%d",irqs + nirq++) != 1) usage(argv[0]);
        for (i = nirq; i < 8; i++) irqs[i] = irqs[nirq-1];
    }


    int res = 0;
    fd = open(devname,port0Addr < 0 ? O_RDONLY : O_RDWR);
    if (fd < 0) {
        fprintf(stderr,"open %s: %s\n",devname,strerror(errno));
        return 1;
    }
    if (do_mode) {
        if (mode < 0) {
            res = getPortMode(fd,devname,eepromAccess,sport);
            fprintf(stdout,"%d\n",res);
            if (res > 0) res = 0;
        }
        else {
            res = setPortMode(fd,devname,eepromAccess,sport,mode);
        }
    }
    else if (getnumber) {
        res = getNumBoards(fd,devname);
        fprintf(stdout,"%d\n",res);
        if (res > 0) res = 0;
    }
    else if (getbaseaddr) {
        unsigned long addr;
        res = getISABaseAddr(fd,devname,&addr);
        fprintf(stdout,"%#lx\n",addr);
    }
    else if (port0Addr >= 0) {
        if ((res = setConfig(fd,devname,port0Addr,irqs,eepromAccess)) == 0)
            res = enablePorts(fd,devname);
    }
    else if (enable) res = enablePorts(fd,devname);
    else res = printConfig(fd,devname,eepromAccess);

    close(fd);
    return (res < 0 ? 1 : 0);
}
