/*
  Support posix-like ioctl calls over a Linux FIFO from
  user space to a Linux module.

  Original Author: Gordon Maclean

  Copyright 2005 UCAR, NCAR, All Rights Reserved

*/

#ifndef IOCTL_FIFO_H
#define IOCTL_FIFO_H

#include <linux/ioctl.h>

/* symbols used by user and kernel space code. */

#define IOCTL_FIFO_MAGIC 'F'

#define GET_NUM_PORTS  _IOR(IOCTL_FIFO_MAGIC,0,int)


/* Below here are symbols used by the ioctl_fifo module. */

#include <linux/fcntl.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/list.h>

struct ioctlCmd {
  int cmd;
  long size;
};

/* typedef for driver module function that is called to
 * satisfy an ioctl request.
 */
typedef int ioctlCallback_t(int cmd, int board, int port,
	void* buf, size_t len);


/* header that is sent over the FIFOs before the ioctl data */
struct ioctlHeader {
  int cmd;		/* ioctl command */
  int port;		/* destination port */
  long size;		/* length in bytes of the data */
};

struct ioctlHandle {

  ioctlCallback_t *ioctlCallback;
  int boardNum;			/* which board number am I */
  struct ioctlCmd* ioctls;	/* pointer to array of struct ioctlCmds */
  int nioctls;			/* how many ioctlCmds do I support */

  char *inFifoName;
  char *outFifoName;
  int inFifofd;
  int outFifofd;
  int bytesRead;
  int bytesToRead;
  char  readETX;		/* logical: read EXT (003) character next */
  int icmd;
  struct ioctlHeader header;
  unsigned char *buf;
  long bufsize;

//  rtl_pthread_mutex_t mutex;
  struct list_head list;

};

struct ioctlHandle*  openIoctlFIFO(const char* prefix,int boardNum,
	ioctlCallback_t* callback,int nioctls,struct ioctlCmd* ioctls);

void closeIoctlFIFO(struct ioctlHandle* ioctls);

const char* getDevDir(void);

char* makeDevName(const char* prefix, const char* suffix, int num);

#endif	/* IOCTL_FIFO_H */
