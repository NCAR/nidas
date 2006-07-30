/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef __RTCORE_KERNEL__
#define __RTCORE_KERNEL__
#endif

#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_unistd.h>
#include <rtl_fcntl.h>
#include <rtl_signal.h>
#include <rtl_errno.h>

#include <linux/slab.h>
#include <linux/list.h>

#include <nidas/rtlinux/dsmlog.h>
#include <nidas/rtlinux/ioctl_fifo.h>
#include <nidas/rtlinux/dsm_version.h>

RTLINUX_MODULE(ioctl_fifo);

static char* devdir = "/dev";
MODULE_PARM(devdir, "s");
MODULE_PARM_DESC(devdir, "directory to place RTL FIFO device files");

// #define DEBUG

LIST_HEAD(ioctlList);

static rtl_pthread_mutex_t listmutex = RTL_PTHREAD_MUTEX_INITIALIZER;

static unsigned char* inputbuf = 0;
static rtl_size_t inputbufsize = 0;

static rtl_pthread_mutex_t bufmutex = RTL_PTHREAD_MUTEX_INITIALIZER;

static int nopen = 0;

static void ioctlHandler(int sig, rtl_siginfo_t *siginfo, void *v);

static unsigned char ETX = '\003';

#define DSMIOCTL_BUFSIZE 8192
 
/**
 * Exposed function to get the device directory.
 */
const char* getDevDir() { return devdir; }
/**
 * Make a FIFO name. This must match what is done with a device prefix
 * on the user side.
 */
char* makeDevName(const char* prefix, const char* suffix,
	int num)
{
    char numstr[16];
    sprintf(numstr,"%d",num);
    char* name = rtl_gpos_malloc(
    	strlen(devdir) + strlen(prefix) + strlen(suffix) + strlen(numstr) + 2 );
    strcpy(name,devdir);
    strcat(name,"/");
    strcat(name,prefix);
    strcat(name,suffix);
    strcat(name,numstr);
    return name;
}

/**********************************************************************
 * Open the FIFO.  Must be called from a drivers init_module, since we do
 * a rtl_gpos_malloc.
 **********************************************************************/
struct ioctlHandle* openIoctlFIFO(const char* devicePrefix,
	int boardNum,ioctlCallback_t* callback,
	int nioctls,struct ioctlCmd* ioctls)
{
    int l;
    struct ioctlHandle* handle = 
	(struct ioctlHandle*) rtl_gpos_malloc( sizeof(struct ioctlHandle) );

    handle->ioctlCallback = callback;
    handle->boardNum = boardNum;
    handle->nioctls = nioctls;
    handle->ioctls = ioctls;
    handle->inFifoName = 0;
    handle->inFifofd = -1;
    handle->outFifoName = 0;
    handle->outFifofd = -1;
    handle->bufsize = 0;
    handle->buf = 0;

    int icmd;
    for (icmd = 0; icmd < nioctls; icmd++)
	if (ioctls[icmd].size > handle->bufsize)
	    handle->bufsize = ioctls[icmd].size;
    
    if (!(handle->buf = rtl_gpos_malloc( handle->bufsize ))) goto error;

    /* increase size of input buffer if necessary */
    l = handle->bufsize + sizeof(struct ioctlHeader) + 1;
    if (inputbufsize < l) {

#ifdef DEBUG
	DSMLOG_DEBUG("nopen=%d, inputbuf=0x%x, inputbufsize=%d\n",
	    nopen,inputbuf,inputbufsize);
#endif

	rtl_pthread_mutex_lock(&bufmutex);

	rtl_gpos_free(inputbuf);
	inputbuf = rtl_gpos_malloc(l);

	rtl_pthread_mutex_unlock(&bufmutex);
	if (!inputbuf) goto error;
	inputbufsize = l;

	if (inputbufsize > DSMIOCTL_BUFSIZE) 
	    DSMLOG_WARNING("increase DSMIOCTL_BUFSIZE=%d, inputbufsize=%d\n",
		DSMIOCTL_BUFSIZE, inputbufsize);
    }
    	
    /* in and out are from the user perspective */
    handle->inFifoName = makeDevName(devicePrefix,"_ictl_",boardNum);
#ifdef DEBUG
    DSMLOG_DEBUG("creating %s\n",handle->inFifoName);
#endif

    // remove broken device file before making a new one
    if (rtl_unlink(handle->inFifoName) < 0 && rtl_errno != RTL_ENOENT)
    	goto error;

    if (rtl_mkfifo(handle->inFifoName, 0666) < 0) goto error;

    if ((handle->inFifofd = rtl_open(handle->inFifoName, RTL_O_NONBLOCK | RTL_O_WRONLY)) < 0) {
	DSMLOG_ERR("error: open %s: %s\n",
		handle->inFifoName,rtl_strerror(rtl_errno));
	closeIoctlFIFO(handle);
	return 0;
    }

// #define DO_FTRUNCATE
#ifdef DO_FTRUNCATE
    size_t fifobufsize = handle->bufsize * 2;
    if (fifobufsize < 128) fifobufsize = 256;
    DSMLOG_DEBUG("ftruncate %s: size=%d\n", handle->inFifoName,fifobufsize);
    if (rtl_ftruncate(handle->inFifofd, fifobufsize) < 0) {
	DSMLOG_ERR("error: ftruncate %s: size=%d: %s\n",
		handle->inFifoName,fifobufsize,
		rtl_strerror(rtl_errno));
	closeIoctlFIFO(handle);
	return 0;
    }
#endif

    handle->outFifoName = makeDevName(devicePrefix,"_octl_",boardNum);
#ifdef DEBUG
    DSMLOG_DEBUG("creating %s\n",handle->outFifoName);
#endif

    // remove broken device file before making a new one
    if (rtl_unlink(handle->outFifoName) < 0 && rtl_errno != RTL_ENOENT)
    	goto error;

    if (rtl_mkfifo(handle->outFifoName, 0666) < 0) goto error;
    if ((handle->outFifofd = rtl_open(handle->outFifoName, RTL_O_NONBLOCK | RTL_O_RDONLY)) < 0) {
	DSMLOG_ERR("error: open %s: %s\n",
		handle->outFifoName,rtl_strerror(rtl_errno));
	closeIoctlFIFO(handle);
	return 0;
    }
#ifdef DO_FTRUNCATE
    DSMLOG_DEBUG("ftruncate %s: size=%d\n", handle->outFifoName,fifobufsize);
    if (rtl_ftruncate(handle->outFifofd, fifobufsize) < 0) {
	DSMLOG_ERR("error: ftruncate %s: size=%d: %s\n",
		handle->outFifoName, fifobufsize,
		rtl_strerror(rtl_errno));
	closeIoctlFIFO(handle);
	return 0;
    }
#endif

    handle->bytesRead = handle->bytesToRead = 0;
    handle->icmd = -1;
    handle->readETX = 0;

    rtl_pthread_mutex_init(&handle->mutex,0);

    /* add this to the list before registering the signal handler.  */
    rtl_pthread_mutex_lock(&listmutex);
    list_add(&handle->list,&ioctlList);
    rtl_pthread_mutex_unlock(&listmutex);

    struct rtl_sigaction sigact;
    rtl_memset(&sigact,0,sizeof(sigact));
    sigact.sa_sigaction = ioctlHandler;
    sigact.sa_fd        = handle->outFifofd;
    rtl_sigemptyset(&sigact.sa_mask);
    sigact.sa_flags     = RTL_SA_RDONLY | RTL_SA_SIGINFO;
    if ( rtl_sigaction(RTL_SIGPOLL, &sigact, NULL ) != 0 ) {
	DSMLOG_WARNING("error in rtl_sigaction(RTL_SIGPOLL,...) for %s\n",
	    handle->outFifoName);
	goto error;
    }

    nopen++;
    DSMLOG_NOTICE("nopen=%d\n",nopen);

    return handle;
error:
    closeIoctlFIFO(handle);
    return 0;
}

/**********************************************************************
 * Close the FIFO, called from other drivers.
 **********************************************************************/
void closeIoctlFIFO(struct ioctlHandle* handle)
{
  rtl_pthread_mutex_lock(&listmutex);
  rtl_pthread_mutex_lock(&handle->mutex);

  if (handle->inFifofd >= 0) rtl_close(handle->inFifofd);
  if (handle->outFifofd >= 0) rtl_close(handle->outFifofd);

  if (handle->inFifoName) {
      rtl_unlink(handle->inFifoName);
#ifdef DEBUG
      DSMLOG_DEBUG("removed %s\n", handle->inFifoName);
#endif
      rtl_gpos_free(handle->inFifoName);
      handle->inFifoName = 0;
  }
  if (handle->outFifoName) {
      rtl_unlink(handle->outFifoName);
#ifdef DEBUG
      DSMLOG_DEBUG("removed %s\n", handle->outFifoName);
#endif
      rtl_gpos_free(handle->outFifoName);
      handle->outFifoName = 0;
  }

  rtl_gpos_free(handle->buf);

  list_del(&handle->list);

  rtl_pthread_mutex_unlock(&handle->mutex);

  rtl_gpos_free(handle);

  rtl_pthread_mutex_unlock(&listmutex);
    nopen--;
    DSMLOG_NOTICE("nopen=%d\n",nopen);
}

/**
 * Send a error response back on the FIFO.
 * 4 byte non-zero rtl_errno int, 4 byte length, length message, ETX.
 */
static int sendError(int fifofd,int errval, const char* msg)
{
  DSMLOG_WARNING("errval=%d,msg=%s\n",errval,msg);
  rtl_write(fifofd,&errval,sizeof(int));
  int len = strlen(msg) + 1;
  rtl_write(fifofd,&len,sizeof(int));
  rtl_write(fifofd,msg,len);
  rtl_write(fifofd,&ETX,1);
  return 0;
}

/**
 * Send a OK response back on the FIFO.
 * 4 byte int 0, 4 byte length, length byte buffer, ETX
 */
static int sendResponse(int fifofd,unsigned char* buf, int len)
{
#ifdef DEBUG
  DSMLOG_DEBUG("sendResponse, len=%d\n",len);
#endif
  int errval = 0;
  rtl_write(fifofd,&errval,sizeof(int));
  rtl_write(fifofd,&len,sizeof(int));
  if (len > 0) rtl_write(fifofd,buf,len);
  rtl_write(fifofd,&ETX,1);
  return 0;
}

/**
 * RTL_SIGPOLL handler for the ioctl FIFO, which is called when there
 * is data to be read on the FIFO from user space programs.
 * We look through our list of registered ioctlHandle structures for
 * one with the correct file descriptor, then read bytes from the FIFO.
 * Once we read the cmd value from the FIFO we then know the
 * direction of the ioctl request (get or set), and the length
 * of the data involved.
 *
 * Since we aren't using SA_NODEFER or SA_NOMASK flags in sig_action
 * only one ioctlHandler function will be executing at a time,
 * which is why we can get away with having only one inputbuf.
 *
 * This function does only one read of the fifo (we want to avoid blocking).
 * We don't assume that one read of the fifo will get all the data
 * needed to perform an ioctl, but save any necessary state so that
 * the ioctl can be completed in subsequent reads.
 * 
 */
static void ioctlHandler(int sig, rtl_siginfo_t *siginfo, void *v)
{
  int outFifofd = siginfo->si_fd;
  struct list_head *ptr;
  struct ioctlHandle* entry;
  struct ioctlHandle* handle = NULL;
  unsigned char* inbufptr;
  unsigned char* inbufeod;
  int icmd;
  int nread;
  int res;

#ifdef DEBUG
  DSMLOG_DEBUG("sig=%d, outFifofd=%d code=%d\n",
  	sig,outFifofd,siginfo->si_code);
#endif


  if (siginfo->si_code == RTL_POLL_OUT) {
#ifdef DEBUG
    DSMLOG_DEBUG("ignoring RTL_POLL_OUT\n");
#endif
    return;	// read at user end
  }

  rtl_pthread_mutex_lock(&listmutex);
  for (ptr = ioctlList.next; ptr != &ioctlList; ptr = ptr->next) {
    entry = list_entry(ptr,struct ioctlHandle, list);
    if (entry->outFifofd == outFifofd) {
      handle = entry;
      break;
    }
  }
  rtl_pthread_mutex_unlock(&listmutex);

  if (handle == NULL) {
    DSMLOG_WARNING("can't find handle for FIFO file descriptor %d\n",
    	outFifofd);
    sendError(handle->inFifofd,RTL_EINVAL,"can't find handle for this FIFO");
    return;
  }

#ifdef DEBUG
  DSMLOG_DEBUG("found handle = 0x%x\n",(unsigned int)handle);
#endif

  rtl_pthread_mutex_lock(&handle->mutex);

  rtl_pthread_mutex_lock(&bufmutex);

  // All returns after this point must go through unlock: label!

  if ((nread = rtl_read(outFifofd,inputbuf,inputbufsize)) < 0) {
    DSMLOG_WARNING("read failure on %s\n",
    	handle->outFifoName);
    sendError(handle->inFifofd,rtl_errno,"error reading from FIFO");
    goto unlock;
  }
#ifdef DEBUG
  DSMLOG_DEBUG("read %d bytes\n",nread);
#endif
  if (nread == 0) goto unlock;

  inbufeod = inputbuf + nread;

  /*
   * The data coming from user space on the FIFO is formatted as
   * follows:
   *   4 byte int containing the command - one of the ioctl
   *       cmds supported by the driver.
   *   4 byte int containing the port index, 0-(N-1), for a board
   *       with N ports (i.e. a serial port card).
   *   4 byte int size of the request.
   *   If the request is a set from the user side
   *       (_IOC_DIR is an IOC_WRITE) a buffer of the specified
   *       size will follow
   *   4 byte ETX ('\003' character).
   *  
   *   The ETX is there in case things get screwed up. After 
   *   reading the above from the FIFO we scan for an ETX
   *   to make sure we're in sync.
   */

  for (inbufptr = inputbuf; inbufptr < inbufeod; ) {

#ifdef DEBUG
    DSMLOG_DEBUG("chars left in buffer=%d\n",inbufeod-inbufptr);
#endif

    if (handle->readETX) {
#ifdef DEBUG
	DSMLOG_DEBUG("search for ETX\n");
#endif
      for (; inbufptr < inbufeod; )  {
#ifdef DEBUG
	DSMLOG_DEBUG("ETX search char=%d, ETX=%d\n",(int) *inbufptr,(int)ETX);
#endif
        if (*inbufptr++ == ETX) {
	    handle->readETX = 0;
#ifdef DEBUG
	    DSMLOG_DEBUG("read ETX\n");
#endif
	    break;
	}
	else DSMLOG_WARNING("didn't find ETX\n");
      }
      if (inbufptr == inbufeod) break;
    }

#ifdef DEBUG
    DSMLOG_DEBUG("bytesRead= %d\n",handle->bytesRead);
#endif

    for ( ; handle->bytesRead < sizeof(struct ioctlHeader) &&
	  inbufptr < inbufeod; )
      ((unsigned char*)&(handle->header))[handle->bytesRead++] = *inbufptr++;

#ifdef DEBUG
    DSMLOG_DEBUG("bytesRead2= %d\n",handle->bytesRead);
#endif

    if (handle->bytesRead < sizeof(struct ioctlHeader))  break;

    /* We now have read the cmd, port number and buffer size from the FIFO */

#ifdef DEBUG
    DSMLOG_DEBUG("cmd= %d\n",handle->header.cmd);
#endif

    if ((icmd = handle->icmd) < 0) {
      /* find out which cmd it is. */
      int cmd = handle->header.cmd;
#ifdef DEBUG
      DSMLOG_DEBUG("cmd= %d\n",cmd);
#endif

      for (icmd = 0; icmd < handle->nioctls; icmd++)
	if (handle->ioctls[icmd].cmd == cmd) break;

#ifdef DEBUG
      DSMLOG_DEBUG("icmd=%d, cmd= %d \n",icmd,cmd);
#endif
     if (icmd == handle->nioctls) {
	DSMLOG_WARNING("cmd= 0x%x not supported\n",
		cmd);
	sendError(handle->inFifofd,RTL_EINVAL,"cmd not supported");
	handle->bytesRead = 0;
	handle->readETX = 1;
	handle->icmd = -1;
	continue;
      }
      handle->icmd = icmd;

      if (handle->header.size > handle->bufsize) {
	  sendError(handle->inFifofd,RTL_EINVAL,"size too large");
	  DSMLOG_WARNING("header size, %d, larger than bufsize %d\n",
	  	handle->header.size,handle->bufsize);
	  handle->bytesRead = 0;
	  handle->readETX = 1;
	  handle->icmd = -1;
	  continue;
      }
    }

    /* user set: read data from fifo */
    if (_IOC_DIR(handle->header.cmd) & _IOC_WRITE) {
#ifdef DEBUG
      DSMLOG_DEBUG("__IOC_WRITE\n");
#endif
      handle->bytesToRead = sizeof(struct ioctlHeader) +
      	handle->header.size;
#ifdef DEBUG
      DSMLOG_DEBUG("_IOC_WRITE, bytesToRead=%d\n",
      	handle->bytesToRead);
#endif
      for (; handle->bytesRead < handle->bytesToRead && inbufptr < inbufeod; )
	handle->buf[handle->bytesRead++ - sizeof(struct ioctlHeader)] = *inbufptr++;

      if (handle->bytesRead < handle->bytesToRead) break;	/* not done */
    }

    /* call ioctl function on device */
#ifdef DEBUG
    DSMLOG_DEBUG("calling ioctlCallback, port=%d\n",
        handle->header.port);
#endif
    res = handle->ioctlCallback(handle->header.cmd,
	  handle->boardNum,handle->header.port,handle->buf,
	  	handle->header.size);

    if (res < 0) sendError(handle->inFifofd,-res,"ioctl error");
    else if (_IOC_DIR(handle->header.cmd) & _IOC_READ) {
#ifdef DEBUG
	DSMLOG_DEBUG("__IOC_READ\n");
#endif
	sendResponse(handle->inFifofd,handle->buf,res);
    }
    else sendResponse(handle->inFifofd,handle->buf,0);

    handle->bytesRead = 0;
    handle->readETX = 1;
    handle->icmd = -1;
  }
unlock:
  rtl_pthread_mutex_unlock(&bufmutex);
  rtl_pthread_mutex_unlock(&handle->mutex);
#ifdef DEBUG
  DSMLOG_DEBUG("returning\n");
#endif
  return;
}
/*
 * Return: negative Linux (not RTLinux) errno.
 */
int init_module(void)
{
    DSMLOG_NOTICE("version: %s\n",DSM_VERSION_STRING);
#ifdef DEBUG
    DSMLOG_NOTICE("inputbuf=0x%x, inputbufsize=%d, nopen=%d\n",
    	inputbuf,inputbufsize,nopen);
#endif
    return 0;
}

void cleanup_module (void)
{
    rtl_gpos_free(inputbuf);
    if (nopen > 0)
	DSMLOG_ERR("Error: %d ioctl fifos still open\n",nopen);
    DSMLOG_NOTICE("done\n");
}

int convert_rtl_errno(int rtlerr)
{
    switch (rtlerr) {
    case RTL_E2BIG: 		return E2BIG;
    case RTL_EACCES: 		return EACCES;
    case RTL_EADDRINUSE: 	return EADDRINUSE;
    case RTL_EADDRNOTAVAIL: 	return EADDRNOTAVAIL;
    case RTL_EAFNOSUPPORT: 	return EAFNOSUPPORT;
    case RTL_EAGAIN: 		return EAGAIN;
    case RTL_EALREADY: 		return EALREADY;
    case RTL_EBADF: 		return EBADF;
    case RTL_EBADMSG: 		return EBADMSG;
    case RTL_EBUSY: 		return EBUSY;
    case RTL_ECANCELED:		return EINTR;	// no ECANCELED, use EINTR
    case RTL_ECHILD: 		return ECHILD;
    case RTL_ECONNABORTED: 		return ECONNABORTED;
    case RTL_ECONNREFUSED: 	return ECONNREFUSED;
    case RTL_ECONNRESET: 	return ECONNRESET;
    case RTL_EDEADLK: 		return EDEADLK;
    case RTL_EDESTADDRREQ: 	return EDESTADDRREQ;
    case RTL_EDOM: 		return EDOM;
    case RTL_EDQUOT: 		return EDQUOT;
    case RTL_EEXIST: 		return EEXIST;
    case RTL_EFAULT: 		return EFAULT;
    case RTL_EFBIG: 		return EFBIG;
    case RTL_EHOSTUNREACH: 	return EHOSTUNREACH;
    case RTL_EIDRM: 		return EIDRM;
    case RTL_EILSEQ: 		return EILSEQ;
#ifdef RTL_EINPROGRESS
    case RTL_EINPROGRESS: 	return EINPROGRESS;
#else							// rtl typo
    case RTL_EINPROGESS: 	return EINPROGRESS;
#endif
    case RTL_EINTR: 		return EINTR;
    case RTL_EINVAL: 		return EINVAL;
    case RTL_EIO: 		return EIO;
    case RTL_EISCONN: 		return EISCONN;
    case RTL_EISDIR: 		return EISDIR;
    case RTL_ELOOP: 		return ELOOP;
    case RTL_EMFILE: 		return EMFILE;
    case RTL_EMLINK: 		return EMLINK;
    case RTL_EMSGSIZE: 		return EMSGSIZE;
    case RTL_EMULTIHOP: 	return EMULTIHOP;
    case RTL_ENAMETOOLONG: 	return ENAMETOOLONG;
    case RTL_ENETDOWN: 		return ENETDOWN;
    case RTL_ENETRESET: 	return ENETRESET;
    case RTL_ENETUNREACH: 	return ENETUNREACH;
    case RTL_ENFILE: 		return ENFILE;
    case RTL_ENOBUFS: 		return ENOBUFS;
    case RTL_ENODATA: 		return ENODATA;
    case RTL_ENODEV: 		return ENODEV;
    case RTL_ENOENT: 		return ENOENT;
    case RTL_ENOEXEC: 		return ENOEXEC;
    case RTL_ENLOCK: 		return ENOLCK;	// no ENLOCK, use ENOLCK
    case RTL_ENOLINK: 		return ENOLINK;
    case RTL_ENOMEM: 		return ENOMEM;
    case RTL_ENOMSG: 		return ENOMSG;
    case RTL_ENOPROTOOPT: 	return ENOPROTOOPT;
    case RTL_ENOSPC: 		return ENOSPC;
    case RTL_ENOSR: 		return ENOSR;
    case RTL_ENOSTR: 		return ENOSTR;
    case RTL_ENOSYS: 		return ENOSYS;
    case RTL_ENOTCONN: 		return ENOTCONN;
    case RTL_ENOTDIR: 		return ENOTDIR;
    case RTL_ENOTEMPTY: 	return ENOTEMPTY;
    case RTL_ENOTSOCK: 		return ENOTSOCK;
    case RTL_ENOTSUP: 		return EINVAL;	// no ENOTSUP, use EINVAL
    case RTL_ENOTTY: 		return ENOTTY;
    case RTL_ENXIO: 		return ENXIO;
    case RTL_EOPNOTSUPP: 	return EOPNOTSUPP;
    case RTL_EOVERFLOW: 	return EOVERFLOW;
    case RTL_EPERM: 		return EPERM;
    case RTL_EPIPE: 		return EPIPE;
    case RTL_EPROTO: 		return EPROTO;
    case RTL_EPROTONOSUPPORT: 	return EPROTONOSUPPORT;
    case RTL_EPROTOTYPE: 	return EPROTOTYPE;
    case RTL_ERANGE: 		return ERANGE;
    case RTL_EROFS: 		return EROFS;
    case RTL_ESPIPE: 		return ESPIPE;
    case RTL_ESRCH: 		return ESRCH;
    case RTL_ESTALE: 		return ESTALE;
    case RTL_ETIME: 		return ETIME;
    case RTL_ETIMEDOUT: 	return ETIMEDOUT;
    case RTL_ETXTBSY: 		return ETXTBSY;
    case RTL_EWOULDBLOCK: 	return EWOULDBLOCK;
    case RTL_EXDEV: 		return EXDEV;
    default: DSMLOG_WARNING("unknown rtl_errno=%d, %s\n",
    	rtlerr,rtl_strerror(rtlerr));
				return EINVAL;
    }
}

