
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

#define USE_AIO16_TIMED_MODE

/* ioport addresses of installed boards, 0=no board installed */
static int ioports[MAX_AIO16_BOARDS] = { 0x200, 0, 0, 0 };

/* irqs. 0=poll instead of using interrupts */
static int irqs[MAX_AIO16_BOARDS] = { 0, 0, 0, 0 };

/* number of AIO16 boards in system (number of non-zero ioport values) */
static int numboards = 0;

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_DESCRIPTION("AccesIO 104-AIO16 A/D driver for RTLinux");
RTLINUX_MODULE(aio16_a2d);

MODULE_PARM(ioports, "1-" __MODULE_STRING(MAX_AIO16_BOARDS) "i");
MODULE_PARM_DESC(ioports, "ISA port address of each board, e.g.: 0x200");

MODULE_PARM(irqs, "1-" __MODULE_STRING(MAX_AIO16_BOARDS) "i");
MODULE_PARM_DESC(irqs, "IRQ of each board, 0=poll");

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

    DSMLOG_INFO("ctrl=0x%x, lobyte=%d,hibyte=%d\n",
    	(int)ctrl,(int)lobyte,(int)hibyte);
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

/**
 * Set the value for a digital potentiometer on the AIO16.
 * pot=0 is the A2D offset.
 * pot=1 is the A2D gain.
 * pot=2 is the DAC0 gain.
 * pot=3 is the DAC1 gain.
 * value is a 8 bit value, determined by trial and error
 * to give the calibration result one desires.
 */
static void setPotValue(struct AIO16_Board* brd,unsigned char pot,
	unsigned char val)
{
    int i;
    unsigned char wval;

    wval = ((pot & 0x2) << 6) + 0x01;
    outb(wval,brd->addr + AIO16_W_CAL_DATA);
    wval = ((pot & 0x1) << 7) + 0x01;
    outb(wval,brd->addr + AIO16_W_CAL_DATA);

    for (i = 0; i < 8; i++) {
        wval = (0x80 & val) + 0x01;
	outb(wval,brd->addr + AIO16_W_CAL_DATA);
	val <<= 1;
    }
    outb(0x01,brd->addr + AIO16_W_CAL_DATA);
}
static void setA2DGainPot(struct AIO16_Board* brd,unsigned char val)
{
    setPotValue(brd,1,val);
}
static void setA2DOffsetPot(struct AIO16_Board* brd,unsigned char val)
{
    setPotValue(brd,0,val);
}
static void setDAC0GainPot(struct AIO16_Board* brd,unsigned char val)
{
    setPotValue(brd,2,val);
}
static void setDAC1GainPot(struct AIO16_Board* brd,unsigned char val)
{
    setPotValue(brd,3,val);
}

static int readEEPROM(struct AIO16_Board* brd,int addr)
{
    int i;
    unsigned char wval;
    outb(0x81,brd->addr + AIO16_RW_EEPROM);
    outb(0x81,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    // send the 6 bit address
    for (i = 0; i < 6; i++) {
        wval = ((addr & 0x20) << 2) + 0x1;
	outb(wval,brd->addr + AIO16_RW_EEPROM);
	addr <<= 1;
    }
    int val = 0;
    for (i = 0; i < 16; i++) {
	val <<= 1;
        wval = inb(brd->addr + AIO16_RW_EEPROM);
	val |= (wval >> 7);
    }
    outb(0,brd->addr + AIO16_RW_EEPROM);
    return val;
}

static void enableEEPROMWrite(struct AIO16_Board* brd)
{
    outb(0x81,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x81,brd->addr + AIO16_RW_EEPROM);
    outb(0x81,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x00,brd->addr + AIO16_RW_EEPROM);
}

static void disableEEPROMWrite(struct AIO16_Board* brd)
{
    outb(0x81,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x00,brd->addr + AIO16_RW_EEPROM);
}
static void writeEEPROM(struct AIO16_Board* brd,int addr,unsigned int val)
{
    int i;
    unsigned char wval;
    outb(0x81,brd->addr + AIO16_RW_EEPROM);
    outb(0x01,brd->addr + AIO16_RW_EEPROM);
    outb(0x81,brd->addr + AIO16_RW_EEPROM);
    // send the 6 bit address
    for (i = 0; i < 6; i++) {
        wval = ((addr & 0x20) << 2) + 0x1;
	outb(wval,brd->addr + AIO16_RW_EEPROM);
	addr <<= 1;
    }
    for (i = 0; i < 16; i++) {
	wval = ((val & 0xf000) >> 8) + 0x01;
	outb(wval,brd->addr + AIO16_RW_EEPROM);
	val <<= 1;
    }
    outb(0,brd->addr + AIO16_RW_EEPROM);
}

static void initCalsFromEEPROM(struct AIO16_Board* brd)
{
    /*
     * First three bits of STATUS are jumper settings:
     * 0: low gain, unipolar, diff (not used)
      *1: low gain, unipolar, single-ended (not used)
     * 2: low gain,  bipolar, diff
     * 3: low gain,  bipolar, single-ended
     * 4:  hi gain, unipolar, diff
     * 5:  hi gain, unipolar, single-ended
     * 6:  hi gain,  bipolar, diff
     * 7:  hi gain,  bipolar, single-ended
     */
    unsigned char status = inb(brd->addr + AIO16_R_CONFIG_STATUS);
    unsigned char adjmprs = status & 0x7;
    unsigned char daa5 = (status & 0x1) >> 4;
    unsigned char dab5 = (status & 0x1) >> 4;

    // read offset value from eeprom, transfer to CAL_DATA register
    int val = readEEPROM(brd,adjmprs);
    setA2DOffsetPot(brd,val);
    DSMLOG_DEBUG("adjmprs=0x%x, offset=%d\n",adjmprs,val);

    // read gain value from eeprom, transfer to CAL_DATA register
    val = readEEPROM(brd,adjmprs+8);
    setA2DGainPot(brd,val);
    DSMLOG_DEBUG("adjmprs=0x%x, gain=%d\n",adjmprs,val);

    // read d2a chan A gain from eeprom, transfer to CAL_DATA
    val = readEEPROM(brd,daa5 + 0x10);
    setDAC0GainPot(brd,val);
    DSMLOG_DEBUG("daa5=0x%x, gain=%d\n", daa5,val);

    // read d2a chan A gain from eeprom, transfer to CAL_DATA
    val = readEEPROM(brd,dab5 + 0x12);
    setDAC1GainPot(brd,val);
    DSMLOG_DEBUG("dab5=0x%x, gain=%d\n", daa5,val);
}

/**
 * pthread function that waits on fifoSem, then
 * reads a fifo sample (containing 1024 a2d samples)
 * from the fifoSamples list, assembles the data into
 * dsm_samples which contain data from one scan over the channels,
 * and buffers this sample on the RTL fifo to user space.
 */
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

    int nChans = brd->highChan - brd->lowChan + 1;
    long scanDeltaT = MSECS_PER_SEC / brd->maxRate;
    DSMLOG_INFO("scanDeltaT=%d\n",scanDeltaT);

#ifdef DEBUG
    dsm_sample_time_t lasttt;
#endif

    for (;; ) {

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
#ifdef DEBUG
	DSMLOG_DEBUG("ThreadFunc, time=%d, status=0x%x, head=%d,tail=%d\n",
	    GET_MSEC_CLOCK,(unsigned int)brd->status.reg,
	    brd->fifoSamples.head,brd->fifoSamples.tail);
#endif

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
	DSMLOG_INFO("nval=%d,numchan=%d,numscan=%d\n",
		nval,numchan,numscan);

	long dt = numscan * scanDeltaT;
	if (tt < dt) tt += MSECS_PER_DAY;
	tt -= dt;

	register unsigned short *dp = (unsigned short *)insamp->data;
	register unsigned short *ep = dp + nval;

	for (; dp < ep; ) {
	    int chanNum = outChan + brd->lowChan;
	    if (brd->requested[chanNum]) {
		unsigned long sum = 0;
		for (i = 0; i < nover; i++) sum += *dp++;
		*outptr++ = sum / nover;
#ifdef DEBUG
		if (chanNum == 0)
			DSMLOG_INFO("sum=%d,nover=%d,res=%d\n",
			sum,nover,sum/nover);
#endif
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

#ifdef USE_AIO16_TIMED_MODE

unsigned int aio16_irq_handler(unsigned int irq,
        void* callbackptr, struct rtl_frame *regs)
{
    static int spurious = 0;
    static int count = 0;
    static int missed = 0;

    if (!(count++ % 1000)) DSMLOG_NOTICE("%d interrupts\n",count);

    struct AIO16_Board* brd = (struct AIO16_Board*) callbackptr;

    brd->status.reg = inb(brd->addr + AIO16_R_CONFIG_STATUS);
    if (!(brd->status.reg & AIO16_STATUS_FIFO_HALF_FULL)) {
        if (!(spurious++ % 100)) DSMLOG_NOTICE("%d spurious interrupts\n",
		spurious);
	return 0;
    }

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
    register unsigned short* dp = (unsigned short *)samp->data;

    insw(brd->addr+AIO16_R_FIFO,dp,AIO16_HALF_FIFO_SIZE);
/*
    for (i = 0; i < AIO16_HALF_FIFO_SIZE; i++)
	*dp++ = inw(brd->addr + AIO16_R_FIFO);
*/

    outb(AIO16_ENABLE_IRQ,brd->addr + AIO16_W_ENABLE_IRQ);

    samp->length = AIO16_HALF_FIFO_SIZE * sizeof(short);

    /* increment head, this sample is ready for consumption */
    INCREMENT_HEAD(brd->fifoSamples,AIO16_FIFO_QUEUE_SIZE);
    rtl_sem_post(&brd->fifoSem);

    return 0;
}

/**
 * Poll function, with AIO16 in TIMED mode.
 * This function downloads the AIO fifo contents, as
 * a alternative to using interrupts
 */
static void timedPollAIOFifoCallback(void *ptr)
{
    static int missed = 0;

    struct AIO16_Board* brd = (struct AIO16_Board*)ptr;
    brd->status.reg = inb(brd->addr + AIO16_R_CONFIG_STATUS);
    if (!(brd->status.reg & AIO16_STATUS_FIFO_HALF_FULL)) return;

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
	return;
    }

    samp->timetag = GET_MSEC_CLOCK;
    register unsigned short* dp = (unsigned short *)samp->data;

    insw(brd->addr+AIO16_R_FIFO,dp,AIO16_HALF_FIFO_SIZE);
/*
    for (i = 0; i < AIO16_HALF_FIFO_SIZE; i++)
	*dp++ = inw(brd->addr + AIO16_R_FIFO);
*/

    unsigned char status = inb(brd->addr + AIO16_R_CONFIG_STATUS);
    dp = (unsigned short *)samp->data;
    DSMLOG_INFO("\n%7d%7d%7d%7d 0x%x\n%7d%7d%7d%7d\n",
	    *dp,*(dp+1),*(dp+2),*(dp+3),status,
	    *(dp+4),*(dp+5),*(dp+6),*(dp+7));

#ifdef DEBUG
    for (i = 0; i < AIO16_HALF_FIFO_SIZE/8; i++) {
        dp = (unsigned short *)samp->data;
	DSMLOG_INFO("%7d%7d%7d%7d%7d%7d%7d%7d\n",
	    *dp,*(dp+1),*(dp+2),*(dp+3),
	    *(dp+4),*(dp+5),*(dp+6),*(dp+7));
	dp += 8;
    }
    DSMLOG_INFO("--------------------------------\n");
#endif
    samp->length = AIO16_HALF_FIFO_SIZE * sizeof(short);

    /* increment head, this sample is ready for consumption */
    INCREMENT_HEAD(brd->fifoSamples,AIO16_FIFO_QUEUE_SIZE);
    rtl_sem_post(&brd->fifoSem);

#ifdef DEBUG
    brd->status.reg = inb(brd->addr + AIO16_R_CONFIG_STATUS);
    DSMLOG_DEBUG("time=%d, status=0x%x, head=%d,tail=%d\n",
    	GET_MSEC_CLOCK,(unsigned int)brd->status.reg,
	brd->fifoSamples.head,brd->fifoSamples.tail);
#endif
}

#else

/**
 * Poll function, a simple in-efficient function for polling the AIO
 * in software mode.
 */
static void softwarePollAIOFifoCallback(void *ptr)
{

    static int missed = 0;
    struct AIO16_Board* brd = (struct AIO16_Board*)ptr;

    int i;

    int nChans = brd->highChan - brd->lowChan + 1;

    struct dsm_sample* samp =
        GET_HEAD(brd->fifoSamples,AIO16_FIFO_QUEUE_SIZE);
    if (!samp) {                // no output sample available
        brd->status.missedSamples += nChans;
	if (!(missed++ % 100))
	    DSMLOG_WARNING("%d missed samples\n",missed);
	return;
    }

    samp->timetag = GET_MSEC_CLOCK;
    register unsigned short* dp = (unsigned short *)samp->data;

    const struct rtl_timespec convSleep = { 0, 100000 };

    for (i = 0; i < nChans; i++) {
	if (brd->interrupted) break;
        outb(0,brd->addr + AIO16_W_START_CONF);

	while (inb(brd->addr + AIO16_R_CONFIG_STATUS) &
		AIO16_STATUS_FIFO_EMPTY) {
	    if (brd->interrupted) return;
	    rtl_nanosleep(&convSleep,0);
	}
	*dp++ = inw(brd->addr + AIO16_R_FIFO);
    }
    samp->length = nChans * sizeof(short);

    /* increment head, this sample is ready for consumption */
    INCREMENT_HEAD(brd->fifoSamples,AIO16_FIFO_QUEUE_SIZE);
    rtl_sem_post(&brd->fifoSem);

#ifdef DEBUG
    brd->status.reg = inb(brd->addr + AIO16_R_CONFIG_STATUS);
    DSMLOG_DEBUG("time=%d, status=0x%x, head=%d,tail=%d\n",
    	GET_MSEC_CLOCK,(unsigned int)brd->status.reg,
	brd->fifoSamples.head,brd->fifoSamples.tail);
#endif
}

#endif
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
		brd->a2dFifoName,gain,
		(jmprHighGain ? "GNH" : "GNL"),
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

#ifdef DEBUG
	DSMLOG_DEBUG("latencyUsecs=%d, latencyMsecs=%d\n",
		 cfg->latencyUsecs,brd->latencyMsecs);
#endif

#ifdef USE_AIO16_TIMED_MODE
    // brd->overSample = 8;
    brd->overSample = 1;
#else
    brd->overSample = 1;
#endif

    return 0;
}

/**
 */
static int stopA2D(struct AIO16_Board* brd) 
{
    int ret = 0;

    DSMLOG_DEBUG("stopA2D entered\n");
#ifdef USE_AIO16_TIMED_MODE
    if (!brd->irq)
    	unregister_irig_callback(&timedPollAIOFifoCallback,IRIG_100_HZ,brd);
#else
    unregister_irig_callback(&softwarePollAIOFifoCallback,IRIG_100_HZ,brd);
#endif
    DSMLOG_DEBUG("unregister_irig_callback done\n");

    // rtl_usleep(20000);

    outb(AIO16_DISABLE_A2D,brd->addr + AIO16_W_OVERSAMPLE);	// disable A2D
    outb(0,brd->addr + AIO16_W_BURST_MODE);	// disable burst mode
    outb(0,brd->addr + AIO16_W_ENABLE_AD_CNT);
    outb(0,brd->addr + AIO16_W_EXT_TRIG_SEL);	// turn off trigger
    outb(AIO16_DISABLE_IRQ,brd->addr + AIO16_W_ENABLE_IRQ);
    outb(0,brd->addr + AIO16_W_FIFO_RESET);	// clear FIFO


    if (brd->sampleThread) {
	void* threadStatus;
	// interrupt the sample thread
	DSMLOG_DEBUG("interrupting\n");
	brd->interrupted = 1;
	rtl_sem_post(&brd->fifoSem);
	DSMLOG_DEBUG("pthread_join\n");
	if (rtl_pthread_join(brd->sampleThread, &threadStatus))
	    DSMLOG_ERR("error: pthread_join %s: %s\n",
		    brd->a2dFifoName,rtl_strerror(rtl_errno));
	DSMLOG_DEBUG("sampleThread joined, status=%d\n",
		 (int)threadStatus);
	brd->sampleThread = 0;
    }

    if (brd->a2dfd >= 0) {
	int fdtmp = brd->a2dfd;
	brd->a2dfd = -1;
	rtl_close(fdtmp);
    }

    brd->busy = 0;	// Reset the busy flag

    return ret;
}

static int startA2D(struct AIO16_Board* brd)
{
    int ret;
    unsigned long flags;

    if ((ret = stopA2D(brd)) != 0) return ret;

    brd->busy = 1;	// Set the busy flag

    rtl_sem_init(&brd->fifoSem,1,0);

    rtl_spin_lock_irqsave(&brd->queuelock,flags);
    brd->fifoSamples.head = brd->fifoSamples.tail = 0;
    rtl_spin_unlock_irqrestore(&brd->queuelock,flags);

    // buffer indices
    brd->head = brd->tail = 0;

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

    // must set overSample value before starting sampleThread
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

    DSMLOG_INFO("gainSetting=%d\n",brd->gainSetting);
    outb(brd->gainSetting, brd->addr + AIO16_W_SW_GAIN);

#ifdef USE_AIO16_TIMED_MODE
    outb(AIO16_TIMED, brd->addr + AIO16_W_AD_COUNTER_MD);
    outb(AIO16_DISABLE_A2D,brd->addr + AIO16_W_OVERSAMPLE);	// disable A2D
    outb(0,brd->addr + AIO16_W_BURST_MODE);	// disable burst mode

    unsigned char val = AIO16_ENABLE_CTR0 + AIO16_ENABLE_CTR12;
    outb(val,brd->addr + AIO16_W_ENABLE_AD_CNT);

    // 500nsec delay when switching channels
    setChanSwitchDelay(brd,500);
    // setChanSwitchDelay(brd,5000);

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
    default:
	DSMLOG_INFO("%s: oversampling=%d not supported\n",
	    brd->a2dFifoName,brd->overSample);
        return -EINVAL;
    }

    if (brd->irq)
	outb(AIO16_ENABLE_IRQ,brd->addr + AIO16_W_ENABLE_IRQ);
    else
	register_irig_callback(&timedPollAIOFifoCallback,IRIG_100_HZ,brd);

    brd->status.reg = inb(brd->addr + AIO16_R_CONFIG_STATUS);
    DSMLOG_INFO("status=0x%x, overval=0x%x\n",
    	(int)brd->status.reg,overval);

    outb(overval, brd->addr + AIO16_W_OVERSAMPLE);
#else
    register_irig_callback(&softwarePollAIOFifoCallback,IRIG_100_HZ,brd);
#endif

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
	    if (port != 0) break;	// port 0 is the A2D
	    DSMLOG_DEBUG("AIO16_START\n");
	    ret = startA2D(brd);
	    DSMLOG_DEBUG("AIO16_START finished\n");
	    break;

    case AIO16_STOP:
	    if (port != 0) break;	// port 0 is the A2D
	    DSMLOG_DEBUG("AIO16_STOP\n");
	    ret = stopA2D(brd);
	    DSMLOG_DEBUG("AIO16_STOP finished, ret=%d\n",ret);
	    break;
    default:
	    break;
    }
    return ret;
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
	if (ioports[ib] == 0) break;
    numboards = ib;
    if (numboards == 0) {
	DSMLOG_ERR("No boards configured, all ioports[]==0\n");
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
	unsigned int addr =  ioports[ib] + SYSTEM_ISA_IOPORT_BASE;
	// Get the mapped board address
	if (check_region(addr, AIO16_IOPORT_WIDTH)) {
	    DSMLOG_ERR("ioport at 0x%x already in use\n", addr);
	    goto err;
	}

	request_region(addr, AIO16_IOPORT_WIDTH, "aio16_a2d");
	brd->addr = addr;

	outb(AIO16_DISABLE_A2D, brd->addr + AIO16_W_OVERSAMPLE);
	outb(AIO16_DISABLE_IRQ,brd->addr + AIO16_W_ENABLE_IRQ);
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
	    DSMLOG_INFO("mallocing %d bytes\n",
	        SIZEOF_DSM_SAMPLE_HEADER +
		AIO16_HALF_FIFO_SIZE * sizeof(short));
	    struct dsm_sample* samp = (struct dsm_sample*)
	        rtl_gpos_malloc(SIZEOF_DSM_SAMPLE_HEADER +
	      	AIO16_HALF_FIFO_SIZE * sizeof(short));
	    if (!samp) goto err;
	    memset(samp,0,SIZEOF_DSM_SAMPLE_HEADER +
	      	AIO16_HALF_FIFO_SIZE * sizeof(short));
	    brd->fifoSamples.buf[i] = samp;
	}

	brd->gainSetting = 1;	// default value

	error = -EBUSY;

#ifdef USE_AIO16_TIMED_MODE
	if (irqs[ib] > 0 &&
	    (error = rtl_request_isa_irq(irqs[ib],aio16_irq_handler,
			  brd)) < 0) goto err;
	brd->irq = irqs[ib];
#else
	brd->irq = 0;
#endif

	initCalsFromEEPROM(brd);

    }

    DSMLOG_DEBUG("complete.\n");

    return 0;
err:

    if (boardInfo) {
	for (ib = 0; ib < numboards; ib++) {
	    struct AIO16_Board* brd = boardInfo + ib;

#ifdef USE_AIO16_TIMED_MODE
	    if (brd->irq) rtl_free_isa_irq(brd->irq);
	    brd->irq = 0;
#endif

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

#ifdef USE_AIO16_TIMED_MODE
	if (brd->irq) rtl_free_isa_irq(brd->irq);
	// doesn't hurt to unregister if isn't registered.
	else unregister_irig_callback(&timedPollAIOFifoCallback,IRIG_100_HZ,brd);
#else
	unregister_irig_callback(&softwarePollAIOFifoCallback,IRIG_100_HZ,brd);
#endif

	// Shut down the sample thread
	if (brd->sampleThread) {
	    rtl_pthread_cancel(brd->sampleThread);
	    rtl_pthread_join(brd->sampleThread, NULL);
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

	if (brd->fifoSamples.buf) {
	    for (i = 0; i < AIO16_FIFO_QUEUE_SIZE; i++)
		if (brd->fifoSamples.buf[i])
		    rtl_gpos_free(brd->fifoSamples.buf[i]);
	    rtl_gpos_free(brd->fifoSamples.buf);
	}

	if (brd->addr)
	    release_region(brd->addr, AIO16_IOPORT_WIDTH);
    }

    rtl_gpos_free(boardInfo);
    boardInfo = 0;

    DSMLOG_DEBUG("complete\n");

    return;
}

