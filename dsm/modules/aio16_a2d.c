
/*  a2d_driver.c/


Time-stamp: <Wed 13-Apr-2005 05:57:57 pm>

Drivers and utility modules for AccesIO 104-AIO16-16 card.

Copyright 2005 UCAR, NCAR, All Rights Reserved

Original author:	Gordon Maclean

Revisions:

*/

/* RTLinux module includes...  */

#define __RTCORE_POLLUTED_APP__

#ifdef NEEDED
#include <rtl_stdio.h>
#include <rtl_stdlib.h>
#include <rtl_string.h>
#include <rtl_fcntl.h>
#include <rtl_posixio.h>
#include <rtl_unistd.h>
#include <sys/rtl_stat.h>


#endif

#include <gpos_bridge/sys/gpos.h>
#include <rtl.h>
#include <rtl_pthread.h>
#include <sys/rtl_ioctl.h>
#include <linux/ioport.h>

#include <irigclock.h>
#include <rtl_isa_irq.h>
#include <dsmlog.h>
#include <ioctl_fifo.h>
#include <aio16_a2d.h>
#include <dsm_version.h>

/* ioport addresses of installed boards, 0=no board installed */
static int ioport_parm[MAX_AIO16_BOARDS] = { 0x200, 0, 0, 0 };

static int irq_param[MAX_AIO16_BOARDS] = { 3, 0, 0, 0 };

/* number of AIO16 boards in system (number of non-zero ioport values) */
static int numboards = 0;

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_DESCRIPTION("AccesIO 104-AIO16 A/D driver for RTLinux");
RTLINUX_MODULE(aio16_a2d);

MODULE_PARM(ioport_parm, "1-" __MODULE_STRING(MAX_AIO16_BOARDS) "i");
MODULE_PARM_DESC(ioport_parm, "ISA port address of each board, e.g.: 0x200");

MODULE_PARM(irq_param, "1-" __MODULE_STRING(MAX_AIO16_BOARDS) "i");
MODULE_PARM_DESC(irq_param, "IRQ of each board");

static struct AIO16_Board* boardInfo = 0;

static const char* devprefix = "aioa2d";

/* number of devices on a board. This is the number of
 * /dev/aioa2d* devices, from the user's point of view, that one
 * board represents.
 */
#define N_AIO16_DEVICES 1

int  init_module(void);
void cleanup_module(void);

/****************  IOCTL Section *************************/

static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,_IOC_SIZE(GET_NUM_PORTS) },
  { AIO16_CONFIG,_IOC_SIZE(AIO16_CONFIG) },
  { AIO16_STATUS,_IOC_SIZE(AIO16_STATUS)  },
  { AIO16_START, _IOC_SIZE(AIO16_START)  },
  { AIO16_STOP,_IOC_SIZE(AIO16_STOP) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);

/****************  End of IOCTL Section ******************/

/**
 * Set amount of conversion delay between channel switches
 * @param val delay in nanoseconds (minimum 500)
 */
static void setTimerClock(struct AIO16_Board* brd,
	int clock, int mode, unsigned short val)
{
    unsigned char ctrl = 0;
    int caddr = 0;
    switch (clock) {
    case 0:
	ctrl = AIO16_8254_CNTR_0;
	caddr = AIO16_RW_COUNTER_0;
	break;
    case 1:
	ctrl = AIO16_8254_CNTR_1;
	caddr = AIO16_RW_COUNTER_1;
	break;
    case 2:
	ctrl = AIO16_8254_CNTR_2;
	caddr = AIO16_RW_COUNTER_2;
	break;
    }

    ctrl |= AIO16_8254_RW_LS_MS + (mode << 1);

    outb(ctrl,brd->addr + AIO16_W_COUNTER_CTRL);

    unsigned char lobyte = val & 0xff;
    outb(lobyte,brd->addr + caddr);
    unsigned char hibyte = val >> 8;
    outb(hibyte,brd->addr + caddr);

    DSMLOG_INFO("lobyte=%d,hibyte=%d\n",(int)lobyte,(int)hibyte);
}

/**
 * Set amount of conversion delay between channel switches
 * @param val delay in nanoseconds (minimum 500)
 */
static void setChanSwitchDelay(struct AIO16_Board* brd, unsigned int val)
{
    if (val < 500) val = 500;
    val /= 100;	// 10MHz input clock to this counter
    setTimerClock(brd,0,2,val);
}

static void* sampleThreadFunc(void *thread_arg)
{
    struct AIO16_Board* brd = (struct AIO16_Board*) thread_arg;
    int i;
    DSMLOG_DEBUG("AIO16 thread started\n");

    struct {
	dsm_sample_time_t timetag;    // timetag of sample
        dsm_sample_length_t length;       // number of bytes in data
        short data[NUM_AIO16_CHANNELS];
    } outsamp;

    int outChan = 0;
    register short *outptr = outsamp.data;
    int nover = brd->overSample;
    if (nover >= 0) nover = 16;
    int nChans = brd->highChan - brd->lowChan + 1;
    long scanDeltaT = MSECS_PER_SEC / brd->maxRate;
    DSMLOG_INFO("scanDeltaT=%d\n",scanDeltaT);
    int nloop;

    dsm_sample_time_t lasttt;

    for (nloop = 0;; nloop++) {
	if (!(nloop++ % 100)) DSMLOG_INFO("nloop=%d\n",nloop);

	if (rtl_sem_wait(&brd->fifoSem) < 0) {
	    DSMLOG_WARNING("sampleThreadFunc: %s\n",
		rtl_strerror(rtl_errno));
		return (void*) convert_rtl_errno(rtl_errno);
        }
	if (brd->interrupted) break;

        /*
         * Only scan one fifo dump sample here
         */
        if (brd->fifoSamples.head == brd->fifoSamples.tail) {
	    DSMLOG_WARNING("semaphore, but no sample\n");
	    continue;
	}

	struct dsm_sample* insamp =
	    brd->fifoSamples.buf[brd->fifoSamples.tail];

	dsm_sample_time_t tt = insamp->timetag;

#ifdef DEBUG
	DSMLOG_INFO("clock=%d, tt=%d, lasttt=%d, diff=%d\n",
		GET_MSEC_CLOCK,insamp->timetag,lasttt,
		insamp->timetag - lasttt);
	lasttt = tt;
#endif

	int nval = insamp->length / sizeof(short);
	int numchan = nval / nover;
	int numscan = numchan / nChans;

	long dt = numscan * scanDeltaT;
	if (tt < dt) tt += MSECS_PER_DAY;
	tt -= dt;

	register short *dp = (short *)insamp->data;
	register short *ep = dp + nval;

	for (; dp < ep; ) {
	    int sum = 0;
	    int chanNum = outChan + brd->lowChan;
	    if (brd->requested[chanNum]) {
		for (i = 0; i < nover; i++) sum += *dp++;
		*outptr++ = sum / nover;
	    }
	    else dp += nover;

#ifdef DEBUG
	    DSMLOG_INFO("outChan=%d, chanNum=%d, outptr-outsamp.data=%d\n",
	    	outChan,chanNum,outptr-outsamp.data);
#endif

	    outChan = (outChan + 1) % nChans;

	    if (outChan == 0) {
		outsamp.timetag = tt;
		outsamp.length = (outptr - outsamp.data) * sizeof(short);
		size_t slen = SIZEOF_DSM_SAMPLE_HEADER + outsamp.length;
	        if (brd->a2dfd >= 0) {	// output sample
		    // check if buffer full, or latency time has elapsed.
		    if (brd->head + slen > sizeof(brd->buffer) ||
			(outsamp.timetag - brd->lastWriteTT) > brd->latencyMsecs) {
			// Write to up-fifo
#ifdef DEBUG
			DSMLOG_INFO("doing write, head=%d,slen=%d,size=%d,tt=%d,wlen=%d\n",
			    brd->head,slen,sizeof(brd->buffer),outsamp.timetag,
			    brd->head-brd->tail);
#endif
			ssize_t wlen;
			if ((wlen = rtl_write(brd->a2dfd,
			    brd->buffer+brd->tail,brd->head - brd->tail)) < 0) {
			    int ierr = rtl_errno;       // save err
			    DSMLOG_ERR("error: write of %d bytes to %s: %s. Closing\n",
				brd->head-brd->tail,brd->a2dFifoName,
				rtl_strerror(rtl_errno));
			    rtl_close(brd->a2dfd);
			    brd->a2dfd = -1;
			    return (void*)convert_rtl_errno(ierr);
			}
			brd->lastWriteTT = outsamp.timetag;
			if (wlen != brd->head-brd->tail)
			    DSMLOG_WARNING(
			    	"warning: short write: request=%d, actual=%d\n",
				brd->head-brd->tail,wlen);
			brd->tail += wlen;
			if (brd->tail == brd->head) brd->head = brd->tail = 0;
		    }
		    if (brd->head + slen <= sizeof(brd->buffer)) {
			memcpy(brd->buffer+brd->head,&outsamp,slen);
			brd->head += slen;
		    }
		    else if (!(brd->skippedSamples++ % 100))
			DSMLOG_WARNING("warning: %d samples lost due to backlog in %s\n",
			    brd->skippedSamples,brd->a2dFifoName);
		}
		outptr = outsamp.data;
		tt += scanDeltaT;
		if (tt >= MSECS_PER_DAY) tt -= MSECS_PER_DAY;
	    }
	}

	unsigned long flags;
	rtl_spin_lock_irqsave(&brd->queuelock,flags);
	INCREMENT_TAIL(brd->fifoSamples,AIO16_FIFO_QUEUE_SIZE);
	rtl_spin_unlock_irqrestore(&brd->queuelock,flags);
    }
    return 0;
}

/*
 * Configure board.
 */
static int configA2D(struct AIO16_Board* brd,struct AIO16_Config* cfg)
{
    int i;

    // disable A/D
    outb(AIO16_DISABLE_A2D, brd->addr + AIO16_W_OVERSAMPLE);

    /* read jumper settings */
    brd->status.reg = inb(brd->addr + AIO16_R_CONFIG_STATUS);
    int jmprHighGain = (brd->status.reg & AIO16_STATUS_GNH) != 0;
    int jmprBipolar = (brd->status.reg & AIO16_STATUS_BIPOLAR) != 0;
    int jmprSingleEnded = brd->status.reg & AIO16_STATUS_16SE;

    if (jmprSingleEnded)
	DSMLOG_WARNING(
		"%s board set for single-ended operation, not differential\n",
		brd->a2dFifoName);

    int gain = 0;
    int bipolar = 0;
    brd->lowChan = 0;
    brd->highChan = 0;

    brd->maxRate = 0;

    for (i = 0; i < NUM_AIO16_CHANNELS; i++) {
	brd->requested[i] = 0;
        if (cfg->rate[i] == 0) continue;

	// non-zero rate means the channel has been requested
	brd->requested[i] = 1;
	if (brd->maxRate == 0) {
	    brd->lowChan = i;
	    gain = cfg->gain[i];
	    if (gain <= 0) return -EINVAL;
	    bipolar = cfg->bipolar[i];
	}
	if (cfg->rate[i] > brd->maxRate) brd->maxRate = cfg->rate[i];
	brd->highChan = i;

	// gains must all be the same and positive.
	if (cfg->gain[i] != gain) return -EINVAL;

	// Must have same polarity.
	if (cfg->bipolar[i] != bipolar) return -EINVAL;
    }

    if (brd->maxRate == 0) {
	DSMLOG_ERR("%s: no channels requested, all rates==0\n",
	    brd->a2dFifoName);
	return -EINVAL;
    }
    if ((USECS_PER_SEC * 10) % brd->maxRate) {
	DSMLOG_ERR("%s: max sampling rate=%d is not a factor of 10 MHz\n",
	    brd->a2dFifoName,brd->maxRate);
	return -EINVAL;
    }

    if (bipolar ^ jmprBipolar) {
	DSMLOG_ERR("%s: requested polarity (%s) does not match jumper settings on board\n",
	    brd->a2dFifoName,(bipolar ? "bipolar" : "unipolar"));
	return -EINVAL;
    }

    /*
     * bipolar, gain=1 means user wants range of +-10 V.
     *		gain=2 is +-5 V, etc.
     * unipolar, gain=1 means user wants range of 0-20 V
     *		gain=2 is 0-10 V, etc.
     * These are the gain values provided on the AIO16, for given
     * jumper settings.
     */
    int gains[][4] = {
        { 0,2,5,10 },	// avail gains when jumpered for low gain, unipolar
        { 1,2,5,10 },	// avail gains when jumpered for low gain, bipolar
        { 2,4,10,20 },	// avail gains when jumpered for high gain, unipolar
        { 2,4,10,20 },	// avail gains when jumpered for high gain, bipolar
    };

    // allowed gains in string form
    const char* gainStrings[] = {"2,5,10","1,2,5,10","2,4,10,20","2,4,10,20"};

    // index into above gains array
    int gainIndex = jmprHighGain * 2 + jmprBipolar;

    for (brd->gainSetting = 0; brd->gainSetting < 4; brd->gainSetting++) {
        if (gain == gains[gainIndex][brd->gainSetting]) break;
    }
    if (brd->gainSetting == 4)  {
	DSMLOG_ERR(
	    "%s: illegal gain=%d for jumpers=%s,%s, allowed gains=%s\n",
		brd->a2dFifoName,(jmprHighGain ? "GNH" : "GNL"),
		(jmprBipolar ? "bipolar" : "unipolar"),
		gainStrings[gainIndex]);
	return -EINVAL;
    }

    /*
     * According to page 14 of 104-AIO16-16 Reference Manual,
     * in TRIGGERED mode, you must scan at least two channels.
     * If user has requested only one, we'll scan two and
     * ignore the data in the unrequested channel.
     */
    if (brd->highChan == brd->lowChan) {
	if (brd->lowChan == 0) brd->highChan = brd->lowChan + 1;
	else brd->lowChan--;
    }

    brd->latencyMsecs = cfg->latencyUsecs / USECS_PER_MSEC;
    if (brd->latencyMsecs == 0) brd->latencyMsecs = 500;

	DSMLOG_DEBUG("latencyUsecs=%d, latencyMsecs=%d\n",
		 cfg->latencyUsecs,brd->latencyMsecs);
#ifdef DEBUG
#endif

    return 0;
}

/**
 */
static int stopA2D(struct AIO16_Board* brd) 
{
    int ret = 0;

    outb(0,brd->addr + AIO16_W_OVERSAMPLE);	// disable A2D
    outb(0,brd->addr + AIO16_W_EXT_TRIG_SEL);	// turn off trigger
    outb(AIO16_DISABLE_IRQ,brd->addr + AIO16_W_ENABLE_IRQ);

    if (brd->sampleThread) {
	// interrupt the sample thread
	brd->interrupted = 1;
	rtl_sem_post(&brd->fifoSem);
	rtl_pthread_join(brd->sampleThread, NULL);
	brd->sampleThread = 0;
    }

    if (brd->a2dfd >= 0) {
	int fdtmp = brd->a2dfd;
	brd->a2dfd = -1;
	rtl_close(fdtmp);
    }

    outb(0,brd->addr + AIO16_W_FIFO_RESET);	// clear FIFO

    brd->busy = 0;	// Reset the busy flag

    return ret;
}

static int startA2D(struct AIO16_Board* brd)
{
    int ret;
    unsigned long flags;

    if ((ret = stopA2D(brd)) != 0) return ret;

    brd->busy = 1;	// Set the busy flag
    brd->overSample = 16;	// Set the busy flag

    rtl_sem_init(&brd->fifoSem,1,0);

    rtl_spin_lock_irqsave(&brd->queuelock,flags);
    brd->fifoSamples.head = brd->fifoSamples.tail = 0;
    rtl_spin_unlock_irqrestore(&brd->queuelock,flags);

    // buffer indices
    brd->head = 0;
    brd->tail = 0;

    if((brd->a2dfd = rtl_open(brd->a2dFifoName,
	    RTL_O_NONBLOCK | RTL_O_WRONLY)) < 0)
    {
	DSMLOG_ERR("error: opening %s: %s\n",
		brd->a2dFifoName,rtl_strerror(rtl_errno));
	return -convert_rtl_errno(rtl_errno);
    }
#ifdef DO_FTRUNCATE
    if (rtl_ftruncate(brd->a2dfd, sizeof(brd->buffer)*4) < 0) {
	DSMLOG_ERR("error: ftruncate %s: size=%d: %s\n",
		brd->a2dFifoName,sizeof(brd->buffer),
		rtl_strerror(rtl_errno));
	return -convert_rtl_errno(rtl_errno);
    }
#endif

    brd->interrupted = 0;
    // Start sample thread
    rtl_pthread_attr_t attr;
    rtl_pthread_attr_init(&attr);
    rtl_pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
    rtl_pthread_attr_setstackaddr(&attr,brd->sampleThreadStack);
    if (rtl_pthread_create(&brd->sampleThread, &attr, sampleThreadFunc, brd)) {
	    DSMLOG_ERR("Error starting sample thread: %s\n",
		    rtl_strerror(rtl_errno));
	    return -convert_rtl_errno(rtl_errno);
    }
    rtl_pthread_attr_destroy(&attr);

    unsigned char chanNibbles = (brd->highChan << 4) + brd->lowChan;
    DSMLOG_INFO("highChan=%d,lowChan=%d,nib=0x%x\n",
    	brd->highChan,brd->lowChan,(int)chanNibbles);
    outb(chanNibbles, brd->addr + AIO16_W_CHANNELS);

    outb(brd->gainSetting, brd->addr + AIO16_W_SW_GAIN);

    outb(AIO16_TIMED, brd->addr + AIO16_W_AD_COUNTER_MD);

    // 500nsec delay when switching channels
    setChanSwitchDelay(brd,500);

    unsigned long ticks = USECS_PER_SEC * 10 / brd->maxRate;
    DSMLOG_INFO("clock ticks=%d,maxRate=%d\n",ticks,brd->maxRate);
    unsigned short c1 = 1;

    while (ticks > 65535) {
        if (!(ticks % 2)) {
	    c1 *= 2;
	    ticks /= 2;
	}
	else if (!(ticks % 5)) {
	    c1 *= 5;
	    ticks /= 5;
	}
    }
    DSMLOG_INFO("clock ticks=%d,%d\n",
	c1,ticks);
    setTimerClock(brd,1,2,c1);
    setTimerClock(brd,2,2,ticks);

    // outb(1,brd->addr + AIO16_W_EXT_TRIG_SEL);
    outb(0,brd->addr + AIO16_W_EXT_TRIG_SEL);

    outb(AIO16_ENABLE_CTR0+AIO16_ENABLE_CTR12,
    	brd->addr + AIO16_W_ENABLE_AD_CNT);
    // outb(AIO16_ENABLE_CTR0,brd->addr + AIO16_W_ENABLE_AD_CNT);

    unsigned char overval;
    switch (brd->overSample) {
    case 16:
	overval = AIO16_OVERSAMPLE_X16;
	break;
    case 8:
	overval = AIO16_OVERSAMPLE_X8;
	break;
    case 2:
	overval = AIO16_OVERSAMPLE_X2;
	break;
    case 1:
	overval = AIO16_OVERSAMPLE_X1;
	break;
    }

    outb(AIO16_ENABLE_IRQ,brd->addr + AIO16_W_ENABLE_IRQ);

    brd->status.reg = inb(brd->addr + AIO16_R_CONFIG_STATUS);
    DSMLOG_INFO("status=0x%x\n",(int)brd->status.reg);

    outb(AIO16_OVERSAMPLE_X16, brd->addr + AIO16_W_OVERSAMPLE);

    return ret;
}

/*
 * Function that is called on receipt of an ioctl request over the
 * ioctl FIFO.
 * Return: negative Linux errno (not RTLinux errnos), or 0=OK
 */
static int ioctlCallback(int cmd, int board, int port,
	void *buf, rtl_size_t len) 
{
	// return LINUX errnos here, not RTL_XXX errnos.
  	int ret = -EINVAL;

	// paranoid check if initialized (probably not necessary)
	if (!boardInfo) return ret;

	struct AIO16_Board* brd = boardInfo + board;

#ifdef DEBUG
  	DSMLOG_DEBUG("ioctlCallback cmd=%x board=%d port=%d len=%d\n",
	    cmd,board,port,len);
#endif

  	switch (cmd) 
	{
  	case GET_NUM_PORTS:		/* user get */
		if (len != sizeof(int)) break;
		DSMLOG_DEBUG("GET_NUM_PORTS\n");
		*(int *) buf = N_AIO16_DEVICES;	
		ret = sizeof(int);
  		break;

  	case AIO16_STATUS:		/* user get of status */
		if (port != 0) break;	// only port 0
		if (len != sizeof(struct AIO16_Status)) break;
		memcpy(buf,&brd->status,len);
		ret = len;
		break;

  	case AIO16_CONFIG:		/* user set */
		DSMLOG_DEBUG("AIO16_CONFIG\n");
		if (port != 0) break;	// only port 0
		if (len != sizeof(struct AIO16_Config)) break;	// invalid length
		if(brd->busy) {
			DSMLOG_ERR("A2D's running. Can't configure\n");
			ret = -EBUSY;
			break;
		}
		ret = configA2D(brd,(struct AIO16_Config*)buf);
		DSMLOG_DEBUG("AIO16_CONFIG done, ret=%d\n", ret);
   		break;

  	case AIO16_START:

		if (port != 0) break;	// port 0 is the A2D, port 1 is I2C temp
		// clean up acquisition thread if it was left around

		DSMLOG_DEBUG("AIO16_START\n");
		ret = startA2D(brd);
		DSMLOG_DEBUG("AIO16_START finished\n");
		break;

  	case AIO16_STOP:
		if (port != 0) break;	// port 0 is the A2D, port 1 is I2C temp
		DSMLOG_DEBUG("AIO16_STOP_IOCTL\n");
		ret = stopA2D(brd);
		DSMLOG_DEBUG("closeA2D, ret=%d\n",ret);
		break;
	default:
		break;
  	}
  	return ret;
}

unsigned int aio16_irq_handler(unsigned int irq,
        void* callbackptr, struct rtl_frame *regs)
{
    static int spurious = 0;
    static int count = 0;
    static int missed = 0;


    struct AIO16_Board* brd = (struct AIO16_Board*) callbackptr;

    brd->status.reg = inb(brd->addr + AIO16_R_CONFIG_STATUS);
    if (!(brd->status.reg & AIO16_STATUS_FIFO_HALF_FULL)) {
        if (!(spurious++ % 100)) DSMLOG_NOTICE("%d spurious interrupts\n",
		spurious);
	outb(0,brd->addr + AIO16_W_FIFO_RESET);	// reset FIFO
	return 0;
    }

    if (!(count++ % 1000)) DSMLOG_NOTICE("%d interrupts\n",count);

    int i;
    struct dsm_sample* samp =
        GET_HEAD(brd->fifoSamples,AIO16_FIFO_QUEUE_SIZE);
    if (!samp) {                // no output sample available
        brd->status.missedSamples += AIO16_HALF_FIFO_SIZE / sizeof(short);
	if (!(missed++ % 100))
	    DSMLOG_WARNING("%d missed samples\n",missed);
	for (i = 0; i < AIO16_HALF_FIFO_SIZE; i++) {
	    brd->junk += inw(brd->addr + AIO16_R_FIFO);
	}
        return 0;
    }

    samp->timetag = GET_MSEC_CLOCK;
    register short* dp = (short *)samp->data;

    for (i = 0; i < AIO16_HALF_FIFO_SIZE; i++)
	*dp++ = inw(brd->addr + AIO16_R_FIFO);
    samp->length = AIO16_HALF_FIFO_SIZE * sizeof(short);

    /* increment head, this sample is ready for consumption */
    INCREMENT_HEAD(brd->fifoSamples,AIO16_FIFO_QUEUE_SIZE);
    rtl_sem_post(&brd->fifoSem);

    return 0;
}


/*-----------------------Module------------------------------*/

int init_module()
{	
    int error = -EINVAL;
    int ib;
    int i;

    boardInfo = 0;

    // softwareVersion is found in dsm_version.h
    DSMLOG_NOTICE("version: %s\n",softwareVersion);

    /* count non-zero ioport addresses, gives us the number of boards */
    for (ib = 0; ib < MAX_AIO16_BOARDS; ib++)
	if (ioport_parm[ib] == 0) break;
    numboards = ib;
    if (numboards == 0) {
	DSMLOG_ERR("No boards configured, all ioport_parm[]==0\n");
	goto err;
    }

    error = -ENOMEM;
    boardInfo = rtl_gpos_malloc( numboards * sizeof(struct AIO16_Board) );
    if (!boardInfo) goto err;

    /* initialize each AIO16_Board structure */
    for (ib = 0; ib < numboards; ib++) {
	struct AIO16_Board* brd = boardInfo + ib;

	// initialize structure to zero, then initialize things
	// that are non-zero
	memset(brd,0,sizeof(struct AIO16_Board));
	brd->a2dfd = -1;
    }

    for (ib = 0; ib < numboards; ib++) {
	struct AIO16_Board* brd = boardInfo + ib;

	error = -EBUSY;
	unsigned int addr =  ioport_parm[ib] + SYSTEM_ISA_IOPORT_BASE;
	// Get the mapped board address
	if (check_region(addr, AIO16_IOPORT_WIDTH)) {
	    DSMLOG_ERR("ioport at 0x%x already in use\n", addr);
	    goto err;
	}

	request_region(addr, AIO16_IOPORT_WIDTH, "aio16_a2d");
	brd->addr = addr;

	outb(AIO16_DISABLE_IRQ,brd->addr + AIO16_W_ENABLE_IRQ);
	outb(AIO16_DISABLE_A2D, brd->addr + AIO16_W_OVERSAMPLE);
	outb(0,brd->addr + AIO16_W_EXT_TRIG_SEL);	// turn off trigger
	outb(0,brd->addr + AIO16_W_FIFO_RESET);	// clear FIFO

	/* Open up my ioctl FIFOs, register my ioctlCallback function */
	error = -EIO;
	brd->ioctlhandle =
		    openIoctlFIFO(devprefix,ib,ioctlCallback,
					nioctlcmds,ioctlcmds);

	if (!brd->ioctlhandle) goto err;

	error = -ENOMEM;

	/* allocate thread stack at init module time */
	if (!(brd->sampleThreadStack =
	    rtl_gpos_malloc(THREAD_STACK_SIZE))) goto err;

	rtl_sem_init(&brd->fifoSem,1,0);

	// Open the RTL fifo to user space
	brd->a2dFifoName = makeDevName(devprefix,"_in_",ib*N_AIO16_DEVICES);
	if (!brd->a2dFifoName) goto err;

	// remove broken device file before making a new one
	if ((rtl_unlink(brd->a2dFifoName) < 0 && rtl_errno != RTL_ENOENT)
	    || rtl_mkfifo(brd->a2dFifoName, 0666) < 0) {
	    DSMLOG_ERR("error: unlink/mkfifo %s: %s\n",
		    brd->a2dFifoName,rtl_strerror(rtl_errno));
	    error = -convert_rtl_errno(rtl_errno);
	    goto err;
	}

	brd->fifoSamples.buf = rtl_gpos_malloc(AIO16_FIFO_QUEUE_SIZE *
	    sizeof(void*) );
	if (!brd->fifoSamples.buf) goto err;
	memset(brd->fifoSamples.buf,0,
	    AIO16_FIFO_QUEUE_SIZE * sizeof(void*));
                
	for (i = 0; i < AIO16_FIFO_QUEUE_SIZE; i++) {
	    struct dsm_sample* samp = (struct dsm_sample*)
	        rtl_gpos_malloc(SIZEOF_DSM_SAMPLE_HEADER +
	      	AIO16_HALF_FIFO_SIZE * sizeof(short));
	    if (!samp) goto err;
	    memset(samp,0,SIZEOF_DSM_SAMPLE_HEADER +
	      	AIO16_HALF_FIFO_SIZE * sizeof(short));
	    brd->fifoSamples.buf[i] = samp;
	}

	brd->gainSetting = 1;	// default value

	brd->overSample = 16;
	error = -EBUSY;
	if ((error = rtl_request_isa_irq(irq_param[ib],aio16_irq_handler,
			  brd)) < 0) goto err;
	brd->irq =  irq_param[ib];

    }

    DSMLOG_DEBUG("AIO16 init_module complete.\n");

    return 0;
err:

    if (boardInfo) {
	for (ib = 0; ib < numboards; ib++) {
	    struct AIO16_Board* brd = boardInfo + ib;

	    if (brd->irq) rtl_free_isa_irq(brd->irq);
	    brd->irq = 0;

	    if (brd->sampleThreadStack) {
		rtl_gpos_free(brd->sampleThreadStack);
		brd->sampleThreadStack = 0;
		rtl_sem_destroy(&brd->fifoSem);
	    }

	    if (brd->a2dFifoName) {
		rtl_unlink(brd->a2dFifoName);
		rtl_gpos_free(brd->a2dFifoName);
	    }

	    if (brd->fifoSamples.buf) {
		for (i = 0; i < AIO16_FIFO_QUEUE_SIZE; i++)
		    if (brd->fifoSamples.buf[i])
		    	rtl_gpos_free(brd->fifoSamples.buf[i]);
		rtl_gpos_free(brd->fifoSamples.buf);
		brd->fifoSamples.buf = 0;
	    }

	    if (brd->ioctlhandle) closeIoctlFIFO(brd->ioctlhandle);
	    brd->ioctlhandle = 0;

	    if (brd->addr)
		release_region(brd->addr, AIO16_IOPORT_WIDTH);
	    brd->addr = 0;

	}

    }

    rtl_gpos_free(boardInfo);
    boardInfo = 0;
    return error;
}

/*-----------------------Module------------------------------*/
// Stops the A/D and releases reserved memory
void cleanup_module(void)
{
	int ib;
	int i;
	if (!boardInfo) return;

	for (ib = 0; ib < numboards; ib++) {
	    struct AIO16_Board* brd = boardInfo + ib;

	    if (brd->irq) rtl_free_isa_irq(brd->irq);
	    brd->irq = 0;

	    // Shut down the sample thread
	    if (brd->sampleThread) {
		rtl_pthread_cancel(brd->sampleThread);
		rtl_pthread_join(brd->sampleThread, NULL);
		brd->sampleThread = 0;
	    }

	    if (brd->sampleThreadStack) {
		rtl_gpos_free(brd->sampleThreadStack);
		brd->sampleThreadStack = 0;
		rtl_sem_destroy(&brd->fifoSem);
	    }

	    // close and remove RTL fifo
	    if (brd->a2dfd >= 0) rtl_close(brd->a2dfd);
	    if (brd->a2dFifoName) {
		rtl_unlink(brd->a2dFifoName);
		rtl_gpos_free(brd->a2dFifoName);
	    }

	    if (brd->ioctlhandle) closeIoctlFIFO(brd->ioctlhandle);
	    brd->ioctlhandle = 0;

	    if (brd->fifoSamples.buf) {
		for (i = 0; i < AIO16_FIFO_QUEUE_SIZE; i++)
		    if (brd->fifoSamples.buf[i])
		    	rtl_gpos_free(brd->fifoSamples.buf[i]);
	        rtl_gpos_free(brd->fifoSamples.buf);
	    }

	    if (brd->addr)
		release_region(brd->addr, AIO16_IOPORT_WIDTH);
	    brd->addr = 0;

	}

        rtl_gpos_free(boardInfo);
        boardInfo = 0;

  	DSMLOG_DEBUG("Analog cleanup complete\n");

	return;
}

