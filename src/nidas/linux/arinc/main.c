/* main.c

   Linux module for interfacing the ISA bus interfaced
   Condor Engineering's CEI-420A-42 ARINC card.

   Original Author: John Wasinger

   Copyright 2008 UCAR, NCAR, All Rights Reserved

   Implementation notes:

      I have tagged areas which need development with a 'TODO' comment.

   Acronyms:

      LPS - Labels  Per Second
      LPB - Labels  Per Buffer
      BPS - Buffers Per Second

   Revisions:

     $LastChangedRevision: 4029 $
         $LastChangedDate: 2007-10-25 09:17:21 -0600 (Thu, 25 Oct 2007) $
           $LastChangedBy: maclean $
                 $HeadURL: http://svn/svn/nidas/trunk/src/nidas/linux/arinc/main.c $
*/

// Linux module includes...
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>         // module_init, module_exit
#include <linux/poll.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <linux/fs.h>           // has to be before <linux/cdev.h>! GRRR! 
//#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <asm/atomic.h>
#include <asm/io.h>             // readb
#include <asm/uaccess.h>        // VERIFY_???

// DSM includes... 
#include <nidas/linux/arinc/arinc.h>
#include <nidas/linux/arinc/CEI420A/Include/utildefs.h>
#include <nidas/linux/types.h>
#include <nidas/linux/isa_bus.h>
#include <nidas/linux/irigclock.h>
#include <nidas/linux/klog.h>
#include <nidas/rtlinux/dsm_version.h>  // provides DSM_VERSION_STRING

MODULE_LICENSE("GPL");
MODULE_AUTHOR("John Wasinger <wasinger@ucar.edu>");
MODULE_DESCRIPTION("CEI420a ISA driver for Linux");

#define ARINC_SAMPLE_QUEUE_SIZE 8
#define BOARD_NUM   0

unsigned int iomem = 0xd0000;

/* module prameters (can be passed in via command line) */
module_param(iomem, uint, 0);
MODULE_PARM_DESC(iomem, "ISA memory base (default 0xd0000)");

/* global variables */

struct arinc_board
{
        /* Set the base address of the ARINC card */
        void* phys_membase;

        unsigned long basemem;

        enum irigClockRates sync_rate;

        struct irig_callback* timeSyncCallback;

        spinlock_t lock;

        atomic_t numRxChannelsOpen;

        int running;

        int display_chn;
};

static struct arinc_board board;

/**
 * Device structure used in the file operations of the character device
 * which provides ARINC samples.
 */
struct arinc_dev
{
        // setup info... 
        char deviceName[64];
        struct dsm_sample_circ_buf samples;     // samples for reading
        struct sample_read_state read_state;
        wait_queue_head_t rwaitq;    // wait queue for user reads
        unsigned int skippedSamples;
        enum irigClockRates poll;
        struct irig_callback *sweepCallback;
        unsigned int speed;
        unsigned int parity;
        int sim_xmit;
        int pollDtMsec;         // number of millisecs between polls

        // run-time info... 
        atomic_t used;
        int nSweeps;
	unsigned int lps_cnt_current;

        dsm_arinc_status status;

        unsigned char rate[0400];
        unsigned char msg_id[0400];
        unsigned char arcfgs[0400];
        unsigned int nArcfg;
};
static struct arinc_dev *chn_info = 0;

/**
 * Info for ARINC user devices
 */
#define DEVNAME_ARINC "arinc"
static dev_t arinc_device = MKDEV(0, 0);
static struct cdev arinc_cdev;

// -- UTILITY --------------------------------------------------------- 
static void error_exit(short board, short err)
{
        if (!err)
                return;

        // display the error message
        KLOG_ERR("\nError while on board %d\n", board);
        KLOG_ERR("  Error reported:  \'%s\'\n", ar_get_error(err));
        KLOG_ERR("  Additional info: \'%s\'\n", ar_get_error(ARS_LAST_ERROR));
}

// -- UTILITY --------------------------------------------------------- 
static short roundUpRate(short rate)
{
        if (rate == 3)
                return 4;
        if (rate == 6)
                return 7;
        if (rate == 12)
                return 13;
        return rate;
}

// -- UTILITY --------------------------------------------------------- 
static short rateToCeiClk(short rate)
{
        if (rate == 3)
                return 320;
        if (rate == 6)
                return 160;
        if (rate == 12)
                return 80;
        return (1000 / rate);
}

// -- UTILITY --------------------------------------------------------- 
static void diag_display(const int chn, const int nData, tt_data_t * data)
{
//   struct arinc_dev *dev = &chn_info[chn]; 
//   int xxx, yyy, zzz, iii, jjj; 
//   char *glyph[] = {"\\","|","/","-"}; 

//   static unsigned int       last_timetag; 
//   static short               sum_sample_length; 
//   static short               tally[0400]; 

//   static int  rx_count=0, sum_of_tally=0; 
//   static int  anim = 0; 

        int iii;
        KLOG_INFO("length %3d\n", nData);
        for (iii = nData - 1; iii < nData; iii++)
                KLOG_INFO("sample[%3d]: %8d %4o %#08X\n", iii,
                           (int) data[iii].time,
                  (unsigned int) data[iii].data & 0xff,
                                 data[iii].data & 0xffffff00);

//   for (iii=0; iii<nData; iii++) 
//     tally[data[iii].data & 0377]++; 
//   sum_sample_length += nData; 

//   /\* move cursor to upper left corner before printing the table *\/ 
//   KLOG_INFO( "%c[H", 27 ); 

//   if (sample->timetag >= last_timetag + 1000) { 
//     last_timetag = sample->timetag; 

//     for (iii=0; iii<dev->nArcfg; iii++) { 
//       jjj = dev->arcfgs[iii]; 

//       if (iii % 8 == 0) KLOG_INFO("\n"); 
//       KLOG_INFO(" %03o %3d/%3d |", jjj, tally[jjj], dev->rate[jjj]); 

//       sum_of_tally += tally[jjj]; 
//       tally[jjj] = 0; 
//     } 

//     rx_count = ar_get_rx_count(BOARD_NUM, chn); 
//     sum_sample_length = 0; 
//     ar_clr_rx_count(BOARD_NUM, chn); 
//   } 
//   KLOG_INFO( "%c[H%c[6B", 27, 27 ); 
//   KLOG_INFO("------------------------------------------------------------------------------------------------------------------------\n"); 
//   KLOG_INFO("%s   chn: %d   poll: %3d Hz   LPS: %4d   sum: %4d   ar_get_rx_count: %4d             \n", 
//              glyph[anim], chn, irigClockEnumToRate(dev->poll), dev->lps, sum_sample_length, rx_count); 
//   xxx = sample->timetag; 
//   yyy = GET_MSEC_CLOCK; 
//   zzz = ar_get_timercntl(BOARD_NUM); 
//   KLOG_INFO("sample->timetag:  %7d                                     \n", xxx); 
//   KLOG_INFO("GET_MSEC_CLOCK:   %7d (%7d)                           \n", yyy, xxx-yyy); 
//   KLOG_INFO("ar_get_timercntl: %7d (%7d)                           \n", zzz, yyy-zzz); 
//   KLOG_INFO("-------------------------------------------------------------------------------------\n"); 
//   if (++anim == 4) anim=0; 
}

/* -- IRIG CALLBACK ---------------------------------------------------
   sync up the i960's internal clock to the IRIG time
   This is called from software interrupt context.
   Be quick, no sleeping, use spinlocks instead of mutexes or semaphores.
*/
static void arinc_timesync(void *junk)
{
//      KLOG_INFO("%6d, %6d\n", GET_MSEC_CLOCK, ar_get_timercntl(BOARD_NUM));
        spin_lock(&board.lock);
        ar_set_timercnt(BOARD_NUM, GET_MSEC_CLOCK);
        // ar_set_timercnt(BOARD_NUM, getSystemTimeMsecs());
        spin_unlock(&board.lock);
}
/* -- IRIG CALLBACK ---------------------------------------------------
   This is called from software interrupt context.
   Be quick, no sleeping, use spinlocks instead of mutexes or semaphores.
*/
static void arinc_sweep(void* channel)
{
        short err;
        int chn = (long) channel;
        struct arinc_dev *dev = &chn_info[chn];
        int nData;
        struct dsm_sample *sample;
        tt_data_t *data;

        sample = GET_HEAD(dev->samples, ARINC_SAMPLE_QUEUE_SIZE);
        if (!sample) {           // no output sample available
                dev->skippedSamples++;
                KLOG_WARNING("%s: skippedSamples=%d\n",
                             dev->deviceName, dev->skippedSamples);
                return;
        }               

        data = (tt_data_t*) sample->data;
        KLOG_DEBUG("%d data:   %x\n", chn, (unsigned int) data);
        // Set the sample block's time tag to an estimate of
        // the timetag of the earliest data in the sweep.
        // We'll use the computed time of the previous sweep.
        //
        // Using the earliest sample time as the time tag
        // of the sweep improves the chances that samples
        // will get sorted correctly later with a minimum
        // of buffering.
        sample->timetag = GET_MSEC_CLOCK;
        // sample->timetag = getSystemTimeMsecs();
        sample->timetag -= dev->pollDtMsec;
        KLOG_DEBUG("%d sample->timetag: %d\n", chn, sample->timetag);

        // read ARINC channel until it's empty or our buffer is full 
        spin_lock(&board.lock);
        err = ar_getwordst(BOARD_NUM, chn, LPB, &nData, data);
        spin_unlock(&board.lock);
        KLOG_DEBUG("%d nData:           %d\n", chn, nData);

        // Can't sync to card.
        if (err == ARS_NOSYNC) {
                dev->status.nosync++;
                return;
        }
        // note possible buffer underflows 
        if (err == ARS_NODATA) {
                dev->status.underflow++;
		return;
        }

        // note the number of received labels per second 
        dev->lps_cnt_current += nData;
        if (++dev->nSweeps == dev->status.pollRate) {
                dev->status.lps_cnt = dev->lps_cnt_current;
                dev->nSweeps = dev->lps_cnt_current = 0;
        }
        // note possible buffer overflows 
        if (nData == LPB)
                dev->status.overflow++;

        // display measurements of the labels as the arrive 
//      if (board.display_chn == chn)
//              diag_display(chn, nData, data);

        sample->length = nData * sizeof(tt_data_t);
        KLOG_DEBUG("%d sample->length:  %d\n", chn, sample->length);
        INCREMENT_HEAD(dev->samples, ARINC_SAMPLE_QUEUE_SIZE);
        wake_up_interruptible(&dev->rwaitq);
}

static int arinc_open(struct inode *inode, struct file *filp)
{
        struct arinc_dev *dev;
        int chn = iminor(inode);
        int err;

        /* Inform kernel that this device is not seekable */
        nonseekable_open(inode,filp);

        if (chn >= N_ARINC_RX) return -ENXIO;

        dev = &chn_info[chn];

        // channel can only be opened once
        if (atomic_inc_return (&dev->used) > 1) {
                KLOG_ERR("chn: %d is already open!\n", chn);
                atomic_dec(&dev->used);
                return -EBUSY; /* already open */
        }
        BUG_ON(atomic_inc_return(&board.numRxChannelsOpen) > N_ARINC_RX);
        // set up the circular buffer
        err = realloc_dsm_circ_buf(&dev->samples,
                                   sizeof(tt_data_t) * LPB,
                                   ARINC_SAMPLE_QUEUE_SIZE);
        if (err) return err;
        KLOG_DEBUG("realloc_dsm_circ_buf(%x,%d,%d)\n", (unsigned int) &dev->samples,
                                   sizeof(tt_data_t) * LPB,
                                   ARINC_SAMPLE_QUEUE_SIZE);

        // reset stuff in dev struct.
        dev->samples.head = dev->samples.tail = 0;
        memset(&dev->read_state,0,
            sizeof(struct sample_read_state));
	memset(&dev->status,0,sizeof(dsm_arinc_status));
        memset(dev->rate,0,sizeof(dev->rate));
        // don't think it is necessary to zero msg_id and arcfgs
        // memset(dev->msg_id,0,sizeof(dev->msg_id));
        // memset(dev->arcfgs,0,sizeof(dev->arcfgs));
        dev->nArcfg = 0;
        dev->nSweeps = 0;
        dev->sim_xmit = 0;
        dev->skippedSamples = 0;
        dev->lps_cnt_current = 0;

        filp->private_data = dev;
        return 0;
}

static int arinc_ioctl(struct inode *inode, struct file *filp,
                       unsigned int cmd, unsigned long arg)
{
        struct arinc_dev *dev = (struct arinc_dev *) filp->private_data;
        int ret;
        void __user *userptr = (void __user *) arg;

        int chn = iminor(inode);

        int err;
        int pollRate;

	// from linux/irigclock, the supported poll rates are
	//  1,2,4,5,10,20,25,50,100
	int pollRates[] = {2,4,10,20,50,100};	// we'll use these
	int nRates = (signed)(sizeof(pollRates)/sizeof(pollRates[0]));

        arcfg_t arcfg;
        archn_t archn;
        short aBIT;
	int i;

        // don't decode wrong cmds: better returning
        // ENOTTY than EFAULT
        if (chn >= N_ARINC_RX)               return -ENXIO;
        if (_IOC_TYPE(cmd) != ARINC_MAGIC)  return -ENOTTY;
        if (_IOC_NR(cmd) > ARINC_IOC_MAXNR) return -ENOTTY;

        // Verify read or write access to the user arg, if necessary
        if (_IOC_DIR(cmd) & _IOC_READ)
                ret = !access_ok(VERIFY_WRITE, userptr,_IOC_SIZE(cmd));
        else if (_IOC_DIR(cmd) & _IOC_WRITE)
                ret =  !access_ok(VERIFY_READ, userptr, _IOC_SIZE(cmd));
        else ret = 0;
        if (ret) return -EFAULT;

        switch (cmd) {

        case ARINC_SET:

                // unfilter a label for this channel 
                if (copy_from_user(&arcfg, userptr, sizeof(arcfg_t))) {
                        KLOG_ERR("copy_from_user error!\n");
                        return -EFAULT;
                }

                // store the rate for this channel's label 
                if (dev->rate[arcfg.label]) {
                        KLOG_ERR("duplicate label: %04o\n", arcfg.label);
                        return -EINVAL;
                }
                dev->rate[arcfg.label] = arcfg.rate;

                // measure the total labels per second for the given channel 
                dev->status.lps += roundUpRate(arcfg.rate);

                spin_lock_bh(&board.lock);

                // stop the board 
                if (board.running) {
                        err = ar_reset(BOARD_NUM);
                        if (err != ARS_NORMAL)
                                goto ar_fail_unlock;
                        board.running = 0;
                }
                // define a periodic message for the i960 to generate 
                if (dev->sim_xmit) {
                        err =
                            ar_define_msg(BOARD_NUM, chn,
                                          rateToCeiClk(arcfg.rate),
                                          0, (long) arcfg.label);
                        if (err > 120)
                                goto ar_fail_unlock;
                        dev->msg_id[arcfg.label] =
                            (unsigned char) err;
                }

                dev->arcfgs[dev->nArcfg++] = arcfg.label;

                // un-filter this label on this channel 
                err =
                    ar_label_filter(BOARD_NUM, chn, arcfg.label,
                                    ARU_FILTER_OFF);
                if (err != ARS_NORMAL)
                        goto ar_fail_unlock;

                spin_unlock_bh(&board.lock);

                KLOG_DEBUG("recv: %04o  rate: %2d Hz\n", arcfg.label, roundUpRate(arcfg.rate));
                break;

        case ARINC_OPEN:

                // store the speed and parity for this channel 
                if (copy_from_user(&archn, userptr, sizeof(archn))) {
                        KLOG_ERR("copy_from_user error!\n");
                        return -EFAULT;
                }
                if (archn.speed != AR_HIGH && archn.speed != AR_LOW) {
                        KLOG_ERR("invalid speed!\n");
                        return -EINVAL;
                }
                if (archn.parity != AR_ODD && archn.parity != AR_EVEN) {
                        KLOG_ERR("invalid parity!\n");
                        return -EINVAL;
                }
                dev->speed = archn.speed;
                dev->parity = archn.parity;

                spin_lock_bh(&board.lock);

                // stop the board 
                if (board.running) {
                        err = ar_reset(BOARD_NUM);
                        if (err != ARS_NORMAL) 
                                goto ar_fail_unlock;
                        board.running = 0;
                }
                // set channel speed 
                err =
                    ar_set_config(BOARD_NUM,
                                  ARU_RX_CH01_BIT_RATE + chn,
                                  dev->speed);
                if (err != ARS_NORMAL)
                        goto ar_fail_unlock;
                if (ar_get_config (BOARD_NUM,
                     ARU_RX_CH01_BIT_RATE + chn) != dev->speed) {
                        spin_unlock_bh(&board.lock);
                        KLOG_ERR("un-settable speed!\n");
                        return -EINVAL;
                }
                // set channel parity 
                err =
                    ar_set_config(BOARD_NUM,
                                  ARU_RX_CH01_PARITY + chn,
                                  dev->parity);
                if (err != ARS_NORMAL)
                        goto ar_fail_unlock;
                if (ar_get_config (BOARD_NUM,
                     ARU_RX_CH01_PARITY + chn) != dev->parity) {
                        spin_unlock_bh(&board.lock);
                        KLOG_ERR("un-settable parity!\n");
                        return -EINVAL;
                }
                if (dev->status.lps == 0) {
                        KLOG_ERR("sequence out of order: use ARINC_SET first/n");
                        spin_unlock_bh(&board.lock);
                        return -EINVAL;
                }

                // determine the poll rate, assuming we only want the
		// buffers to get 1/2 full
                pollRate = dev->status.lps / (LPB / 2);
		for (i = 0; i < nRates; i++) {
			if (pollRate < pollRates[i]) {
				pollRate = pollRates[i];
				break;
			}
		}
		if (i == nRates) pollRate = pollRates[i-1];
		
		dev->status.pollRate = pollRate;

                if ( (dev->poll = irigClockRateToEnum(pollRate)) == IRIG_NUM_RATES) {
                        spin_unlock_bh(&board.lock);
                        KLOG_ERR("invalid poll rate: %d Hz\n", pollRate);
                        return -EINVAL;
                }
                dev->pollDtMsec = MSECS_PER_SEC / pollRate;

                // register a sweeping routine for this channel
                dev->sweepCallback =
                    register_irig_callback(arinc_sweep, dev->poll, (void *)(long)chn, &err);
                if (err) {
                        spin_unlock_bh(&board.lock);
                        KLOG_ERR("%s: Error registering callback\n",
                                 dev->deviceName);
                        return err;
                }

                // launch the board 
                err = ar_go(BOARD_NUM);
                if (err != ARS_NORMAL)
                        goto ar_fail_unlock;

                board.running = 1;
                spin_unlock_bh(&board.lock);
                break;

        case ARINC_CLOSE:
                break;

        case ARINC_SIM_XMIT:
                // define a periodic messages for the i960 to generate 
                if (chn > N_ARINC_TX - 1) {
                        KLOG_ERR("there are only 2 xmit channels!/n");
                        return -EINVAL;
                }
                dev->sim_xmit = 1;
                break;

        case ARINC_BIT:

                if (dev->status.lps) {
                        KLOG_ERR("cannot run buit in test, already configured!\n");
                        return -EALREADY;
                }
                // perform a series of Built In Tests on the card
                if (copy_from_user(&aBIT, userptr, sizeof(aBIT))) {
                        KLOG_ERR("copy_from_user error!\n");
                        return -EFAULT;
                }

                spin_lock_bh(&board.lock);

                err = ar_execute_bit(BOARD_NUM, aBIT);
                if (err != ARS_NORMAL)
                        goto ar_fail_unlock;

                spin_unlock_bh(&board.lock);
                break;

        case ARINC_STAT:
                // stop displaying measurements 
                board.display_chn = -1;

                if (copy_to_user(userptr, &dev->status, sizeof (dsm_arinc_status))) {
                        KLOG_ERR("copy_to_user error!\n");
                        return -EFAULT;
                }
                break;

        case ARINC_MEASURE:

                // toggle or change the channel that we are displaying 
                if (board.display_chn == chn)
                        board.display_chn = -1;
                else
                        board.display_chn = chn;

                // move cursor to upper left corner and clear the screen 
//              KLOG_INFO("%c[2J%c[H", 27, 27 );
                break;

        default:
                KLOG_ERR("unrecognized ioctl %d (number %d, size %d)\n",
                     cmd, _IOC_NR(cmd), _IOC_SIZE(cmd));
                ret = -EINVAL;
                break;
        }
        return ret;

      ar_fail_unlock:
        spin_unlock_bh(&board.lock);
        if (err < 0) return err;
        error_exit(BOARD_NUM, err);
        return -EIO;
}

/*
 * Implementation of poll fops.
 */
static unsigned int arinc_poll(struct file *filp, poll_table * wait)
{
        struct arinc_dev *dev = (struct arinc_dev *) filp->private_data;
        unsigned int mask = 0;

        poll_wait(filp, &dev->rwaitq, wait);

        if (sample_remains(&dev->read_state) ||
            dev->samples.head != dev->samples.tail)
                mask |= POLLIN | POLLRDNORM;    /* readable */
        return mask;
}

/*
 * Implementation of read fops.
 */
static ssize_t arinc_read(struct file *filp, char __user * buf,
                          size_t count, loff_t * pos)
{
        struct arinc_dev *dev = (struct arinc_dev *) filp->private_data;

        return nidas_circbuf_read(filp, buf, count,
                                  &dev->samples, &dev->read_state,
                                  &dev->rwaitq);
}

/*
 * Implemention of release (close) fops.
 */
static int arinc_release(struct inode *inode, struct file *filp)
{
        struct arinc_dev *dev = (struct arinc_dev *) filp->private_data;
        int err;
        int chn = iminor(inode);

        // unregister poll recv routine with the IRIG driver 
        if (dev->sweepCallback &&
                unregister_irig_callback(dev->sweepCallback) == 0)
                        flush_irig_callbacks();
        dev->sweepCallback = 0;
        flush_irig_callbacks();

        spin_lock_bh(&board.lock);

        // filter out all labels on this channel 
        err =
            ar_label_filter(BOARD_NUM, chn, ARU_ALL_LABELS,
                            ARU_FILTER_ON);
        if (err != ARS_NORMAL) {
                spin_unlock_bh(&board.lock);
                if (err < 0) return err;
                error_exit(BOARD_NUM, err);
                return -EIO;
        }

        // stop the board 
        if (atomic_dec_and_test(&board.numRxChannelsOpen) && board.running) {
                err = ar_reset(BOARD_NUM);
                if (err != ARS_NORMAL) {
                        spin_unlock_bh(&board.lock);
                        if (err < 0) return err;
                        error_exit(BOARD_NUM, err);
                        return -EIO;
                }
                board.running = 0;
        }
        spin_unlock_bh(&board.lock);

        if (dev->samples.buf)
                free_dsm_circ_buf(&dev->samples);
        dev->samples.buf = 0;

	atomic_dec(&dev->used);
        return 0;
}

static struct file_operations arinc_fops = {
    .owner   = THIS_MODULE,
    .open    = arinc_open,
    .ioctl   = arinc_ioctl,
    .poll    = arinc_poll,
    .read    = arinc_read,
    .release = arinc_release,
    .llseek  = no_llseek,
};

// -- MODULE ---------------------------------------------------------- 
/* Don't add __exit macro to the declaration of this cleanup function
 * since it is also called at init time, if init fails. */
static void arinc_cleanup(void)
{
        short err;
        int chn;
        struct arinc_dev *dev;

        // unregister the channel sweeping routine(s)
        for (chn = 0; chn_info && (chn < N_ARINC_RX); chn++) {
                dev = &chn_info[chn]; 
                if (dev->sweepCallback &&
                        unregister_irig_callback(dev->sweepCallback) == 0)
                                flush_irig_callbacks();
                dev->sweepCallback = 0;
                if (dev->samples.buf)
                        free_dsm_circ_buf(&dev->samples);
                dev->samples.buf = 0;
        }
        // unregister a timesync routine 
        if (board.timeSyncCallback)
                unregister_irig_callback(board.timeSyncCallback);

        // remove device
        cdev_del(&arinc_cdev);
        if (MAJOR(arinc_device) != 0)
                unregister_chrdev_region(arinc_device, N_ARINC_RX);

        // free up the ISA memory region 
        if (board.basemem)
                release_region(board.basemem, PAGE_SIZE);

        // free up the chn_info
        if (chn_info)
                kfree(chn_info);

        // close the board 
        err = ar_close(BOARD_NUM);
        if (err != ARS_NORMAL)
                error_exit(BOARD_NUM, err);

        // unmap the DPRAM address 
        if (board.phys_membase)
                iounmap(board.phys_membase);
}

// -- UTILITY --------------------------------------------------------- 
///
// There are not a lot of tests to perform to see if it is a
// CEI 420 card. The tests performed are:
//   - configuration register 1 : looking at bits 4-7, bit 6 is set
//                                (CEI-420-XXJ) or bit 4 and 5 are
//                                set (CEI-420-70J)
//   - configuration register 2 : value has to be 0x0
//   - configuration register 3 : value of bits 4-7 has to be 0x0
///
static int scan_ceiisa(void)
{
        unsigned char value;
        unsigned int indx;

#ifdef DEBUG
        char *boardID[] = { "Standard CEI-220",
                "Standard CEI-420",
                "Custom CEI-220 6-Wire",
                "CEI-420-70J",
                "CEI-420-XXJ",
                "Obsolete",
                "CEI-420A-42-A",
                "CEI-420A-XXJ"
        };
#endif

        value = readb(board.phys_membase + 0x808);
        value >>= 4;
        if (value != 0x0) {
                // passed register 1 test... 
                value = readb(board.phys_membase + 0x80A);
                value >>= 4;
                if (value == 0x5)
                        KLOG_ERR("Obsolete CEI-420a\n");
                if (value == 0x0 || value == 0x1 || value == 0x2 ||
                    value == 0x3 || value == 0x4 || value == 0x6
                    || value == 0x7) {
                        // passed register 2 test... 
                        indx = value;
                        KLOG_DEBUG("cei220/420 found.  Board = %s\n",
                                   boardID[indx]);
                        value = readb(board.phys_membase + 0x80C);
                        if (value == 0x0) {
                                // passed register 3 test... 
                                value = readb(board.phys_membase + 0x80E);
                                value >>= 4;
                                if (value == 0) {
                                        // passed register 4 test... 
                                        KLOG_DEBUG
                                            ("found CEI-220/420 at 0x%lX\n",
                                             board.basemem);
                                        return 0;
                                }
                        }
                }
        }
        KLOG_ERR("No CEI420 ARINC board found!\n");
        return -ENODEV;
}

// -- MODULE ---------------------------------------------------------- 
static int __init arinc_init(void)
{
        int chn;
        int err;
        char api_version[150];
        struct arinc_dev *dev;

        KLOG_NOTICE("version: %s\n", DSM_VERSION_STRING);
        KLOG_NOTICE("compiled on %s at %s\n", __DATE__, __TIME__);

        memset(&board,0,sizeof(board));
        board.sync_rate = IRIG_1_HZ;
        board.display_chn = -1;
        spin_lock_init(&board.lock);
        atomic_set(&board.numRxChannelsOpen,0);

        // map ISA card memory into kernel memory 
        board.basemem = SYSTEM_ISA_IOMEM_BASE + iomem;
        board.phys_membase = ioremap(board.basemem, PAGE_SIZE);
        if (!board.phys_membase) {
                KLOG_ERR("ioremap failed.\n");
                return -EIO;
        }
        // reserve the ISA memory region 
        if (!request_region(board.basemem, PAGE_SIZE, "arinc")) {
                KLOG_ERR("couldn't allocate I/O range %lX - %lX\n",
                           board.basemem, board.basemem + PAGE_SIZE - 1);
                err = -EBUSY;
                goto fail;
        }
        KLOG_DEBUG("basemem:      %X\n", (unsigned int) board.basemem);
        KLOG_DEBUG("phys_membase: %X\n", (unsigned int) board.phys_membase);

        // scan the ISA bus for the device 
        err = scan_ceiisa();
        if (err) goto fail;

        // obtain the API version string
        ar_version(api_version);
        KLOG_DEBUG("API Version %s\n", api_version);

        // load the board (the size and address are not used - must specify zero) 
        err = ar_loadslv(BOARD_NUM,(unsigned long)board.phys_membase, 0, 0);
        if (err != ARS_NORMAL) goto fail;

        // initialize the slave 
        err = ar_init_slave(BOARD_NUM);
        if (err != ARS_NORMAL) goto fail;

        // Display the board type 
        KLOG_DEBUG("Board %s detected\n", ar_get_boardname(BOARD_NUM, NULL));
        KLOG_DEBUG("Supporting %d transmitters and %d receivers.\n",
                   ar_num_xchans(BOARD_NUM), ar_num_rchans(BOARD_NUM));

        // select buffered mode 
        err = ar_set_storage_mode(BOARD_NUM, ARU_BUFFERED);
        if (err != ARS_NORMAL) goto fail;

        // adjust the i960's internal clock rate 
        err = ar_set_timerrate(BOARD_NUM, 4 * 1000);
        if (err != ARS_NORMAL) goto fail;

        // select high speed transmit 
        err = ar_set_config(BOARD_NUM, ARU_XMIT_RATE, AR_HIGH);
        if (err != ARS_NORMAL) goto fail;

        // enable scheduled mode (clears the buffer of scheduled messages) 
        err = ar_msg_control(BOARD_NUM, AR_ON);
        if (err != ARS_NORMAL) goto fail;

        // disable internal wrap
        err = ar_set_config(BOARD_NUM, ARU_INTERNAL_WRAP, AR_WRAP_OFF);
        if (err != ARS_NORMAL) goto fail;

        // instruct the board to time tag each label 
        err = ar_timetag_control(BOARD_NUM, ARU_ENABLE_TIMETAG);
        if (err != ARS_NORMAL) goto fail;

        // prematurely launch the board (it will be reset and re-launched via ioctls) 
        err = ar_go(BOARD_NUM);
        if (err != ARS_NORMAL) goto fail;

        // sync up the i960's internal clock to the IRIG time 
        ar_set_timercnt(BOARD_NUM, GET_MSEC_CLOCK);
        // ar_set_timercnt(BOARD_NUM, getSystemTimeMsecs());

        // register a timesync routine 
        board.timeSyncCallback = register_irig_callback(arinc_timesync, board.sync_rate, (void *) 0, &err);
        if (!board.timeSyncCallback) goto fail;

        // Initialize and add user-visible devices
        err = alloc_chrdev_region(&arinc_device, 0, N_ARINC_RX, "arinc");
        if (err < 0) goto fail;
        KLOG_DEBUG("major device number %d\n", MAJOR(arinc_device));

        // reserve and clear kernel memory for chn_info
        err = -ENOMEM;
        chn_info =
            kmalloc(N_ARINC_RX * sizeof(struct arinc_dev), GFP_KERNEL);
        if (!chn_info) goto fail;
        memset(chn_info, 0, N_ARINC_RX * sizeof(struct arinc_dev));

        // initialize each channel structure
        for (chn = 0; chn < N_ARINC_RX; chn++) {
                dev = &chn_info[chn];
                atomic_set(&dev->used,0);
                init_waitqueue_head(&dev->rwaitq);
                sprintf(dev->deviceName, "/dev/arinc%d", chn);
        }
        cdev_init(&arinc_cdev, &arinc_fops);

        // after calling cdev_add the devices are live and ready for user operations.
        err = cdev_add(&arinc_cdev, arinc_device, N_ARINC_RX);
        if (err < 0) goto fail;

        KLOG_DEBUG("ARINC init_module complete.\n");
        return 0;               // success 

      fail:
        arinc_cleanup();
        if (err < 0)
                return err;

        // ar_???() error codes are positive... 
        error_exit(BOARD_NUM, err);
        return -EIO;
}

module_init(arinc_init);
module_exit(arinc_cleanup);
