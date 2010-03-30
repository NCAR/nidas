/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedRevision$
        $LastChangedDate$
          $LastChangedBy$
                $HeadURL$

    LAMS driver for the ADS3 DSM.

 ********************************************************************
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
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>

#include "lamsx.h"
#include <nidas/rtlinux/dsm_version.h>
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

/* ISA irqs, required for each board. Can be shared. */
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

#define IOPORT_REGION_SIZE 16  // number of 1-byte registers

#define LAMS_ISR_SAMPLE_QUEUE_SIZE 64
#define LAMS_OUTPUT_SAMPLE_QUEUE_SIZE 16

/*
 * Holds the major number of all LAMS devices.
 */
static dev_t lams_device = MKDEV(0,0);

struct bh_data {
        unsigned int nAvg;
        long long sum[SIZE_LAMS_BUFFER];
};

struct LAMS_board {
        int num;
        unsigned long addr;
        int irq;
        spinlock_t reglock;             // lock when accessing board registers
                                        // to avoid messing up the board.

        char deviceName[32];

        struct cdev cdev;

        atomic_t num_opened;                     // number of times opened

        unsigned int nAVG;
        unsigned int nPEAKS;
        unsigned int nPeaks;

        struct work_struct worker;

        struct bh_data bh_data;       // data for use by bottom half

        struct dsm_sample_circ_buf isr_samples;     // samples for bottom half
        struct dsm_sample_circ_buf samples;         // samples out of b.h.

        wait_queue_head_t read_queue;   // user read & poll methods wait on this
        struct sample_read_state read_state;

        struct lams_status status;
};


/*
 * Pointer to first of dynamically allocated structures containing
 * all data pertaining to the configured LAMS boards on the system.
 */
static struct LAMS_board* boards = 0;

static struct workqueue_struct* work_queue = 0;

/**
 * Tasklet that invokes filters on the data in a fifo sample.
 * The contents of the fifo sample must be a multiple
 * of the number of channels scanned.
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


        KLOG_DEBUG("%s: worker entry, fifo head=%d,tail=%d\n",
            brd->deviceName,brd->isr_samples.head,brd->isr_samples.tail);

        /* ISR is incrementing head, we're incrementing the tail */
        while (brd->isr_samples.head != brd->isr_samples.tail) {
                struct lams_sample* insamp =
                    (struct lams_sample*) brd->isr_samples.buf[brd->isr_samples.tail];

                for (i = 0; i < SIZE_LAMS_BUFFER; i++)
                        bhd->sum[i] += insamp->data[i];

                if (++bhd->nAvg >= brd->nAVG) {

                        struct lams_sample* samp;
                        samp = (struct lams_sample*) GET_HEAD(brd->samples,LAMS_OUTPUT_SAMPLE_QUEUE_SIZE);
                        if (!samp) {                // no output sample available
                            brd->status.missedOutSamples++;
                            INCREMENT_TAIL(brd->isr_samples,LAMS_ISR_SAMPLE_QUEUE_SIZE);
                            continue;
                        }
                        else {
                                for (i = 0; i < SIZE_LAMS_BUFFER; i++) {
                                        samp->data[i] = bhd->sum[i] / brd->nAVG;
                                        samp->peak[i] = insamp->peak[i];
                                }
                                samp->timetag = insamp->timetag;
                                samp->length = insamp->length;
                                samp->type = 0;

                                INCREMENT_TAIL(brd->isr_samples,LAMS_ISR_SAMPLE_QUEUE_SIZE);

                                /* increment head, this sample is ready for consumption */
                                INCREMENT_HEAD(brd->samples,LAMS_OUTPUT_SAMPLE_QUEUE_SIZE);

                                /* wake up reader */
                                wake_up_interruptible(&brd->read_queue);

                                memset((void *)bhd->sum, 0, sizeof(bhd->sum));
                                bhd->nAvg = 0;
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
        struct lams_sample* samp;
        int i,ip,id;

        spin_lock(&brd->reglock);

        samp = (struct lams_sample*) GET_HEAD(brd->isr_samples,LAMS_ISR_SAMPLE_QUEUE_SIZE);

        if (!samp) {                // no output sample available
            brd->status.missedISRSamples++;
            /*
            KLOG_WARNING("%s: missedISRSamples=%d\n",
                brd->deviceName,brd->status.missedISRSamples);
            */
            //
            // Clear Dual Port memory address counter
            inw(brd->addr + RAM_CLEAR_OFFSET);
            for (i = 0; i < SIZE_LAMS_BUFFER + 4; i++) {
                    inw(brd->addr + AVG_LSW_DATA_OFFSET);
                    inw(brd->addr + AVG_MSW_DATA_OFFSET);
                    inw(brd->addr + PEAK_DATA_OFFSET);
            }
            if (++brd->nPeaks >= brd->nPEAKS) {
                    inw(brd->addr + PEAK_CLEAR_OFFSET);
                    brd->nPeaks = 0;
            }

            spin_unlock(&brd->reglock);
            return IRQ_HANDLED;
        }

        samp->timetag = getSystemTimeTMsecs();
        samp->length = sizeof(struct lams_sample) - SIZEOF_DSM_SAMPLE_HEADER,
        
        // Clear Dual Port memory address counter
        inw(brd->addr + RAM_CLEAR_OFFSET);

        id = 1;
        ip = 0;
        for (i = 0; i < SIZE_LAMS_BUFFER + 4; i++) {
                unsigned short lsw = inw(brd->addr + AVG_LSW_DATA_OFFSET);
                unsigned short msw = inw(brd->addr + AVG_MSW_DATA_OFFSET);
                unsigned short apk = inw(brd->addr + PEAK_DATA_OFFSET);

                // why this index offset?
                if(i >= 4) samp->peak[ip++] = apk;
                  
                // why this index offset?
                if(i >= 5) samp->data[id++] = ((unsigned int)msw << 16) + lsw;
        }
        if (++brd->nPeaks >= brd->nPEAKS) {
                inw(brd->addr + PEAK_CLEAR_OFFSET);
                brd->nPeaks = 0;
        }
        spin_unlock(&brd->reglock);

        samp->data[0] = samp->data[1];
        BUG_ON(id != SIZE_LAMS_BUFFER);
        BUG_ON(ip != SIZE_LAMS_BUFFER);

        /* increment head, this sample is ready for consumption */
        INCREMENT_HEAD(brd->isr_samples,LAMS_ISR_SAMPLE_QUEUE_SIZE);

        queue_work(work_queue,&brd->worker);
        return IRQ_HANDLED;
}

static int lams_request_irq(struct LAMS_board* brd)
{
        int result;
        int irq;

#ifdef GET_SYSTEM_ISA_IRQ
        irq = GET_SYSTEM_ISA_IRQ(irqs[brd->num]);
#else
        irq = irqs[brd->num];
#endif
        KLOG_INFO("board %d: requesting irq: %d,%d\n",brd->num,irqs[brd->num],irq);
        result = request_irq(irq,lams_irq_handler,IRQF_SHARED,driver_name,brd);
        if (result) return result;
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

        if (atomic_inc_return(&brd->num_opened) >= 1) {
                KLOG_ERR("%s is already open!\n", brd->deviceName);
                atomic_dec(&brd->num_opened);
                return -EBUSY; /* already open */
        }

        /*
         * Allocate buffer for samples from the ISR.
         */
        result = alloc_dsm_disc_circ_buf(&brd->isr_samples,
                sizeof(struct lams_sample) - SIZEOF_DSM_SAMPLE_HEADER,
                LAMS_ISR_SAMPLE_QUEUE_SIZE);
        if (result) return result;

        /*
         * Allocate output samples.
         */
        result = alloc_dsm_disc_circ_buf(&brd->samples,
                sizeof(struct lams_sample) - SIZEOF_DSM_SAMPLE_HEADER,
                LAMS_OUTPUT_SAMPLE_QUEUE_SIZE);
        if (result) return result;

        memset(&brd->read_state,0,sizeof(struct sample_read_state));
        memset(&brd->status,0,sizeof(brd->status));

        result = lams_request_irq(brd);

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

        free_dsm_disc_circ_buf(&brd->isr_samples);

        free_dsm_disc_circ_buf(&brd->samples);

        atomic_dec(&brd->num_opened);

        return result;
}

static ssize_t lams_read(struct file *filp, char __user *buf,
    size_t count,loff_t *f_pos)
{
        struct LAMS_board* brd = (struct LAMS_board*) filp->private_data;
        return nidas_circbuf_read(filp,buf,count,&brd->samples,&brd->read_state,
                &brd->read_queue);
}

static int lams_ioctl(struct inode *inode, struct file *filp,
              unsigned int cmd, unsigned long arg)
{
        struct LAMS_board* brd = (struct LAMS_board*) filp->private_data;
        int ibrd = iminor(inode);
        unsigned long flags;

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
                        if (copy_from_user(&brd->nAVG,userptr,
                                sizeof(brd->nAVG))) return -EFAULT;
                        result = 0;
                        KLOG_DEBUG("nAVG:          %d\n", brd->nAVG);
                        break;
                case LAMS_N_PEAKS:
                        if (copy_from_user(&brd->nPEAKS,userptr,
                                sizeof(brd->nPEAKS))) return -EFAULT;
                        result = 0;
                        KLOG_DEBUG("nPEAKS:        %d\n", brd->nPEAKS);
                        break;
                case LAMS_GET_STATUS:
                        if (copy_to_user(userptr,&brd->status,
                            sizeof(struct lams_status))) return -EFAULT;
                        result = 0;
                        break;
                default:
                        result = -ENOTTY;
                        break;
        }
        return result;
}

/*
 * Implementation of poll fops.
 */
static unsigned int lams_poll(struct file *filp, poll_table *wait)
{
        struct LAMS_board* brd = (struct LAMS_board*) filp->private_data;
        unsigned int mask = 0;
        poll_wait(filp, &brd->read_queue, wait);

        if (sample_remains(&brd->read_state) ||
                brd->samples.head != brd->samples.tail)
                         mask |= POLLIN | POLLRDNORM;    /* readable */
        return mask;
}

static struct file_operations lams_fops = {
        .owner   = THIS_MODULE,
        .read    = lams_read,
        .poll    = lams_poll,
        .open    = lams_open,
        .ioctl   = lams_ioctl,
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

                        free_dsm_disc_circ_buf(&brd->isr_samples);

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

        boards = 0;

        work_queue = create_singlethread_workqueue(driver_name);

        // DSM_VERSION_STRING is found in dsm_version.h
        KLOG_NOTICE("version: %s, HZ=%d\n",DSM_VERSION_STRING,HZ);

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
                return result;
        }

        KLOG_DEBUG("complete.\n");

        return 0;
err:
        lams_cleanup();
        return result;
}

module_init(lams_init);
module_exit(lams_cleanup);

