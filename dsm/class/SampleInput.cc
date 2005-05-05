/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <SampleInput.h>
#include <DSMSensor.h>
#include <DSMService.h>
#include <SampleFileHeader.h>

#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SampleInputStream)

SampleInputStream::SampleInputStream(IOChannel* iochannel):
    service(0),iochan(iochannel),iostream(0),
    pseudoPort(0),
     samp(0),leftToRead(0),dptr(0),
    unrecognizedSamples(0)
{
}

/* Copy constructor. */
SampleInputStream::SampleInputStream(const SampleInputStream& x):
    service(x.service),dsms(x.dsms),
    iochan(x.iochan->clone()),iostream(0),
    pseudoPort(x.pseudoPort),
    samp(0),leftToRead(0),dptr(0),
    unrecognizedSamples(0)
{
}

SampleInputStream::~SampleInputStream()
{
    delete iostream;
    delete iochan;
}

SampleInputStream* SampleInputStream::clone() const
{
    // invoke copy constructor
    return new SampleInputStream(*this);
}

string SampleInputStream::getName() const {
    if (iochan) return string("SampleInputStream: ") + iochan->getName();
    return string("SampleInputStream");
}

void SampleInputStream::requestConnection(DSMService* requester)
            throw(atdUtil::IOException)
{
    service = requester;
    iochan->requestConnection(this,getPseudoPort());
}

void SampleInputStream::connected(IOChannel* iochannel) throw()
{
    assert(iochan == iochannel);
    service->connected(this);
}

void SampleInputStream::addProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    sensorMapMutex.lock();
    sensorMap[sensor->getId()] = sensor;
    sensorMapMutex.unlock();

    sensor->addSampleClient(client);
}

void SampleInputStream::removeProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    sensor->removeSampleClient(client);
}

void SampleInputStream::init() throw()
{
#ifdef DEBUG
    cerr << "SampleInputStream::init(), buffer size=" << 
    	iochan->getBufferSize() << endl;
#endif
    delete iostream;
    iostream = new IOStream(*iochan,iochan->getBufferSize());
}

void SampleInputStream::close() throw(atdUtil::IOException)
{
    delete iostream;
    iostream = 0;
    iochan->close();
}

atdUtil::Inet4Address SampleInputStream::getRemoteInet4Address() const
{
    if (iochan) return iochan->getRemoteInet4Address();
    else return atdUtil::Inet4Address();
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
void SampleInputStream::readSamples() throw(atdUtil::IOException)
{
// #define DEBUG
#ifdef DEBUG
    static int nsamps = 0;

    cerr << "readSamples, iostream->read(), available=" << iostream->available() <<
    	", iostream=" << iostream << endl;
#endif
    iostream->read();		// read a buffer's worth
    if (iostream->isNewFile()) {	// first read from a new file
	SampleFileHeader header;
	header.check(iostream);
	if (samp) samp->freeReference();
	samp = 0;
    }

    SampleHeader header;
    map<unsigned long,DSMSensor*>::const_iterator sensori;

    // process all in buffer
    for (;;) {
	if (!samp) {
#ifdef DEBUG
	    cerr << "available=" << iostream->available() << endl;
#endif
	    if (iostream->available() < header.getSizeOf()) break;

#ifndef DEBUG
	    iostream->read(&header,header.getSizeOf());
#else
	    size_t len = iostream->read(&header,header.getSizeOf());
	    assert(header.getSizeOf() == 16);
	    assert(len == 16);

	    cerr << "read header " <<
	    	" getTimeTag=" << header.getTimeTag() <<
	    	" getId=" << header.getId() <<
	    	" getType=" << (int) header.getType() <<
	    	" getDataByteLength=" << header.getDataByteLength() <<
		endl;
#endif
	    if (header.getType() >= UNKNOWN_ST) {
	        unrecognizedSamples++;
		atdUtil::Logger::getInstance()->log(LOG_WARNING,
		    "SampleInputStream UNKNOWN_ST unrecognizedSamples=%d",
			    unrecognizedSamples);
		cerr << "read header " <<
		    " getTimeTag=" << header.getTimeTag() <<
		    " getId=" << header.getId() <<
		    " getType=" << (int) header.getType() <<
		    " getDataByteLength=" << header.getDataByteLength() <<
		    " chars=" << string((const char*)&header,16) << endl;
		continue;
	    }
	    else
		samp = dsm::getSample((sampleType)header.getType(),
		    header.getDataByteLength());

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
	if (!(nsamps++ % 100)) cerr << "read " << nsamps << " samples" << endl;
#endif

	// if we're an input of raw samples, pass them to the
	// appropriate sensor for distribution.
	dsm_sample_id_t sampid = samp->getId();
	sensorMapMutex.lock();
	if (sensorMap.size() > 0) {
	    sensori = sensorMap.find(sampid);
	    if (sensori != sensorMap.end()) sensori->second->receive(samp);
	    else if (!(unrecognizedSamples++) % 100) {
		atdUtil::Logger::getInstance()->log(LOG_WARNING,
		    "SampleInputStream unrecognizedSamples=%d",
			    unrecognizedSamples);
	    }
	}
	sensorMapMutex.unlock();

	// distribute samples to my own clients
	distribute(samp);
	samp->freeReference();
	samp = 0;
    }
}

/**
 * Read the next sample. The caller must call freeReference on the
 * sample when they're done with it.
 */
Sample* SampleInputStream::readSample() throw(atdUtil::IOException)
{
    // user probably won't mix the two readSample methods on one stream,
    // but if they do, checking for non-null samp here should make things work.
restart:
    if (!samp) {
	SampleHeader header;
	while (iostream->available() < header.getSizeOf()) {
	    iostream->read();
	    if (iostream->isNewFile()) {
		SampleFileHeader header;
		header.check(iostream);
	    }
	}

	iostream->read(&header,header.getSizeOf());
	if (header.getType() >= UNKNOWN_ST) {
	    unrecognizedSamples++;
	    samp = dsm::getSample((sampleType)CHAR_ST,
		header.getDataByteLength());
	}
	else
	    samp = dsm::getSample((sampleType)header.getType(),
		header.getDataByteLength());

	samp->setTimeTag(header.getTimeTag());
	samp->setId(header.getId());
	leftToRead = samp->getDataByteLength();
	dptr = (char*) samp->getVoidDataPtr();
    }
    while (leftToRead > 0) {
	size_t len = iostream->read(dptr, leftToRead);
	if (iostream->isNewFile()) {
	    iostream->putback(dptr,len);
	    samp->freeReference();
	    samp = 0;
	    SampleFileHeader header;
	    header.check(iostream);
	    goto restart;
	}
	dptr += len;
	leftToRead -= len;
	if (leftToRead == 0) break;
    }
    Sample* tmp = samp;
    samp = 0;
    return tmp;
}

/*
 * process <input> element
 */
void SampleInputStream::fromDOMElement(const DOMElement* node)
        throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    if(node->hasAttributes()) {
        // get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
        }
    }

    // process <socket>, <fileset> child elements (should only be one)

    int niochan = 0;
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;

        XDOMElement xchild((DOMElement*) child);
        const string& elname = xchild.getNodeName();

	iochan = IOChannel::createIOChannel(elname);

	iochan->fromDOMElement((DOMElement*)child);

	if (++niochan > 1)
	    throw atdUtil::InvalidParameterException(
		    "SampleInputStream::fromDOMElement",
		    "input", "must have one child element");
    }
    if (!iochan)
        throw atdUtil::InvalidParameterException(
                "SampleInputStream::fromDOMElement",
		"input", "must have one child element");
}
                                                           
DOMElement* SampleInputStream::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
                        DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
                                                                                
DOMElement* SampleInputStream::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

SampleInputMerger::SampleInputMerger() :
	name("SampleInputMerger"),
	sorter1(250,name + "Sorter1"),
	sorter2(250,name + "Sorter2"),
	unrecognizedSamples(0)
{
}

SampleInputMerger::~SampleInputMerger()
{
    if (sorter1.isRunning()) {
	sorter1.interrupt();
	sorter1.join();
    }
    if (sorter2.isRunning()) {
	sorter2.interrupt();
	sorter2.join();
    }
}

void SampleInputMerger::addInput(SampleInput* input)
{
    if (!sorter1.isRunning()) sorter1.start();
#ifdef DEBUG
    cerr << "SampleInputMerger: " << input->getName() << 
    	" addSampleClient, &sorter1=" << &sorter1 << endl;
#endif
    input->addSampleClient(&sorter1);
}

void SampleInputMerger::removeInput(SampleInput* input)
{
    input->removeSampleClient(&sorter1);
}

void SampleInputMerger::addProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    sensorMapMutex.lock();
    sensorMap[sensor->getId()] = sensor;
    sensorMapMutex.unlock();

    if (!sorter2.isRunning()) sorter2.start();
    sorter2.addSampleClient(client);


#ifdef DEBUG
    cerr << "SampleInputMerger::SampleSource::addSampleClient, &sorter=" << &sorter2 << endl;
#endif
    sensor->addSampleClient(&sorter2);

    if (!sorter1.isRunning()) sorter1.start();
    sorter1.addSampleClient(this);
}

void SampleInputMerger::removeProcessedSampleClient(SampleClient* client,
	DSMSensor* sensor)
{
    sensor->removeSampleClient(this);
    SampleSource::removeSampleClient(&sorter2);
    sorter2.removeSampleClient(client);
}

void SampleInputMerger::addSampleClient(SampleClient* client) throw()
{
#ifdef DEBUG
    cerr << "SampleInputMerger::addSampleClient, client=" << client << endl;
#endif
    sorter1.addSampleClient(client);
}

void SampleInputMerger::removeSampleClient(SampleClient* client) throw()
{
    sorter1.removeSampleClient(client);
}

bool SampleInputMerger::receive(const Sample* samp) throw()
{
#ifdef DEBUG
    static int nsamps = 0;
#endif
    // pass sample to the appropriate sensor for distribution.
    dsm_sample_id_t sampid = samp->getId();
    sensorMapMutex.lock();
    if (sensorMap.size() > 0) {
	map<unsigned long,DSMSensor*>::const_iterator sensori
		= sensorMap.find(sampid);
	if (sensori != sensorMap.end()) sensori->second->receive(samp);
	else if (!(unrecognizedSamples++) % 100) {
	    atdUtil::Logger::getInstance()->log(LOG_WARNING,
		"SampleInputStream unrecognizedSamples=%d",
			unrecognizedSamples);
	}
    }
    sensorMapMutex.unlock();

    // distribute samples to my own clients
#ifdef DEBUG
    if (!(nsamps++%100)) cerr << "SampleInputMerger::receive, nsamps=" << nsamps << endl;
#endif
    distribute(samp);
    return true;
}
