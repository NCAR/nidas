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
    _expectHeader(true),_inputHeaderParsed(false),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_dataToRead(0),_dptr(0),
    _badSamples(0),
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
    _expectHeader(true),_inputHeaderParsed(false),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_dataToRead(0),_dptr(0),
    _badSamples(0),
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
    _inputHeaderParsed(false),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_dataToRead(0),_dptr(0),
    _badSamples(0),
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
        n_u::Inet4Address remoteAddr =
            _iochan->getConnectionInfo().getRemoteSocketAddress().getInet4Address();
        if (remoteAddr != n_u::Inet4Address()) {
            _dsm = Project::getInstance()->findDSM(remoteAddr);
            if (!_dsm) {
                n_u::Socket tmpsock;
                list<n_u::Inet4NetworkInterface> ifaces = tmpsock.getInterfaces();
                tmpsock.close();
                list<n_u::Inet4NetworkInterface>::const_iterator ii = ifaces.begin();
                for ( ; !_dsm && ii != ifaces.end(); ++ii) {
                    n_u::Inet4NetworkInterface iface = *ii;
                    if (iface.getAddress() == remoteAddr) {
                        remoteAddr = n_u::Inet4Address(INADDR_LOOPBACK);
                        _dsm = Project::getInstance()->findDSM(remoteAddr);
                    }
                }
            }
        }
    }
}

string SampleInputStream::getName() const {
    if (_iochan) return string("SampleInputStream: ") + _iochan->getName();
    return string("SampleInputStream");
}

void SampleInputStream::flush() throw()
{
#ifdef DEBUG
    cerr << getName() << " flush, #clients=" << _source.getClientCount() << endl;
#endif
    _source.flush();
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
                "%s: raw sample not of type char(%d): #bad=%zd,filepos=%lld,id=(%d,%d),type=%d,len=%d",
                name.c_str(),CHAR_ST,nbad,pos,GET_DSM_ID(header.getId()),GET_SPS_ID(header.getId()),
                (int)header.getType(),header.getDataByteLength());
        }
        else {
            n_u::Logger::getInstance()->log(LOG_WARNING,
                "%s: bad sample header: #bad=%zd,filepos=%lld,id=(%d,%d),type=%d,len=%ud",
                name.c_str(),nbad,pos,GET_DSM_ID(header.getId()),GET_SPS_ID(header.getId()),
                (int)header.getType(),header.getDataByteLength());
        }
    }
}


/**
 * Read a buffer of data and process all samples in the buffer.
 * This is typically used when a select has determined that there
 * is data available on our file descriptor. Process all available
 * data from the InputStream and distribute() samples to the receive()
 * method of my SampleClients and to the receive() method of
 * DSMSenors.  This will perform only one physical
 * read of the underlying device.
 */
void SampleInputStream::readSamples() throw(n_u::IOException)
{
    size_t len;
    len = _iostream->read();		// read a buffer's worth

    // no data in buffer, and end of a file, or an EAGAIN on a non-blocking read
    if (len == 0 && _iostream->available() == 0) return;

    // first read from a new file
    if (_expectHeader && _iostream->isNewInput()) _inputHeaderParsed = false;
    
    if (!_inputHeaderParsed && !parseInputHeader()) return;

    // process all in buffer
    for (;;) {
	if (_headerToRead > 0) {
	    len = _iostream->readBuf(_hptr,_headerToRead);
            _headerToRead -= len;
            _hptr += len;
            if (_headerToRead > 0) break;   // no more data

#if __BYTE_ORDER == __BIG_ENDIAN
            _sheader.setTimeTag(bswap_64(_sheader.getTimeTag()));
            _sheader.setDataByteLength(bswap_32(_sheader.getDataByteLength()));
            _sheader.setRawId(bswap_32(_sheader.getRawId()));
#endif

            // screen bad headers.
	    if ((_raw && _sheader.getType() != CHAR_ST) ||
                    (_filterBadSamples &&
                    (_sheader.getType() >= UNKNOWN_ST ||
                        GET_DSM_ID(_sheader.getId()) > _maxDsmId ||
                    _sheader.getDataByteLength() > _maxSampleLength ||
                    _sheader.getDataByteLength() == 0 ||
                    _sheader.getTimeTag() < _minSampleTime ||
                    _sheader.getTimeTag() > _maxSampleTime))) {
                    _samp = 0;
            }
            // getSample can return NULL if type or length are bad
            else _samp = nidas::core::getSample((sampleType)_sheader.getType(),
                _sheader.getDataByteLength());

            if (!_samp) {
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
	    _dptr = (char*) _samp->getVoidDataPtr();

	    _samp->setTimeTag(_sheader.getTimeTag());
	    _samp->setId(_sheader.getId());
	}

	len = _iostream->readBuf(_dptr, _dataToRead);
	_dptr += len;
	_dataToRead -= len;
	if (_dataToRead > 0) break;	// no more data in iostream buffer

	_source.distribute(_samp);
	_samp = 0;
        // next read is the header
        _headerToRead = _sheader.getSizeOf();
        _hptr = (char*)&_sheader;
    }
}

/*
 * Read the next sample. The caller must call freeReference on the
 * sample when they're done with it.
 */
Sample* SampleInputStream::readSample() throw(n_u::IOException)
{
    size_t len;
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

            // screen bad headers.
	    if ((_raw && _sheader.getType() != CHAR_ST) ||
                (_filterBadSamples &&
                (_sheader.getType() >= UNKNOWN_ST ||
                GET_DSM_ID(_sheader.getId()) > _maxDsmId ||
                _sheader.getDataByteLength() > _maxSampleLength ||
                _sheader.getDataByteLength() == 0 ||
                _sheader.getTimeTag() < _minSampleTime ||
                _sheader.getTimeTag() > _maxSampleTime))) {
                _samp = 0;
            }
            // getSample can return NULL if type or length are bad
            else _samp = nidas::core::getSample((sampleType)_sheader.getType(),
		    _sheader.getDataByteLength());

            if (!_samp) {
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
	    _dptr = (char*) _samp->getVoidDataPtr();

            _samp->setTimeTag(_sheader.getTimeTag());
            _samp->setId(_sheader.getId());
        }

        while (_dataToRead > 0) {
            len = _iostream->read(_dptr, _dataToRead);
            // new file
            if (_expectHeader && _iostream->isNewInput()) {
                _iostream->backup(len);
                readInputHeader();  // sets _samp to 0
                break;
            }
            _dataToRead -= len;
            _dptr += len;
        }
        if (_samp) {
            Sample* tmp = _samp;
            _samp = 0;
            _headerToRead = _sheader.getSizeOf();
            _hptr = (char*)&_sheader;
            return tmp;
        }
    }
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
                                                           
