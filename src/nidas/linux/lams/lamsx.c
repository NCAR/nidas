/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedRevision$
        $LastChangedDate$
          $LastChangedBy$
                $HeadURL$

    Laser Air Motion Sensor (LAMS) driver for the ADS3 DSM.

            Description of the LAMS data, from Mike Spowart

    The output of the LAMS card FFT magnitude is 36 bits (before any accumulation
    is done).  Normally, each accumulation would require that the word length
    grow by one bit in order to guarantee no overflow. But the LAMS card does not
    do this (word size remains at 36 after each accumulation).  After all
    accumulations are done the  36-bit word is reduced to 32-bits by shifting
    right 4 times. The card then generates an interrupt.
    
    Upon an interrupt, these 32 bit averages are read by the ISR in this driver
    (lams_irq_handler) as 2 16 bit words from AVG_LSW_DATA_OFFSET and
    AVG_MSW_DATA_OFFSET. These values are then further averaged by this
    driver before being passed onto the user.  The number of points in an average
    is set by via the LAMS_N_AVG ioctl.
   
    The spectra contain 1024 values. Since the input is real,
    only 512 values are independent (the other 512 are complex conjugates).
    Due to latency in the FPGA and due to the continuous
    pipelined nature of the data stream, it is necessary to find the beginning
    value of one or the other of the two 512 groups. This is done best in
    hardware by connecting a specific frequency to the A/D and seeing where
    in the spectrum it shows up. If it is off by a few spectral points then
    I must shift the spectrum left or right. This is what the specPointSkip
    value in the board structure is for.  If/when I make a significant change
    to the FPGA I have found it necessary to re-calibrate the beginning of the
    spectrum and the SKIP value could become +3 or +5, etc.

    The 16 bit peak values are the maximums of the high order 16 bits of the 36 bit
    spectral values. After nPEAKS number of interrupts, the driver does a read from
    PEAK_CLEAR_OFFSET, which causes the LAMS card to zero its maximum calculations.
    nPEAKS is set by the LAMS_N_PEAKS ioctl. By checking these peak values one can
    determine that the accumulators are not overflowing.
*/


#include <linux/types.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>		/* kmalloc, kfree */
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>

#include "lamsx.h"
#include <nidas/linux/SvnInfo.h>    // SVNREVISION
// #define DEBUG
#include <nidas/linux/klog.h>
#include <nidas/linux/isa_bus.h>
#include <nidas/linux/util.h>

#define MAX_LAMS_BOARDS 3 // maximum number of LAMS cards in sys

/* SA_SHIRQ is deprecated starting in 2.6.22 kernels */
#ifndef IRQF_SHARED
# define IRQF_SHARED SA_SHIRQ
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,16)
#define mutex_init(x)               init_MUTEX(x)
#define mutex_lock_interruptible(x) ( down_interruptible(x) ? -ERESTARTSYS : 0)
#define mutex_unlock(x)             up(x)
#endif

static const char* driver_name = "lamsx";

/* ioport addresses of installed boards, 0=no board installed */
static unsigned int ioports[MAX_LAMS_BOARDS] = { 0x220, 0, 0 };

/* number of LAMS boards in system (number of non-zero ioport values) */
static int numboards = 0;

/* ISA irqs, required for each board. Cannot be shared. */
static int irqs[MAX_LAMS_BOARDS] = { 4, 0, 0 };

static int numirqs = 0;

#if defined(module_param_array) && LINUX_VERSION_CODE > KERNEL_VERSION(2,6,9)
module_param_array(ioports,int,&numboards,0);
module_param_array(irqs,int,&numirqs,0);
#else
module_param_array(ioports,int,numboards,0);
module_param_array(irqs,int,numirqs,0);
#endif

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_LICENSE("Dual BSD/GPL");

#define RAM_CLEAR_OFFSET         0x00
#define PEAK_CLEAR_OFFSET        0x02
#define AVG_LSW_DATA_OFFSET      0x04
#define AVG_MSW_DATA_OFFSET      0x06
#define PEAK_DATA_OFFSET         0x08
#define TAS_BELOW_OFFSET         0x0A
#define TAS_ABOVE_OFFSET         0x0C

// Interrupt enable is not implemented.
// #define ENABLE_IRQ_OFFSET        0x0E

#define IOPORT_REGION_SIZE 16  // number of 1-byte registers

// Initial skip value. Also set-able via an ioctl
#define SPECTRAL_POINTS_TO_SKIP   0

#define LAMS_ISR_SAMPLE_QUEUE_SIZE 128
#define LAMS_OUTPUT_SAMPLE_QUEUE_SIZE 32

/*
 * Holds the major number of all LAMS devices.
 */
static dev_t lams_device = MKDEV(0,0);

/*
 * Data used by bottom-half processing.
 */
struct bh_data {
        dsm_sample_time_t timetag;
        unsigned int nAvg;
#ifdef USE_64BIT_SUMS
        long long sum[LAMS_SPECTRA_SIZE];
#else
        /* sum of high order bits */
        unsigned int hosum[LAMS_SPECTRA_SIZE];
        /* sum of low order bits */
        unsigned int losum[LAMS_SPECTRA_SIZE];
#endif
};

/*
 * Info kept for each LAMS board in system.
 */
struct LAMS_board {

        int num;

        unsigned long addr;

        unsigned long ram_clear_addr;
        unsigned long avg_lsw_data_addr;
        unsigned long avg_msw_data_addr;
        unsigned long peak_data_addr;
        unsigned long peak_clear_addr;

        int irq;

        spinlock_t reglock;

        char deviceName[32];

        struct cdev cdev;

        /**
         * Used to force exclusive open.
         */
        atomic_t num_opened;

        /**
         * How many spectra to further average.
         */
        int nAVG;

#ifndef USE_64BIT_SUMS
        unsigned int avgMask;

        int avgShift;
#endif

        int nPEAKS;

        int nPeaks;

        struct work_struct worker;

        /**
         * data for use by bottom half
         */
        struct bh_data bh_data;

        /**
         * spectra average samples read from LAMS board in ISR,
         * passed to bottom half for further averaging.
         */
        struct dsm_sample_circ_buf isr_avg_samples;

        /**
         * output spectra average samples from bottom half.
         */
        struct dsm_sample_circ_buf avg_samples;

        /**
         * output spectra peak samples read from LAMS board in ISR.
         */
        struct dsm_sample_circ_buf peak_samples;

        /**
         * user read & poll methods wait on this
         */
        wait_queue_head_t read_queue;

        struct sample_read_state avg_read_state;

        struct sample_read_state peak_read_state;

        struct lams_status status;

        /**
         * Initial points to skip in the spectrum
         */
        int specPointSkip;
};

/*
 * Pointer to first of dynamically allocated structures containing
 * all data pertaining to the configured LAMS boards on the system.
 */
static struct LAMS_board* boards = 0;

static struct workqueue_struct* work_queue = 0;

/*
 * Set the number of spectra to average.
 */
static int setNAvg(struct LAMS_board* brd, int val)
{
#ifndef USE_64BIT_SUMS
        /* Verify that val is a power of two, by checking
         * if first bit set in val is the same as the last bit set:
         * ffs(blen) == fls(blen)
         */
        if (val <= 0 || ffs(val) != fls(val)) {
            KLOG_ERR("number of spectra to average (%d) is not a positive power of 2\n",val);
            return -EINVAL;
        }
        brd->avgMask = val - 1;
        brd->avgShift = ffs(val) - 1;
#endif
        brd->nAVG = val;
        return 0;
}

/**
 * Work queue function that processes the raw samples
 * from the ISR.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void lams_bottom_half(struct work_struct* work)
#else
static void lams_bottom_half(void* work)
#endif
{
        struct LAMS_board* brd = container_of(work,struct LAMS_board,worker);
        struct bh_data *bhd = &brd->bh_data;
        int i;
        struct lams_avg_sample* insamp;
#ifndef USE_64BIT_SUMS
        int nshift = brd->avgShift;
        unsigned int mask = brd->avgMask;
#endif

        KLOG_DEBUG("%s: worker entry, fifo head=%d,tail=%d\n",
            brd->deviceName,brd->isr_avg_samples.head,brd->isr_avg_samples.tail);

        /* average spectra samples */
        /* ISR is writing/incrementing head, we're reading/incrementing the tail */
        while ((insamp = (struct lams_avg_sample*)
                    GET_TAIL(brd->isr_avg_samples,brd->isr_avg_samples.size))) {
                for (i = 0; i < LAMS_SPECTRA_SIZE; i++) {
#ifdef USE_64BIT_SUMS
                        bhd->sum[i] += insamp->data[i];
#else
                        bhd->hosum[i] += insamp->data[i] >> nshift;
                        bhd->losum[i] += insamp->data[i] & mask;
#endif
                }
                // user might reduce nAVG via an ioctl, so check for >=
                if (++bhd->nAvg >= brd->nAVG) bhd->timetag = insamp->timetag;
                INCREMENT_TAIL(brd->isr_avg_samples,LAMS_ISR_SAMPLE_QUEUE_SIZE);

                if (bhd->nAvg >= brd->nAVG) {

                        struct lams_avg_sample* samp;
                        samp = (struct lams_avg_sample*) GET_HEAD(brd->avg_samples,LAMS_OUTPUT_SAMPLE_QUEUE_SIZE);
                        if (!samp) {                // no output sample available
                                brd->status.missedOutSamples++;
                                continue;
                        }
                        else {
                                for (i = 0; i < LAMS_SPECTRA_SIZE; i++) {
#ifdef USE_64BIT_SUMS
                                        samp->data[i] = cpu_to_le32(bhd->sum[i] / brd->nAVG);
#else
                                        samp->data[i] = cpu_to_le32(bhd->hosum[i] + (bhd->losum[i] >> nshift));
#endif
                                }
                                samp->timetag = bhd->timetag;
                                samp->length = sizeof(struct lams_avg_sample) - SIZEOF_DSM_SAMPLE_HEADER;
                                samp->type = cpu_to_le32(LAMS_SPECAVG_SAMPLE_TYPE);

                                /* increment head, this sample is ready for reading */
                                INCREMENT_HEAD(brd->avg_samples,LAMS_OUTPUT_SAMPLE_QUEUE_SIZE);

                                /* wake up reader */
                                wake_up_interruptible(&brd->read_queue);

                                // zero the sums and nAvg
                                memset(bhd,0,sizeof(brd->bh_data));
                        }
                }
        }
}

/*
 * IRQ handler for the board.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
static irqreturn_t lams_irq_handler(int irq, void* dev_id)
#else
static irqreturn_t lams_irq_handler(int irq, void* dev_id, struct pt_regs *regs)
#endif
{
        struct LAMS_board* brd = (struct LAMS_board*) dev_id;
        struct lams_avg_sample* asamp;
        struct lams_peak_sample* psamp = 0;
        int i;
        unsigned short msw, lsw;

        dsm_sample_time_t ttag = getSystemTimeTMsecs();

        // The data portion (not the timetag or length) of the samples that are sent to the user
        // side should be little-endian. The timetag and length should be host-endian.

        // The average sample (asamp) is passed on to the bottom half for further averaging,
        // so that data is left as host-endian, which is what it is after being read with inw.
        // The peak samples (psamp) are sent straight to the user side, so the data portion
        // should be converted to little-endian here in the ISR.

        asamp = (struct lams_avg_sample*) GET_HEAD(brd->isr_avg_samples,LAMS_ISR_SAMPLE_QUEUE_SIZE);

        if (!asamp) {                
                // No output sample available.  Do the minimum necessary to
                // acknowledge the interrupt and then return.

                brd->status.missedISRSamples++;

                spin_lock(&brd->reglock);
                // Clear Dual Port memory address counter, which also acknowledges the interrupt.
                inw(brd->ram_clear_addr);

                // don't clear the peaks. Otherwise the averaging will get out-of-sync
                // with the peaks.

                spin_unlock(&brd->reglock);
                return IRQ_HANDLED;
        }

        brd->nPeaks++;

        asamp->timetag = ttag;
        asamp->length = sizeof(struct lams_avg_sample) - SIZEOF_DSM_SAMPLE_HEADER;
        asamp->type = LAMS_SPECAVG_SAMPLE_TYPE;

        // Send a peak sample every nAVG times.
        if (!(brd->nPeaks % brd->nAVG)) {
                psamp = (struct lams_peak_sample*) GET_HEAD(brd->peak_samples,LAMS_OUTPUT_SAMPLE_QUEUE_SIZE);

                if (!psamp) {                // no output sample available
                        brd->status.missedOutSamples++;
                }
                else {
                        psamp->timetag = ttag;
                        psamp->length = sizeof(struct lams_peak_sample) - SIZEOF_DSM_SAMPLE_HEADER;
                        // data to user side is little-endian
                        psamp->type = cpu_to_le32(LAMS_SPECPEAK_SAMPLE_TYPE);
                }
        }

        spin_lock(&brd->reglock);

        // Clear Dual Port memory address counter, which also acknowledges the interrupt.
        inw(brd->ram_clear_addr);

        // skip initial values to position at beginning of spectrum
        for (i = 0; i < brd->specPointSkip; i++) {
                inw(brd->avg_lsw_data_addr);
                inw(brd->avg_msw_data_addr);
                inw(brd->peak_data_addr);
        }

        for (i = 0; i < LAMS_SPECTRA_SIZE; i++) {
                lsw = inw(brd->avg_lsw_data_addr);
                msw = inw(brd->avg_msw_data_addr);
                asamp->data[i] = ((unsigned int)msw << 16) + lsw;
                /* Reading of the peaks increments the counter on the LAMS
                 * card for both peaks and averages.  In other words the
                 * peaks must be read at the same time as the averages and
                 * must be done after the averages. Read the peak even
                 * if we aren't storing them in a sample.
                 */
                lsw = inw(brd->peak_data_addr);
                // data to user side is little-endian
                if (psamp) psamp->data[i] = cpu_to_le16(lsw);
        }
        if (brd->nPeaks >= brd->nPEAKS) {
                inw(brd->peak_clear_addr);
                brd->nPeaks = 0;
        }
        spin_unlock(&brd->reglock);

        /* increment head, this sample is ready for processing by bottom half */
        INCREMENT_HEAD(brd->isr_avg_samples,LAMS_ISR_SAMPLE_QUEUE_SIZE);

        queue_work(work_queue,&brd->worker);

        if (psamp) {
                /* increment head, this sample is ready for reading */
                INCREMENT_HEAD(brd->peak_samples,LAMS_OUTPUT_SAMPLE_QUEUE_SIZE);

                /* wake up reader */
                wake_up_interruptible(&brd->read_queue);
        }
        return IRQ_HANDLED;
}

static int lams_request_irq(struct LAMS_board* brd)
{
        int result;
        int irq;
        unsigned long flags = 0;

#ifdef GET_SYSTEM_ISA_IRQ
        irq = GET_SYSTEM_ISA_IRQ(irqs[brd->num]);
#else
        irq = irqs[brd->num];
#endif
        KLOG_INFO("board %d: requesting irq: %d,%d\n",brd->num,irqs[brd->num],irq);
        
        /* The LAMS card does not have an interrupt status register,
         * so we cannot find out if an interrupt is for us. Therefore we
         * can't share this interrupt.
         * In initial testing of the driver without an actual LAMS card
         * we registered our handler on the same interrupt line as the
         * PC104SG IRIG card (isa irq=10) which would call our handler at 100 Hz.
         * Reads from the ioports returned -1. For this testing we
         * set IRQF_SHARED in flags, but remove it otherwise.
         */
        // if (irqs[brd->num] == 10) flags |= IRQF_SHARED;

        result = request_irq(irq,lams_irq_handler,flags,driver_name,brd);
        if (result) {
            if (result == -EBUSY) KLOG_ERR("%s cannot share interrupts\n", brd->deviceName);
            return result;
        }
        brd->irq = irq;
        return result;
}

static int lams_remove_irq(struct LAMS_board* brd)
{
        int result = 0;
        KLOG_NOTICE("freeing irq %d\n",brd->irq);
        free_irq(brd->irq,brd);
        brd->irq = 0;
        return result;
}

/************ LAMS File Operations ****************/

static int lams_open(struct inode *inode, struct file *filp)
{
        int ibrd = iminor(inode);

        struct LAMS_board* brd;
        int result = 0;

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        KLOG_DEBUG("open_lams, iminor=%d,numboards=%d\n",ibrd,numboards);

        if (ibrd >= numboards) return -ENXIO;

        brd = boards + ibrd;

        if (atomic_inc_return(&brd->num_opened) > 1) {
                KLOG_ERR("%s is already open!\n", brd->deviceName);
                atomic_dec(&brd->num_opened);
                return -EBUSY; /* already open */
        }

        /*
         * Allocate buffer for samples from the ISR.
         */
        result = alloc_dsm_circ_buf(&brd->isr_avg_samples,
                sizeof(struct lams_avg_sample) - SIZEOF_DSM_SAMPLE_HEADER,
                LAMS_ISR_SAMPLE_QUEUE_SIZE);
        if (result) return result;

        /*
         * Allocate output samples.
         */
        result = alloc_dsm_circ_buf(&brd->avg_samples,
                sizeof(struct lams_avg_sample) - SIZEOF_DSM_SAMPLE_HEADER,
                LAMS_OUTPUT_SAMPLE_QUEUE_SIZE);
        if (result) return result;

        result = alloc_dsm_circ_buf(&brd->peak_samples,
                sizeof(struct lams_peak_sample) - SIZEOF_DSM_SAMPLE_HEADER,
                LAMS_OUTPUT_SAMPLE_QUEUE_SIZE);
        if (result) return result;

        memset(&brd->avg_read_state,0,sizeof(struct sample_read_state));
        memset(&brd->peak_read_state,0,sizeof(struct sample_read_state));

        memset(&brd->status,0,sizeof(brd->status));
        memset(&brd->bh_data,0,sizeof(brd->bh_data));
        brd->nPeaks = 0;

        result = lams_request_irq(brd);

#ifdef ENABLE_IRQ_OFFSET
        // enable IRQs if implemented.  They will remain enabled. There is no disable.
        inw(brd->addr + ENABLE_IRQ_OFFSET);
#endif

        filp->private_data = brd;

        return result;
}

static int lams_release(struct inode *inode, struct file *filp)
{
        struct LAMS_board* brd = (struct LAMS_board*) filp->private_data;

        int ibrd = iminor(inode);

        int result = 0;

        KLOG_DEBUG("lams_release, iminor=%d,numboards=%d\n",ibrd,numboards);

        if (ibrd >= numboards) return -ENXIO;

        BUG_ON(brd != boards + ibrd);

        result = lams_remove_irq(brd);

        flush_workqueue(work_queue);

        free_dsm_circ_buf(&brd->isr_avg_samples);

        free_dsm_circ_buf(&brd->avg_samples);
        free_dsm_circ_buf(&brd->peak_samples);

        atomic_dec(&brd->num_opened);

        return result;
}

static ssize_t lams_read(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct LAMS_board* brd = (struct LAMS_board*) filp->private_data;
        ssize_t l1 = 0, l2 = 0;

        while(brd->avg_read_state.bytesLeft == 0 &&
                !GET_TAIL(brd->avg_samples,brd->avg_samples.size) &&
                 brd->peak_read_state.bytesLeft == 0 &&
                !GET_TAIL(brd->peak_samples,brd->peak_samples.size)) {
            if (filp->f_flags & O_NONBLOCK) return -EAGAIN;
            if (wait_event_interruptible(brd->read_queue,
                    GET_TAIL(brd->avg_samples,brd->avg_samples.size) ||
                    GET_TAIL(brd->peak_samples,brd->peak_samples.size)))
                return -ERESTARTSYS;
        }

        l1 = nidas_circbuf_read_nowait(filp,buf,count,&brd->avg_samples,&brd->avg_read_state);
        count -= l1;
        if (count) {
            buf += l1;
            l2 = nidas_circbuf_read_nowait(filp,buf,count,&brd->peak_samples,&brd->peak_read_state);
        }
        return l1 + l2;
}

/*
 * Implementation of poll fops.
 */
static unsigned int lams_poll(struct file *filp, poll_table *wait)
{
        struct LAMS_board* brd = (struct LAMS_board*) filp->private_data;
        unsigned int mask = 0;
        poll_wait(filp, &brd->read_queue, wait);

        if (sample_remains(&brd->avg_read_state) ||
                GET_TAIL(brd->avg_samples,brd->avg_samples.size))
                         mask |= POLLIN | POLLRDNORM;    /* readable */
        else if (sample_remains(&brd->peak_read_state) ||
                GET_TAIL(brd->peak_samples,brd->peak_samples.size))
                         mask |= POLLIN | POLLRDNORM;    /* readable */
        return mask;
}

static long lams_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
        struct LAMS_board* brd = (struct LAMS_board*) filp->private_data;
        int ibrd = iminor(filp->f_dentry->d_inode);
        unsigned long flags;
        int intval;

        int result = -EINVAL,err = 0;
        void __user *userptr = (void __user *) arg;

        KLOG_DEBUG("lams_ioctl, iminor=%d,numboards=%d\n",
            ibrd,numboards);

         /* don't even decode wrong cmds: better returning
          * ENOTTY than EFAULT */
        if (_IOC_TYPE(cmd) != LAMS_MAGIC)
                return -ENOTTY;
        if (_IOC_NR(cmd) > LAMS_IOC_MAXNR) return -ENOTTY;

        /*
         * the type is a bitmask, and VERIFY_WRITE catches R/W
         * transfers. Note that the type is user-oriented, while
         * verify_area is kernel-oriented, so the concept of "read" and
         * "write" is reversed
         */
        if (_IOC_DIR(cmd) & _IOC_READ)
                err = !access_ok(VERIFY_WRITE, userptr,_IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                err =  !access_ok(VERIFY_READ, userptr, _IOC_SIZE(cmd));
        if (err) return -EFAULT;

        if (ibrd >= numboards) return -ENXIO;

        BUG_ON(brd != boards + ibrd);

        switch (cmd) 
        {
                case LAMS_SET_CHN:  // does nothing in this driver
                        result = 0;
                        break;
                case LAMS_TAS_BELOW:
                        spin_lock_irqsave(&brd->reglock,flags);
                        inw(brd->addr + TAS_BELOW_OFFSET);
                        spin_unlock_irqrestore(&brd->reglock,flags);
                        KLOG_DEBUG("TAS_BELOW\n");
                        result = 0;
                        break;
                case LAMS_TAS_ABOVE:
                        spin_lock_irqsave(&brd->reglock,flags);
                        inw(brd->addr + TAS_ABOVE_OFFSET);
                        spin_unlock_irqrestore(&brd->reglock,flags);
                        KLOG_DEBUG("TAS_ABOVE\n");
                        result = 0;
                        break;
                case LAMS_N_AVG:
                        if (copy_from_user(&intval,userptr, sizeof(intval))) return -EFAULT;
                        spin_lock_irqsave(&brd->reglock,flags);
                        result = setNAvg(brd,intval);
                        spin_unlock_irqrestore(&brd->reglock,flags);
                        KLOG_DEBUG("nAVG:          %d\n", brd->nAVG);
                        break;
                case LAMS_N_PEAKS:
                        if (copy_from_user(&intval,userptr,sizeof(intval))) return -EFAULT;
                        if (intval > 0) {
                            spin_lock_irqsave(&brd->reglock,flags);
                            brd->nPEAKS = intval;
                            spin_unlock_irqrestore(&brd->reglock,flags);
                            result = 0;
                            KLOG_DEBUG("nPEAKS:        %d\n", brd->nPEAKS);
                        }
                        break;
                case LAMS_N_SKIP:
                        if (copy_from_user(&intval,userptr, sizeof(intval))) return -EFAULT;
                        spin_lock_irqsave(&brd->reglock,flags);
                        brd->specPointSkip = intval;
                        spin_unlock_irqrestore(&brd->reglock,flags);
                        result = 0;
                        KLOG_DEBUG("specPointSkip:        %d\n", brd->specPointSkip);
                        break;
                case LAMS_GET_STATUS:
                        if (copy_to_user(userptr,&brd->status,
                            sizeof(brd->status))) return -EFAULT;
                        result = 0;
                        break;
                default:
                        result = -ENOTTY;
                        break;
        }
        return result;
}

static struct file_operations lams_fops = {
        .owner   = THIS_MODULE,
        .read    = lams_read,
        .poll    = lams_poll,
        .open    = lams_open,
        .unlocked_ioctl   = lams_ioctl,
        .release = lams_release,
        .llseek  = no_llseek,
};

/*-----------------------Module ------------------------------*/

/* Don't add __exit macro to the declaration of lams_cleanup,
 * since it is also called at init time, if init fails. */
static void lams_cleanup(void)
{
        int ib;

        if (boards) {

                for (ib = 0; ib < numboards; ib++) {
                        struct LAMS_board* brd = boards + ib;

                        cdev_del(&brd->cdev);

                        if (brd->irq) {
                                KLOG_NOTICE("freeing irq %d\n",brd->irq);
                                free_irq(brd->irq,brd);
                        }

                        if (brd->addr)
                            release_region(brd->addr, IOPORT_REGION_SIZE);
                }
                kfree(boards);
                boards = 0;
        }

        if (MAJOR(lams_device) != 0)
            unregister_chrdev_region(lams_device, numboards);

        if (work_queue) destroy_workqueue(work_queue);

        KLOG_DEBUG("complete\n");

        return;
}

static int __init lams_init(void)
{	
        int result = -EINVAL;
        int ib;

        work_queue = create_singlethread_workqueue(driver_name);

#ifndef SVNREVISION
#define SVNREVISION "unknown"
#endif
        KLOG_NOTICE("version: %s, HZ=%d\n",SVNREVISION,HZ);

        /* count non-zero ioport addresses, gives us the number of boards */
        for (ib = 0; ib < MAX_LAMS_BOARDS; ib++)
            if (ioports[ib] == 0) break;

        numboards = ib;
        if (numboards == 0) {
            KLOG_ERR("No boards configured, all ioports[]==0\n");
            goto err;
        }

        result = alloc_chrdev_region(&lams_device, 0,numboards,driver_name);
        if (result < 0) goto err;
        KLOG_DEBUG("alloc_chrdev_region done, major=%d minor=%d\n",
                MAJOR(lams_device),MINOR(lams_device));

        result = -ENOMEM;
        boards = kmalloc( numboards * sizeof(struct LAMS_board),GFP_KERNEL);
        if (!boards) goto err;
        memset(boards,0,numboards * sizeof(struct LAMS_board));

        for (ib = 0; ib < numboards; ib++) {
                struct LAMS_board* brd = boards + ib;
                dev_t devno;
                unsigned long addr =  (unsigned long)ioports[ib] + SYSTEM_ISA_IOPORT_BASE;
                KLOG_DEBUG("isa base=%x\n",SYSTEM_ISA_IOPORT_BASE);

                brd->num = ib;
                spin_lock_init(&brd->reglock);
                atomic_set(&brd->num_opened,0);

                result = -EBUSY;
                // Get the mapped board address
                if (!request_region(addr, IOPORT_REGION_SIZE,driver_name)) {
                    KLOG_ERR("ioport at 0x%lx already in use\n", addr);
                    goto err;
                }
                brd->addr = addr;

                // save values of oft-used addresses
                brd->ram_clear_addr = addr + RAM_CLEAR_OFFSET;
                brd->avg_lsw_data_addr = addr + AVG_LSW_DATA_OFFSET;
                brd->avg_msw_data_addr = addr + AVG_MSW_DATA_OFFSET;
                brd->peak_data_addr = addr + PEAK_DATA_OFFSET;
                brd->peak_clear_addr = addr + PEAK_CLEAR_OFFSET;

                result = -EINVAL;
                // irqs are requested at open time.
                if (irqs[ib] <= 0) {
                    KLOG_ERR("missing irq value for board #%d at addr 0x%x\n",
                        ib,ioports[ib]);
                    goto err;
                }

                // for informational messages only at this point
                sprintf(brd->deviceName,"/dev/%s%d",driver_name,brd->num);

                // lams device
                cdev_init(&brd->cdev,&lams_fops);
                brd->cdev.owner = THIS_MODULE;

                devno = MKDEV(MAJOR(lams_device),brd->num);
                KLOG_DEBUG("%s: MKDEV, major=%d minor=%d\n",
                    brd->deviceName,MAJOR(devno),MINOR(devno));

                brd->specPointSkip = SPECTRAL_POINTS_TO_SKIP;

#ifdef USE_64BIT_SUMS
                result = setNAvg(brd,80);
#else
                result = setNAvg(brd,64);
#endif
                if (result) goto err;

                // Every nPEAKS interrupts, zero the peak values.
                brd->nPEAKS = 2000;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
                INIT_WORK(&brd->worker,lams_bottom_half);
#else
                INIT_WORK(&brd->worker,lams_bottom_half,&brd->worker);
#endif
                init_waitqueue_head(&brd->read_queue);

                /* After calling cdev_add the device is "live"
                 * and ready for user operation.
                 */
                result = cdev_add(&brd->cdev, devno, 1);
        }

        KLOG_DEBUG("complete.\n");

        return result;
err:
        lams_cleanup();
        return result;
}

module_init(lams_init);
module_exit(lams_cleanup);

