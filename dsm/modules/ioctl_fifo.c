#ifndef __KERNEL__
#define __KERNEL__
#endif

#include <ioctl_fifo.h>

#include <unistd.h>
#include <fcntl.h>

#include <linux/slab.h>
#include <linux/list.h>

#include <rtl_signal.h>
#include <rtl_errno.h>

/* #define DEBUG */

LIST_HEAD(ioctlList);

pthread_mutex_t listmutex = PTHREAD_MUTEX_INITIALIZER;

static void ioctlHandler(int sig, rtl_siginfo_t *siginfo, void *v);

static unsigned char ETX = '\003';
  
/**
 * Make a FIFO name. This must match what is done with a device prefix
 * on the user side.
 */
char* makeDevName(const char* prefix, const char* suffix,
	int num)
{
    char numstr[16];
    sprintf(numstr,"%d",num);
    char* name = kmalloc(strlen(prefix) + strlen(suffix) + strlen(numstr) + 6,
    	GFP_KERNEL);
    strcpy(name,"/dev/");
    strcat(name,prefix);
    strcat(name,suffix);
    strcat(name,numstr);
    return name;
}

/**********************************************************************
 * Open the FIFO.  Must be called from init_module, since we do
 * a kmalloc.
 **********************************************************************/
struct ioctlHandle* openIoctlFIFO(const char* devicePrefix,
	int boardNum,ioctlCallback_t* callback,
	int nioctls,struct ioctlCmd* ioctls)
{
    struct ioctlHandle* handle = 
	(struct ioctlHandle*) kmalloc(sizeof(struct ioctlHandle), GFP_KERNEL);

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

    /* in and out are from the user perspective */
    handle->inFifoName = makeDevName(devicePrefix,"_ictl_",boardNum);
    rtl_printf("creating %s\n",handle->inFifoName);

#ifdef OK_TO_EXIST
    if (mkfifo(handle->inFifoName, 0666) < 0 && rtl_errno != EEXIST) goto error;
#else
    if (mkfifo(handle->inFifoName, 0666)) goto error;
#endif

    if ((handle->inFifofd = open(handle->inFifoName, O_NONBLOCK | O_WRONLY)) < 0)
	goto error;

    handle->outFifoName = makeDevName(devicePrefix,"_octl_",boardNum);
    rtl_printf("creating %s\n",handle->outFifoName);
    if (mkfifo(handle->outFifoName, 0666) < 0) goto error;
    if ((handle->outFifofd = open(handle->outFifoName, O_NONBLOCK | O_RDONLY))
    	< 0) goto error;

    int icmd;
    for (icmd = 0; icmd < nioctls; icmd++)
    if (ioctls[icmd].size > handle->bufsize)
	handle->bufsize = ioctls[icmd].size;
    
    handle->buf = kmalloc(handle->bufsize, GFP_KERNEL);

    handle->bytesRead = handle->bytesToRead = 0;
    handle->icmd = -1;
    handle->readETX = 0;

    pthread_mutex_init(&handle->mutex,0);

    /* add this to the list before registering the signal handler.  */
    pthread_mutex_lock(&listmutex);
    list_add(&handle->list,&ioctlList);
    pthread_mutex_unlock(&listmutex);

    struct sigaction sigact;
    memset(&sigact,0,sizeof(sigact));
    sigact.sa_sigaction = ioctlHandler;
    sigact.sa_fd        = handle->outFifofd;
    rtl_sigemptyset(&sigact.sa_mask);
    sigact.sa_flags     = RTL_SA_RDONLY | RTL_SA_SIGINFO;
    if ( rtl_sigaction(RTL_SIGPOLL, &sigact, NULL ) != 0 ) {
	rtl_printf("Error in rtl_sigaction(RTL_SIGPOLL,...) for %s\n",
	    handle->outFifoName);
	goto error;
    }

    rtl_printf("openIoctlFIFO finished, handle=0x%x\n",(unsigned int)handle);

    return handle;
error:
    closeIoctlFIFO(handle);
    return 0;
}

/**********************************************************************
 * Close the FIFO. Done on module cleanup.
 **********************************************************************/
void closeIoctlFIFO(struct ioctlHandle* handle)
{
  pthread_mutex_lock(&listmutex);
  pthread_mutex_lock(&handle->mutex);

  if (handle->inFifofd >= 0) close(handle->inFifofd);
  if (handle->outFifofd >= 0) close(handle->outFifofd);

  if (handle->inFifoName) {
      unlink(handle->inFifoName);
      rtl_printf("removed %s\n", handle->inFifoName);
      kfree(handle->inFifoName);
  }
  if (handle->outFifoName) {
      unlink(handle->outFifoName);
      rtl_printf("removed %s\n", handle->outFifoName);
      kfree(handle->outFifoName);
  }

  kfree(handle->buf);

  list_del(&handle->list);

  pthread_mutex_unlock(&handle->mutex);

  kfree(handle);

  pthread_mutex_unlock(&listmutex);
}

/**
 * Send a error response back on the FIFO.
 * 4 byte non-zero errno int, 4 byte length, length message, ETX.
 */
static int sendError(int fifofd,int errval, const char* msg)
{
  rtl_printf("sendError, errval=%d,msg=%s\n",errval,msg);
  write(fifofd,&errval,sizeof(int));
  int len = strlen(msg) + 1;
  write(fifofd,&len,sizeof(int));
  write(fifofd,msg,len);
  write(fifofd,&ETX,1);
  return 0;
}

/**
 * Send a OK response back on the FIFO.
 * 4 byte int 0, 4 byte length, length byte buffer, ETX
 */
static int sendResponse(int fifofd,unsigned char* buf, int len)
{
  // rtl_printf("sendResponse, len=%d\n",len);
  int errval = 0;
  write(fifofd,&errval,sizeof(int));
  write(fifofd,&len,sizeof(int));
  if (len > 0) write(fifofd,buf,len);
  write(fifofd,&ETX,1);
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
 */
static void ioctlHandler(int sig, rtl_siginfo_t *siginfo, void *v)
{
  int outFifofd = siginfo->rtl_si_fd;
  struct list_head *ptr;
  struct ioctlHandle* entry;
  struct ioctlHandle* handle = NULL;
  unsigned char buf[MAX_IOCTL_BUF_SIZE];
  unsigned char* inbufptr;
  int icmd;
  int nread;
  int res;

#ifdef DEBUG
  rtl_printf("ioctlHandler entered, sig=%d, outFifofd=%d code=%d\n",
  	sig,outFifofd,siginfo->si_code);
#endif


  if (siginfo->si_code == RTL_POLL_OUT) {
#ifdef DEBUG
    rtl_printf("ignoring RTL_POLL_OUT\n");
#endif
    return;	// read at user end
  }

  pthread_mutex_lock(&listmutex);
  for (ptr = ioctlList.next; ptr != &ioctlList; ptr = ptr->next) {
    entry = list_entry(ptr,struct ioctlHandle, list);
    if (entry->outFifofd == outFifofd) {
      handle = entry;
      break;
    }
  }
  pthread_mutex_unlock(&listmutex);

#ifdef DEBUG
  rtl_printf("ioctlHandler looked for handle\n");
#endif

  if (handle == NULL) {
    rtl_printf("ioctlHandler can't find handle for FIFO file descriptor %d\n",
    	outFifofd);
    sendError(handle->inFifofd,EINVAL,"can't find handle for this FIFO");
    return;
  }

#ifdef DEBUG
  rtl_printf("ioctlHandler found handle = 0x%x\n",(unsigned int)handle);
#endif

  pthread_mutex_lock(&handle->mutex);

  if ((nread = read(outFifofd,buf,sizeof(buf))) < 0) {
    rtl_printf("ioctlHandler read failure on %s\n",handle->outFifoName);
    sendError(handle->inFifofd,rtl_errno,"error reading from FIFO");
    goto unlock;
  }
#ifdef DEBUG
  rtl_printf("ioctlHandler read %d bytes\n",nread);
#endif
  if (nread == 0) goto unlock;

  /*
   * The data coming from user space on the FIFO is formatted as
   * follows:
   *   4 byte int containing the command - one of the ioctl
   *       cmds supported by the driver.
   *   4 byte int containing the port index, 0-(N-1), for a board
   *       with N ports (i.e. a serial port card).
   *   Then, if the command is a set (sending data to the driver),
   *   the next bytes are the contents of the set, of the size
   *   specified by the command length.
   *   4 byte ETX ('\003' character).
   *  
   *   The ETX is there in case things get screwed up. After 
   *   reading the above from the FIFO we scan for an ETX
   *   to make sure we're in sync.
   */

  for (inbufptr = buf; inbufptr < buf + nread; ) {

#ifdef DEBUG
    rtl_printf("chars left in buffer=%d\n",buf+nread-inbufptr);
#endif

    if (handle->readETX) {
#ifdef DEBUG
	rtl_printf("search for ETX\n");
#endif
      for (; inbufptr < buf + nread; )  {
#ifdef DEBUG
	rtl_printf("ETX search char=%d, ETX=%d\n",(int) *inbufptr,(int)ETX);
#endif
        if (*inbufptr++ == ETX) {
	    handle->readETX = 0;
#ifdef DEBUG
	    rtl_printf("read ETX\n");
#endif
	    break;
	}
	else rtl_printf("didn't find ETX\n");
      }
      if (inbufptr == buf + nread) break;
    }

#ifdef DEBUG
    rtl_printf("ioctlHandler bytesRead= %d\n",handle->bytesRead);
#endif

    for ( ; handle->bytesRead < sizeof(struct ioctlHeader) &&
	  inbufptr < buf + nread; )
      ((unsigned char*)&(handle->header))[handle->bytesRead++] = *inbufptr++;

#ifdef DEBUG
    rtl_printf("ioctlHandler bytesRead2= %d\n",handle->bytesRead);
#endif

    if (handle->bytesRead < sizeof(struct ioctlHeader))  break;

    /* We now have read the cmd, port number and buffer size from the FIFO */

#ifdef DEBUG
    rtl_printf("ioctlHandler cmd= %d\n",handle->header.cmd);
#endif

    if ((icmd = handle->icmd) < 0) {
      /* find out which cmd it is. */
      int cmd = handle->header.cmd;
#ifdef DEBUG
      rtl_printf("ioctlHandler cmd= %d\n",cmd);
#endif

      for (icmd = 0; icmd < handle->nioctls; icmd++)
	if (handle->ioctls[icmd].cmd == cmd) break;

#ifdef DEBUG
      rtl_printf("ioctlHandler icmd=%d, cmd= %d \n",icmd,cmd);
#endif
     if (icmd == handle->nioctls) {
	rtl_printf("ioctlHandler cmd= %d not supported\n",cmd);
	sendError(handle->inFifofd,EINVAL,"cmd not supported");
	handle->bytesRead = 0;
	handle->readETX = 1;
	handle->icmd = -1;
	continue;
      }
      handle->icmd = icmd;

      if (handle->header.size > handle->bufsize) {
	  sendError(handle->inFifofd,EINVAL,"size too large");
	  rtl_printf("header size, %d, larger than bufsize %d\n",
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
      rtl_printf("ioctlHandler __IOC_WRITE\n");
#endif
      handle->bytesToRead = sizeof(struct ioctlHeader) +
      	handle->header.size;
#ifdef DEBUG
      rtl_printf("ioctlHandler _IOC_WRITE, bytesToRead=%d\n",
      	handle->bytesToRead);
#endif
      for (; handle->bytesRead < handle->bytesToRead && inbufptr < buf + nread; )
	handle->buf[handle->bytesRead++ - sizeof(struct ioctlHeader)] = *inbufptr++;

      if (handle->bytesRead < handle->bytesToRead) break;	/* not done */
    }

    /* call ioctl function on device */
#ifdef DEBUG
    rtl_printf("ioctlHandler calling ioctlCallback, port=%d\n",
        handle->header.port);
#endif
    res = handle->ioctlCallback(handle->header.cmd,
	  handle->boardNum,handle->header.port,handle->buf,
	  	handle->header.size);

    if (res < 0) sendError(handle->inFifofd,-res,"ioctl error");
    else if (_IOC_DIR(handle->header.cmd) & _IOC_READ) {
#ifdef DEBUG
	rtl_printf("ioctlHandler __IOC_READ\n");
#endif
	sendResponse(handle->inFifofd,handle->buf,res);
    }
    else sendResponse(handle->inFifofd,handle->buf,0);

    handle->bytesRead = 0;
    handle->readETX = 1;
    handle->icmd = -1;
  }
unlock:
  pthread_mutex_unlock(&handle->mutex);
#ifdef DEBUG
  rtl_printf("ioctlHandler returning\n");
#endif
  return;
}
