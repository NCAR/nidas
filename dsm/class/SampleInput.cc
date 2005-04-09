/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#include <SampleInput.h>
#include <DSMSensor.h>
#include <DSMService.h>

#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SampleInputStream)

SampleInputStream::SampleInputStream(IOChannel* iochannel):
    service(0),dsm(0),iochan(iochannel),iostream(0),
    pseudoPort(0),
     samp(0),leftToRead(0),dptr(0),
    unrecognizedSamples(0)
{
}

/* Copy constructor. */
SampleInputStream::SampleInputStream(const SampleInputStream& x):
    service(x.service),dsm(x.dsm),
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

SampleInput* SampleInputStream::clone() const
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
    iochan->setDSMService(service);
    iochan->requestConnection(this,getPseudoPort());
}

void SampleInputStream::connected(IOChannel* iochannel) throw()
{
    assert(iochan == iochannel);
    service->connected(this);
}

void SampleInputStream::setPseudoPort(int val) { pseudoPort = val; }

int SampleInputStream::getPseudoPort() const { return pseudoPort; }

void SampleInputStream::addSensor(DSMSensor* sensor)
{
    sensorMapMutex.lock();
    sensorMap[sensor->getId()] = sensor;
    sensorMapMutex.unlock();
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
#ifdef DEBUG
    static int nsamps = 0;
#endif

    iostream->read();		// read a buffer's worth

    SampleHeader header;
    map<unsigned long,DSMSensor*>::const_iterator sensori;

    // process all in buffer
    for (;;) {
	if (!samp) {
#ifdef DEBUG
	    cerr << "available=" << iostream->available() << endl;
#endif
	    if (iostream->available() < header.getSizeOf()) break;
	    iostream->read(&header,header.getSizeOf());

#ifdef DEBUG
	    assert(header.getSizeOf() == 12);
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
		samp = dsm::getSample((sampleType)CHAR_ST,
		    header.getDataByteLength());
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
	if (isRaw() && sampid != CLOCK_SAMPLE_ID && sensorMap.size() > 0) {
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
 * Blocking read of the next sample from the buffer. The caller must
 * call freeReference on the sample when they're done with it.
 */
Sample* SampleInputStream::readSample() throw(atdUtil::IOException)
{
    // user shouldn't mix the two readSample methods on one stream, but if they
    // do, checking for non-null samp here should make things work.
    if (!samp) {
	SampleHeader header;
	while (iostream->available() < header.getSizeOf()) 
	    iostream->read();

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
	size_t len = iostream->available();
	if (leftToRead < len) len = leftToRead;
	len = iostream->read(dptr, len);
	dptr += len;
	leftToRead -= len;
	if (leftToRead == 0) break;
	iostream->read();
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

