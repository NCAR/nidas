// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/dynld/SampleOutputStream.h>
#include <nidas/core/StatusThread.h>

#include <nidas/util/Logger.h>

#include <iostream>

#if __BYTE_ORDER == __BIG_ENDIAN
#include <byteswap.h>
#endif

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(SampleOutputStream)

SampleOutputStream::SampleOutputStream():
    SampleOutputBase(),_iostream(0),
    _maxUsecs(0),_lastFlushTT(0)
{
    _maxUsecs = (int)(getLatency() * USECS_PER_SEC);
    _maxUsecs = std::max(_maxUsecs,USECS_PER_SEC / 50);
}

SampleOutputStream::SampleOutputStream(IOChannel* i):
    SampleOutputBase(i),_iostream(0),
    _maxUsecs(0),_lastFlushTT(0)
{
    _maxUsecs = (int)(getLatency() * USECS_PER_SEC);
    _maxUsecs = std::max(_maxUsecs,USECS_PER_SEC / 50);
    _iostream = new IOStream(*getIOChannel(),getIOChannel()->getBufferSize());
    setName("SampleOutputStream: " + getIOChannel()->getName());
}

/*
 * Copy constructor, with a new IOChannel.
 */

SampleOutputStream::SampleOutputStream(SampleOutputStream& x,IOChannel* ioc):
    SampleOutputBase(x,ioc),_iostream(0),
    _maxUsecs(0),_lastFlushTT(0)
{
    _maxUsecs = (int)(getLatency() * USECS_PER_SEC);
    _maxUsecs = std::max(_maxUsecs,USECS_PER_SEC / 50);
    _iostream = new IOStream(*getIOChannel(),getIOChannel()->getBufferSize());
    setName("SampleOutputStream: " + getIOChannel()->getName());
}

SampleOutputStream::~SampleOutputStream()
{
#ifdef DEBUG
    cerr << "~SampleOutputStream(), this=" << this << endl;
#endif
    delete _iostream;
}

SampleOutputStream* SampleOutputStream::clone(IOChannel* ioc)
{
    // invoke copy constructor
    return new SampleOutputStream(*this,ioc);
}

void SampleOutputStream::close() throw(n_u::IOException)
{
#ifdef DEBUG
    cerr << "SampleOutputStream::close" << endl;
#endif
    delete _iostream;
    _iostream = 0;
    SampleOutputBase::close();
}

void SampleOutputStream::setLatency(float val)
    	throw(nidas::util::InvalidParameterException)
{
    int usecs = (int)(val * USECS_PER_SEC);
    if (usecs < USECS_PER_SEC / 50 || usecs > USECS_PER_SEC * 60)
        throw n_u::InvalidParameterException(getName(),"latency","out of range");
    SampleOutputBase::setLatency(val);
    _maxUsecs = usecs;
}

SampleOutput* SampleOutputStream::connected(IOChannel* ioc) throw()
{
    // If this is a new IOChannel, then SampleOutputBase::connected
    // will create and return a clone of this SampleOutputStream.
    // Otherwise we need to create the IOStream.
    if (ioc == getIOChannel()) {
        delete _iostream;
        _iostream = new IOStream(*getIOChannel(),getIOChannel()->getBufferSize());
    }
    SampleOutput* so = SampleOutputBase::connected(ioc);
    if (so == this && !_iostream)
        _iostream = new IOStream(*getIOChannel(),getIOChannel()->getBufferSize());
    return so;
}

void SampleOutputStream::finish() throw()
{
#ifdef DEBUG
    cerr << "SampleOutputStream::finish, name=" << getName() << endl;
#endif
    try {
	if (_iostream) _iostream->flush();
    }
    catch (n_u::IOException& ioe) {
        // Don't log an EPIPE error on finish(). It has very likely been
        // logged when writing samples in the receive(const Sample*) method.
        if (ioe.getError() != EPIPE)
            n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s",getName().c_str(),ioe.what());
    }
}

bool SampleOutputStream::receive(const Sample *samp) throw()
{
#ifdef DEBUG
    cerr << "SampleOutputStream::receive sample id=" <<
        samp->getDSMId() << ',' << samp->getSpSId() << endl;
#endif

    dsm_time_t tsamp = samp->getTimeTag();
    bool streamFlush = false;

    try {
	if (tsamp >= getNextFileTime()) {
            if (_iostream) _iostream->flush();
	    createNextFile(tsamp);
	}
        if ((tsamp - _lastFlushTT) > _maxUsecs) {
            _lastFlushTT = tsamp;
            streamFlush = true;
        }

	bool success = write(samp,streamFlush) > 0;
	if (!success) {
	    if (!(incrementDiscardedSamples() % 1000)) 
		n_u::Logger::getInstance()->log(LOG_WARNING,
		    "%s: %zd samples discarded due to output jambs\n",
		    getName().c_str(),getNumDiscardedSamples());
	}
    }
    catch(const n_u::IOException& ioe) {
        // broken pipe is the typical result of a client closing its end of the socket.
        // Just report a notice, not an error.
        if (ioe.getError() == EPIPE)
            n_u::Logger::getInstance()->log(LOG_NOTICE,
                "%s: %s, disconnecting",getName().c_str(),ioe.what());
        else
            n_u::Logger::getInstance()->log(LOG_ERR,
                "%s: %s, disconnecting",getName().c_str(),ioe.what());
        // this disconnect will schedule this object to be deleted
        // in another thread, so don't do anything after the
        // disconnect except return;
	disconnect();
	return false;
    }
    return true;
}

size_t SampleOutputStream::write(const void* buf, size_t len, bool flush)
	throw(n_u::IOException)
{
    if (!_iostream) return 0;
    return _iostream->write(buf,len,flush);
}

size_t SampleOutputStream::write(const Sample* samp, bool streamFlush) throw(n_u::IOException)
{
    if (!_iostream) return 0;
#ifdef DEBUG
    static int nsamps = 0;
#endif
    struct iovec iov[2];

#if __BYTE_ORDER == __BIG_ENDIAN
    SampleHeader header;
    header.setTimeTag(bswap_64(samp->getTimeTag()));
    header.setDataByteLength(bswap_32(samp->getDataByteLength()));
    header.setRawId(bswap_32(samp->getRawId()));
    iov[0].iov_base = &header;
    iov[0].iov_len = SampleHeader::getSizeOf();
#else
    iov[0].iov_base = const_cast<void*>(samp->getHeaderPtr());
    iov[0].iov_len = samp->getHeaderLength();
#endif

    assert(samp->getHeaderLength() == 16);

    iov[1].iov_base = const_cast<void*>(samp->getConstVoidDataPtr());
    iov[1].iov_len = samp->getDataByteLength();

    // cerr << "iostream->write" << endl;
#ifdef DEBUG
    if (!(nsamps++ % 100)) cerr << "wrote " << nsamps << " samples" << endl;
#endif
    size_t l = _iostream->write(iov,2,streamFlush);
    return l;
}

