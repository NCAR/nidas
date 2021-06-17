/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8; -*- */
/* vim: set shiftwidth=8 softtabstop=8 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/* 
   Utility functions for nidas device drivers.

   Original Author: Gordon Maclean

*/

#ifndef NIDAS_LINUX_NIDAS_UTIL_H
#define NIDAS_LINUX_NIDAS_UTIL_H

#if defined(__KERNEL__)

#include "types.h"
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/fs.h>

#include <linux/circ_buf.h>
#include <linux/ktime.h>
#include <linux/version.h>

/**
 * General utility functions and macros for NIDAS drivers.
 *
 * This file defines a simple circular buffer of data samples, and
 * macros for manipulating circular buffers, in addition to
 * the macros in linux/circ_buf.h.  These circular buffers are used
 * to pass data between sample producers and consumers, without using
 * locks. To make sure things are coherent, memory barriers are used.
 *
 * For information on using circular buffers in kernel code, refer to
 * the Documentation directory of the linux kernel.
 * The RPM kernel-doc places the documentation in
 * /usr/share/doc/kernel-doc-x.y.z/Documentation, where x.y.z is the
 * kernel version of your system.
 *
 * These doc files have useful information on circular buffers:
 *    core-api/circular-buffers.rst
 *    memory-barriers.txt (after a strong cup of coffee)
 *    process/volatile-considered-harmful.txt 
 *
 * If anyone masters a real firm understanding of all this, especially
 * memory-barriers.txt, hats off to them!
 *
 * Access to a circular buffer using these macros should performed
 * only by code whose threads do not combine the following roles:
 *   a producer puts objects into the buffer, only incrementing the head pointer
 *   a consumer reads objects from the buffer, only incrementing the tail pointer
 * One consumer and one producer can run concurrently.
 */

/* 
 * https://elixir.bootlin.com/linux/latest/source is a useful site for searching
 * through kernel versions for an indentifier.
 *
 * Vulcans are running 2.6.21 kernel, ACCESS_ONCE and READ_ONCE are not defined
 * Vipers/titans: 3.16.0, ACCESS_ONCE is defined, but READ_ONCE is not
 * 3.18.13: first appearance of READ_ONCE
 */
#ifndef READ_ONCE
#ifdef ACCESS_ONCE
#define READ_ONCE(x) ACCESS_ONCE(x)
#else
/* def of ACCESS_ONCE in include/linux/compiler.h in 3.16 */
#define READ_ONCE(x) (*(volatile typeof(x) *)&(x))
#endif
#endif

#ifndef WRITE_ONCE
#ifdef ACCESS_ONCE
#define WRITE_ONCE(x,val) ACCESS_ONCE(x) = (val)
#else
#define WRITE_ONCE(x,val) (*(volatile typeof(x) *)&(x)) = (val)
#endif
#endif

/**
 * A circular buffer of time-tagged data samples.
 *
 * There is a temptation to declare head and tail volatile, to prevent
 * the compiler from temporarily caching the values, so that
 * changes to head or tail are known immediately to all threads.
 * Read volatile-considered-harmful.txt in the kernel documentation.
 *
 * Instead we use READ_ONCE or similar macros
 * when accessing the "opposition" index (head pointer in the consumer,
 * tail pointer in a producer), and memory barriers to ensure
 * memory stores have completed.
 * READ_ONCE ironically casts the argument to (volatile *) :-).
 */
struct dsm_sample_circ_buf {
        struct dsm_sample **buf;
        int head;
        int tail;
        int size;
        void** pages;
        int npages;
};

/**
 * GET_HEAD is used by a producer, and accesses the head index twice.
 * The idea is that space in the buffer will only stay the same or
 * get bigger between the CIRC_SPACE check and the return of buf[head]
 * element, because only the calling thread should be changing head.
 * If a separate consumer thread is messing with tail, CIRC_SPACE
 * will only stay the same or get bigger, not smaller, and
 * and the head element will not become invalid.
 * The producer should only be writing into the head element,
 * not reading it, so you don't need a barrier between the check of
 * CIRC_SPACE and the return of the pointer to the element.
 * The write barrier is in INCREMENT_HEAD.
 */
#define GET_HEAD(cbuf,size) \
        ({\
         (CIRC_SPACE((cbuf).head,READ_ONCE((cbuf).tail),(size)) > 0 ? \
          (cbuf).buf[(cbuf).head] : 0);\
         })

/**
 * Used by a producer.  Use smp_wmb or smp_store_release memory barrier
 * before incrementing the head pointer to make sure the item at the
 * head is committed before the increment and store of head, so that
 * readers are sure to get a completed item.
 *
 * smp_store_release() and smp_load_acquire() first appear in 3.12.47
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
#define INCREMENT_HEAD(cbuf,size) \
        smp_store_release(&(cbuf).head, ((cbuf).head + 1) & ((size) - 1))
#else
#define INCREMENT_HEAD(cbuf,size) \
        do { smp_wmb(); (cbuf).head = ((cbuf).head + 1) & ((size) - 1); } while (0)
#endif

#define NEXT_HEAD(cbuf,size) \
        (INCREMENT_HEAD(cbuf,size), GET_HEAD(cbuf,size))

/**
 * GET_TAIL is used by a consumer, accessing the tail index twice.
 * The count of items in the buffer will only stay the same or get
 * bigger between the CIRC_CNT check and the return of buf[tail] element,
 * because only the calling thread should be changing tail.
 * If a separate producer thread is messing with head, CIRC_CNT
 * will only stay the same or get bigger, not smaller, and
 * and the tail element will not become invalid.
 *
 * Use barriers in smp_load_acquire() or the explicit
 * smp_read_barrier_depends(), depending on the kernel version.
 * smp_read_barrier_depends() has been completely removed
 * from 5.9 kernels.
 *
 * A useful quote from memory-barriers.txt describing the
 * function of smp_read_barrier_depends():
 * "for any load preceding it, if that load touches
 * one of a sequence of stores from another CPU, then
 * by the time the barrier completes, the effects of all
 * the stores prior to that touched by the load will be
 * perceptible to any loads issued after the data dependency
 * barrier".
 * In GET_TAIL, we load the tail index, and if another CPU (a producer)
 * has stored buf[tail], then the barrier between the load of
 * tail and the load of buf[tail] will ensure that the store of
 * buf[tail] is perceptible to the calling CPU.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
#define GET_TAIL(cbuf,size) \
        ({\
         int tmp = CIRC_CNT(smp_load_acquire(&(cbuf).head),(cbuf).tail,size);\
         (tmp > 0 ? (cbuf).buf[(cbuf).tail] : 0);\
         })
#else
#define GET_TAIL(cbuf,size) \
        ({\
         int tmp = CIRC_CNT(READ_ONCE((cbuf).head),(cbuf).tail,size);\
         smp_read_barrier_depends();\
         (tmp > 0 ? (cbuf).buf[(cbuf).tail] : 0);\
         })
#endif

/**
 * Used by a consumer.
 * Use smp_mb or smp_store_release barriers to ensure the item
 * is fully read before the tail is incremented and stored.
 * After the increment of the tail a producer is free
 * to start writing into the element, so we make
 * sure we have finished reading it. The example in
 * circular-buffers.txt uses smp_mb(), when it seems
 * to me that smp_rmb() is sufficient.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
#define INCREMENT_TAIL(cbuf,size) \
        smp_store_release(&(cbuf).tail, ((cbuf).tail + 1) & ((size) - 1))

#else
#define INCREMENT_TAIL(cbuf,size) \
        do { smp_mb(); (cbuf).tail = ((cbuf).tail + 1) & ((size) - 1); } while(0)
#endif

/**
 * zero the head and tail and throw in a full memory barrier.
 */
#define EMPTY_CIRC_BUF(cbuf) \
        do { (cbuf).tail = (cbuf).head = 0; smp_mb(); } while(0)

/**
 * kmalloc, with flags = GFP_KERNEL, the buffer within a
 * dsm_sample_circ_buf, containing blen number of dsm_samples,
 * with each dsm_sample having a data portion of size dlen.
 * Since kmalloc is called with flags = GFP_KERNEL, this cannot be
 * called from interrupt context.
 * The samples are allocated in contiguous blocks of size PAGE_SIZE.
 *
 * @param dlen: length of data portion of samples. The data portion
 *      includes everything except the time tag and the length.
 * @param blen: number of samples in the circular buffer.
 * @return 0 is OK, or -errno if an error.
 */
extern int alloc_dsm_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen);

/**
 * kfree the buffer that was allocated with alloc_dsm_circ_buf().
 */
extern void free_dsm_circ_buf(struct dsm_sample_circ_buf* c);

/**
 * Reallocate the buffer within a dsm_sample_circ_buf,
 * if dlen or blen changed from the original allocation.
 * Since kmalloc is called with flags = GFP_KERNEL, this cannot be
 * called from interrupt context.
 * @param dlen: length of data portion of samples. The data portion
 *      includes everything except the time tag and the length.
 * @param blen: number of samples in the circular buffer.
 * @return 0 is OK, or -errno if an error.
 */
extern int realloc_dsm_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen);

/**
 * Initialize a dsm_sample_circ_buf. Basically sets head and tail to 0.
 */
extern void init_dsm_circ_buf(struct dsm_sample_circ_buf* c);

/**
 * Define a thiskernel_timespec_t, the natural timespec
 * for this machine and kernel.  It is what is used by the
 * ktime_get_*_*() functions. It is only defined in __KERNEL__ space.
 * timespec64 first appears in 3.17, include/linux/time64.h.
 * 
 * The typedef of thiskernel_timespec_t has the following member sizes,
 * where a 4 byte tv_sec field is not year 2038 compatible.
 *
 * 32 bit machine, kernel < 3.17:  4 byte tv_sec and 4 byte tv_nsec
 * 32 bit machine, kernel >= 3.17: 8 byte tv_sec and 4 byte tv_nsec
 * 64 bit machine, most any kernel?: 8 byte tv_sec and 8 byte tv_nsec
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0)
typedef struct timespec thiskernel_timespec_t;
#else
typedef struct timespec64 thiskernel_timespec_t;
#endif

/*
 * Return system time in a thiskernel_timespec_t.
 * ktime_get_real_ts64 first appears in 3.17
 */
inline void getSystemTimeTs(thiskernel_timespec_t *ts)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0)
        ktime_get_real_ts(ts);
#else
        ktime_get_real_ts64(ts);
#endif
}

/*
 * Return time in milliseconds since 00:00 UTC.
 */
inline dsm_sample_time_t getSystemTimeMsecs(void)
{
        thiskernel_timespec_t ts;
        getSystemTimeTs(&ts);
        /* BITS_PER_LONG seems to be defined in all kernels, but
         * __BITS_PER_LONG isn't defined in 2.6 kernels.
         * Use __BITS_PER_LONG if exporting a .h to user space. */
#if BITS_PER_LONG == 32 && LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
	/* do_div changes dividend in place, returns remainder */
        return do_div(ts.tv_sec, SECS_PER_DAY) * MSECS_PER_SEC +
		ts.tv_nsec / NSECS_PER_MSEC;
#else
        return (ts.tv_sec % SECS_PER_DAY) * MSECS_PER_SEC +
                ts.tv_nsec / NSECS_PER_MSEC;
#endif
}

/*
 * Return time in tenths of milliseconds since 00:00 UTC.
 */
inline dsm_sample_time_t getSystemTimeTMsecs(void)
{
        thiskernel_timespec_t ts;
        getSystemTimeTs(&ts);
#if BITS_PER_LONG == 32 && LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)
	/* do_div changes dividend in place, returns remainder */
        return do_div(ts.tv_sec, SECS_PER_DAY) * TMSECS_PER_SEC +
		ts.tv_nsec / NSECS_PER_TMSEC;
#else
        return (ts.tv_sec % SECS_PER_DAY) * TMSECS_PER_SEC +
                ts.tv_nsec / NSECS_PER_TMSEC;
#endif
}

struct sample_read_state
{
        char* samplePtr;
        unsigned int bytesLeft;
};

/**
 * Return non-zero if some portion of the last sample remains to be read.
 */
inline int sample_remains(const struct sample_read_state* state)
{
        return state->bytesLeft != 0;
}

/**
 * Utility function for nidas driver read methods.
 * This function provides support for user reads of dsm_samples
 * which are kept in a circular buffer (dsm_sample_circ_buf).
 *
 * If no data is available for reading this function will do a
 * wait_event_interruptible on the readq until data are available,
 * or the read is interrupted.
 * Then this function will copy all samples which are available
 * in the circular buffer into the user's buffer, until
 * the count limit is reached, or until no samples are left.
 * If the count limit is reached, the end of the user's buffer
 * may contain a partial sample.  The pointer into the
 * partially copied sample, and the number of bytes left to copy
 * are kept in struct read_state, so that the next user read(s) will
 * get the remaining contents of the sample.
 * Therefore this function will work for samples which are
 * larger than the user's read request, and is efficient in
 * that it tries to copy all available data without blocking.
 *
 * struct read_state should be zeroed out in the open method.
 */
extern ssize_t
nidas_circbuf_read(struct file *filp, char __user* buf, size_t count,
                struct dsm_sample_circ_buf* cbuf, struct sample_read_state* state,
                wait_queue_head_t* readq);

extern ssize_t
nidas_circbuf_read_nowait(struct file *filp, char __user* buf, size_t count,
                struct dsm_sample_circ_buf* cbuf, struct sample_read_state* state);

/**
 * Info needed to adjust time tags in a time series which
 * should have a fixed delta-T.
 */
struct screen_timetag_data
{
        /**
         * Defined delta-T, in units of 1/10 msec, passed to init function.
         */
        unsigned int dtTmsec;

        /**
         * In case delta-T is is not a integral number of  1/10 msec,
         * the fractional part, in micro-seconds..
         */
        unsigned int dtUsec;

        /**
         * Number of points to compute the running average, or the
         * minimum of the difference between the actual and expected
         * time tags. The time tags in the result time series will
         * be adjusted every nptsCalc number of input times.
         */
        unsigned int nptsCalc;

        /**
         * Result time tags will have a integral number of delta-Ts
         * from this base time. This base time is slowly adjusted
         * by averaging or computing the minimum difference between
         * the result time tags and the input time tags.
         */
        dsm_sample_time_t tt0;

        /**
         * Current number of delta-Ts from tt0.
         */
        int nDt;

        /**
         * Once nDt exceeds this value, roll back, to avoid overflow.
         */
        unsigned int samplesPerDay;

#ifdef DO_TIMETAG_ERROR_AVERAGE
        /**
         * Running averge N.
         */
        int nSum;

        /**
         * Running average of time tag differences, in usecs.
         */
        int errAvg;
#else

        /**
         * Number of points of current minimum time difference.
         */
        int nmin;

        /**
         * How many points to compute minimum time difference.
         */
        int nptsMin;

        /**
         * Minimum diffence between actual time tags and expected.
         */
        int tdiffmin;

#endif


};

extern void screen_timetag_init(struct screen_timetag_data* td,
        int deltaT_Usec, int adjustUsec);

extern dsm_sample_time_t screen_timetag(struct screen_timetag_data* td,
        dsm_sample_time_t tt);

#endif  /* KERNEL */

#endif
