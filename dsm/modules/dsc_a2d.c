
/*  a2d_driver.c/

Driver and utility modules for Diamond System A2D cards.

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
#include <dsc_a2d.h>
#include <dsm_version.h>

/* ioport addresses of installed boards, 0=no board installed */
static int ioports[MAX_DSC_BOARDS] = { 0x330, 0, 0, 0 };

/* irqs, required for each board */
static int irqs[MAX_DSC_BOARDS] = { 6, 0, 0, 0 };

/* board types: 0=MM16AT, 1=MM32XAT 
 * See #defines for DSC_XXXXX_BOARD)
 * Doesn't seem to be an easy way to auto-detect the board type,
 * but it's probably do-able.
 */
static int types[MAX_DSC_BOARDS] = { DSC_MM16AT_BOARD, 0, 0, 0 };

/* number of DSC boards in system (number of non-zero ioport values) */
static int numboards = 0;

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_DESCRIPTION("RTLinux A/D driver for Diamond Systems cards");
RTLINUX_MODULE(dsc_a2d);

MODULE_PARM(ioports, "1-" __MODULE_STRING(MAX_DSC_BOARDS) "i");
MODULE_PARM_DESC(ioports, "ISA port address of each board, e.g.: 0x200");

MODULE_PARM(irqs, "1-" __MODULE_STRING(MAX_DSC_BOARDS) "i");
MODULE_PARM_DESC(irqs, "IRQ of each board, 0=poll");

MODULE_PARM(types, "1-" __MODULE_STRING(MAX_DSC_BOARDS) "i");
MODULE_PARM_DESC(types, "Type of each board: 0=MM16AT, 1=MM32XAT");

static struct DSC_Board* boardInfo = 0;

static const char* devprefix = "dsc_a2d";

/* number of devices on a board. This is the number of
 * /dev/dsc_a2d* devices, from the user's point of view, that one
 * board represents.
 */
#define N_DSC_DEVICES 1

int  init_module(void);
void cleanup_module(void);

/****************  IOCTL Section *************************/

static struct ioctlCmd ioctlcmds[] = {
  { GET_NUM_PORTS,_IOC_SIZE(GET_NUM_PORTS) },
  { DSC_CONFIG,sizeof(struct DSC_Config) },
  { DSC_STATUS,_IOC_SIZE(DSC_STATUS)  },
  { DSC_START, _IOC_SIZE(DSC_START)  },
  { DSC_STOP,_IOC_SIZE(DSC_STOP) },
  { DSC_GET_NCHAN,_IOC_SIZE(DSC_GET_NCHAN) },
};

static int nioctlcmds = sizeof(ioctlcmds) / sizeof(struct ioctlCmd);

/****************  End of IOCTL Section ******************/

/**
 * Set counter value in a 82C54 clock.
 * Works on both MM16AT and MM32XAT, assuming both have set
 * page bit(s) to 0:
 *	MM16AT: bit 6 in base+10 is the page bit
 *	MM32XAT: bits 0-2 in base+8 are page bits
 */
static void setTimerClock(struct DSC_Board* brd,
	int clock, int mode, unsigned short val)
{
    /*
     * 82C54 control word:
     * bits 7,6: 0=select ctr 0, 1=ctr 1, 2=ctr 2, 3=read-back cmd
     * bits 5,4: 0=ctr latch, 1=r/w lsbyte only, 2=r/w msbyte, 3=lsbtye,msbyte
     * bits 3,2,1: mode, 2=rate generator
     * bit 0: 0=binary 16 bit counter, 1=4 decade BCD
     */
    unsigned char ctrl = 0;
    int caddr = 0;
    switch (clock) {
    case 0:
	ctrl = 0x00;	// select counter 0
	caddr = 12;
	break;
    case 1:
	ctrl = 0x40;	// select counter 1
	caddr = 13;
	break;
    case 2:
	ctrl = 0x80;	// select counter 2
	caddr = 14;
	break;
    }

    ctrl |= 0x30	// ls byte followed by ms byte
    	+ (mode << 1);

    outb(ctrl,brd->addr + 15);

    unsigned char lobyte = val & 0xff;
    outb(lobyte,brd->addr + caddr);
    unsigned char hibyte = val >> 8;
    outb(hibyte,brd->addr + caddr);

    DSMLOG_INFO("ctrl=0x%x, lobyte=%d,hibyte=%d\n",
    	(int)ctrl,(int)lobyte,(int)hibyte);
}

static void initializeA2DClock(struct DSC_Board* brd)
{

    unsigned long ticks = (USECS_PER_SEC * 10 + brd->maxRate/2) / brd->maxRate;
    if ((USECS_PER_SEC * 10) % brd->maxRate) {
	DSMLOG_WARNING(
	"maximum sampling rate=%d Hz does not divide evenly into 10 MHz. Actual sampling rate will be %d Hz\n",
		brd->maxRate,(USECS_PER_SEC*10)/ticks);
    }

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
}


/**
 * pthread function that waits on sampleSem, then
 * reads a fifo sample (containing a2d samples)
 * from the samples list, assembles the data into
 * dsm_samples which contain data from one scan over the channels,
 * and buffers this sample on the RTL fifo to user space.
 */
static void* sampleThreadFunc(void *thread_arg)
{
    struct DSC_Board* brd = (struct DSC_Board*) thread_arg;
    DSMLOG_DEBUG("DSC thread started\n");

    struct {
	dsm_sample_time_t timetag;    // timetag of sample
        dsm_sample_length_t length;       // number of bytes in data
        short data[NUM_DSC_CHANNELS];
    } outsamp;

    int outChanIndex = 0;
    register short *outptr = outsamp.data;

    long scanDeltaT = MSECS_PER_SEC / brd->maxRate;
    DSMLOG_INFO("scanDeltaT=%d\n",scanDeltaT);

#ifdef DEBUG
    dsm_sample_time_t lasttt;
#endif

    dsm_sample_time_t tt0 = 0;
    dsm_sample_time_t tt = tt0;

    for (;; ) {

	if (rtl_sem_wait(&brd->sampleSem) < 0) {
	    DSMLOG_WARNING("sampleThreadFunc: %s\n",
		rtl_strerror(rtl_errno));
		return (void*) convert_rtl_errno(rtl_errno);
        }
	if (brd->interrupted) break;

        /*
         * Only scan one input sample here
         */
        if (brd->samples.head == brd->samples.tail) {
	    DSMLOG_WARNING("semaphore, but no input sample\n");
	    continue;
	}
#ifdef DEBUG
	DSMLOG_DEBUG("ThreadFunc, time=%d, head=%d,tail=%d\n",
	    GET_MSEC_CLOCK,
	    brd->samples.head,brd->samples.tail);
#endif

	struct dsm_sample* insamp =
	    brd->samples.buf[brd->samples.tail];

	int nval = insamp->length / sizeof(short);

	// compute time of first whole scan in this fifo dump.
	// The fifoThreshold of MM16AT is fixed at 256 samples,
	// which may not be an integral number of scans.
	// This sample may not contain an integral number of scans.
	tt0 = insamp->timetag;
	int ndt;
	if (outChanIndex == 0)
	    ndt = nval / brd->nchans;
	else
	    ndt = (nval - (brd->nchans-outChanIndex)) / brd->nchans;
	long dt = ndt * scanDeltaT;

	// tt0 is conversion time of first sample in input
	if (tt0 < dt) tt0 += MSECS_PER_DAY;
	tt0 -= dt;
	if (outChanIndex == 0) tt = tt0;

#ifdef DEBUG
	DSMLOG_INFO("clock=%d, tt=%d, tt0=%d, nval=%d, ndt=%d, dt=%d, lasttt=%d, diff=%d\n",
		GET_MSEC_CLOCK,insamp->timetag,tt0,nval, ndt,dt,lasttt,
		insamp->timetag - lasttt);
	lasttt = tt;
#endif

	register short *dp = (short *)insamp->data;
	register short *ep = dp + nval;

#ifdef DEBUG
	DSMLOG_DEBUG("thread: data0=%d,dataN=%d\n",
	    dp[0],dp[brd->fifoThreshold-1]);
#endif


	for (; dp < ep; ) {
	    int chanNum = outChanIndex + brd->lowChan;
	    if (brd->requested[chanNum]) *outptr++ = *dp++;
	    else dp++;

#ifdef DEBUG
	    DSMLOG_INFO("outChanIndex=%d, chanNum=%d, outptr-outsamp.data=%d\n",
	    	outChanIndex,chanNum,outptr-outsamp.data);
#endif

	    outChanIndex = (outChanIndex + 1) % brd->nchans;

	    if (outChanIndex == 0) {
		outsamp.timetag = tt;
		outsamp.length = (outptr - outsamp.data) * sizeof(short);
		size_t slen = SIZEOF_DSM_SAMPLE_HEADER + outsamp.length;
	        if (brd->outfd >= 0) {	// output sample
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
			if ((wlen = rtl_write(brd->outfd,
			    brd->buffer+brd->tail,brd->head - brd->tail)) < 0) {
			    int ierr = rtl_errno;       // save err
			    DSMLOG_ERR("error: write of %d bytes to %s: %s. Closing\n",
				brd->head-brd->tail,brd->outFifoName,
				rtl_strerror(rtl_errno));
			    rtl_close(brd->outfd);
			    brd->outfd = -1;
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
		    else brd->status.missedSamples++;
		}
		outptr = outsamp.data;
		tt0 += scanDeltaT;
		if (tt0 >= MSECS_PER_DAY) tt0 -= MSECS_PER_DAY;
		tt = tt0;
	    }
	}
	unsigned long flags;
	rtl_spin_lock_irqsave(&brd->queuelock,flags);
	INCREMENT_TAIL(brd->samples,DSC_SAMPLE_QUEUE_SIZE);
	rtl_spin_unlock_irqrestore(&brd->queuelock,flags);

    }
    return 0;
}


unsigned int dsc_irq_handler(unsigned int irq,
        void* callbackptr, struct rtl_frame *regs)
{

    struct DSC_Board* brd = (struct DSC_Board*) callbackptr;

    unsigned char status = inb(brd->int_status_reg);
    if (!(status & brd->ad_int_mask)) return 0;	// not my interrupt

    int flevel = brd->getFifoLevel(brd);
    switch (flevel) {
    default:
    case 3:
        brd->status.fifoOverflows++;
	brd->resetFifo(brd);
	// acknowledge interrupt
	outb(brd->int_ack_val, brd->int_ack_reg);
	return 0;
    case 2: break;	// expected value
    case 1:
	brd->resetFifo(brd);
    case 0:
        brd->status.fifoUnderflows++;
	// acknowledge interrupt
	outb(brd->int_ack_val, brd->int_ack_reg);
	return 0;
	break;
    }

    register int i;
    struct dsm_sample* samp =
	GET_HEAD(brd->samples,DSC_SAMPLE_QUEUE_SIZE);
    if (!samp) {                // no output sample available
        brd->status.missedSamples += brd->fifoThreshold;
	for (i = 0; i < brd->fifoThreshold; i++) inw(brd->addr);
        return 0;
    }

    samp->timetag = GET_MSEC_CLOCK;
    register char* dp = samp->data;

    // the actual fifo read
    insw(brd->addr,dp,brd->fifoThreshold);
    samp->length = brd->fifoThreshold * sizeof(short);

    // acknowledge interrupt
    outb(brd->int_ack_val, brd->int_ack_reg);

    /* On the MM16AT the fifo empty bit isn't set after reading
     * threshold number of values.  Perhaps it's a board bug?
     * On the MM32XAT the fifo empty bit is set at this point.
     */
    flevel = brd->getFifoLevel(brd);
    if (flevel != 0) {
        brd->status.fifoNotEmpty++;
#ifdef DEBUG
	DSMLOG_INFO("fifo level=%d, base+8=0x%x, base+10=0x%x\n",
		flevel,inb(brd->addr+8),inb(brd->addr+10));
#endif
    }

#ifdef DEBUG
    dp = samp->data;
    DSMLOG_DEBUG("irq: timetag=%d, data0=%d,dataN=%d\n",
	samp->timetag,
    	((short *)dp)[0],((short *)dp)[brd->fifoThreshold-1]);
#endif

    /* increment head, this sample is ready for consumption */
    INCREMENT_HEAD(brd->samples,DSC_SAMPLE_QUEUE_SIZE);
    rtl_sem_post(&brd->sampleSem);

    return 0;
}

static void stopMM16AT(struct DSC_Board* brd)
{

    unsigned long flags;
    rtl_spin_lock_irqsave(&brd->boardlock,flags);

    // disable triggering and interrupts
    outb(0, brd->addr + 9);

    // disble fifo, scans
    outb(0,brd->addr + 10);

    // reset fifo
    outb(0x80,brd->addr + 10);

    rtl_spin_unlock_irqrestore(&brd->boardlock,flags);
}

static void stopMM32XAT(struct DSC_Board* brd)
{
    unsigned long flags;
    rtl_spin_lock_irqsave(&brd->boardlock,flags);

    // full reset
    // outb(0x20, brd->addr + 8);

    // set page to 0
    outb(0x00, brd->addr + 8);
    
    // disable interrupts, hardware clock
    outb(0,brd->addr + 9);

    // disable and reset fifo, disable scan mode
    outb(0x2,brd->addr + 9);

    rtl_spin_unlock_irqrestore(&brd->boardlock,flags);
}

/**
 * Determine number of available channels - depends
 * on single-ended vs differential jumpering.
 */
static int getNumChannelsMM16AT(struct DSC_Board* brd)
{

    int nchan = 8;
    unsigned long flags;

    rtl_spin_lock_irqsave(&brd->boardlock,flags);
    unsigned char status = inb(brd->addr + 8);
    rtl_spin_unlock_irqrestore(&brd->boardlock,flags);

    if (status & 0x20) nchan += 8;	// single-ended
    return nchan;
}

/**
 * Determine number of available channels - depends
 * on single-ended vs differential jumpering.
 */
static int getNumChannelsMM32XAT(struct DSC_Board* brd)
{

    unsigned long flags;

    // Jumpered options:
    //   1. channels 0-31 single-ended
    //   2. channels 0-15 differential
    //   3. 0-7 DI, 8-15 SE, 24-31 SE (not continuous)
    //   4. 0-7 SE, 8-15 DI, 16-23 SE
    rtl_spin_lock_irqsave(&brd->boardlock,flags);
    unsigned char status = inb(brd->addr + 8);
    rtl_spin_unlock_irqrestore(&brd->boardlock,flags);

    int nchan = 16;
    if (status & 0x20) nchan += 8;	// 0-7, 16-23 single-ended
    if (status & 0x40) nchan += 8;	// 8-15, 24-31 single-ended
    return nchan;
}

/**
 * returns: 0: ok,  -EINVAL: channels out of range.
 */
static int selectChannelsMM16AT(struct DSC_Board* brd)
{

    int nchan = brd->getNumChannels(brd);

    if (brd->lowChan < 0) return -EINVAL;
    if (brd->highChan >= nchan ||
    	brd->highChan < brd->lowChan) return -EINVAL;
    if (brd->lowChan > brd->highChan) return -EINVAL;
      
    unsigned char chanNibbles = (brd->highChan << 4) + brd->lowChan;
    DSMLOG_INFO("highChan=%d,lowChan=%d,nib=0x%x\n",
    	brd->highChan,brd->lowChan,(int)chanNibbles);

    unsigned long flags;
    rtl_spin_lock_irqsave(&brd->boardlock,flags);
    outb(chanNibbles, brd->addr + 2);
    rtl_spin_unlock_irqrestore(&brd->boardlock,flags);

    brd->waitForA2DSettle(brd);
    return 0;
}

/**
 * returns: 0: ok,  -EINVAL: channels out of range.
 */
static int selectChannelsMM32XAT(struct DSC_Board* brd)
{

    int nchan = brd->getNumChannels(brd);

    if (brd->lowChan < 0) return -EINVAL;
    if (brd->highChan >= nchan ||
    	brd->highChan < brd->lowChan) return -EINVAL;
    if (brd->lowChan > brd->highChan) return -EINVAL;
      
    unsigned long flags;
    rtl_spin_lock_irqsave(&brd->boardlock,flags);
    outb(brd->lowChan, brd->addr + 2);
    outb(brd->highChan, brd->addr + 3);
    rtl_spin_unlock_irqrestore(&brd->boardlock,flags);

    brd->waitForA2DSettle(brd);
    return 0;
}

static int getConvRateMM16AT(struct DSC_Board* brd,
	unsigned char* val)
{
    int totalRate = brd->maxRate * (brd->highChan - brd->lowChan + 1);
    // setting bit 4 enables 5.3 usec conversions, 9.3 usec otherwise.
    // 9.3 usec gives an approx sampling rate of 108 KHz.
    // We'll switch to 5.3 for rates over 80 KHz.
    if (totalRate > 80000) *val |= 0x10;
    else *val &= ~0x10;
    return 0;
}

static int getConvRateMM32XAT(struct DSC_Board* brd,
	unsigned char* val)
{
    int totalRate = brd->maxRate * (brd->highChan - brd->lowChan + 1);

    // bits 4&5:
    //	0: 20usec, 50KHz
    //	1: 15usec, 66KHz
    //	2: 10usec, 100KHz
    //	3:  4usec, 250KHz

    *val &= ~0x30;
    if (totalRate > 90000) *val |= 0x30;
    else if (totalRate > 60000) *val |= 0x20;
    else if (totalRate > 40000) *val |= 0x10;
    return 0;
}

/*
 * bipolar gains
 *	gain	range
 *	1	+-10V
 *	2	+-5V
 *	4	+-2.5V
 *	8	+-1.25V
 *	16	+-0.625V
 * unipolar gains
 *	gain	range
 *	1	0-20V	not avail
 *	2	0-10V
 *	4	0-5V
 *	8	0-2.5
 *	16	0-1.25
 */
static int getGainSettingMM16AT(struct DSC_Board* brd,int gain,
	int bipolar,unsigned char* val)
{
    int i;
    const char* gainStrings[2] = {"2,4,8,16","1,2,4,8,16"};

    unsigned char aconfig[][3] = {
    //bipolar,gain,register
    	{0,  2, DSC_UNIPOLAR | DSC_RANGE_10V | DSC_GAIN_1 },	// 0:10V
    	{0,  4, DSC_UNIPOLAR | DSC_RANGE_10V | DSC_GAIN_2 },	// 0:5V
    	{0,  8, DSC_UNIPOLAR | DSC_RANGE_10V | DSC_GAIN_4 },	// 0:2.5V
    	{0, 16, DSC_UNIPOLAR | DSC_RANGE_10V | DSC_GAIN_8 },	// 0:1.25V

    	{1,  1, DSC_BIPOLAR | DSC_RANGE_10V | DSC_GAIN_1 },	// -10:10
    	{1,  2, DSC_BIPOLAR | DSC_RANGE_5V | DSC_GAIN_1 },	// -5:5
    	{1,  4, DSC_BIPOLAR | DSC_RANGE_10V | DSC_GAIN_4 },	// -2.5:2.5
    	{1,  8, DSC_BIPOLAR | DSC_RANGE_10V | DSC_GAIN_8 },	// -1.25:1.25
    	{1, 16, DSC_BIPOLAR | DSC_RANGE_5V | DSC_GAIN_8 },	// -0.625:.625
    };
    int n = sizeof(aconfig) / sizeof(aconfig[0]);

    for (i = 0; i < n; i++)
	if (aconfig[i][0] == bipolar && aconfig[i][1] == gain) break;
    if (i == n) {
	DSMLOG_ERR(
	    "%s: illegal gain=%d for polarity=%s. Allowed gains=%s\n",
		brd->outFifoName,gain,(bipolar ? "bipolar" : "unipolar"),
		gainStrings[bipolar]);
	return -EINVAL;
    }
    *val = aconfig[i][2];
    return 0;
}

static int getGainSettingMM32XAT(struct DSC_Board* brd, int gain,
	int bipolar, unsigned char* val)
{
    int i;
    const char* gainStrings[2] = {"2,4,8,16,32","1,2,4,8,16"};

    unsigned char aconfig[][3] = {
    //bipolar,gain,register setting
    	{0,  2, DSC_UNIPOLAR | DSC_RANGE_10V | DSC_GAIN_1 },	// 0:10V
    	{0,  4, DSC_UNIPOLAR | DSC_RANGE_10V | DSC_GAIN_2 },	// 0:5V
    	{0,  8, DSC_UNIPOLAR | DSC_RANGE_10V | DSC_GAIN_4 },	// 0:2.5V
    	{0, 16, DSC_UNIPOLAR | DSC_RANGE_10V | DSC_GAIN_8 },	// 0:1.25V
    	{0, 32, DSC_UNIPOLAR | DSC_RANGE_5V | DSC_GAIN_8 },	// 0:0.625V

    	{1,  1, DSC_BIPOLAR | DSC_RANGE_10V | DSC_GAIN_1 },	// -10:10
    	{1,  2, DSC_BIPOLAR | DSC_RANGE_5V | DSC_GAIN_1 },		// -5:5
    	{1,  4, DSC_BIPOLAR | DSC_RANGE_10V | DSC_GAIN_4 },	// -2.5:2.5
    	{1,  8, DSC_BIPOLAR | DSC_RANGE_10V | DSC_GAIN_8 },	// -1.25:1.25
    	{1, 16, DSC_BIPOLAR | DSC_RANGE_5V | DSC_GAIN_8 },	// -0.625:.625
    };

    int n = sizeof(aconfig) / sizeof(aconfig[0]);

    for (i = 0; i < n; i++)
        if (aconfig[i][0] == bipolar && aconfig[i][1] == gain) break;
    if (i == n) {
	DSMLOG_ERR(
	    "%s: illegal gain=%d for polarity=%s. Allowed gains=%s\n",
		brd->outFifoName,gain,(bipolar ? "bipolar" : "unipolar"),
		gainStrings[bipolar]);
	return -EINVAL;
    }
    *val = aconfig[i][2];
    return 0;
}

/*
 * Returns:
 * 3: fifo full or overflowed
 * 2: fifo at or above threshold but not full
 * 1: fifo not empty but less than threshold
 * 0: fifo empty
 */
static int getFifoLevelMM16AT(struct DSC_Board* brd)
{
    /* base + 10 
     * bit 2: 0=no overflow, 1=overflow
     * bit 1: 0=less than half, 1=at least half
     * bit 0: 0=not empty, 1= empty
     *
     * Note: on the MM16AT, the FIFO must be reset (base + 10)
     * to clear an overflow.
     */
    unsigned char fifo = inb(brd->addr + 10) & 0x3;
    switch (fifo) {
    case 1: return 0;
    case 0: return 1;
    case 2: return 2;
    case 6: return 3;

    /* other settings don't make sense - return overflow */
    default: return 3;
    }
}

/*
 * Returns:
 * 3: fifo full or overflowed
 * 2: fifo at or above threshold but not full
 * 1: fifo not empty but less than threshold
 * 0: fifo empty
 */
static int getFifoLevelMM32XAT(struct DSC_Board* brd)
{
    /* base + 7 
     * bit 7: 0=not empty, 1= empty
     * bit 6: 0=less than threshold, 1=at least threshold
     * bit 5: 0=not full, 1=full
     * bit 4: 0=not overflowed, 1=overflow
     *
     * Note: on the MM16AT, the FIFO must be reset (base + 10)
     * to clear an overflow.
     */
    unsigned char fifo = (inb(brd->addr + 7) & 0xf0) >> 4;
    switch (fifo) {
    case 8: return 0;
    case 0: return 1;
    case 4: return 2;
    case 3:
    case 2:
    case 1: return 3;

    /* other settings don't make sense - return overflow */
    default: return 3;
    }
}

static void resetFifoMM16AT(struct DSC_Board* brd)
{
    outb(0x80,brd->addr + 10);
}

static void resetFifoMM32XAT(struct DSC_Board* brd)
{
    outb(0x02,brd->addr + 7);
}

static void waitForA2DSettleMM16AT(struct DSC_Board* brd)
{
    int ntry = 0;
    do {
	unsigned long j = jiffies + 1;
	while (jiffies < j) schedule();
    } while(ntry++ < 50 && inb(brd->addr + 10) & 0x80);
    DSMLOG_INFO("ntry=%d\n",ntry);
}

static void waitForA2DSettleMM32XAT(struct DSC_Board* brd)
{
    do {
	unsigned long j = jiffies + 1;
	while (jiffies < j) schedule();
    } while(inb(brd->addr + 11) & 0x80);
}
/*
 * Configure board.  Board should not be busy.
 */
static int configA2D(struct DSC_Board* brd,struct DSC_Config* cfg)
{

    if(brd->busy) {
	    DSMLOG_ERR("A2D's running. Can't configure\n");
	    return -EBUSY;
    }

    int result = 0;
    int i;

    int gain = 0;
    int bipolar = 0;
    brd->lowChan = 0;
    brd->highChan = 0;

    brd->maxRate = 0;

    for (i = 0; i < NUM_DSC_CHANNELS; i++) {
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
    brd->nchans = brd->highChan - brd->lowChan + 1;

    if (brd->maxRate == 0) {
	DSMLOG_ERR("%s: no channels requested, all rates==0\n",
	    brd->outFifoName);
	return -EINVAL;
    }
    if ((USECS_PER_SEC * 10) % brd->maxRate)
	DSMLOG_WARNING("%s: max sampling rate=%d is not a factor of 10 MHz\n",
	    brd->outFifoName,brd->maxRate);

    result = brd->getGainSetting(brd,gain,bipolar,&brd->gainSetting);
    if (result != 0) return result;

    result = brd->getConvRateSetting(brd,&brd->gainSetting);
    if (result != 0) return result;

    brd->latencyMsecs = cfg->latencyUsecs / USECS_PER_MSEC;
    if (brd->latencyMsecs == 0) brd->latencyMsecs = 500;

#ifdef DEBUG
	DSMLOG_DEBUG("latencyUsecs=%d, latencyMsecs=%d\n",
		 cfg->latencyUsecs,brd->latencyMsecs);
#endif

    return 0;
}

/**
 */
static int stopA2D(struct DSC_Board* brd) 
{
    int ret = 0;

    DSMLOG_DEBUG("stopA2D entered\n");

    brd->stop(brd);

    if (brd->sampleThread) {
	void* threadStatus;
	// interrupt the sample thread
	DSMLOG_DEBUG("interrupting\n");
	brd->interrupted = 1;
	rtl_sem_post(&brd->sampleSem);
	DSMLOG_DEBUG("pthread_join\n");
	if (rtl_pthread_join(brd->sampleThread, &threadStatus))
	    DSMLOG_ERR("error: pthread_join %s: %s\n",
		    brd->outFifoName,rtl_strerror(rtl_errno));
	DSMLOG_DEBUG("sampleThread joined, status=%d\n",
		 (int)threadStatus);
	brd->sampleThread = 0;
    }

    if (brd->outfd >= 0) {
	int fdtmp = brd->outfd;
	brd->outfd = -1;
	rtl_close(fdtmp);
    }

    brd->busy = 0;	// Reset the busy flag

    return ret;
}

static int startA2D(struct DSC_Board* brd)
{
    int result;
    unsigned long flags;

    if (brd->busy) stopA2D(brd);

    brd->busy = 1;	// Set the busy flag

    rtl_spin_lock_irqsave(&brd->queuelock,flags);
    brd->samples.head = brd->samples.tail = 0;
    rtl_spin_unlock_irqrestore(&brd->queuelock,flags);

    rtl_sem_init(&brd->sampleSem,1,0);

    // buffer indices
    brd->head = brd->tail = 0;

    if((brd->outfd = rtl_open(brd->outFifoName,
	    RTL_O_NONBLOCK | RTL_O_WRONLY)) < 0)
    {
	DSMLOG_ERR("error: opening %s: %s\n",
		brd->outFifoName,rtl_strerror(rtl_errno));
	return -convert_rtl_errno(rtl_errno);
    }
#ifdef DO_FTRUNCATE
    if (rtl_ftruncate(brd->outfd, sizeof(brd->buffer)*4) < 0) {
	DSMLOG_ERR("error: ftruncate %s: size=%d: %s\n",
		brd->outFifoName,sizeof(brd->buffer),
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

    if ((result = brd->selectChannels(brd))) return result;

    DSMLOG_INFO("gainSetting=%x\n",brd->gainSetting);
    rtl_spin_lock_irqsave(&brd->boardlock,flags);
    // same addr on MM16AT and MM32XAT
    outb(brd->gainSetting,brd->addr + 11);
    rtl_spin_unlock_irqrestore(&brd->boardlock,flags);

    brd->waitForA2DSettle(brd);

    memset(&brd->status,0,sizeof(brd->status));

    brd->start(brd);
    return 0;
}

static int startMM16AT(struct DSC_Board* brd)
{
    int result = 0;
    unsigned long flags;
    rtl_spin_lock_irqsave(&brd->boardlock,flags);

    // reset fifo
    outb(0x80,brd->addr + 10);

    /*
     * base+10, Counter/timer and FIFO control
     * 
     * bit 7: fifo reset
     * bit 6: page number for registers at base+12 to base+15:
     *        0=82C54 counter/timer, 1=calibration
     * bit 5: fifo enable
     * bit 4: scan enable
     * bit 3: 0=input to counter/timer 1 is 10MHz, 1=1MHz
     * bit 2: 1=IN0- (IO pin 29) is a gate for A/D sample control
     *        when extern A/D clock is enabled.
     *        0=IN0- is not a gate for A/D sample control
     * bit 1: 1=input to counter 0 is a 100kHz reference, gated by IN0-.
     *        0=input to counter 0 is IN0-.
     * bit 0: 1=counters 1&2 are gated by DIN0 (IO pin 48).
     *        0=counters 1&2 run freely
     */
    outb(0x30,brd->addr + 10);

    brd->fifoThreshold = 256;

    initializeA2DClock(brd);

     /*
      * base+9, Control register
      *
      * bit 7: enable A/D interrupts
      * bit 3: enable timer 0 interrupts (don't set both 3 and 7)
      * bit 1: 1=enable hardware A/D hardware clock
	      0=A/D software triggered by write to base+0
      * bit 0: 1=internal hardware trigger, counter/timer 1&2
            0=external trigger: DIN0, IO pin 48
      */
    outb(0x83,brd->addr + 9);

    rtl_spin_unlock_irqrestore(&brd->boardlock,flags);

    return result;
}

static int startMM32XAT(struct DSC_Board* brd)
{
    int result = 0;
    unsigned long flags;

    rtl_spin_lock_irqsave(&brd->boardlock,flags);

    // compute fifo threshold
    // number of scans in latencyMsecs
    int nscans = (brd->latencyMsecs * brd->maxRate) / MSECS_PER_SEC;
    if (nscans == 0) nscans = 1;

    int nsamps = nscans * brd->nchans;
    // fifo is 1024 samples. Want an interrupt before it is 1/2 full
    if (nsamps > 512) nsamps = (512 / nsamps) * nsamps;
    if ((nsamps % 2)) nsamps += brd->nchans;		// must be even

    brd->fifoThreshold = nsamps;
    DSMLOG_INFO("fifoThreshold=%d,latency=%d,nchans=%d,nsamps=%d\n",
    	brd->fifoThreshold,brd->latencyMsecs,brd->nchans,nsamps);
    // register value is 1/2 the threshold
    nsamps /= 2;

    if (nsamps > 255) {
        outb(0x03,brd->addr + 8);	// set page 3
        outb(0xA6,brd->addr + 15);	// enable enhanced features
        outb(0x02,brd->addr + 8);	// set page 2
        outb(0x01,brd->addr + 12);	// set high bit for fifo threshold
        outb(0x00,brd->addr + 8);	// set page 0
    }
    outb(nsamps & 0xff, brd->addr + 6);

    /*
     * base+7, Counter/timer and FIFO control
     * 
     * bit 3: fifo enable
     * bit 2: scan enable
     * bit 1: fifo reset
     */
    outb(0x0e,brd->addr + 7);

    /*
     * base+10, Counter/timer and DIO control
     * 
     * bit 7: 1=input to counter1&2 is 100 KHz, 0=10MHz
     * bit 6: 1=input to counter0 is 10 KHz, 0=10MHz
     * bit 5: 1=counter 1&2 output on J3,pin 42, 0=42 is digital out
     * bit 4: 1=counter 0 output on J3,pin 44, 0=44 is digital out
     * bit 2: 1=J3 pin 47 is gate for counter0, 0=counter0 free running
     * bit 1: 1=counter0 input is clock, set by bit6. 0=input is J3 pin 48
     * bit 0: 1=J3 pin 46 is gate for counter1&2, 0=counter1&2 free running
     */
    outb(0x0,brd->addr + 10);

    initializeA2DClock(brd);

     /*
      * base+9, Control register
      *
      * bit 7: enable A/D interrupts
      * bit 6: enable dio interrupts
      * bit 5: enable timer 0 interrupts
      * bit 1: 1=enable hardware A/D hardware clock
	      0=A/D software triggered by write to base+0
      * bit 0: 1=internal hardware trigger, counter/timer 1&2
            0=external trigger: DIN0, IO pin 48
      */
    outb(0x83,brd->addr + 9);

    rtl_spin_unlock_irqrestore(&brd->boardlock,flags);

    return result;
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

#ifdef DEBUG
    DSMLOG_DEBUG("ioctlCallback cmd=%x board=%d port=%d len=%d\n",
	cmd,board,port,len);
#endif

    // paranoid check if initialized (probably not necessary)
    if (!boardInfo) return ret;

    if (board >= numboards) return ret;

    struct DSC_Board* brd = boardInfo + board;

    switch (cmd) 
    {
    case GET_NUM_PORTS:		/* user get */
	    if (len != sizeof(int)) break;
	    DSMLOG_DEBUG("GET_NUM_PORTS\n");
	    *(int *) buf = N_DSC_DEVICES;	
	    ret = sizeof(int);
	    break;

    case DSC_GET_NCHAN:
	    if (port != 0) break;	// port 0 is the A2D
	    DSMLOG_DEBUG("DSC_GET_NCHAN\n");
	    if (len != sizeof(int)) break;
	    *(int *)buf = brd->getNumChannels(brd);
	    DSMLOG_DEBUG("DSC_GET_NCHAN finished\n");
	    ret = sizeof(int);
	    break;
    case DSC_STATUS:		/* user get of status */
	    if (port != 0) break;	// only port 0
	    if (len != sizeof(struct DSC_Status)) break;
	    memcpy(buf,&brd->status,len);
	    ret = len;
	    break;

    case DSC_CONFIG:		/* user set */
	    DSMLOG_DEBUG("DSC_CONFIG\n");
	    if (port != 0) break;	// only port 0
	    if (len != sizeof(struct DSC_Config)) break;// invalid length
	    ret = configA2D(brd,(struct DSC_Config*)buf);
	    DSMLOG_DEBUG("DSC_CONFIG done, ret=%d\n", ret);
	    break;

    case DSC_START:
	    if (port != 0) break;	// port 0 is the A2D
	    DSMLOG_DEBUG("DSC_START\n");
	    ret = startA2D(brd);
	    DSMLOG_DEBUG("DSC_START finished\n");
	    break;

    case DSC_STOP:
	    if (port != 0) break;	// port 0 is the A2D
	    DSMLOG_DEBUG("DSC_STOP\n");
	    ret = stopA2D(brd);
	    DSMLOG_DEBUG("DSC_STOP finished, ret=%d\n",ret);
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
    for (ib = 0; ib < MAX_DSC_BOARDS; ib++)
	if (ioports[ib] == 0) break;
    numboards = ib;
    if (numboards == 0) {
	DSMLOG_ERR("No boards configured, all ioports[]==0\n");
	goto err;
    }

    error = -ENOMEM;
    boardInfo = rtl_gpos_malloc( numboards * sizeof(struct DSC_Board) );
    if (!boardInfo) goto err;

    /* initialize each DSC_Board structure */
    for (ib = 0; ib < numboards; ib++) {
	struct DSC_Board* brd = boardInfo + ib;

	// initialize structure to zero, then initialize things
	// that are non-zero
	memset(brd,0,sizeof(struct DSC_Board));
	brd->outfd = -1;
    }

    for (ib = 0; ib < numboards; ib++) {
	struct DSC_Board* brd = boardInfo + ib;
	error = -EBUSY;
	unsigned int addr =  ioports[ib] + SYSTEM_ISA_IOPORT_BASE;
	// Get the mapped board address
	if (check_region(addr, DSC_IOPORT_WIDTH)) {
	    DSMLOG_ERR("ioport at 0x%x already in use\n", addr);
	    goto err;
	}

	request_region(addr, DSC_IOPORT_WIDTH, "dsc_a2d");
	brd->addr = addr;

	error = -EINVAL;
	if (irqs[ib] <= 0) {
	    DSMLOG_ERR("missing irq value for board #%d at addr 0x%x\n",
	    	ib,ioports[ib]);
	    goto err;
	}
	if ((error = rtl_request_isa_irq(irqs[ib],dsc_irq_handler,brd)) < 0)
		goto err;
	brd->irq = irqs[ib];

	switch (types[ib]) {
	/* board registers */
	case DSC_MM16AT_BOARD:
	    brd->int_status_reg = addr + 8;
	    brd->ad_int_mask = 0x10;

	    brd->int_ack_reg = addr + 8;
	    brd->int_ack_val = 0x00;

	    brd->start = startMM16AT;
	    brd->stop = stopMM16AT;
	    brd->getNumChannels = getNumChannelsMM16AT;
	    brd->selectChannels = selectChannelsMM16AT;
	    brd->getConvRateSetting = getConvRateMM16AT;
	    brd->getGainSetting = getGainSettingMM16AT;
	    brd->getFifoLevel = getFifoLevelMM16AT;
	    brd->resetFifo = resetFifoMM16AT;
	    brd->waitForA2DSettle = waitForA2DSettleMM16AT;
	    brd->maxFifoThreshold = 256;
	    break;

	case DSC_MM32XAT_BOARD:
	    brd->int_status_reg = addr + 9;
	    brd->ad_int_mask = 0x80;

	    brd->int_ack_reg = addr + 8;
	    brd->int_ack_val = 0x08;

	    brd->start = startMM32XAT;
	    brd->stop = stopMM32XAT;
	    brd->getNumChannels = getNumChannelsMM32XAT;
	    brd->selectChannels = selectChannelsMM32XAT;
	    brd->getConvRateSetting = getConvRateMM32XAT;
	    brd->getGainSetting = getGainSettingMM32XAT;
	    brd->getFifoLevel = getFifoLevelMM32XAT;
	    brd->resetFifo = resetFifoMM32XAT;
	    brd->waitForA2DSettle = waitForA2DSettleMM32XAT;
	    brd->maxFifoThreshold = 512;
	    break;
	}

	brd->stop(brd);

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

	rtl_sem_init(&brd->sampleSem,1,0);

	// Open the RTL fifo to user space
	brd->outFifoName = makeDevName(devprefix,"_in_",ib*N_DSC_DEVICES);
	if (!brd->outFifoName) goto err;

	// remove broken device file before making a new one
	if ((rtl_unlink(brd->outFifoName) < 0 && rtl_errno != RTL_ENOENT)
	    || rtl_mkfifo(brd->outFifoName, 0666) < 0) {
	    DSMLOG_ERR("error: unlink/mkfifo %s: %s\n",
		    brd->outFifoName,rtl_strerror(rtl_errno));
	    error = -convert_rtl_errno(rtl_errno);
	    goto err;
	}

	brd->samples.buf = rtl_gpos_malloc(DSC_SAMPLE_QUEUE_SIZE *
	    sizeof(void*) );
	if (!brd->samples.buf) goto err;
	memset(brd->samples.buf,0,
	    DSC_SAMPLE_QUEUE_SIZE * sizeof(void*));
                
	for (i = 0; i < DSC_SAMPLE_QUEUE_SIZE; i++) {
	    struct dsm_sample* samp = (struct dsm_sample*)
	        rtl_gpos_malloc(SIZEOF_DSM_SAMPLE_HEADER +
	      	brd->maxFifoThreshold * sizeof(short));
	    if (!samp) goto err;
	    memset(samp,0,SIZEOF_DSM_SAMPLE_HEADER +
	      	brd->maxFifoThreshold * sizeof(short));
	    brd->samples.buf[i] = samp;
	}

	brd->gainSetting = 0;	// default value
    }

    DSMLOG_DEBUG("complete.\n");

    return 0;
err:

    if (boardInfo) {
	for (ib = 0; ib < numboards; ib++) {
	    struct DSC_Board* brd = boardInfo + ib;

	    if (brd->sampleThreadStack) {
		rtl_gpos_free(brd->sampleThreadStack);
		brd->sampleThreadStack = 0;
		rtl_sem_destroy(&brd->sampleSem);
	    }

	    if (brd->outFifoName) {
		rtl_unlink(brd->outFifoName);
		rtl_gpos_free(brd->outFifoName);
	    }

	    if (brd->samples.buf) {
		for (i = 0; i < DSC_SAMPLE_QUEUE_SIZE; i++)
		    if (brd->samples.buf[i])
		    	rtl_gpos_free(brd->samples.buf[i]);
		rtl_gpos_free(brd->samples.buf);
		brd->samples.buf = 0;
	    }

	    if (brd->ioctlhandle) closeIoctlFIFO(brd->ioctlhandle);
	    brd->ioctlhandle = 0;

	    if (brd->irq) rtl_free_isa_irq(brd->irq);
	    brd->irq = 0;

	    if (brd->addr)
		release_region(brd->addr, DSC_IOPORT_WIDTH);
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
	struct DSC_Board* brd = boardInfo + ib;

	rtl_free_isa_irq(brd->irq);

	brd->stop(brd);

	// Shut down the sample thread
	if (brd->sampleThread) {
	    rtl_pthread_cancel(brd->sampleThread);
	    rtl_pthread_join(brd->sampleThread, NULL);
	}

	if (brd->sampleThreadStack) {
	    rtl_gpos_free(brd->sampleThreadStack);
	    brd->sampleThreadStack = 0;
	    rtl_sem_destroy(&brd->sampleSem);
	}

	// close and remove RTL fifo
	if (brd->outfd >= 0) rtl_close(brd->outfd);
	if (brd->outFifoName) {
	    rtl_unlink(brd->outFifoName);
	    rtl_gpos_free(brd->outFifoName);
	}

	if (brd->ioctlhandle) closeIoctlFIFO(brd->ioctlhandle);

	if (brd->samples.buf) {
	    for (i = 0; i < DSC_SAMPLE_QUEUE_SIZE; i++)
		if (brd->samples.buf[i])
		    rtl_gpos_free(brd->samples.buf[i]);
	    rtl_gpos_free(brd->samples.buf);
	}

	if (brd->addr)
	    release_region(brd->addr, DSC_IOPORT_WIDTH);
    }

    rtl_gpos_free(boardInfo);
    boardInfo = 0;

    DSMLOG_DEBUG("complete\n");

    return;
}

