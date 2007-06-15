/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/IOStream.h>
#include <nidas/core/DSMTime.h>

#include <iostream>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

IOStream::IOStream(IOChannel& iochan,size_t blen):
	iochannel(iochan),buffer(0),
        maxUsecs(USECS_PER_SEC/4),
        newFile(true),nbytes(0),nEAGAIN(0)
{
    reallocateBuffer(blen * 2);
    lastWrite = getSystemTime();
}

IOStream::~IOStream()
{
    delete [] buffer;
}

void IOStream::reallocateBuffer(size_t len)
{
#ifdef DEBUG
    cerr << "IOStream::reallocateBuffer, len=" << len << endl;
#endif
    if (buffer) {
        char* newbuf = new char[len];
        // will silently lose data if len  is too small.
        size_t wlen = head - tail;
        if (wlen > len) wlen = len;
        memcpy(newbuf,tail,wlen);

        delete [] buffer;
        buffer = newbuf;
        buflen = len;
        tail = buffer;
        head = tail + wlen;
    }
    else {
        buffer = new char[len];
        buflen = len;
        head = tail = buffer;
    }
    eob = buffer + buflen;
    halflen = buflen / 2;
}

/*
 * Shift data in the IOStream buffer down, then do an iochannel.read()
 * to the head of the buffer.
 */
size_t IOStream::read() throw(n_u::IOException)
{
    newFile = false;
    size_t l = available();
    // shift data down. memmove supports overlapping memory areas
    if (tail > buffer) {
	memmove(buffer,tail,l);
	tail = buffer;
	head = tail + l;
    }

    if (head < eob) {
	// l==0 means end of file, next read may throw EOFException
	l = iochannel.read(head,eob-head);

	if (iochannel.isNewFile()) {
	    tail = head;	// discard last portion of previous file
	    newFile = true;
	}
    }
    else l = 0;

    head += l;
#ifdef DEBUG
    cerr << "IOStream, read =" << l << ", avail=" << available() << endl;
#endif
    return l;
}

/*
 * Read available data from tail of IOStream buffer into user buffer.
 * May return less than len.
 */
size_t IOStream::read(void* buf, size_t len) throw(n_u::IOException)
{
    newFile = false;
    size_t l = available();
    if (l == 0) {
        read();
	l = available();
    }
    if (len < l) l = len;
    memcpy(buf,tail,l);
    tail += l;
    nbytes += l;
    return l;
}

/*
 * Skip over nbytes of IOStream buffer.
 * May return less than len.
 */
size_t IOStream::skip(size_t len) throw(n_u::IOException)
{
    newFile = false;
    size_t l = available();
    if (l == 0) {
        read();
	l = available();
    }
    if (len < l) l = len;
    tail += l;
    nbytes += l;
    return l;
}

/*
 * Read data until finding a terminator character or the user's
 * buffer is filled.  This may do more than one physical read.
 */
size_t IOStream::readUntil(void* buf, size_t len,char term)
	throw(n_u::IOException)
{
    register char* outp = (char*) buf;
    const char* eout = outp + len - 1;	// leave room for trailing '\0'

    bool done = false;
    for (;;) {
	newFile = false;
	size_t l = available();
	if (l == 0) {
	    read();
	    l = available();
	}
	for ( ; tail < head && !done; )
	    done = outp == eout || (*outp++ = *tail++) == term;

	if (done) break;
    }
    *outp = '\0';
    len = outp - (const char*)buf;
    nbytes += len;
    return len;
}

/*
 * Put data back in buffer.
 */
size_t IOStream::backup(size_t len) throw()
{
    size_t space = tail - buffer;
    if (space < len) len = space;
    tail -= len;
    nbytes -= len;
    return len;
}

size_t IOStream::write(const void*buf,size_t len) throw (n_u::IOException)
{
    return write(&buf,&len,1);
}

/*
 * Buffered atomic write - all data is written to buffer, or none.
 */
size_t IOStream::write(const void *const *bufs,const size_t* lens, int nbufs) throw (n_u::IOException)
{
    size_t l;
    int ibuf;

    /* compute total length of user buffers */
    size_t tlen = 0;
    for (ibuf = 0; ibuf < nbufs; ibuf++) tlen += lens[ibuf];

    // If we need to expand the buffer for a large sample.
    // This does not screen ridiculous sample sizes.
    if (tlen > buflen) reallocateBuffer(tlen);

    dsm_time_t tnow = getSystemTime();
    int tdiff = tnow - lastWrite;	// microseconds

    /* space available for in buffer */
    size_t space = eob - head;

    /* number of bytes already in buffer */
    size_t wlen = head - tail;

    // there is data in the buffer and the buffer is full enough,
    // or maxUsecs has elapsed since the last write.
    if (wlen >= halflen || (wlen > 0 && tdiff >= maxUsecs)) {

        // if streaming small samples, don't write more than
        // halflen number of bytes.  The idea is that <= halflen
        // is a good size for the output device.
        if (tlen < halflen && wlen > halflen) wlen = halflen;
        try {
            l = iochannel.write(tail,wlen);
        }
        catch (const n_u::IOException& ioe) {
            if (ioe.getError() == EAGAIN) l = 0;
            else throw ioe;
        }
        if (l == 0 && (nEAGAIN++ % 100) == 0) {
            n_u::Logger::getInstance()->log(LOG_WARNING,
                    "%s: nEAGAIN=%d, wlen=%d, tlen=%d\n",
                getName().c_str(),nEAGAIN,wlen,tlen);
        }
        tail += l;
        if (tail == head) {
            tail = head = buffer;	// empty buffer
            space = eob - head;
        }
        lastWrite = tnow;
    }

    // not enough space for the entire request.
    // See if we can make more space
    if (space < tlen) {
        if (tail == buffer) return 0;   // nope
        // shift data down. memmove supports overlapping memory areas
        wlen = head - tail;
        memmove(buffer,tail,wlen);
        tail = buffer;
        head = tail + wlen;
        space = eob - head;
        if (space < tlen) return 0;	// gave it our best shot
    }

    // copy all user buffers. We know there is space
    for (ibuf = 0; ibuf < nbufs; ibuf++) {
        l = lens[ibuf];
        memcpy(head,bufs[ibuf],l);
        head += l;
    }
    return tlen;
}

void IOStream::flush() throw (n_u::IOException)
{
    ssize_t l;

    /* number of bytes in buffer */
    size_t wlen = head - tail;

    for (int ntry = 0; wlen > 0 && ntry < 5; ntry++) {
	try {
	    l = iochannel.write(tail,wlen);
	}
	catch (const n_u::IOException& ioe) {
	    if (ioe.getError() == EAGAIN) l = 0;
	    else throw ioe;
	}
	tail += l;
        wlen -= l;
	if (tail == head) tail = head = buffer;
    }
    iochannel.flush();
    lastWrite = getSystemTime();
}

