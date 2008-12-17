/* 
   Time-stamp: <Wed 13-Apr-2005 05:52:10 pm>

   Utility functions for nidas device drivers.

   Original Author: Gordon Maclean

   Copyright 2005 UCAR, NCAR, All Rights Reserved
 
   Revisions:

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

/* circular buffer of samples, compatible with macros in
   linux/circ_buf.h
*/
struct dsm_sample_circ_buf {
    struct dsm_sample **buf;
    volatile int head;
    volatile int tail;
    int size;
};

/* Macros for manipulating sample circular buffers
 * (in addition to those in linux/circ_buf.h */
 
/*
 * GET_HEAD accesses the head index twice.  The idea is that
 * the space will only stay the same or get bigger between
 * the CIRC_SPACE check and the return of buf[head] element,
 * because only the calling thread should be changing head.
 * If a separate consumer thread is messing with tail, CIRC_SPACE,
 * will only stay the same or get bigger, not smaller, and
 * and the head element will not become invalid.
 */
#define GET_HEAD(cbuf,size) \
    ((CIRC_SPACE((cbuf).head,(cbuf).tail,size) > 0) ? \
        (cbuf).buf[(cbuf).head] : 0)

/*
 * use barrier() to disable optimizations so that head is always OK,
 * just in case the compiler would build code like the following:
 *  head = head + 1;
 *  head = head & (size -1);
 * which leaves head in a bad state for a moment.
 */
#define INCREMENT_HEAD(cbuf,size) \
        ({\
            int tmp = ((cbuf).head + 1) & ((size) - 1);\
            barrier();\
            (cbuf).head = tmp;\
        })

/*
 * use barrier() to disable optimizations so that tail is always OK.
 */
#define INCREMENT_TAIL(cbuf,size) \
        ({\
            int tmp = ((cbuf).tail + 1) & ((size) - 1);\
            barrier();\
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
 * @param dlen: length of data portion of samples. The data portion
 *      includes everything except the time tag and the length.
 * @param blen: number of samples in the circular buffer.
 * @return 0 is OK, or -errno if an error.
 */
extern int alloc_dsm_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen);

/**
 * kfree the buffer within a dsm_sample_circ_buf.
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
 * This function will copy all samples which are available
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

#endif

#endif

#endif
