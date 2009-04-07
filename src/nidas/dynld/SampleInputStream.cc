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

#include <byteswap.h>

#include <nidas/util/Logger.h>

#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(SampleInputStream)

/*
 * Constructor, with a IOChannel (which may be null).
 */
SampleInputStream::SampleInputStream(IOChannel* iochannel):
    _service(0),_iochan(iochannel),_iostream(0),
    _inputHeaderParsed(false),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_dataToRead(0),_dptr(0),
    _badSamples(0),
    _filterBadSamples(false),_maxDsmId(1024),
    _maxSampleLength(UINT_MAX),
    _minSampleTime(LONG_LONG_MIN),
    _maxSampleTime(LONG_LONG_MAX),
    _nsamples(0),_lastTimeTag(0LL)
{
    if (_iochan)
        _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
    // _minSampleTime = n_u::UTime::parse(true,"2004 jan 1 00:00").toUsecs();
    // _maxSampleTime = n_u::UTime::parse(true,"2010 jan 1 00:00").toUsecs();
}

/*
 * Copy constructor, with a new IOChannel.
 */
SampleInputStream::SampleInputStream(const SampleInputStream& x,
	IOChannel* iochannel):
    _service(x._service),
    _iochan(iochannel),_iostream(0),
    _sampleTags(x._sampleTags),
    _inputHeaderParsed(false),
    _headerToRead(_sheader.getSizeOf()),_hptr((char*)&_sheader),
    _samp(0),_dataToRead(0),_dptr(0),
    _badSamples(0),
    _filterBadSamples(x._filterBadSamples),_maxDsmId(x._maxDsmId),
    _maxSampleLength(x._maxSampleLength),_minSampleTime(x._minSampleTime),
    _maxSampleTime(x._maxSampleTime),
    _nsamples(0),_lastTimeTag(0LL)
{
    if (_iochan)
        _iostream = new IOStream(*_iochan,_iochan->getBufferSize());
    // _minSampleTime = n_u::UTime::parse(true,"2004 jan 1 00:00").toUsecs();
    // _maxSampleTime = n_u::UTime::parse(true,"2010 jan 1 00:00").toUsecs();
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

string SampleInputStream::getName() const {
    if (_iochan) return string("SampleInputStream: ") + _iochan->getName();
    return string("SampleInputStream");
}

void SampleInputStream::requestConnection(DSMService* requester)
            throw(n_u::IOException)
{
    _service = requester;
    _iochan->requestConnection(this);
}

void SampleInputStream::connected(IOChannel* iochannel) throw()
{
    // cerr << "SampleInputStream connected, iochannel=" <<
    //	iochannel->getRemoteInet4Address().getHostAddress() << endl;
    // this create a clone of myself
    _service->connected(clone(iochannel));
}

void SampleInputStream::addProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    _sensorMapMutex.lock();
    _sensorMap[sensor->getId()] = sensor;

    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
    	_sensorsByClient.find(client);
    if (sci != _sensorsByClient.end()) sci->second.push_back(sensor);
    else {
        list<DSMSensor*> sensors;
	sensors.push_back(sensor);
	_sensorsByClient[client] = sensors;
    }
    _sensorMapMutex.unlock();

    sensor->addSampleClient(client);
}

void SampleInputStream::removeProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    if (!sensor) {		// remove client for all sensors
	_sensorMapMutex.lock();
	map<SampleClient*,list<DSMSensor*> >::iterator sci = 
	    _sensorsByClient.find(client);
	if (sci != _sensorsByClient.end()) {
	    list<DSMSensor*>& sensors = sci->second;
	    for (list<DSMSensor*>::iterator si = sensors.begin();
	    	si != sensors.end(); ++si) {
		sensor = *si;
		sensor->removeSampleClient(client);
		if (sensor->getClientCount() == 0)
		    _sensorMap.erase(sensor->getId());
	    }
	}
	_sensorMapMutex.unlock();
    }
    else {
        sensor->removeSampleClient(client);
	if (sensor->getClientCount() == 0)
	    _sensorMap.erase(sensor->getId());
    }
}

void SampleInputStream::init() throw()
{
#ifdef DEBUG
    cerr << "SampleInputStream::init(), buffer size=" << 
    	_iochan->getBufferSize() << endl;
#endif
    if (!_iostream)
	_iostream = new IOStream(*_iochan,_iochan->getBufferSize());
}

void SampleInputStream::close() throw(n_u::IOException)
{
    delete _iostream;
    _iostream = 0;
    _iochan->close();
}

n_u::Inet4Address SampleInputStream::getRemoteInet4Address() const
{
    if (_iochan) return _iochan->getRemoteInet4Address();
    else return n_u::Inet4Address();
}

void SampleInputStream::readInputHeader() throw(n_u::IOException)
{
    if (_samp) _samp->freeReference();
    _samp = 0;
    _headerToRead = _sheader.getSizeOf();
    _hptr = (char*)&_sheader;
    _dataToRead = 0;
    inputHeader.read(_iostream);
    _inputHeaderParsed = true;
}

bool SampleInputStream::parseInputHeader() throw(n_u::IOException)
{
    if (_samp) _samp->freeReference();
    _samp = 0;
    _headerToRead = _sheader.getSizeOf();
    _hptr = (char*)&_sheader;
    _dataToRead = 0;
    try {
        _inputHeaderParsed = inputHeader.parse(_iostream);
    }
    catch(const n_u::ParseException& e) {
        throw n_u::IOException(getName(),"read header",e.what());
    }
    return _inputHeaderParsed;
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
    if (_iostream->isNewInput()) _inputHeaderParsed = false;
    
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
	    if (_filterBadSamples &&
                    (_sheader.getType() >= UNKNOWN_ST ||
                        GET_DSM_ID(_sheader.getId()) > _maxDsmId ||
                    _sheader.getDataByteLength() > _maxSampleLength ||
                    _sheader.getDataByteLength() == 0 ||
                    _sheader.getTimeTag() < _minSampleTime ||
                    _sheader.getTimeTag() > _maxSampleTime)) {
                    _samp = 0;
            }
            // getSample can return NULL if type or length are bad
            else _samp = nidas::core::getSample((sampleType)_sheader.getType(),
                _sheader.getDataByteLength());

            if (!_samp) {
                if (!(_badSamples++ % 1000)) {
                    n_u::Logger::getInstance()->log(LOG_WARNING,
                        "%s: bad sample hdr: #bad=%d,filepos=%d,id=(%d,%d),type=%d,len=%d",
                        getName().c_str(), _badSamples,
                        _iostream->getNumInputBytes()-_sheader.getSizeOf(),
                        GET_DSM_ID(_sheader.getId()),GET_SHORT_ID(_sheader.getId()),
                        _sheader.getType(),_sheader.getDataByteLength());
                }
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
	// cerr << "read len=" << len << endl;
	_dptr += len;
	_dataToRead -= len;
	if (_dataToRead > 0) break;	// no more data in iostream buffer

        incrementNumInputSamples();
        setLastDistributedTimeTag(_samp->getTimeTag());
	distribute(_samp);
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
                if (_iostream->isNewInput()) {
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
            if (_filterBadSamples &&
                (_sheader.getType() >= UNKNOWN_ST ||
                GET_DSM_ID(_sheader.getId()) > _maxDsmId ||
                _sheader.getDataByteLength() > _maxSampleLength ||
                _sheader.getDataByteLength() == 0 ||
                _sheader.getTimeTag() < _minSampleTime ||
                _sheader.getTimeTag() > _maxSampleTime)) {
                _samp = 0;
            }
            // getSample can return NULL if type or length are bad
            else _samp = nidas::core::getSample((sampleType)_sheader.getType(),
		    _sheader.getDataByteLength());

            if (!_samp) {
                if (!(_badSamples++ % 1000)) {
                    n_u::Logger::getInstance()->log(LOG_WARNING,
                        "%s: bad sample hdr: #bad=%d,filepos=%d,id=(%d,%d),type=%d,len=%d",
                        getName().c_str(), _badSamples,
                        _iostream->getNumInputBytes()-_sheader.getSizeOf(),
                        GET_DSM_ID(_sheader.getId()),GET_SHORT_ID(_sheader.getId()),
                        _sheader.getType(),_sheader.getDataByteLength());
                }
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
            if (_iostream->isNewInput()) {
                _iostream->backup(len);
                readInputHeader();
                break;
            }
            _dataToRead -= len;
            _dptr += len;
        }
        if (_dataToRead == 0) {
            Sample* tmp = _samp;
            _samp = 0;
            _headerToRead = _sheader.getSizeOf();
            _hptr = (char*)&_sheader;
            incrementNumInputSamples();
            setLastDistributedTimeTag(tmp->getTimeTag());
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
                if (_iostream->isNewInput()) {
                    _iostream->backup(len);
                    readInputHeader();
                }
            }

#if __BYTE_ORDER == __BIG_ENDIAN
            _sheader.setTimeTag(bswap_64(_sheader.getTimeTag()));
            _sheader.setDataByteLength(bswap_32(_sheader.getDataByteLength()));
            _sheader.setRawId(bswap_32(_sheader.getRawId()));
#endif
            if (_filterBadSamples &&
                (_sheader.getType() >= UNKNOWN_ST ||
                    GET_DSM_ID(_sheader.getId()) > _maxDsmId ||
                _sheader.getDataByteLength() > _maxSampleLength ||
                _sheader.getDataByteLength() == 0 ||
                _sheader.getTimeTag() < _minSampleTime ||
                _sheader.getTimeTag() > _maxSampleTime)) {
                if (!(_badSamples++ % 1000)) {
                    n_u::Logger::getInstance()->log(LOG_WARNING,
                        "%s: bad sample hdr: #bad=%d,filepos=%d,id=(%d,%d),type=%d,len=%d",
                        getName().c_str(), _badSamples,
                        _iostream->getNumInputBytes()-_sheader.getSizeOf(),
                        GET_DSM_ID(_sheader.getId()),GET_SHORT_ID(_sheader.getId()),
                        _sheader.getType(),_sheader.getDataByteLength());
                }
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
                    if (!(_badSamples++ % 1000)) {
                        n_u::Logger::getInstance()->log(LOG_WARNING,
                            "%s: bad sample hdr: #bad=%d,filepos=%d,id=(%d,%d),type=%d,len=%d",
                            getName().c_str(), _badSamples,
                            _iostream->getNumInputBytes()-_sheader.getSizeOf(),
                            GET_DSM_ID(_sheader.getId()),GET_SHORT_ID(_sheader.getId()),
                            _sheader.getType(),_sheader.getDataByteLength());
                    }
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
            if (_iostream->isNewInput()) {
                _iostream->backup(len);
                readInputHeader();
                break;
            }
            _dataToRead -= len;
        }
        incrementNumInputSamples();
        setLastDistributedTimeTag(_sheader.getTimeTag());

	_headerToRead = _sheader.getSizeOf();
	_hptr = (char*)&_sheader;
    }
}

void SampleInputStream::distribute(const Sample* samp) throw()
{
    // pass samples to the appropriate sensor for processing
    // and distribution to processed sample clients
    dsm_sample_id_t sampid = samp->getId();
    _sensorMapMutex.lock();
    if (_sensorMap.size() > 0) {
	map<unsigned int,DSMSensor*>::const_iterator sensori;
	sensori = _sensorMap.find(sampid);
	if (sensori != _sensorMap.end()) sensori->second->receive(samp);
    }
    _sensorMapMutex.unlock();
    SampleSource::distribute(samp);
}

/*
 * process <input> element
 */
void SampleInputStream::fromDOMElement(const xercesc::DOMElement* node)
        throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);

    // process <socket>, <fileset> child elements (should only be one)

    int niochan = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;

	_iochan = IOChannel::createIOChannel((const xercesc::DOMElement*)child);

	_iochan->fromDOMElement((xercesc::DOMElement*)child);

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
                                                           
void SampleInputStream::addSampleTag(const SampleTag* tag)
{
    if (find(_sampleTags.begin(),_sampleTags.end(),tag) == _sampleTags.end())
        _sampleTags.push_back(tag);
}

/*
 * Constructor, with a IOChannel (which may be null).
 */
SortedSampleInputStream::SortedSampleInputStream(IOChannel* iochannel):
    SampleInputStream(iochannel),
    sorter1(0),sorter2(0),
    heapMax(10000000),heapBlock(false),
    sorterLengthMsecs(250)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */
SortedSampleInputStream::SortedSampleInputStream(const SortedSampleInputStream& x,IOChannel* iochannel):
	SampleInputStream(x,iochannel),
	sorter1(0),sorter2(0),
	heapMax(x.heapMax),heapBlock(x.heapBlock),
	sorterLengthMsecs(x.sorterLengthMsecs)
{
}

/*
 * Clone myself, with a new IOChannel.
 */
SortedSampleInputStream* SortedSampleInputStream::clone(IOChannel* iochannel)
{
    return new SortedSampleInputStream(*this,iochannel);
}

SortedSampleInputStream::~SortedSampleInputStream()
{
    // cerr << "~SortedSampleInputStream" << endl;
    delete sorter1;
    delete sorter2;
}

void SortedSampleInputStream::addSampleClient(SampleClient* client) throw()
{
    if (!sorter1) {
        sorter1 = new SampleSorter("Sorter1");
	sorter1->setLengthMsecs(getSorterLengthMsecs());
	sorter1->setHeapBlock(getHeapBlock());
	sorter1->setHeapMax(getHeapMax());
    }
    SampleInputStream::addSampleClient(sorter1);
    sorter1->addSampleClient(client);
    if (!sorter1->isRunning()) sorter1->start();
}

void SortedSampleInputStream::removeSampleClient(SampleClient* client) throw()
{
    if (sorter1) {
        sorter1->removeSampleClient(client);
	SampleInputStream::removeSampleClient(sorter1);
    }
}
void SortedSampleInputStream::addProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    _sensorMapMutex.lock();
    _sensorMap[sensor->getId()] = sensor;
    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
    	_sensorsByClient.find(client);
    if (sci != _sensorsByClient.end()) sci->second.push_back(sensor);
    else {
        list<DSMSensor*> sensors;
	sensors.push_back(sensor);
	_sensorsByClient[client] = sensors;
    }
    _sensorMapMutex.unlock();

    if (!sorter2) {
        sorter2 = new SampleSorter("Sorter2");
	sorter2->setLengthMsecs(getSorterLengthMsecs());
	sorter2->setHeapBlock(getHeapBlock());
	sorter2->setHeapMax(getHeapMax());
    }

    sensor->addSampleClient(sorter2);
    if (!sorter2->isRunning()) sorter2->start();

    SampleTagIterator si = sensor->getSampleTagIterator();
    for ( ; si.hasNext(); ) {
	const SampleTag* stag = si.next();
        addSampleTag(stag);
	sorter2->addSampleTag(stag,client);
    }
}

void SortedSampleInputStream::removeProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    if (!sensor) {		// remove client for all sensors
	_sensorMapMutex.lock();
	map<SampleClient*,list<DSMSensor*> >::iterator sci = 
	    _sensorsByClient.find(client);
	if (sci != _sensorsByClient.end()) {
	    list<DSMSensor*>& sensors = sci->second;
	    for (list<DSMSensor*>::iterator si = sensors.begin();
	    	si != sensors.end(); ++si) {
		sensor = *si;
		sensor->removeSampleClient(sorter2);
		if (sensor->getClientCount() == 0)
		    _sensorMap.erase(sensor->getId());
	    }
	}
	_sensorMapMutex.unlock();
    }
    else {
        sensor->removeSampleClient(sorter2);
	if (sensor->getClientCount() == 0)
	    _sensorMap.erase(sensor->getId());
    }
    if (sorter2) sorter2->removeSampleClient(client);
}

void SortedSampleInputStream::flush() throw()
{
    if (sorter1) sorter1->finish();
    if (sorter2) sorter2->finish();
}


void SortedSampleInputStream::close() throw(n_u::IOException)
{
    if (sorter1) {
        if (sorter1->isRunning()) sorter1->interrupt();
	sorter1->join();
    }
    if (sorter2) {
        if (sorter2->isRunning()) sorter2->interrupt();
	sorter2->join();
    }
}
void SortedSampleInputStream::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    SampleInputStream::fromDOMElement(node);
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
	    if (aname == "sorterLength") {
	        istringstream ist(aval);
		int len;
		ist >> len;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	"SortedSampleInputStream",
			attr.getName(),attr.getValue());
		setSorterLengthMsecs(len);
	    }
	    else if (aname == "heapMax") {
	        istringstream ist(aval);
		int len;
		ist >> len;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
		    	"SortedSampleInputStream",
			attr.getName(),attr.getValue());
		setHeapMax(len);
	    }
	}
    }
}

