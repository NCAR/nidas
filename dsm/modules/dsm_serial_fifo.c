/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

   FIFO interface between dsm_serial serial port driver and the
   user side.

   Since user space code cannot read directly from RT-Linux devices
   one has to put a read/write pair of FIFOs between user space
   and hardware.

   And, since an RT-Linux interrupt function cannot write directly
   to a FIFO, we need a pair of threads to broker the data
   back and forth.

   This module provides the FIFOs and the threads.

   This module also supports an IOCTL FIFO that the user can
   send commands over.  Upon receipt of an ioctl command
   this module does things like opening/closing fifos and
   the serial port device, and also can send ioctl commands
   on to the RT-Linux device.

*/

#define __RTCORE_POLLUTED_APP__
#include <gpos_bridge/sys/gpos.h>
#include <rtl_pthread.h>
#include <rtl_unistd.h>
#include <rtl_string.h>
#include <rtl_stdlib.h>
#include <rtl_fcntl.h>
#include <sys/rtl_ioctl.h>

#include <dsm_serial_fifo.h>
#include <dsm_serial.h>
#include <ioctl_fifo.h>

RTLINUX_MODULE(dsm_serial_fifo);

#define THREAD_STACK_SIZE 16384

static const char* devprefix = 0;

/* how many serial cards is the dsm_serial module driving.
 */
static int numboards = 0;

static struct dsm_serial_fifo_board *boardInfo = 0;

static struct dsm_serial_fifo_port* fifo_to_port_map[64];


/****************  IOCTL Section ***************8*********/

static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,_IOC_SIZE(GET_NUM_PORTS) },
  { DSMSER_OPEN, _IOC_SIZE(DSMSER_OPEN) },
  { DSMSER_CLOSE, _IOC_SIZE(DSMSER_CLOSE) },
  { DSMSER_TCSETS,SIZEOF_TERMIOS },
  { DSMSER_TCGETS,SIZEOF_TERMIOS },
  { DSMSER_WEEPROM, _IOC_SIZE(DSMSER_WEEPROM) },
  { DSMSER_SET_PROMPT, _IOC_SIZE(DSMSER_SET_PROMPT) },
  { DSMSER_GET_PROMPT, _IOC_SIZE(DSMSER_GET_PROMPT)},
  { DSMSER_START_PROMPTER, _IOC_SIZE(DSMSER_START_PROMPTER) },
  { DSMSER_STOP_PROMPTER, _IOC_SIZE(DSMSER_STOP_PROMPTER) },
  { DSMSER_SET_RECORD_SEP, _IOC_SIZE(DSMSER_SET_RECORD_SEP) },
  { DSMSER_GET_RECORD_SEP, _IOC_SIZE(DSMSER_GET_RECORD_SEP)},
  { DSMSER_GET_STATUS, _IOC_SIZE(DSMSER_GET_STATUS)},
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);

/****************  End of IOCTL Section ******************/

static void thread_dev_close(void* arg)
{
    int fd = (int)arg;
    rtl_printf("dev_close: fd=%d\n",fd);
    rtl_close(fd);
}

/*
 * Thread which reads from the dsm_serial device
 * and writes to the fifo, i.e. sending data to the user side.
 * The read file operation in dsm_serial waits on a semaphore,
 * so these are blocking reads from port->devfd.
 *
 * Return: positive errno cast to void*. These should be
 *	Linux errnos, but right now we're not doing
 *	anything with them.
 */
static void* in_thread_func(void* arg)
{
#ifdef DEBUG
    rtl_printf("running in_thread_func\n");
#endif
    struct dsm_serial_fifo_port* port =
    	(struct dsm_serial_fifo_port*) arg;

    char buf[1024];
    int l;
    char* cp;
    char* eob;

    rtl_pthread_cleanup_push(thread_dev_close,(void*)port->devfd);

    for (;;) {
	/*
	 * this read will wait on a semaphore within dsm_serial
	 * until data is ready
	 */
        if ((l = rtl_read(port->devfd,buf,sizeof(buf))) < 0) {
	    rtl_printf("%s error: reading %s: %s\n",
		    __FILE__,port->devname,rtl_strerror(rtl_errno));
	    if (rtl_errno == RTL_EINTR) break;
	    return (void*)convert_rtl_errno(rtl_errno);
	}
#ifdef DEBUG
	rtl_printf("in_thread_func: l=%d\n",l);
#endif
	eob = buf + l;
	for (cp = buf; cp < eob; cp += l) {
	    if ((l = rtl_write(port->inFifoFd,cp,eob-cp)) < 0) {
		rtl_printf(
		"%s error: writing %d bytes to %s: %s\n",
			__FILE__,(int)(eob-cp),port->inFifoName,rtl_strerror(rtl_errno));
		if (rtl_errno == RTL_EINTR) break;
		return (void*)convert_rtl_errno(rtl_errno);
	    }
	}
    }
    rtl_pthread_cleanup_pop(1);
    return 0;
}

/*
 * SIG_POLL handler to read output from the user side
 * and write to the dsm_serial device.
 * Because the FIFO must be accessed in a non-blocking mode,
 * we use a SIG_POLL handler here rather than a thread.
 */
static void outFifoHandler(int sig, rtl_siginfo_t *siginfo, void *v)
{
    int outFifoFd = siginfo->si_fd;
    struct dsm_serial_fifo_port* port =
    	fifo_to_port_map[outFifoFd];

    char buf[1024];
    int l;
    char* cp;
    char* eob;

    if ((l = rtl_read(port->outFifoFd,buf,sizeof(buf))) < 0) {
	rtl_printf("%s error: reading %s: %s\n",
		__FILE__,port->outFifoName,rtl_strerror(rtl_errno));
	return;
    }
    eob = buf + l;
    for (cp = buf; cp < eob; cp += l) {
	if ((l = rtl_write(port->devfd,cp,eob-cp)) < 0) {
	    rtl_printf("%s error: writing %s: %s\n",
		    __FILE__,port->devname,rtl_strerror(rtl_errno));
	    return;
	}
    }
}

/*
 * Return: negative Linux errno.
 */
static int close_port(struct dsm_serial_fifo_port* port)
{
    if (port->in_thread) {

	rtl_printf("rtl_pthread_kill SIGTERM of in_thread for %s\n",port->inFifoName);
        if (rtl_pthread_kill(port->in_thread,SIGTERM) < 0) goto error;

	// rtl_printf("rtl_pthread_cancel of in_thread for %s\n",port->inFifoName);
        // if (rtl_pthread_cancel(port->in_thread) < 0) goto error;

	rtl_printf("rtl_pthread_join of in_thread for %s\n",port->inFifoName);
        if (rtl_pthread_join(port->in_thread,NULL) < 0) goto error;
	rtl_printf("rtl_pthread_joined in_thread for %s\n",port->inFifoName);
	port->in_thread = 0;
    }

    if (port->out_thread) {
        if (rtl_pthread_cancel(port->out_thread) < 0) goto error;
	rtl_printf("rtl_pthread_join of out_thread\n");
        if (rtl_pthread_join(port->out_thread,NULL) < 0) goto error;
	rtl_printf("rtl_pthread_joined out_thread\n");
	port->out_thread = 0;
    }

    if (port->devfd >= 0) {
	rtl_printf("closing %s, fd=%d\n",port->devname,port->devfd);
	// rtl_ioctl(port->devfd, DSMSER_STOP_PROMPTER,0);
        rtl_close(port->devfd);
	port->devfd = -1;
    }

    if (port->inFifoFd >= 0) {
	rtl_printf("closing %s\n",port->inFifoName);
        rtl_close(port->inFifoFd);
	port->inFifoFd = -1;
    }

    if (port->outFifoFd >= 0) {
	rtl_printf("closing %s\n",port->outFifoName);
        rtl_close(port->outFifoFd);
	port->outFifoFd = -1;
    }

    return 0;
error:
    return -convert_rtl_errno(rtl_errno);
}
/*
 * Return: negative Linux errno.
 */
static int open_port(struct dsm_serial_fifo_port* port,int mode)
{
    rtl_pthread_attr_t attr;
    int retval;

    if ((retval = close_port(port))) return retval;

    rtl_pthread_attr_init(&attr);
    rtl_pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);

    /* in and out are from the user perspective */

    /* user opens device for read. */
    if (mode == RTL_O_RDONLY || mode == RTL_O_RDWR) {
	rtl_printf("opening %s\n",port->inFifoName);
	if ((port->inFifoFd = rtl_open(port->inFifoName, RTL_O_NONBLOCK | RTL_O_WRONLY)) <
		0) goto error;

	rtl_printf("opening %s\n",port->devname);
	if ((port->devfd = rtl_open(port->devname,mode)) < 0) goto error;
	rtl_printf("opened %s, fd=%d\n",port->devname,port->devfd);

	rtl_pthread_attr_setstackaddr(&attr,port->in_thread_stack);

	rtl_printf("open_port: rtl_pthread_create in_thread\n");
	if (rtl_pthread_create(&port->in_thread, &attr,
	    in_thread_func, port) != 0) {
	    rtl_pthread_attr_destroy(&attr);
	    goto error;
	}
    }

    /* user opens device for writing. */
    if (mode == RTL_O_WRONLY || mode == RTL_O_RDWR) {
	rtl_printf("opening %s\n",port->outFifoName);
	if ((port->outFifoFd = rtl_open(port->outFifoName, RTL_O_NONBLOCK | RTL_O_RDONLY))
	    < 0) goto error;

	if (port->devfd < 0) {
	    rtl_printf("opening %s\n",port->devname);
	    if ((port->devfd = rtl_open(port->devname,mode)) < 0) goto error;
	}

	if (port->outFifoFd >
		sizeof(fifo_to_port_map) / sizeof(fifo_to_port_map[0])) {
	    rtl_printf("outFifoFd=%d, max is=%d\n",
		port->outFifoFd,
		sizeof(fifo_to_port_map) / sizeof(fifo_to_port_map[0]));
	    return -EINVAL;
	}

	fifo_to_port_map[port->outFifoFd] = port;

	struct rtl_sigaction sigact;
	rtl_memset(&sigact,0,sizeof(sigact));
	sigact.sa_sigaction = outFifoHandler;
	sigact.sa_fd        = port->outFifoFd;
	rtl_sigemptyset(&sigact.sa_mask);
	sigact.sa_flags     = RTL_SA_RDONLY | RTL_SA_SIGINFO;
	if ( rtl_sigaction(RTL_SIGPOLL, &sigact, NULL ) != 0 ) {
	    rtl_printf("Error in rtl_sigaction(RTL_SIGPOLL,...) for %s\n",
		port->outFifoName);
	    goto error;
	}
    }
    rtl_pthread_attr_destroy(&attr);

    return 0;
error:
    return -convert_rtl_errno(rtl_errno);
}

/*
 * Return: negative Linux errno.
 */
static int create_fifos(struct dsm_serial_fifo_port* port,int mode)
{
    /* in and out are from the user perspective */

    /* user opens device for read. */
    if (mode == RTL_O_RDONLY || mode == RTL_O_RDWR) {
	rtl_printf("creating %s\n",port->inFifoName);

        // remove broken device file before making a new one
        if ((rtl_unlink(port->inFifoName) < 0 && rtl_errno != RTL_ENOENT) ||
	    rtl_mkfifo(port->inFifoName, 0666) < 0) {
	    rtl_printf("%s error: unlink/mkfifo %s: %s\n",
	    	__FILE__,port->inFifoName,rtl_strerror(rtl_errno));
	    return -convert_rtl_errno(rtl_errno);
	}
    }

    if (mode == RTL_O_WRONLY || mode == RTL_O_RDWR) {
	rtl_printf("creating %s\n",port->outFifoName);
        // remove broken device file before making a new one
        if ((rtl_unlink(port->outFifoName) < 0 && rtl_errno != RTL_ENOENT) ||
	    rtl_mkfifo(port->outFifoName, 0666) < 0) {
	    rtl_printf("%s error: unlink/mkfifo %s: %s\n",
	    	__FILE__,port->outFifoName,rtl_strerror(rtl_errno));
	    return -convert_rtl_errno(rtl_errno);
	}
    }
    return 0;
}

/*
 * initialize a struct dsm_serial_fifo_port
 */
static void init_port_struct(struct dsm_serial_fifo_port* port)
{
    port->inFifoName = port->outFifoName = port->devname = 0;
    port->inFifoFd = port->outFifoFd = port->devfd = -1;
    port->in_thread_stack = port->out_thread_stack = 0;
    port->in_thread = port->out_thread = 0;
}

/*
 * Function that is called on receipt of ioctl request over the
 * ioctl FIFO.
 * Return: negative Linux errno (not RTLinux errnos), or 0=OK
 */
static int ioctlCallback(int cmd, int board, int portNum,
	void *buf, rtl_size_t len)
{
    int retval = -EINVAL;
    struct dsm_serial_fifo_port* port;
#ifdef DEBUG
    rtl_printf("ioctlCallback, cmd=%d, board=%d, portNum=%d,len=%d\n",
		cmd,board,portNum,len);
#endif

    if (board < 0 || board >= numboards) return retval;

    // check whether this is valid later
    port = boardInfo[board].ports + portNum;

    switch (cmd) {
    case GET_NUM_PORTS:		/* user get */
#ifdef DEBUG
	rtl_printf("%s: GET_NUM_PORTS\n",devprefix);
#endif
	*(int *) buf = boardInfo[board].numports;
	retval = sizeof(int);
	break;
    case DSMSER_OPEN:		/* open port */
#ifdef DEBUG
	rtl_printf("DSMSER_OPEN\n");
#endif
	if (portNum < 0 || portNum >= boardInfo[board].numports) break;
	int mode = *(int*) buf;
        // RTLinux uses a different bitmask for these values...
        if      (mode == O_RDWR)   mode = RTL_O_RDWR;
        else if (mode == O_RDONLY) mode = RTL_O_RDONLY;
        else if (mode == O_WRONLY) mode = RTL_O_WRONLY;
        else break;
	retval = open_port(port,mode);
	break;
    case DSMSER_CLOSE:		/* close port */
#ifdef DEBUG
	rtl_printf("DSMSER_CLOSE\n");
#endif
	if (portNum < 0 || portNum >= boardInfo[board].numports) return retval;
	retval = close_port(port);
	break;
    case DSMSER_TCSETS:		/* user set of termios parameters */
    case DSMSER_TCGETS:		/* user get of termios parameters */
    case DSMSER_WEEPROM:	/* write config to eeprom */
    case DSMSER_SET_PROMPT:	/* set the prompt for this port */
    case DSMSER_GET_PROMPT:	/* get the prompt for this port */
    case DSMSER_START_PROMPTER:	/* start the prompter for this port */
    case DSMSER_STOP_PROMPTER:	/* stop the prompter for this port */
    case DSMSER_SET_RECORD_SEP:	/* set the prompt for this port */
    case DSMSER_GET_RECORD_SEP:	/* get the prompt for this port */
    case DSMSER_GET_STATUS:	/* get the status for this port */
	if (portNum < 0 || portNum >= boardInfo[board].numports) return retval;
	retval = -EBADF;
        /* check if the port is open, send the ioctl */
	if (port->devfd < 0) break;
	if (rtl_ioctl(port->devfd,cmd,buf) < 0) {
	    retval = -convert_rtl_errno(rtl_errno);
	    break;
	}
	retval = len;
	break;
    default:
	rtl_printf("%s: unknown ioctl cmd\n",devprefix);
	break;
    }
    return retval;
}

/*
 * Return: negative Linux errno.
 */
int init_module(void)
{
    const char* dsm_ser_devname;
    int retval = -EINVAL;
    int ib,ip;

    numboards = dsm_serial_get_numboards();

    if (numboards == 0) {
	rtl_printf("No boards configured\n");
	goto err0;
    }

    devprefix = dsm_serial_get_devprefix();

    retval = -ENOMEM;
    boardInfo = rtl_gpos_malloc( numboards * sizeof(struct dsm_serial_fifo_board) );
    if (!boardInfo) goto err0;
    for (ib = 0; ib < numboards; ib++) {
	boardInfo[ib].numports = 0;
	boardInfo[ib].ioctlhandle = 0;
    }

    int portcounter = 0;

    for (ib = 0; ib < numboards; ib++) {
        boardInfo[ib].numports = dsm_serial_get_numports(ib);

	retval = -ENOMEM;
	boardInfo[ib].ports = rtl_gpos_malloc(
	    boardInfo[ib].numports * sizeof(struct dsm_serial_fifo_port) );
	if (!boardInfo[ib].ports) goto err1;

	for (ip = 0; ip < boardInfo[ib].numports; ip++) {
	    struct dsm_serial_fifo_port* port = boardInfo[ib].ports + ip;
	    init_port_struct(port);
	}

	for (ip = 0; ip < boardInfo[ib].numports; ip++) {
	    struct dsm_serial_fifo_port* port = boardInfo[ib].ports + ip;

	    port->inFifoName = makeDevName(devprefix,"_in_",portcounter);
	    if (!port->inFifoName) goto err1;

	    port->outFifoName = makeDevName(devprefix,"_out_",portcounter);
	    if (!port->outFifoName) goto err1;

	    retval = -EINVAL;
	    dsm_ser_devname = dsm_serial_get_devname(portcounter);
	    if (!dsm_ser_devname) goto err1;

	    retval = -ENOMEM;
	    if (!(port->devname = rtl_gpos_malloc( strlen(dsm_ser_devname) + 1 )))
              goto err1;
	    strcpy(port->devname,dsm_ser_devname);

	    /* allocate thread stacks at init module time */
	    if (!(port->in_thread_stack = rtl_gpos_malloc(THREAD_STACK_SIZE)))
	    	goto err1;

	    if (!(port->out_thread_stack = rtl_gpos_malloc(THREAD_STACK_SIZE)))
	    	goto err1;

	    if ((retval = create_fifos(port,RTL_O_RDWR))) goto err1;

	    portcounter++;
	}

	if (!(boardInfo[ib].ioctlhandle = openIoctlFIFO(devprefix,
		ib,ioctlCallback,nioctlcmds,ioctlcmds))) {
	    retval = -EINVAL;
	    goto err1;
	}
    }

    return 0;

err1:
    if (boardInfo) {
	for (ib = 0; ib < numboards; ib++) {

	    if (boardInfo[ib].ports) {
		for (ip = 0; ip < boardInfo[ib].numports; ip++) {
		    struct dsm_serial_fifo_port* port = boardInfo[ib].ports + ip;
		    if (port->inFifoName) rtl_gpos_free(port->inFifoName);
		    port->inFifoName = 0;
		    if (port->outFifoName) rtl_gpos_free(port->outFifoName);
		    port->outFifoName = 0;

		    if (port->devname) rtl_gpos_free(port->devname);
		    port->devname = 0;

		    if (port->in_thread_stack)
		    	rtl_gpos_free(port->in_thread_stack);
		    port->in_thread_stack = 0;

		    if (port->out_thread_stack)
		    	rtl_gpos_free(port->out_thread_stack);
		    port->out_thread_stack = 0;
		}
		rtl_gpos_free(boardInfo[ib].ports);
		boardInfo[ib].ports = 0;
	    }
	    if (boardInfo[ib].ioctlhandle)
		    closeIoctlFIFO(boardInfo[ib].ioctlhandle);
	    boardInfo[ib].ioctlhandle = 0;
	}

	rtl_gpos_free(boardInfo);
	boardInfo = 0;
    }
err0:
    return retval;
}

/* -- MODULE ---------------------------------------------------------- */
void cleanup_module (void)
{
    rtl_printf("cleanup module: %s\n",devprefix);
    int ib, ip;
    for (ib = 0; ib < numboards; ib++) {

	if (boardInfo[ib].ports) {
	    for (ip = 0; ip < boardInfo[ib].numports; ip++) {
		struct dsm_serial_fifo_port* port = boardInfo[ib].ports + ip;

		close_port(port);

		if (port->inFifoName) {
		    rtl_unlink(port->inFifoName);
		    rtl_gpos_free(port->inFifoName);
		    port->inFifoName = 0;
		}
		if (port->outFifoName) {
		    rtl_unlink(port->outFifoName);
		    rtl_gpos_free(port->outFifoName);
		    port->outFifoName = 0;
		}
		if (port->devname) {
		    rtl_gpos_free(port->devname);
		    port->devname = 0;
		}
		if (port->in_thread_stack) rtl_gpos_free(port->in_thread_stack);
		port->in_thread_stack = 0;

		if (port->out_thread_stack) rtl_gpos_free(port->out_thread_stack);
		port->out_thread_stack = 0;
	    }
	    rtl_gpos_free(boardInfo[ib].ports);
	    boardInfo[ib].ports = 0;
	}
	if (boardInfo[ib].ioctlhandle)
		closeIoctlFIFO(boardInfo[ib].ioctlhandle);
	boardInfo[ib].ioctlhandle = 0;
    }

    rtl_gpos_free(boardInfo);
    boardInfo = 0;
}
