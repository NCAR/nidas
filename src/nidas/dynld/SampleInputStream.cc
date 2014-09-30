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

#include <nidas/core/Project.h>
#include <nidas/dynld/SampleInputStream.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/DSMService.h>
#include <nidas/core/IOChannel.h>
#include <nidas/core/IOStream.h>
#include <nidas/util/Socket.h>

#include <byteswap.h>

#include <nidas/util/Logger.h>

#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(SampleInputStream)

/*
 * Constructor
 */
SampleInputStream::SampleInputStream(bool raw):
    _iochan(0),_source(raw),_service(0),_iostream(0),_dsm(0),
    _expectHeader(true),_inputHeaderParsed(false),_sheader(),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_dataToRead(0),_dptr(0),
    _badSamples(0),_inputHeader(),
    _filterBadSamples(false),_maxDsmId(1024),
    _maxSampleLength(UINT_MAX),
    _minSampleTime(LONG_LONG_MIN),
    _maxSampleTime(LONG_LONG_MAX),
    _original(this),_raw(raw)
{
}

/*
 * Constructor, with a connected IOChannel.
 */
SampleInputStream::SampleInputStream(IOChannel* iochannel, bool raw):
    _iochan(0),_source(raw),_service(0),_iostream(0),_dsm(0),
    _expectHeader(true),_inputHeaderParsed(false),_sheader(),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_dataToRead(0),_dptr(0),
    _badSamples(0),_inputHeader(),
    _filterBadSamples(false),_maxDsmId(1024),
    _maxSampleLength(UINT_MAX),
    _minSampleTime(LONG_LONG_MIN),
    _maxSampleTime(LONG_LONG_MAX),
    _original(this),_raw(raw)
{
    setIOChannel(iochannel);
    _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
}

/*
 * Copy constructor, with a new, connected IOChannel.
 */
SampleInputStream::SampleInputStream(SampleInputStream& x,
	IOChannel* iochannel):
    _iochan(0),_source(x._source),_service(x._service),_iostream(0),
    _dsm(x._dsm),
    _expectHeader(x._expectHeader),
    _inputHeaderParsed(false),_sheader(),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_dataToRead(0),_dptr(0),
    _badSamples(0),_inputHeader(),
    _filterBadSamples(x._filterBadSamples),_maxDsmId(x._maxDsmId),
    _maxSampleLength(x._maxSampleLength),_minSampleTime(x._minSampleTime),
    _maxSampleTime(x._maxSampleTime),
    _original(&x),_raw(x._raw)
{
    setIOChannel(iochannel);
    _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
}

/*
 * Clone myself, with a new IOChannel.
 */
SampleInputStream* SampleInputStream::clone(IOChannel* iochannel)
{
    return new SampleInputStream(*this,iochannel);
}

SampleInputStream::~SampleInputStream()
{
    if (_samp)
        _samp->freeReference();
    delete _iostream;
    delete _iochan;
}

void SampleInputStream::setIOChannel(IOChannel* val)
{
    if (val != _iochan) {
        if (_iochan) _iochan->close();
	delete _iochan;
	_iochan = val;
    }
    if (_iochan) {
        setExpectHeader(_iochan->writeNidasHeader());
        n_u::Inet4Address remoteAddr =
            _iochan->getConnectionInfo().getRemoteSocketAddress().getInet4Address();
        if (remoteAddr != n_u::Inet4Address()) {
            _dsm = Project::getInstance()->findDSM(remoteAddr);
        }
    }
}

void SampleInputStream::setNonBlocking(bool val) throw(n_u::IOException)
{
    if (_iochan) _iochan->setNonBlocking(val);
}

bool SampleInputStream::isNonBlocking() const throw(n_u::IOException)
{
    if (_iochan) return _iochan->isNonBlocking();
    return false;
}

string SampleInputStream::getName() const {
    if (_iochan) return string("SampleInputStream: ") + _iochan->getName();
    return string("SampleInputStream");
}

int SampleInputStream::getFd() const 
{
    if (_iochan) return _iochan->getFd();
    return -1;
}

void SampleInputStream::flush() throw()
{
#ifdef DEBUG
    cerr << getName() << " flush, #clients=" << _source.getClientCount() << endl;
#endif
    // process all samples in buffer
    for (;;) {
        Sample* samp = nextSample();
        if (!samp) break;
        _source.distribute(samp);
    }
}

void SampleInputStream::requestConnection(DSMService* requester)
            throw(n_u::IOException)
{
    _service = requester;
    _iochan->requestConnection(this);
}

SampleInput* SampleInputStream::connected(IOChannel* ioc) throw()
{
    if (_iochan && _iochan != ioc) {
        if (_service) {
            SampleInputStream* newist = clone(ioc);
            _service->connect(newist);
            return newist;
        }
        else {
            setIOChannel(ioc);
            delete _iostream;
            _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
        }
    }
    else {
        setIOChannel(ioc);
        delete _iostream;
        _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
        if (_service) _service->connect(this);
    }
    return this;
}

#ifdef NEEDED

void SampleInputStream::init() throw()
{
#ifdef DEBUG
    cerr << "SampleInputStream::init(), buffer size=" << 
    	_iochan->getBufferSize() << endl;
#endif
    if (!_iostream)
	_iostream = new IOStream(*_iochan,_iochan->getBufferSize());
}
#endif

void SampleInputStream::close() throw(n_u::IOException)
{
    delete _iostream;
    _iostream = 0;
    _iochan->close();
}

const DSMConfig* SampleInputStream::getDSMConfig() const
{
    return _dsm;
}

void SampleInputStream::readInputHeader() throw(n_u::IOException)
{
    if (_samp) _samp->freeReference();
    _samp = 0;
    _headerToRead = _sheader.getSizeOf();
    _hptr = (char*)&_sheader;
    _dataToRead = 0;
    _inputHeader.read(_iostream);
    _inputHeaderParsed = true;
}

bool SampleInputStream::parseInputHeader() throw(n_u::IOException)
{
    if (!_expectHeader) {
        _inputHeaderParsed = true;
        return true;
    }
    if (_samp) _samp->freeReference();
    _samp = 0;
    _headerToRead = _sheader.getSizeOf();
    _hptr = (char*)&_sheader;
    _dataToRead = 0;
    try {
        _inputHeaderParsed = _inputHeader.parse(_iostream);
    }
    catch(const n_u::ParseException& e) {
        throw n_u::IOException(getName(),"read header",e.what());
    }
    return _inputHeaderParsed;
}

namespace {
    void logBadSampleHeader(const string& name,size_t nbad,long long pos,bool raw, const SampleHeader& header)
    {
        if (raw && header.getType() != CHAR_ST) {
            n_u::Logger::getInstance()->log(LOG_WARNING,
                "%s: raw sample not of type char(%d): #bad=%zd,filepos=%lld,id=(%d,%d),type=%d,len=%u",
                name.c_str(),CHAR_ST,nbad,pos,GET_DSM_ID(header.getId()),GET_SPS_ID(header.getId()),
                (int)header.getType(),header.getDataByteLength());
        }
        else {
            n_u::Logger::getInstance()->log(LOG_WARNING,
                "%s: bad sample header: #bad=%zd,filepos=%lld,id=(%d,%d),type=%d,len=%u",
                name.c_str(),nbad,pos,GET_DSM_ID(header.getId()),GET_SPS_ID(header.getId()),
                (int)header.getType(),header.getDataByteLength());
        }
    }
}


/*
 * Read a buffer of data and process all samples in the buffer.
 * This is typically used when a select has determined that there
 * is data available on our file descriptor. Process all available
 * data from the InputStream and distribute() samples to the receive()
 * method of my SampleClients and to the receive() method of
 * DSMSenors.  This will perform only one physical
 * read of the underlying device.
 */
bool SampleInputStream::readSamples() throw(n_u::IOException)
{
    _iostream->read();		// read a buffer's worth

    // no data in buffer after above read, must have been
    // EAGAIN on a non-blocking read
    if (_iostream->available() == 0) return false;

    // first read from a new file
    if (_expectHeader && _iostream->isNewInput()) _inputHeaderParsed = false;
    
    if (!_inputHeaderParsed && !parseInputHeader()) return true;

    // process all samples in buffer
    for (;;) {
        Sample* samp = nextSample();
        if (!samp) break;
        _source.distribute(samp);
    }
    return true;
}


bool
SampleInputStream::
readSampleHeader(bool keepreading) throw(n_u::IOException)
{
    while (_headerToRead > 0) {
        size_t len;
        if (keepreading)
            len = _iostream->read(_hptr, _headerToRead);
        else
            len = _iostream->readBuf(_hptr, _headerToRead);
        _headerToRead -= len;
        _hptr += len;

        if (keepreading && _expectHeader && _iostream->isNewInput()) {
                _iostream->backup(len);
                readInputHeader();
        }
        if (!keepreading && _headerToRead > 0) 
            return false;   // no more data
    }
    return true;
}


bool
SampleInputStream::
readSampleData(bool keepreading) throw(n_u::IOException)
{
    while (_dataToRead > 0) {
        size_t len;
        if (keepreading)
            len = _iostream->read(_dptr, _dataToRead);
        else
            len = _iostream->readBuf(_dptr, _dataToRead);

        if (keepreading && _expectHeader && _iostream->isNewInput()) {
            _iostream->backup(len);
            readInputHeader();  // sets _samp to 0
            // abort this sample data and start over
            return false;
        }
        _dataToRead -= len;
        _dptr += len;

        if (!keepreading && _dataToRead > 0)
            return false;   // no more data
    }
    return true;
}


/*
 * Use readNextSample() to return the next sample from the buffer, if any.
 */
Sample* SampleInputStream::nextSample() throw()
{
    try {
        return nextSample(false);
    }
    catch (n_u::IOException& ioe)
    {
        // this should not happen when keepreading is passed as false.
    }
    return 0;
}

/*
 * Try to read a sample, either by reading more from the stream or by
 * reading exclusively from the buffer.  If @p keepreading is false, then
 * only read what's in the buffer and return NULL if there is not a
 * complete sample available.  Otherwise keep reading until a full sample
 * is read.
 */
Sample* SampleInputStream::nextSample(bool keepreading) throw(n_u::IOException)
{
    for (;;) {
        if (_headerToRead > 0) {

            if (! readSampleHeader(keepreading))
                return 0;

#if __BYTE_ORDER == __BIG_ENDIAN
            _sheader.setTimeTag(bswap_64(_sheader.getTimeTag()));
            _sheader.setDataByteLength(bswap_32(_sheader.getDataByteLength()));
            _sheader.setRawId(bswap_32(_sheader.getRawId()));
#endif

            _samp = sampleFromHeader();
            if (!_samp) {
                continue;
            }

            _dataToRead = _sheader.getDataByteLength();
            _dptr = (char*) _samp->getVoidDataPtr();

            _samp->setTimeTag(_sheader.getTimeTag());
            _samp->setId(_sheader.getId());
        }

        if (!readSampleData(keepreading))
        {
            if (!keepreading)
                return 0;
            continue;
        }

        Sample* out = _samp;
        _samp = 0;
        // next read is the header
        _headerToRead = _sheader.getSizeOf();
        _hptr = (char*)&_sheader;
        return out;
    }
}


Sample*
SampleInputStream::
sampleFromHeader() throw()
{
    Sample* samp = 0;

    // screen bad headers.
    if ((_raw && _sheader.getType() != CHAR_ST) ||
        (_filterBadSamples &&
         (_sheader.getType() >= UNKNOWN_ST ||
          GET_DSM_ID(_sheader.getId()) > _maxDsmId ||
          _sheader.getDataByteLength() > _maxSampleLength ||
          _sheader.getDataByteLength() == 0 ||
          _sheader.getTimeTag() < _minSampleTime ||
          _sheader.getTimeTag() > _maxSampleTime))) {
        samp = 0;
    }
    // getSample can return NULL if type or length are bad
    else {
        samp = nidas::core::getSample((sampleType)_sheader.getType(),
                                      _sheader.getDataByteLength());
    }

    if (!samp) {
        if (!(_badSamples++ % 1000))
            logBadSampleHeader(getName(),_badSamples,
                               _iostream->getNumInputBytes()-_sheader.getSizeOf(),
                               _raw,_sheader);
        // bad header. Shift left by one byte, read next byte.
        memmove(&_sheader,((const char *)&_sheader)+1, _sheader.getSizeOf() - 1);
        _headerToRead = 1;
        _hptr--;
    }
    return samp;
}


/*
 * Read the next full sample. The caller must call freeReference on the
 * sample when they're done with it.
 */
Sample* SampleInputStream::readSample() throw(n_u::IOException)
{
    return nextSample(true);
}

/*
 * Search for a sample with timetag >= tt.
 */
void SampleInputStream::search(const n_u::UTime& tt) throw(n_u::IOException)
{
    size_t len;
    if (_samp) _samp->freeReference();
    _samp = 0;
    for (;;) {
        if (_headerToRead > 0) {
            while (_headerToRead > 0) {
		len = _iostream->read(_hptr,_headerToRead);
                _headerToRead -= len;
                _hptr += len;
                // new file
                if (_expectHeader && _iostream->isNewInput()) {
                    _iostream->backup(len);
                    readInputHeader();
                }
            }

#if __BYTE_ORDER == __BIG_ENDIAN
            _sheader.setTimeTag(bswap_64(_sheader.getTimeTag()));
            _sheader.setDataByteLength(bswap_32(_sheader.getDataByteLength()));
            _sheader.setRawId(bswap_32(_sheader.getRawId()));
#endif
	    if ((_raw && _sheader.getType() != CHAR_ST) ||
                (_filterBadSamples &&
                (_sheader.getType() >= UNKNOWN_ST ||
                    GET_DSM_ID(_sheader.getId()) > _maxDsmId ||
                _sheader.getDataByteLength() > _maxSampleLength ||
                _sheader.getDataByteLength() == 0 ||
                _sheader.getTimeTag() < _minSampleTime ||
                _sheader.getTimeTag() > _maxSampleTime))) {
                if (!(_badSamples++ % 1000))
                    logBadSampleHeader(getName(),_badSamples,
                            _iostream->getNumInputBytes()-_sheader.getSizeOf(),_raw,_sheader);
                // bad header. Shift left by one byte, read next byte.
                memmove(&_sheader,((const char *)&_sheader)+1,_sheader.getSizeOf() - 1);
                _headerToRead = 1;
                _hptr--;
                continue;
            }

            _dataToRead = _sheader.getDataByteLength();
            // cerr << "time=" << n_u::UTime(_sheader.getTimeTag()).format(true,"%c %6f") << endl;
            if (_sheader.getTimeTag() >= tt.toUsecs()) {
                // getSample can return NULL if type or length are bad
                _samp = nidas::core::getSample((sampleType)_sheader.getType(),
                    _sheader.getDataByteLength());
                if (!_samp) {
                    if (!(_badSamples++ % 1000))
                        logBadSampleHeader(getName(),_badSamples,
                                _iostream->getNumInputBytes()-_sheader.getSizeOf(),_raw,_sheader);
                    // bad header. Shift left by one byte, read next byte.
                    memmove(&_sheader,((const char *)&_sheader)+1,_sheader.getSizeOf() - 1);
                    _headerToRead = 1;
                    _hptr--;
                    _dataToRead = 0;
                    continue;
                }
                _samp->setTimeTag(_sheader.getTimeTag());
                _samp->setId(_sheader.getId());
                _dptr = (char*) _samp->getVoidDataPtr();
                return;
            }
        }
        while (_dataToRead > 0) {
            len = _iostream->skip(_dataToRead);
            // new file
            if (_expectHeader && _iostream->isNewInput()) {
                _iostream->backup(len);
                readInputHeader();
                break;
            }
            _dataToRead -= len;
        }
	_headerToRead = _sheader.getSizeOf();
	_hptr = (char*)&_sheader;
    }
}

/*
 * process <input> element
 */
void SampleInputStream::fromDOMElement(const xercesc::DOMElement* node)
        throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);

    if(node->hasAttributes()) {
    // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            // const std::string& aval = attr.getValue();
            // Sample sorter length in seconds
	    if (aname == "sorterLength") {
                WLOG(("SampleInputStream: attribute ") << aname << " is deprecated");
#ifdef NEEDED
	        istringstream ist(aval);
		float len;
		ist >> len;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	"SampleInputStream",
			attr.getName(),attr.getValue());
		setSorterLengthMsecs((int)rint(len * MSECS_PER_SEC));
#endif
	    }
	    else if (aname == "heapMax") {
                WLOG(("SampleInputStream: attribute ") << aname << " is deprecated");
#ifdef NEEDED
	        istringstream ist(aval);
		int len;
		ist >> len;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	"SampleInputStream",
			attr.getName(),attr.getValue());
		setHeapMax(len);
#endif
	    }
	}
    }

    // process <socket>, <fileset> child elements (should only be one)

    int niochan = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;

        IOChannel* iochan = IOChannel::createIOChannel((const xercesc::DOMElement*)child);
	iochan->fromDOMElement((xercesc::DOMElement*)child);
        setIOChannel(iochan);

	if (++niochan > 1)
	    throw n_u::InvalidParameterException(
		    "SampleInputStream::fromDOMElement",
		    "input", "must have one child element");
    }
    if (!_iochan)
        throw n_u::InvalidParameterException(
                "SampleInputStream::fromDOMElement",
		"input", "must have one child element");
}
                                                           
