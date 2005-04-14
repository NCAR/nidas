/*
  *******************************************************************
  Copyright 2005 UCAR, NCAR, All Rights Reserved
									      
  $LastChangedDate$
									      
  $LastChangedRevision$
									      
  $LastChangedBy$
									      
  $HeadURL$

  *******************************************************************
									      
  FIFO interface between dsm_serial serial port driver and the
  user side.
									      
  Since user space code cannot read directly from RT-Linux devices
  one has to provide a read/write pair of FIFOs to the user side.
									      
  And, since an RT-Linux interrupt function cannot write directly
  to a FIFO, we implement an RT-Linux device in dsm_serial, and
  this module provides the pair of threads to broker the data
  between the device and the FIFOs.
									      
  This module also supports an IOCTL FIFO that the user can
  send commands over.  Upon receipt of an ioctl command
  this module does things like opening/closing fifos and
  the serial port device, and also can send ioctl commands
  on to the RT-Linux device.
									      
*/

#ifndef DSM_SERIAL_FIFO_H
#define DSM_SERIAL_FIFO_H

#ifdef BOZO

#ifndef __RTCORE_KERNEL__
#include <sys/rtl_ioctl.h>
#endif

#endif

#define DSM_SERIAL_FIFO_MAGIC 'F'

#define DSMSER_OPEN _IOW(DSM_SERIAL_FIFO_MAGIC,0,int)
#define DSMSER_CLOSE _IO(DSM_SERIAL_FIFO_MAGIC,1)

#ifdef __RTCORE_KERNEL__

#include <ioctl_fifo.h>

struct dsm_serial_fifo_port {
  char* inFifoName;
  char* outFifoName;
  char* devname;
  int devfd;
  int inFifoFd;
  int outFifoFd;
  void* in_thread_stack;
  void* out_thread_stack;
  rtl_pthread_t in_thread;
  rtl_pthread_t out_thread;

};

struct dsm_serial_fifo_board {
    struct dsm_serial_fifo_port* ports;
    int numports;
    struct ioctlHandle* ioctlhandle;
};

  

#endif	/* __RTCORE_KERNEL__ */

#endif
