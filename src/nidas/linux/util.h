/* 
   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   Utility functions for nidas device drivers.

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
*/

#ifndef NIDAS_LINUX_NIDAS_UTIL_H
#define NIDAS_LINUX_NIDAS_UTIL_H

#if defined(__RTCORE_KERNEL__) || defined(__KERNEL__)

#include <nidas/linux/types.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/fs.h>

#include <linux/circ_buf.h>
#include <linux/time.h>

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
 * the linux kernel documentation, contained in the kernel-doc RPM.
 * kernel-doc places the documentation in
 * /usr/share/doc/kernel-doc-x.y.z/Documentation, where x.y.z is the
 * kernel version of your system.
 *
 * These doc files have useful information on circular buffers:
 *    circular-buffers.txt
 *    memory-barriers.txt (after a strong cup of coffee)
 */
 
/**
 * A circular buffer of time-tagged data samples.
 *
 * There is a tempation to declare head and tail volatile, to prevent
 * the compiler from temporarily caching the values somewhere, so that
 * changes to head or tail are known immediately to all threads.
 * Read volatile-considered-harmful.txt in the kernel documentation.
 * We use memory barriers, rather than volatile.
 */
struct dsm_sample_circ_buf {
    struct dsm_sample **buf;
    int head;
    int tail;
    int size;
};

/*
 * GET_HEAD accesses the head index twice.  The idea is that
 * the space will only stay the same or get bigger between
 * the CIRC_SPACE check and the return of buf[head] element,
 * because only the calling thread should be changing head.
 * If a separate consumer thread is messing with tail, CIRC_SPACE
 * will only stay the same or get bigger, not smaller, and
 * and the head element will not become invalid.
 * Use smb_rmb() to force loads of head and tail before the address
 * of the head is used.
 * We use a read barrier, smp_rmb(), instead of the data dependency
 * barrier, smp_read_barrier_depends(), because we also want to force
 * a load of the tail value. smp_rmp() is also a compiler barrier,
 * and smp_read_barrier_depends() is not. 
 */
#define GET_HEAD(cbuf,size) \
    ({\
        int tmp = CIRC_SPACE((cbuf).head,(cbuf).tail,size);\
        smp_rmb();\
        (tmp > 0 ? (cbuf).buf[(cbuf).head] : 0);\
     })

/*
 * use smp_wmb() memory barrier before incrementing the head pointer.
 * This does two things. It makes sure the item at the head is committed
 * before the increment and store of head, so that readers
 * are sure to get a completed item.
 * Also (and I'm not sure how critical this is) it implies a
 * compiler barrier, so that the compiler is prevented from creating
 * code like the following:
 *  head = head + 1;
 *  head = head & (size -1);
 * which *could* leave head in a bad state for a moment.
 */
#define INCREMENT_HEAD(cbuf,size) \
        ({\
            int tmp = ((cbuf).head + 1) & ((size) - 1);\
            smp_wmb();\
            (cbuf).head = tmp;\
        })

/*
 * GET_TAIL accesses the tail index twice.  The idea is that
 * the count will only stay the same or get bigger between
 * the CIRC_CNT check and the return of buf[tail] element,
 * because only the calling thread should be changing tail.
 * If a separate producer thread is messing with head, CIRC_CNT
 * will only stay the same or get bigger, not smaller, and
 * and the tail element will not become invalid.
 * Use smp_rmp() barrier to force loads of head and tail before
 * the item is accessed.
 */
#define GET_TAIL(cbuf,size) \
    ({\
        int tmp = CIRC_CNT((cbuf).head,(cbuf).tail,size);\
        smp_rmb();\
        (tmp > 0 ? (cbuf).buf[(cbuf).tail] : 0);\
     })

/*
 * Us smp_mb barrier to ensure the item is fully read
 * before the tail is incremented and stored, and as a
 * compiler barrier as in INCREMENT_HEAD above.
 */
#define INCREMENT_TAIL(cbuf,size) \
        ({\
            int tmp = ((cbuf).tail + 1) & ((size) - 1);\
            smp_mb();\
            (cbuf).tail = tmp;\
        })

#define NEXT_HEAD(cbuf,size) \
        (INCREMENT_HEAD((cbuf),size), GET_HEAD((cbuf),size))

/**
 * kmalloc, with flags = GFP_KERNEL, the buffer within a
 * dsm_sample_circ_buf, containing blen number of dsm_samples,
 * with each dsm_sample having a data portion of size dlen.
 * Since kmalloc is called with flags = GFP_KERNEL, this cannot be
 * called from interrupt context.
 * The samples are allocated in one kmalloc'd block, so if the
 * samples are large, this will require that
 * (dlen+SIZEOF_DSM_SAMPLE_HEADER)*blen contiguous bytes of memory
 * are available.  If that is problematic, use alloc_dsm_disc_circ_buf().
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
 * kmalloc, with flags = GFP_KERNEL, the buffer within a
 * dsm_sample_circ_buf, containing blen number of dsm_samples,
 * with each dsm_sample having a data portion of size dlen.
 * Since kmalloc is called with flags = GFP_KERNEL, this cannot be
 * called from interrupt context.
 * The samples are allocated from memory in separate calls to kmalloc,
 * so this function is recommended when allocating
 * (dlen+SIZEOF_DSM_SAMPLE_HEADER)*blen contiguous bytes of memory
 * is problematic.
 * @param dlen: length of data portion of samples. The data portion
 *      includes everything except the time tag and the length.
 * @param blen: number of samples in the circular buffer.
 * @return 0 is OK, or -errno if an error.
 */
extern int alloc_dsm_disc_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen);

/**
 * kfree the buffer that was allocated with alloc_dsm_disc_circ_buf().
 */
extern void free_dsm_disc_circ_buf(struct dsm_sample_circ_buf* c);
    
/**
 * Reallocate the buffer within a dsm_sample_circ_buf,
 * if dlen or blen changed from the original allocation.
 * Since kmalloc is called with flags = GFP_KERNEL, this cannot be
 * called from interrupt context.
 * The samples are allocated from memory in separate calls to kmalloc,
 * so this function is recommended when allocating
 * @param dlen: length of data portion of samples. The data portion
 *      includes everything except the time tag and the length.
 * @param blen: number of samples in the circular buffer.
 * @return 0 is OK, or -errno if an error.
 */
extern int realloc_dsm_disc_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen);

/**
 * Initialize a dsm_sample_circ_buf. Basically sets head and tail to 0.
 */
extern void init_dsm_circ_buf(struct dsm_sample_circ_buf* c);

/*
 * Return time in milliseconds since 00:00 UTC.
 */
inline dsm_sample_time_t getSystemTimeMsecs(void)
{
    struct timeval tv;
    do_gettimeofday(&tv);
    return (tv.tv_sec % 86400) * MSECS_PER_SEC +
        tv.tv_usec / USECS_PER_MSEC;
}


/*
 * Return time in tenths of milliseconds since 00:00 UTC.
 */
inline dsm_sample_time_t getSystemTimeTMsecs(void)
{
    struct timeval tv;
    do_gettimeofday(&tv);
    return (tv.tv_sec % 86400) * TMSECS_PER_SEC +
        tv.tv_usec / USECS_PER_TMSEC;
}

#if !defined(__RTCORE_KERNEL__)

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
 * or in interrupt is received.
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

#endif

#endif

#endif
