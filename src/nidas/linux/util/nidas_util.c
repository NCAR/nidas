/*

Module containing utility functions for NIDAS linux device drivers.

Copyright 2005 UCAR, NCAR, All Rights Reserved

Original author:	Gordon Maclean

Revisions:

*/

#include <nidas/linux/util.h>
// #define DEBUG
#include <nidas/linux/klog.h>
#include <nidas/rtlinux/dsm_version.h>

#include <linux/module.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/slab.h>		/* kmalloc, kfree */
#include <asm/uaccess.h>

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_DESCRIPTION("NCAR nidas utilities");
MODULE_LICENSE("GPL");

int alloc_dsm_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen)
{
        char* sp;
        int i;

        /* count number of bits set, which should be one for a
         * power of 2.  Or check if first bit set
         * is the same as the last bit set: ffs(blen) == fls(blen)
         */
        if (blen == 0 || ffs(blen) != fls(blen)) {
            KLOG_ERR("circular buffer size=%d is not a power of 2\n",blen);
            return -EINVAL;
        }

        KLOG_DEBUG("kmalloc %u bytes\n",blen * sizeof(void*));
        if (!(c->buf = kmalloc(blen * sizeof(void*),GFP_KERNEL))) return -ENOMEM;
        memset(c->buf,0,blen * sizeof(void*));

        dlen += SIZEOF_DSM_SAMPLE_HEADER;
        if (dlen % 4) dlen += 4 - dlen % 4;
        KLOG_DEBUG("kmalloc %u bytes\n",blen * dlen);
        sp = kmalloc(blen * dlen,GFP_KERNEL);
        if (!sp) {
                kfree(c->buf);
                c->buf = 0;
                return -ENOMEM;
        }
        memset(sp,0,blen * dlen);
        for (i = 0; i < blen; i++) {
                c->buf[i] = (struct dsm_sample*)sp;
                sp += dlen;
        }
        c->head = c->tail = 0;
        c->size = blen;
        return 0;
}

void free_dsm_circ_buf(struct dsm_sample_circ_buf* c)
{
        if (c->buf && c->buf[0]) kfree(c->buf[0]);
        kfree(c->buf);
        c->buf = 0;
        c->size = 0;
}

int realloc_dsm_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen)
{
        free_dsm_circ_buf(c);
        return alloc_dsm_circ_buf(c,dlen,blen);
}

/*
 * Allocate a circular buffer of dsm_samples, where the dsm_samples
 * are allocated with separate kmallocs, rather than in one block.
 */
int alloc_dsm_disc_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen)
{
        int i;

        /* count number of bits set, which should be one for a
         * power of 2.  Or check if first bit set
         * is the same as the last bit set: ffs(blen) == fls(blen)
         */
        if (blen == 0 || ffs(blen) != fls(blen)) {
                KLOG_ERR("circular buffer size=%d is not a power of 2\n",blen);
                return -EINVAL;
        }

        KLOG_DEBUG("kmalloc %u bytes\n",blen * sizeof(void*));
        if (!(c->buf = kmalloc(blen * sizeof(void*),GFP_KERNEL))) return -ENOMEM;
        memset(c->buf,0,blen * sizeof(void*));

        dlen += SIZEOF_DSM_SAMPLE_HEADER;
        for (i = 0; i < blen; i++) {
                char* sp = kmalloc(dlen,GFP_KERNEL);
                if (!sp) {
                        int j;
                        for (j = 0; j < i; j++) kfree(c->buf[j]);
                        kfree(c->buf);
                        c->buf = 0;
                        return -ENOMEM;
                }
                memset(sp,0,dlen);
                c->buf[i] = (struct dsm_sample*) sp;
        }
        c->head = c->tail = 0;
        c->size = blen;
        return 0;
}

void free_dsm_disc_circ_buf(struct dsm_sample_circ_buf* c)
{
        int i;
        if (c->buf) {
                for (i = 0; i < c->size; i++) kfree(c->buf[i]);
                kfree(c->buf);
                c->buf = 0;
        }
        c->buf = 0;
        c->size = 0;
}

int realloc_dsm_disc_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen)
{
    free_dsm_disc_circ_buf(c);
    return alloc_dsm_disc_circ_buf(c,dlen,blen);
}

void init_dsm_circ_buf(struct dsm_sample_circ_buf* c)
{
    c->head = c->tail = 0;
}

ssize_t
nidas_circbuf_read_nowait(struct file *filp, char __user* buf, size_t count,
    struct dsm_sample_circ_buf* cbuf, struct sample_read_state* state)
{
        size_t countreq = count;
        struct dsm_sample* insamp;
        size_t bytesLeft = state->bytesLeft;
        char* samplePtr = state->samplePtr;
        size_t n;

        for ( ; count; ) {
                if ((n = min(bytesLeft,count)) > 0) {
                        if (copy_to_user(buf,samplePtr,n)) return -EFAULT;
                        bytesLeft -= n;
                        count -= n;
                        samplePtr += n;
                        buf += n;
                        if (bytesLeft > 0) break;   // user buffer filled
                        samplePtr = 0;
                        INCREMENT_TAIL(*cbuf,cbuf->size);
                }
                insamp = GET_TAIL(*cbuf,cbuf->size);
                if (!insamp) break;    // no more samples
                samplePtr = (char*)insamp;
                bytesLeft = insamp->length + SIZEOF_DSM_SAMPLE_HEADER;
                KLOG_DEBUG("bytes left=%zd\n",bytesLeft);
        }
        state->samplePtr = samplePtr;
        state->bytesLeft = bytesLeft;
        KLOG_DEBUG("read return = %u\n",countreq - count);
        return countreq - count;
}


ssize_t
nidas_circbuf_read(struct file *filp, char __user* buf, size_t count,
    struct dsm_sample_circ_buf* cbuf, struct sample_read_state* state,
    wait_queue_head_t* readq)
{
        while(state->bytesLeft == 0 && cbuf->head == cbuf->tail) {
            if (filp->f_flags & O_NONBLOCK) return -EAGAIN;
            KLOG_DEBUG("waiting for data,head=%d,tail=%d\n",cbuf->head,cbuf->tail);
            if (wait_event_interruptible(*readq,(cbuf->head != cbuf->tail)))
                return -ERESTARTSYS;
            KLOG_DEBUG("woken\n");
        }
        return nidas_circbuf_read_nowait(filp,buf,count,cbuf,state);
}

static void __exit nidas_util_cleanup(void)
{
    KLOG_DEBUG("nidas_util done\n");
    return;
}
static int __init nidas_util_init(void)
{	
        // DSM_VERSION_STRING is found in dsm_version.h
        KLOG_NOTICE("version: %s, HZ=%d\n",DSM_VERSION_STRING,HZ);
        return 0;
}

EXPORT_SYMBOL(alloc_dsm_circ_buf);
EXPORT_SYMBOL(free_dsm_circ_buf);
EXPORT_SYMBOL(realloc_dsm_circ_buf);

EXPORT_SYMBOL(alloc_dsm_disc_circ_buf);
EXPORT_SYMBOL(free_dsm_disc_circ_buf);
EXPORT_SYMBOL(realloc_dsm_disc_circ_buf);

EXPORT_SYMBOL(init_dsm_circ_buf);
EXPORT_SYMBOL(nidas_circbuf_read);
EXPORT_SYMBOL(nidas_circbuf_read_nowait);

module_init(nidas_util_init);
module_exit(nidas_util_cleanup);
