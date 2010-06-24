/* -*- mode: c++; c-basic-offset: 4; -*-
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
	_iochannel(iochan),_buffer(0),
        _newInput(true),_nbytesIn(0),_nbytesOut(0),
        _nEAGAIN(0)
{
    reallocateBuffer(blen * 2);
}

IOStream::~IOStream()
{
    delete [] _buffer;
}

void IOStream::reallocateBuffer(size_t len)
{
#ifdef DEBUG
    cerr << "IOStream::reallocateBuffer, len=" << len << endl;
#endif
    if (_buffer) {
        char* newbuf = new char[len];
        // will silently lose data if len  is too small.
        size_t wlen = _head - _tail;
        if (wlen > len) wlen = len;
        memcpy(newbuf,_tail,wlen);

        delete [] _buffer;
        _buffer = newbuf;
        _buflen = len;
        _tail = _buffer;
        _head = _tail + wlen;
    }
    else {
        _buffer = new char[len];
        _buflen = len;
        _head = _tail = _buffer;
    }
    _eob = _buffer + _buflen;
    _halflen = _buflen / 2;
    DLOG(("%s: halflen=%d",getName().c_str(),_halflen));
}

/*
 * Will return length of 0 if there is already data in the buffer,
 * or at end-of-file, or if the _iochannel is non-blocking.
 */
size_t IOStream::read() throw(n_u::IOException)
{
    _newInput = false;
    size_t l = available(); 	// head - tail;

    // Avoid blocking on more data if there's already some in the buffer.
    if (l > 0) return 0;

    // shift data down. memmove supports overlapping memory areas
    if (_tail > _buffer) {
	memmove(_buffer,_tail,l);
	_tail = _buffer;
	_head = _tail + l;
    }

    l = _iochannel.read(_head,_eob-_head);
    _head += l;
    if (_iochannel.isNewInput()) {
        _newInput = true;
        _nbytesIn = 0;
    }
#ifdef DEBUG
    DLOG(("IOStream, read =") << l << ", avail=" << available());
#endif
    return l;
}

/*
 * Read until len bytes have been transfered.
 * May perform zero or more physical read()s.
 * May return less than len bytes if read() encounters
 * an end-of-file or if the _iochannel is non-blocking
 * and no data is available.
 */
size_t IOStream::read(void* buf, size_t len) throw(n_u::IOException)
{
    size_t req = len;
    _newInput = false;
    while (len > 0) {
        if (available() == 0) {
            // if read returns 0, we're at the end of file or
            // EAGAIN on noblocking read.
            if (read() == 0) return req - len;
            if (!_newInput) _newInput = _iochannel.isNewInput();
        }
        size_t l = readBuf(buf,len);
        len -= l;
        buf = (char*) buf + l;
    }
    return req;
}

/*
 * Skip over nbytes of IOStream buffer.
 * May return less than len.
 */
size_t IOStream::skip(size_t len) throw(n_u::IOException)
{
    if (available() == 0) read();
    _newInput = false;
    size_t l = available();
    if (len < l) l = len;
    _tail += l;
    _nbytesIn += l;
    return l;
}

/*
 * Read data until finding a terminator character or the user's
 * buffer is filled.  This may do more than one physical read.
 * Will not work well with non-blocking reads.
 */
size_t IOStream::readUntil(void* buf, size_t len,char term)
	throw(n_u::IOException)
{
    register char* outp = (char*) buf;
    const char* eout = outp + len - 1;	// leave room for trailing '\0'

    bool done = false;
    _newInput = false;
    for (;;) {
	if (available() == 0) {
            // If end of input, (or EAGAIN) discard previous data, keep reading
	    if (read() == 0) outp = (char*) buf;
            if (!_newInput) _newInput = _iochannel.isNewInput();
	}
	for ( ; _tail < _head && !done; )
	    done = outp == eout || (*outp++ = *_tail++) == term;
	if (done) break;
    }
    *outp = '\0';
    len = outp - (const char*)buf;
    _nbytesIn += len;
    return len;
}

/*
 * Put data back in buffer.
 */
size_t IOStream::backup(size_t len) throw()
{
    size_t maxbackup = _tail - _buffer;
    // cerr << "IOStream::backup, len=" << len << " maxbackup=" << maxbackup << endl;
    if (len > maxbackup) len = maxbackup;
    _tail -= len;
    _nbytesIn -= len;
    return len;
}

size_t IOStream::backup() throw()
{
    return backup(_tail - _buffer);
}

size_t IOStream::write(const void*buf,size_t len,bool flush) throw (n_u::IOException)
{
    struct iovec iov;
    iov.iov_base = const_cast<void*>(buf);
    iov.iov_len = len;
    return write(&iov,1,flush);
}

/*
 * Buffered atomic write - all data is written to buffer, or none.
 */
size_t IOStream::write(const struct iovec*iov, int nbufs, bool flush) throw (n_u::IOException)
{
    size_t l;
    int ibuf;

    /* compute total length of user buffers */
    size_t tlen = 0;
    for (ibuf = 0; ibuf < nbufs; ibuf++) tlen += iov[ibuf].iov_len;

    // If we need to expand the buffer for a large sample.
    // This does not screen ridiculous sample sizes.
    if (tlen > _buflen) reallocateBuffer(tlen);

    // Only make two attempts at most.  Most likely the first attempt will
    // be enough to copy in the user buffers and then potentially write it
    // out.  The second attempt is in case the current buffer must first be
    // written to make room for the user buffers.
    for (int attempts = 0; attempts < 2; ++attempts)
    {
	/* number of bytes in buffer waiting to be written */
	size_t wlen = _head - _tail;

	/* space available in buffer */
	size_t space = _eob - _head;

	// If there's not space in the buffer, but we can make some, do it now.
	if (tlen > space && wlen + tlen <= _buflen && _tail != _buffer) {
	    // shift data down. memmove supports overlapping memory areas
	    memmove(_buffer,_tail,wlen);
	    _tail = _buffer;
	    _head = _tail + wlen;
	    space = _eob - _head;
	}

	// If there's space now for this write in the buffer, add it.
	if (tlen <= space) {
	    for (ibuf = 0; ibuf < nbufs; ibuf++) {
		l = iov[ibuf].iov_len;
		memcpy(_head,iov[ibuf].iov_base,l);
		_head += l;
	    }
	    // Indicate the user buffers have been added.
	    nbufs = 0;
	    wlen = _head - _tail;
	    space = _eob - _head;
	}

	// There is data in the buffer and the buffer is full enough, or
	// maxUsecs has elapsed since the last write, or else we need to
	// write to make room for the user buffers.
	if (nbufs > 0 || wlen >= _halflen || flush) {

	    // if streaming small samples, don't write more than
	    // _halflen number of bytes.  The idea is that <= _halflen
	    // is a good size for the output device.
	    // if (tlen < _halflen && wlen > _halflen) wlen = _halflen;
	    try {
                // cerr << "wlen=" << wlen << endl;
		l = _iochannel.write(_tail,wlen);
                addNumOutputBytes(l);
	    }
	    catch (const n_u::IOException& ioe) {
		if (ioe.getError() == EAGAIN) {
                    l = 0;
#define REPORT_EAGAINS
#ifdef REPORT_EAGAINS
                    if ((_nEAGAIN++ % 100) == 0) {
                        WLOG(("%s: nEAGAIN=%d, wlen=%d, tlen=%d",
                              getName().c_str(),_nEAGAIN,wlen,tlen));
                    }
#endif
                }
		else throw ioe;
	    }
	    _tail += l;
	    if (_tail == _head) {
		_tail = _head = _buffer;	// empty buffer
		space = _eob - _head;
	    }
	}

	// We're done when the user buffers have been copied into this buffer.
	if (nbufs == 0) break;
    }

    // Return zero when the user buffers could not be copied, which happens
    // when writes for the data currently in the buffer don't succeed.
    return (nbufs > 0) ? 0 : tlen;
}

void IOStream::flush() throw (n_u::IOException)
{
    size_t l;

    /* number of bytes in buffer */
    size_t wlen = _head - _tail;

    for (int ntry = 0; wlen > 0 && ntry < 5; ntry++) {
	try {
	    l = _iochannel.write(_tail,wlen);
            addNumOutputBytes(l);
	}
	catch (const n_u::IOException& ioe) {
	    if (ioe.getError() == EAGAIN) l = 0;
	    else throw ioe;
	}
	_tail += l;
        wlen -= l;
	if (_tail == _head) _tail = _head = _buffer;
    }
    _iochannel.flush();
}

