/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <IOStream.h>
#include <DSMTime.h>

#include <iostream>

using namespace dsm;
using namespace std;


IOStream::IOStream(IOChannel& iochan,size_t blen):
	iochannel(iochan),buflen(blen),maxMsecs(1000),newFile(true)
{
    buffer = new char[buflen * 2];
    eob = buffer + buflen * 2;
    head = tail = buffer;
    lastWrite = getCurrentTimeInMillis();
}

IOStream::~IOStream()
{
    delete [] buffer;
}

/*
 * Shift data in the IOStream buffer down, then do an iochannel.read()
 * to the head of the buffer.
 */
size_t IOStream::read() throw(atdUtil::IOException)
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
size_t IOStream::read(void* buf, size_t len) throw(atdUtil::IOException)
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
    return l;
}

/*
 * Put data back in buffer.
 */
size_t IOStream::putback(const void* buf, size_t len) throw()
{
    size_t space = tail - buffer;
    if (space < len) len = space;
    memcpy(tail - len,buf,len);
    tail -= len;
    return len;
}

bool IOStream::write(const void*buf,size_t len) throw (atdUtil::IOException)
{
    return write(&buf,&len,1);
}

/**
 * Buffered atomic write. We change the contents of lens argument.
 */
bool IOStream::write(const void**bufs,size_t* lens, int nbufs) throw (atdUtil::IOException)
{
    size_t l;
    int ibuf;

    /* total length of user buffers */
    size_t tlen = 0;
    for (ibuf = 0; ibuf < nbufs; ibuf++) tlen += lens[ibuf];

    // large writes, bigger than 1/2 the buffer size
    bool largewrite = tlen > buflen;

    dsm_time_t tnow = getCurrentTimeInMillis();
    int tdiff = tnow - lastWrite;

    int nagain = 0;

    ibuf = 0;

    while (tlen > 0) {

	/* space for more data in buffer */
	size_t space = eob - head;

	/* number of bytes in buffer */
	size_t wlen = head - tail;

	if ((wlen > 0) && (wlen >= buflen || tdiff >= maxMsecs)) {
	    if (wlen > buflen) wlen = buflen;
	    try {
		l = iochannel.write(tail,wlen);
	    }
	    catch (const atdUtil::IOException& ioe) {
		if (ioe.getError() == EAGAIN) {
		    l = 0;
		    nagain++;
		    if (nagain > 5) throw ioe;
		}
		else throw ioe;
	    }
	    tail += l;
	    if (tail == head) {
	        tail = head = buffer;
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

	for (; ibuf < nbufs && space > 0; ibuf++) {
	    l = lens[ibuf];
	    if (l > space) l = space;
	    memcpy(head,bufs[ibuf],l);
	    head += l;
	    ((const char*&)(bufs[ibuf])) += l;
	    lens[ibuf] -= l;
	    space -= l;
	    tlen -= l;
	    if (lens[ibuf] > 0) break;	// out of space but data left
	}
	// if tlen > 0 at this point then we're doing a large write
    }

    return true;
}

void IOStream::flush() throw (atdUtil::IOException)
{
    ssize_t l;

    /* number of bytes in buffer */
    size_t wlen = head - tail;

    if (wlen == 0) return;
    try {
	l = iochannel.write(tail,wlen);
    }
    catch (const atdUtil::IOException& ioe) {
	if (ioe.getError() == EAGAIN) l = 0;
	else throw ioe;
    }
    tail += l;
    if (tail == head) tail = head = buffer;
    lastWrite = getCurrentTimeInMillis();
}

