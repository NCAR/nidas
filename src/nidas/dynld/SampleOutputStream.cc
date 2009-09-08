/* -*- mode: c++; c-basic-offset: 4; -*-
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
	SampleOutputBase(),_iostream(0)
{
}

SampleOutputStream::SampleOutputStream(IOChannel* i):
	SampleOutputBase(i)
{
    _iostream = new IOStream(*getIOChannel(),getIOChannel()->getBufferSize());
}

/*
 * Copy constructor, with a new IOChannel.
 */

SampleOutputStream::SampleOutputStream(SampleOutputStream& x,IOChannel* ioc):
	SampleOutputBase(x,ioc)
{
    _iostream = new IOStream(*getIOChannel(),getIOChannel()->getBufferSize());
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

SampleOutput* SampleOutputStream::connected(IOChannel* ioc) throw()
{
    SampleOutput* so = SampleOutputBase::connected(ioc);
    // If a clone is not returned, create the iostream
    if (so == this) {
        delete _iostream;
        _iostream = new IOStream(*getIOChannel(),getIOChannel()->getBufferSize());
    }
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
    bool first_sample = false;

    dsm_time_t tsamp = samp->getTimeTag();

    try {
	if (tsamp >= getNextFileTime()) {
	    _iostream->flush();
	    createNextFile(tsamp);
	    first_sample = true;
	}
	bool success = write(samp) > 0;
	if (!success) {
	    if (!(incrementDiscardedSamples() % 1000)) 
		n_u::Logger::getInstance()->log(LOG_WARNING,
		    "%s: %lld samples discarded due to output jambs\n",
		    getName().c_str(),getNumDiscardedSamples());
	}
	else if (first_sample) {
	    // Force the first sample to get written out with the header,
	    // so that initial samples from slower streams are not delayed
	    // by the iostream buffering.
	    _iostream->flush();
	}
    }
    catch(const n_u::IOException& ioe) {
	n_u::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s",getName().c_str(),ioe.what());
	disconnect();
	return false;
    }
    return true;
}

size_t SampleOutputStream::write(const void* buf, size_t len)
	throw(n_u::IOException)
{
    return _iostream->write(buf,len);
}

size_t SampleOutputStream::write(const Sample* samp) throw(n_u::IOException)
{
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
    size_t l = _iostream->write(iov,2);
    return l;
}

