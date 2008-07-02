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

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(SampleInputStream)

/*
 * Constructor, with a IOChannel (which may be null).
 */
SampleInputStream::SampleInputStream(IOChannel* iochannel):
    service(0),iochan(iochannel),iostream(0),
    samp(0),leftToRead(0),dptr(0),
    badInputSamples(0),
    filterBadSamples(false),maxDsmId(1024),
    maxSampleLength(UINT_MAX),
    minSampleTime(LONG_LONG_MIN),
    maxSampleTime(LONG_LONG_MAX)
{
    if (iochan)
        iostream = new IOStream(*iochan,iochan->getBufferSize());
    // minSampleTime = n_u::UTime::parse(true,"2004 jan 1 00:00").toUsecs();
    // maxSampleTime = n_u::UTime::parse(true,"2010 jan 1 00:00").toUsecs();
}

/*
 * Copy constructor, with a new IOChannel.
 */
SampleInputStream::SampleInputStream(const SampleInputStream& x,
	IOChannel* iochannel):
    service(x.service),
    iochan(iochannel),iostream(0),
    sampleTags(x.sampleTags),
    samp(0),leftToRead(0),dptr(0),
    badInputSamples(0),
    filterBadSamples(x.filterBadSamples),maxDsmId(x.maxDsmId),
    maxSampleLength(x.maxSampleLength),minSampleTime(x.minSampleTime),
    maxSampleTime(x.maxSampleTime)
{
    if (iochan)
        iostream = new IOStream(*iochan,iochan->getBufferSize());
    // minSampleTime = n_u::UTime::parse(true,"2004 jan 1 00:00").toUsecs();
    // maxSampleTime = n_u::UTime::parse(true,"2010 jan 1 00:00").toUsecs();
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
    delete iostream;
    delete iochan;
}

string SampleInputStream::getName() const {
    if (iochan) return string("SampleInputStream: ") + iochan->getName();
    return string("SampleInputStream");
}

void SampleInputStream::requestConnection(DSMService* requester)
            throw(n_u::IOException)
{
    service = requester;
    iochan->requestConnection(this);
}

void SampleInputStream::connected(IOChannel* iochannel) throw()
{
    cerr << "SampleInputStream connected, iochannel=" <<
    	iochannel->getRemoteInet4Address().getHostAddress() << endl;
    // this create a clone of myself
    service->connected(clone(iochannel));
}

void SampleInputStream::addProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    sensorMapMutex.lock();
    sensorMap[sensor->getId()] = sensor;

    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
    	sensorsByClient.find(client);
    if (sci != sensorsByClient.end()) sci->second.push_back(sensor);
    else {
        list<DSMSensor*> sensors;
	sensors.push_back(sensor);
	sensorsByClient[client] = sensors;
    }
    sensorMapMutex.unlock();

    sensor->addSampleClient(client);
}

void SampleInputStream::removeProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    if (!sensor) {		// remove client for all sensors
	sensorMapMutex.lock();
	map<SampleClient*,list<DSMSensor*> >::iterator sci = 
	    sensorsByClient.find(client);
	if (sci != sensorsByClient.end()) {
	    list<DSMSensor*>& sensors = sci->second;
	    for (list<DSMSensor*>::iterator si = sensors.begin();
	    	si != sensors.end(); ++si) {
		sensor = *si;
		sensor->removeSampleClient(client);
		if (sensor->getClientCount() == 0)
		    sensorMap.erase(sensor->getId());
	    }
	}
	sensorMapMutex.unlock();
    }
    else {
        sensor->removeSampleClient(client);
	if (sensor->getClientCount() == 0)
	    sensorMap.erase(sensor->getId());
    }
}

void SampleInputStream::init() throw()
{
#ifdef DEBUG
    cerr << "SampleInputStream::init(), buffer size=" << 
    	iochan->getBufferSize() << endl;
#endif
    if (!iostream)
	iostream = new IOStream(*iochan,iochan->getBufferSize());
}

void SampleInputStream::close() throw(n_u::IOException)
{
    delete iostream;
    iostream = 0;
    iochan->close();
}

n_u::Inet4Address SampleInputStream::getRemoteInet4Address() const
{
    if (iochan) return iochan->getRemoteInet4Address();
    else return n_u::Inet4Address();
}

void SampleInputStream::readHeader() throw(n_u::IOException)
{
    inputHeader.check(iostream);
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
    iostream->read();		// read a buffer's worth
    if (iostream->isNewFile()) {	// first read from a new file
	readHeader();
	if (samp) samp->freeReference();
	samp = 0;
    }

    SampleHeader header;

    // process all in buffer
    for (;;) {
	if (!samp) {
	    if (iostream->available() < header.getSizeOf()) break;

	    iostream->read(&header,header.getSizeOf());

#if __BYTE_ORDER == __BIG_ENDIAN
            header.setTimeTag(bswap_64(header.getTimeTag()));
            header.setDataByteLength(bswap_32(header.getDataByteLength()));
            header.setRawId(bswap_32(header.getRawId()));
#endif

            // screen bad headers.
	    if (filterBadSamples &&
                (header.getType() >= UNKNOWN_ST ||
                    GET_DSM_ID(header.getId()) > maxDsmId ||
                header.getDataByteLength() > maxSampleLength ||
                header.getTimeTag() < minSampleTime ||
                header.getTimeTag() > maxSampleTime)) {
                samp = 0;
	    }
	    else samp = nidas::core::getSample((sampleType)header.getType(),
		    header.getDataByteLength());

            if (!samp) {
	        if (!(badInputSamples++ % 1000)) {
                    n_u::Logger::getInstance()->log(LOG_WARNING,
                        "%s: bad sample hdr: #bad=%d,filepos=%d,id=(%d,%d),type=%d,len=%d",
                        getName().c_str(), badInputSamples,
                        iostream->getNBytes()-header.getSizeOf(),
                        GET_DSM_ID(header.getId()),GET_SHORT_ID(header.getId()),
                        header.getType(),header.getDataByteLength());
                }
                iostream->backup(header.getSizeOf() - 1);
                continue;
            }
	    samp->setTimeTag(header.getTimeTag());
	    samp->setId(header.getId());
	    leftToRead = samp->getDataByteLength();
	    // cerr << "leftToRead=" << leftToRead << endl;
	    dptr = (char*) samp->getVoidDataPtr();
	}
	size_t len = iostream->available();
	if (len == 0) break;
	// cerr << "leftToRead=" << leftToRead << " available=" << len << endl;
	if (leftToRead < len) len = leftToRead;
	len = iostream->read(dptr, len);
	// cerr << "read len=" << len << endl;
	dptr += len;
	leftToRead -= len;
	if (leftToRead > 0) break;	// no more data in iostream buffer

#ifdef DEBUG
	//if (!(nsamps++ % 100)) cerr << "read " << nsamps << " samples" << endl;
#endif

	distribute(samp);
	samp = 0;
    }
}

void SampleInputStream::distribute(const Sample* samp) throw()
{
    // pass samples to the appropriate sensor for processing
    // and distribution to processed sample clients
    dsm_sample_id_t sampid = samp->getId();
    sensorMapMutex.lock();
    if (sensorMap.size() > 0) {
	map<unsigned int,DSMSensor*>::const_iterator sensori;
	sensori = sensorMap.find(sampid);
	if (sensori != sensorMap.end()) sensori->second->receive(samp);
    }
    sensorMapMutex.unlock();
    SampleSource::distribute(samp);
}

/*
 * Read the next sample. The caller must call freeReference on the
 * sample when they're done with it.
 */
Sample* SampleInputStream::readSample() throw(n_u::IOException)
{
    // user probably won't mix the two readSample methods on one stream,
    // but if they do, checking for non-null samp here should make things work.
    for (;;) {
        if (!samp) {
            SampleHeader header;
            while (iostream->available() < header.getSizeOf()) {
                iostream->read();
                if (iostream->isNewFile()) {
                    inputHeader.check(iostream);
                }
            }

            iostream->read(&header,header.getSizeOf());
#if __BYTE_ORDER == __BIG_ENDIAN
            header.setTimeTag(bswap_64(header.getTimeTag()));
            header.setDataByteLength(bswap_32(header.getDataByteLength()));
            header.setRawId(bswap_32(header.getRawId()));
#endif

            // screen bad headers.
            if (filterBadSamples &&
                (header.getType() >= UNKNOWN_ST ||
                GET_DSM_ID(header.getId()) > maxDsmId ||
                header.getDataByteLength() > maxSampleLength ||
                header.getDataByteLength() == 0 ||
                header.getTimeTag() < minSampleTime ||
                header.getTimeTag() > maxSampleTime)) {
                if (!(badInputSamples++ % 1000)) {
                    n_u::Logger::getInstance()->log(LOG_WARNING,
                        "%s: bad sample hdr: #bad=%d,filepos=%d,id=(%d,%d),type=%d,len=%d",
                        getName().c_str(), badInputSamples,
                        iostream->getNBytes()-header.getSizeOf(),
                        GET_DSM_ID(header.getId()),GET_SHORT_ID(header.getId()),
                        header.getType(),header.getDataByteLength());
                }
                iostream->backup(header.getSizeOf() - 1);
                continue;
	    }
	    else samp = nidas::core::getSample((sampleType)header.getType(),
		    header.getDataByteLength());

            samp->setTimeTag(header.getTimeTag());
            samp->setId(header.getId());
            leftToRead = samp->getDataByteLength();
            dptr = (char*) samp->getVoidDataPtr();
        }
        while (leftToRead > 0) {
            size_t len = iostream->read(dptr, leftToRead);
            if (iostream->isNewFile()) {
                iostream->backup(len);
                samp->freeReference();
                samp = 0;
                inputHeader.check(iostream);
                // go back and read the header
                break;
            }
            dptr += len;
            leftToRead -= len;
        }
        if (leftToRead == 0) {
            Sample* tmp = samp;
            samp = 0;
            return tmp;
        }
    }
}

/*
 * Read the next sample. The caller must call freeReference on the
 * sample when they're done with it.
 */
void SampleInputStream::search(const n_u::UTime& tt) throw(n_u::IOException)
{
    SampleHeader header;
    for (;;) {
        while (iostream->available() < header.getSizeOf()) {
            iostream->read();
            if (iostream->isNewFile()) {
                inputHeader.check(iostream);
            }
        }

        iostream->read(&header,header.getSizeOf());
#if __BYTE_ORDER == __BIG_ENDIAN
            header.setTimeTag(bswap_64(header.getTimeTag()));
            header.setDataByteLength(bswap_32(header.getDataByteLength()));
            header.setRawId(bswap_32(header.getRawId()));
#endif

        if (header.getType() >= UNKNOWN_ST) badInputSamples++;
        // cerr << header.getTimeTag() << " " << tt.toUsecs() << endl;
        if (header.getTimeTag() >= tt.toUsecs()) {
            iostream->backup(header.getSizeOf());
            return;
        }

        leftToRead = header.getDataByteLength();

        while (leftToRead > 0) {
            size_t len = iostream->skip(leftToRead);
            if (iostream->isNewFile()) {
                iostream->backup(len);
                inputHeader.check(iostream);
                break;
            }
            leftToRead -= len;
        }
    }
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

	iochan = IOChannel::createIOChannel((const xercesc::DOMElement*)child);

	iochan->fromDOMElement((xercesc::DOMElement*)child);

	if (++niochan > 1)
	    throw n_u::InvalidParameterException(
		    "SampleInputStream::fromDOMElement",
		    "input", "must have one child element");
    }
    if (!iochan)
        throw n_u::InvalidParameterException(
                "SampleInputStream::fromDOMElement",
		"input", "must have one child element");
}
                                                           
void SampleInputStream::addSampleTag(const SampleTag* tag)
{
    if (find(sampleTags.begin(),sampleTags.end(),tag) == sampleTags.end())
        sampleTags.push_back(tag);
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
    sensorMapMutex.lock();
    sensorMap[sensor->getId()] = sensor;
    map<SampleClient*,list<DSMSensor*> >::iterator sci = 
    	sensorsByClient.find(client);
    if (sci != sensorsByClient.end()) sci->second.push_back(sensor);
    else {
        list<DSMSensor*> sensors;
	sensors.push_back(sensor);
	sensorsByClient[client] = sensors;
    }
    sensorMapMutex.unlock();

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
	sensorMapMutex.lock();
	map<SampleClient*,list<DSMSensor*> >::iterator sci = 
	    sensorsByClient.find(client);
	if (sci != sensorsByClient.end()) {
	    list<DSMSensor*>& sensors = sci->second;
	    for (list<DSMSensor*>::iterator si = sensors.begin();
	    	si != sensors.end(); ++si) {
		sensor = *si;
		sensor->removeSampleClient(sorter2);
		if (sensor->getClientCount() == 0)
		    sensorMap.erase(sensor->getId());
	    }
	}
	sensorMapMutex.unlock();
    }
    else {
        sensor->removeSampleClient(sorter2);
	if (sensor->getClientCount() == 0)
	    sensorMap.erase(sensor->getId());
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

