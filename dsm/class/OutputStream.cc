/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <OutputStream.h>

#include <iostream>

// #define DEBUG

using namespace dsm;
using namespace std;


OutputStream::OutputStream(Output& outputref,int buflen):
    output(outputref),_bufsize(buflen*2),_writelen(buflen),_inbuf(0),_outbuf(1),
    _maxMsecs(1000),_backoffMsecs(250)
{
    // Note that the actual buffer is twice the size that the user
    // requested. This gives us some spare space to store
    // data if the output device is not responding.
    // The actual physical writes won't be larger than buflen.
    _bufs[0] = new char[_bufsize];
    _bufs[1] = new char[_bufsize];
    _bufPtrs[0] = _bufs[0];
    _bufPtrs[1] = _bufs[1];
    _bufN[0] = 0;
    _bufN[1] = 0;
}

OutputStream::~OutputStream()
{
    try {
        close();
    }
    catch(const atdUtil::IOException& e) {}

    delete [] _bufs[0];
    delete [] _bufs[1];
}

/**
 * Buffered atomic write.
 */
bool OutputStream::write(const void**bufs,size_t* lens, int nbufs) throw (atdUtil::IOException)
{
    int i;
    ssize_t l;
    bool result = false;

    dsm_sys_time_t tnow = getCurrentTimeInMillis();
    int tdiff = tnow - _lastWrite;

    /* are we not yet finished writing output buffer? */
    int olen;
    if ((olen = (_bufs[_outbuf] + _bufN[_outbuf] - _bufPtrs[_outbuf])) > 0) {
#ifdef DEBUG
	cerr << " unfinished write, olen=" << olen << endl;
#endif
	if (tdiff >= _backoffMsecs) {
	    try {
		l = output.write(_bufPtrs[_outbuf],olen);
	    }
	    // catch non-blocking write failures.
	    catch (const atdUtil::IOException& ioe) {
		if (ioe.getError() == EAGAIN) l = 0;
		else throw ioe;
	    }
	    _bufPtrs[_outbuf] += l;
	    olen -= l;
	    _lastWrite = tnow;
	    tdiff = 0;
	}
    }

    /* total length of passed buffers */
    size_t tlen = 0;
    for (i = 0; i < nbufs; i++) tlen += lens[i];

    size_t wlen = _bufN[_inbuf];

    // if this send will exceed _writelen or we've waited long enough,
    // then switch buffers and send
    if (wlen + tlen > _writelen || tdiff >= _maxMsecs) {

	// if we have something to write and the output buffer is empty
	if (wlen > 0 && olen == 0) {

	    // switch buffers
	    i = _inbuf; _inbuf = _outbuf; _outbuf = i;

	    // reset input buffer
	    _bufPtrs[_inbuf] = _bufs[_inbuf];
	    _bufN[_inbuf] = 0;

	    _bufPtrs[_outbuf] = _bufs[_outbuf];
	    _bufN[_outbuf] = wlen;

    #ifdef DEBUG
	    cerr << "writing " << wlen << " bytes" << endl;
    #endif
	    if (wlen > _writelen) wlen = _writelen;
	    try {
		l = output.write(_bufPtrs[_outbuf],wlen);
	    }
	    catch (const atdUtil::IOException& ioe) {
		if (ioe.getError() == EAGAIN) l = 0;
		else throw ioe;
	    }
	    _bufPtrs[_outbuf] += l;
	    _lastWrite = tnow;
	}
    }

#ifdef DEBUG
    cerr << "_inbuf=" << _inbuf << " _outbuf=" << _outbuf <<
	  " _bufsize=" << _bufsize << endl;
    cerr << "_bufN[_inbuf]=" << _bufN[_inbuf] <<
	  " _bufPtrs[_inbuf]-_bufs[_inbuf]=" <<
	    _bufPtrs[_inbuf]-_bufs[_inbuf] << endl;
#endif

    /* put sample in input buffer if there is room for it */
    if ((_bufsize - _bufN[_inbuf]) >= tlen) {
	for (i = 0; i < nbufs; i++) {
	    memcpy(_bufPtrs[_inbuf],bufs[i],lens[i]);
	    _bufPtrs[_inbuf] += lens[i];
	    _bufN[_inbuf] += lens[i];
	}
	result = true;	// success
    }

#ifdef DEBUG
    cerr << "_bufN[_outbuf]=" << _bufN[_outbuf] <<
	" _bufPtrs[_outbuf]-_bufs[_outbuf]=" << _bufPtrs[_outbuf]-_bufs[_outbuf] << endl;
#endif

    return result;
}

void OutputStream::flush() throw (atdUtil::IOException)
{
    int i;
    ssize_t l;

    dsm_sys_time_t tnow = getCurrentTimeInMillis();
    int tdiff = tnow - _lastWrite;

    /* are we not yet finished writing output buffer? */
    int olen;
    if ((olen = (_bufs[_outbuf] + _bufN[_outbuf] - _bufPtrs[_outbuf])) > 0) {
#ifdef DEBUG
	cerr << " unfinished write, olen=" << olen << endl;
#endif
	if (tdiff >= _backoffMsecs) {
	    try {
		l = output.write(_bufPtrs[_outbuf],olen);
	    }
	    // catch non-blocking write failures.
	    catch (const atdUtil::IOException& ioe) {
		if (ioe.getError() == EAGAIN) l = 0;
		else throw ioe;
	    }
	    _bufPtrs[_outbuf] += l;
	    olen -= l;
	    _lastWrite = tnow;
	    tdiff = 0;
	}
    }

    int wlen = _bufN[_inbuf];
    if (wlen > 0 && olen == 0) {
	// switch buffers
	i = _inbuf; _inbuf = _outbuf; _outbuf = i;

	// reset input buffer
	_bufPtrs[_inbuf] = _bufs[_inbuf];
	_bufN[_inbuf] = 0;

	_bufPtrs[_outbuf] = _bufs[_outbuf];
	_bufN[_outbuf] = wlen;

    #ifdef DEBUG
	cerr << "writing " << wlen << " bytes" << endl;
    #endif
	if (wlen > _writelen) wlen = _writelen;
	try {
	    l = output.write(_bufPtrs[_outbuf],wlen);
	}
	catch (const atdUtil::IOException& ioe) {
	    if (ioe.getError() == EAGAIN) l = 0;
	    else throw ioe;
	}
	_bufPtrs[_outbuf] += l;
	_lastWrite = tnow;
    }
}

void OutputStream::close() throw(atdUtil::IOException)
{
    output.close();
}
