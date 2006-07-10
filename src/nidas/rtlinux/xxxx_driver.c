/* xxxx_driver.c

   Time-stamp: <Wed 13-Apr-2005 05:52:09 pm>

   Test rtl driver.

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

*/

#include <xxxx_driver.h>

/* RTLinux module includes...  */
#define __RTCORE_POLLUTED_APP__
#include <rtl.h>
#include <rtl_posixio.h>
#include <rtl_pthread.h>
#include <rtl_stdio.h>
#include <rtl_unistd.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>

/****************  IOCTL Section ***************8*********/

static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,_IOC_SIZE(GET_NUM_PORTS) },
  { XXXX_GET_IOCTL,_IOC_SIZE(XXXX_GET_IOCTL) },
  { XXXX_SET_IOCTL,_IOC_SIZE(XXXX_SET_IOCTL) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);

static struct ioctlHandle* ioctlhandle = 0;

/****************  End of IOCTL Section ******************/

static int boardNum = 0;
static int nports = 4;

/* module prameters (can be passed in via command line) */
static unsigned int xxxx_parm = 0;
MODULE_PARM(xxxx_parm, "1l");

RTLINUX_MODULE(xxxx);
MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_DESCRIPTION("Test RTL driver");

/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 */
static int ioctlCallback(int cmd, int board, int port,
	void *buf, rtl_size_t len) {

  rtl_printf("ioctlCallback, cmd=%d, board=%d, port=%d,len=%d\n",
		cmd,board,port,len);

  int ret = -RTL_EINVAL;
  switch (cmd) {
  case GET_NUM_PORTS:		/* user get */
    {
	rtl_printf("GET_NUM_PORTS\n");
	*(int *) buf = nports;
	ret = sizeof(int);
    }
    break;
  case XXXX_GET_IOCTL:		/* user get */
    {
	struct xxxx_get* tg = (struct xxxx_get*) buf;
	strncpy(tg->c,"0123456789",len);
	tg->c[len-1] = 0;
	ret = len;
    }
    break;
  case XXXX_SET_IOCTL:		/* user set */
      {
	struct xxxx_set* ts = (struct xxxx_set*) buf;
	rtl_printf("received xxxx_set.c=%s\n",ts->c);
	ret = len;
    }
    break;
  }
  return ret;
}


/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{

  /* Close my ioctl FIFOs, deregister my ioctlCallback function */
  closeIoctlFIFO(ioctlhandle);

}

/* -- MODULE ---------------------------------------------------------- */
int init_module (void)
{
  rtl_printf("(%s) %s:\t compiled on %s at %s\n\n",
	     __FILE__, __FUNCTION__, __DATE__, __TIME__);

  /* Open up my ioctl FIFOs, register my ioctlCallback function */
  ioctlhandle = openIoctlFIFO("xxxx",boardNum,ioctlCallback,
  	nioctlcmds,ioctlcmds);
  if (!ioctlhandle) return -RTL_EIO;

  return 0;

}
