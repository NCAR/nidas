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
	iochannel(iochan),buflen(blen),maxUsecs(USECS_PER_SEC/4),newFile(true),nbytes(0)
{
    buffer = new char[buflen * 2];
    eob = buffer + buflen * 2;
    head = tail = buffer;
    lastWrite = getSystemTime();
}

IOStream::~IOStream()
{
    delete [] buffer;
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

bool IOStream::write(const void*buf,size_t len) throw (n_u::IOException)
{
    return write(&buf,&len,1);
}

/*
 * Buffered atomic write - all data is written to buffer, or none.
 */
bool IOStream::write(const void *const *bufs,const size_t* lens, int nbufs) throw (n_u::IOException)
{
    size_t l;
    int ibuf;

    /* compute total length of user buffers */
    size_t tlen = 0;
    for (ibuf = 0; ibuf < nbufs; ibuf++) tlen += lens[ibuf];

    // large writes, bigger than 1/2 the buffer size
    bool largewrite = tlen > buflen;

    dsm_time_t tnow = getSystemTime();
    int tdiff = tnow - lastWrite;	// microseconds

    int nEAGAIN = 0;

    ibuf = 0;
    const char* bufptr = (const char*)bufs[ibuf];
    size_t lenbuf = lens[ibuf];
    while (tlen > 0) {

	/* space for more data in buffer */
	size_t space = eob - head;

	/* number of bytes already in buffer */
	size_t wlen = head - tail;

	if ((wlen > 0) && (wlen >= buflen || tdiff >= maxUsecs)) {
	    if (wlen > buflen) wlen = buflen;
	    try {
		l = iochannel.write(tail,wlen);
	    }
	    catch (const n_u::IOException& ioe) {
		n_u::Logger::getInstance()->log(LOG_WARNING,
		    "%s: %s, nEAGAIN=%d, wlen=%d, tlen=%d\n",
		    getName().c_str(),ioe.what(),nEAGAIN,wlen,tlen);
		if (ioe.getError() == EAGAIN) {
		    l = 0;
		    nEAGAIN++;
		    // if (nEAGAIN > 5) throw ioe;
		    struct timespec ns = {0, NSECS_PER_SEC / 100};
		    nanosleep(&ns,0);
		}
		else throw ioe;
	    }
	    if (l == 0) {
		nEAGAIN++;
		if ((nEAGAIN % 100) == 0) {
		    n_u::Logger::getInstance()->log(LOG_WARNING,
			    "%s: nEAGAIN=%d, wlen=%d, tlen=%d\n",
			getName().c_str(),nEAGAIN,wlen,tlen);
		}
		struct timespec ns = {0, NSECS_PER_SEC / 100};
		nanosleep(&ns,0);
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
	    if (largewrite) {
		// less than 1/2 buffer available
	        if (space < buflen && tail > buffer) {
		    // shift data down
		    wlen = head - tail;
		    memmove(buffer,tail,wlen);
		    tail = buffer;
		    head = tail + wlen;
		    space = eob - head;
		}
		// don't give up yet if we're doing a large write
		// the only time we give up on a large write is after
		// too many EAGAINs.
	    }
	    else {
		if (tail == buffer) return false;	// can't make more
		// shift data down. memmove supports overlapping memory areas
		wlen = head - tail;
		memmove(buffer,tail,wlen);
		tail = buffer;
		head = tail + wlen;
		space = eob - head;
		if (space < tlen) return false;	// gave it our best shot
		// small writes will either succeed or fail completely
	    }
	}

	for (; tlen > 0 && space > 0; ) {
	    l = lenbuf;
	    if (l > space) l = space;
	    memcpy(head,bufptr,l);
	    head += l;
	    lenbuf -= l;
	    space -= l;
	    tlen -= l;

	    if (lenbuf == 0 && ++ibuf < nbufs) {
		bufptr = (const char*) bufs[ibuf];
		lenbuf = lens[ibuf];
	    }
	    else bufptr += l;	// space == 0
	}
	// if tlen > 0 at this point then we're doing a large write
    }

    return true;
}

void IOStream::flush() throw (n_u::IOException)
{
    ssize_t l;

    /* number of bytes in buffer */
    size_t wlen = head - tail;

    if (wlen == 0) return;
    for (int ntry = 0; head > tail && ntry < 1000; ntry++) {
	try {
	    l = iochannel.write(tail,wlen);
	}
	catch (const n_u::IOException& ioe) {
	    if (ioe.getError() == EAGAIN) {
	        l = 0;
		struct timespec ns = {0, NSECS_PER_SEC / 10};
		nanosleep(&ns,0);
	    }
	    else throw ioe;
	}
	tail += l;
	if (tail == head) tail = head = buffer;
    }
    iochannel.flush();
    lastWrite = getSystemTime();
}

