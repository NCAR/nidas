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
#include <byteswap.h>

using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(SampleOutputStream)

SampleOutputStream::SampleOutputStream(IOChannel* i):
	SampleOutputBase(i),
        _iostream(0),_nsamplesDiscarded(0),_nsamples(0),_lastTimeTag(0)
{
}

/*
 * Copy constructor.
 */
SampleOutputStream::SampleOutputStream(const SampleOutputStream& x):
	SampleOutputBase(x),
        _iostream(0),_nsamplesDiscarded(0),_nsamples(0),_lastTimeTag(0)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */

SampleOutputStream::SampleOutputStream(const SampleOutputStream& x,IOChannel* ioc):
	SampleOutputBase(x,ioc),
	_iostream(0),_nsamplesDiscarded(0),_nsamples(0),_lastTimeTag(0)
{
}

SampleOutputStream::~SampleOutputStream()
{
#ifdef DEBUG
    cerr << "~SampleOutputStream(), this=" << this << endl;
#endif
    delete _iostream;
}

SampleOutputStream* SampleOutputStream::clone(IOChannel* ioc) const
{
    // invoke copy constructor
    if (!ioc) return new SampleOutputStream(*this);
    else return new SampleOutputStream(*this,ioc);
}

void SampleOutputStream::init() throw()
{
    SampleOutputBase::init();
    delete _iostream;
#ifdef DEBUG
    cerr << "SampleOutputStream::init, buffer size=" <<
    	getIOChannel()->getBufferSize() << " fd=" << getIOChannel()->getFd() << endl;
#endif
    _iostream = new IOStream(*getIOChannel(),getIOChannel()->getBufferSize());
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

void SampleOutputStream::finish() throw()
{
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
    bool first_sample = false;
    if (!_iostream) return false;

    dsm_time_t tsamp = samp->getTimeTag();

    try {
	if (tsamp >= getNextFileTime()) {
	    _iostream->flush();
	    createNextFile(tsamp);
	    first_sample = true;
	}
	bool success = write(samp) > 0;
	if (!success) {
	    if (!(_nsamplesDiscarded++ % 1000)) 
		n_u::Logger::getInstance()->log(LOG_WARNING,
		    "%s: %d samples discarded due to output jambs\n",
		    getName().c_str(),_nsamplesDiscarded);
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
    const void* bufs[2];
    size_t lens[2];

#if __BYTE_ORDER == __BIG_ENDIAN
    SampleHeader header;
    header.setTimeTag(bswap_64(samp->getTimeTag()));
    header.setDataByteLength(bswap_32(samp->getDataByteLength()));
    header.setRawId(bswap_32(samp->getRawId()));
    bufs[0] = &header;
    lens[0] = SampleHeader::getSizeOf();
#else
    bufs[0] = samp->getHeaderPtr();
    lens[0] = samp->getHeaderLength();
#endif

    assert(samp->getHeaderLength() == 16);

    bufs[1] = samp->getConstVoidDataPtr();
    lens[1] = samp->getDataByteLength();

    // cerr << "iostream->write" << endl;
#ifdef DEBUG
    if (!(nsamps++ % 100)) cerr << "wrote " << nsamps << " samples" << endl;
#endif
    size_t l = _iostream->write(bufs,lens,2);
    if (l > 0) {
        setLastReceivedTimeTag(samp->getTimeTag());
        incrementNumOutputSamples();
    }
    return l;
}

SortedSampleOutputStream::SortedSampleOutputStream():
	SampleOutputStream(),
    _sorter(0),_proxy(*this),
    _sorterLengthMsecs(250),
#ifdef NIDAS_EMBEDDED
    _heapMax(5000000)
#else
    _heapMax(50000000)
#endif
{
}
/*
 * Copy constructor.
 */
SortedSampleOutputStream::SortedSampleOutputStream(
	const SortedSampleOutputStream& x)
	: SampleOutputStream(x),
    _sorter(0),
    _proxy(*this),
    _sorterLengthMsecs(x._sorterLengthMsecs),
    _heapMax(x._heapMax)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */
SortedSampleOutputStream::SortedSampleOutputStream(
	const SortedSampleOutputStream& x,IOChannel* ioc)
	: SampleOutputStream(x,ioc),
	_sorter(0),
	_proxy(*this),
	_sorterLengthMsecs(x._sorterLengthMsecs),
	_heapMax(x._heapMax)
{
}

SortedSampleOutputStream::~SortedSampleOutputStream()
{
#ifdef DEBUG
    cerr << "~SortedSampleOutputStream(), this=" << this << endl;
#endif
    if (_sorter) {
	_sorter->interrupt();
	n_u::ThreadJoiner* joiner = new n_u::ThreadJoiner(_sorter);
	joiner->start();	// joiner deletes _sorter and itself
    }
}

SortedSampleOutputStream* SortedSampleOutputStream::clone(IOChannel* ioc) const 
{
    if (ioc) return new SortedSampleOutputStream(*this,ioc);
    else return new SortedSampleOutputStream(*this);
}

void SortedSampleOutputStream::init() throw()
{
    SampleOutputStream::init();
    if (getSorterLengthMsecs() > 0) {
	if (!_sorter) _sorter = new SampleSorter("SortedSampleOutputStream");
	_sorter->setLengthMsecs(getSorterLengthMsecs());
	_sorter->setHeapMax(getHeapMax());
	try {
	    _sorter->start();
	}
	catch(const n_u::Exception& e) {
	}
	_sorter->addSampleClient(&_proxy);
    }
}
bool SortedSampleOutputStream::receive(const Sample *s) throw()
{
    if (_sorter) return _sorter->receive(s);
    return SampleOutputStream::receive(s);
}

void SortedSampleOutputStream::finish() throw()
{
    if (_sorter) _sorter->finish();
    SampleOutputStream::finish();
}

void SortedSampleOutputStream::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    SampleOutputStream::fromDOMElement(node);
    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
            // Sample sorter length in seconds
	    if (aname == "sorterLength") {
	        istringstream ist(aval);
		float len;  
		ist >> len;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	"SortedSampleOutputStream",
			attr.getName(),attr.getValue());
		setSorterLengthMsecs((int)rint(len * MSECS_PER_SEC));
	    }
	    else if (aname == "heapMax") {
	        istringstream ist(aval);
		size_t len;
		ist >> len;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	"SortedSampleOutputStream",
			attr.getName(),attr.getValue());
		setHeapMax(len);
	    }
	}
    }
}
