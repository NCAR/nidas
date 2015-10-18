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

Module containing utility functions for NIDAS linux device drivers.

Original author:	Gordon Maclean

*/

#include <nidas/linux/util.h>
// #define DEBUG
#include <nidas/linux/klog.h>
#include <nidas/linux/Revision.h>    // REPO_REVISION

#include <linux/module.h>
#include <linux/init.h>

#include <linux/sched.h>
#include <linux/slab.h>		/* kmalloc, kfree */
#include <asm/uaccess.h>

#ifndef REPO_REVISION
#define REPO_REVISION "unknown"
#endif

MODULE_AUTHOR("Gordon Maclean <maclean@ucar.edu>");
MODULE_DESCRIPTION("NCAR nidas utilities");
MODULE_LICENSE("GPL");
MODULE_VERSION(REPO_REVISION);

/*
 * Allocate a circular buffer of dsm_samples.  If the size of one
 * sample is less than PAGE_SIZE, they are allocated in blocks of
 * size up to PAGE_SIZE.
 * dlen: length in bytes of the data portion of each sample.
 * blen: number of samples in the circular buffer.
 */
int alloc_dsm_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen)
{
        int isamp = 0;
        int samps_per_page;
        int j,n;
        char *sp;

        /* count number of bits set, which should be one for a
         * power of 2.  Or check if first bit set
         * is the same as the last bit set: ffs(blen) == fls(blen)
         */
        if (blen == 0 || ffs(blen) != fls(blen)) {
                KLOG_ERR("circular buffer size=%d is not a power of 2\n",blen);
                return -EINVAL;
        }

        c->head = c->tail = c->size = c->npages = 0;
        c->pages = 0;

        KLOG_DEBUG("kmalloc %u bytes\n",blen * sizeof(void*));
        if (!(c->buf = kmalloc(blen * sizeof(void*),GFP_KERNEL))) return -ENOMEM;
        memset(c->buf,0,blen * sizeof(void*));

        /* Total size of a sample. Make it a multiple of sizeof(int)
         * so that samples are aligned to an int */
        dlen += SIZEOF_DSM_SAMPLE_HEADER;
        n = dlen % sizeof(int);
        if (n) dlen += sizeof(int) - n;

        samps_per_page = PAGE_SIZE / dlen;
        if (samps_per_page < 1) samps_per_page = 1;

        /* number of pages to allocate */
        n = (blen - 1) / samps_per_page + 1;
        if (!(c->pages = kmalloc(n * sizeof(void*),GFP_KERNEL))) {
                kfree(c->buf);
                c->buf = 0;
                return -ENOMEM;
        }
        memset(c->pages,0,n * sizeof(void*));
        c->npages = n;

        KLOG_INFO("sample len=%zu, buf len=%d, samps_per_page=%d, npages=%d\n",
                        dlen,blen,samps_per_page,n);

        for (n = 0; n < c->npages; n++) {
                j = blen - isamp;       /* left to allocate */
                if (j > samps_per_page) j = samps_per_page;
                sp = kmalloc(dlen * j,GFP_KERNEL);
                if (!sp) {
                        for (j = 0; j < n; j++) kfree(c->pages[j]);
                        kfree(c->pages);
                        c->pages = 0;
                        c->npages = 0;
                        kfree(c->buf);
                        c->buf = 0;
                        return -ENOMEM;
                }
                c->pages[n] = sp;

                for (j = 0; j < samps_per_page && isamp < blen; j++) {
                        c->buf[isamp++] = (struct dsm_sample*) sp;
                        sp += dlen;
                }
        }
        c->size = blen;
        smp_mb();
        return 0;
}

void free_dsm_circ_buf(struct dsm_sample_circ_buf* c)
{
        int i;
        if (c->pages) {
                for (i = 0; i < c->npages; i++) kfree(c->pages[i]);
        }
        kfree(c->pages);
        c->pages = 0;
        c->npages = 0;

        kfree(c->buf);
        c->buf = 0;

        c->size = 0;
        smp_mb();
}

int realloc_dsm_circ_buf(struct dsm_sample_circ_buf* c,size_t dlen,int blen)
{
        free_dsm_circ_buf(c);
        return alloc_dsm_circ_buf(c,dlen,blen);
}

void init_dsm_circ_buf(struct dsm_sample_circ_buf* c)
{
        c->head = c->tail = 0;
        smp_mb();
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
        while(state->bytesLeft == 0 && ACCESS_ONCE(cbuf->head) == cbuf->tail) {
                if (filp->f_flags & O_NONBLOCK) return -EAGAIN;
                KLOG_DEBUG("waiting for data,head=%d,tail=%d\n",cbuf->head,cbuf->tail);
                if (wait_event_interruptible(*readq,(ACCESS_ONCE(cbuf->head) != cbuf->tail)))
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
        KLOG_NOTICE("version: %s\n",REPO_REVISION);
        return 0;
}

EXPORT_SYMBOL(alloc_dsm_circ_buf);
EXPORT_SYMBOL(free_dsm_circ_buf);
EXPORT_SYMBOL(realloc_dsm_circ_buf);
EXPORT_SYMBOL(init_dsm_circ_buf);
EXPORT_SYMBOL(nidas_circbuf_read);
EXPORT_SYMBOL(nidas_circbuf_read_nowait);

module_init(nidas_util_init);
module_exit(nidas_util_cleanup);
