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

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SampleInputStream)

SampleInputStream::SampleInputStream():
    name("SampleInputStream"),iochan(0),iostream(0),
    pseudoPort(0),connectionRequester(0),
    dsm(0),service(0), samp(0),left(0),dptr(0),
    unrecognizedSamples(0)
{
}

SampleInputStream::SampleInputStream(const SampleInputStream& x):
    name(x.name),iochan(x.iochan->clone()),iostream(0),
    pseudoPort(x.pseudoPort),connectionRequester(x.connectionRequester),
    dsm(x.dsm),service(x.service),
    samp(0),left(0),dptr(0),
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

void SampleInputStream::requestConnection(SampleConnectionRequester* requester)
            throw(atdUtil::IOException)
{
    connectionRequester = requester;
    iochan->requestConnection(this,getPseudoPort());
}

void SampleInputStream::connected(IOChannel* iochannel)
{
    assert(iochan == iochannel);
    assert(connectionRequester);
    setName(string("SampleInputStream: ") + iochan->getName());
    connectionRequester->connected(this);
}

void SampleInputStream::setDSMConfig(const DSMConfig* val)
{
    dsm = val;
    if (iochan) iochan->setDSMConfig(val);
}

const DSMConfig* SampleInputStream::getDSMConfig() const
{
    return dsm;
}

void SampleInputStream::setDSMService(const DSMService* val)
{
    service = val;
    if (iochan) iochan->setDSMService(val);
}

const DSMService* SampleInputStream::getDSMService() const
{
    return service;
}


void SampleInputStream::setPseudoPort(int val) { pseudoPort = val; }

int SampleInputStream::getPseudoPort() const { return pseudoPort; }

void SampleInputStream::addSensor(DSMSensor* sensor)
{
    sensor_map[sensor->getId()] = sensor;
}

void SampleInputStream::init()
{
    cerr << "SampleInputStream::init(), buffer size=" << 
    	iochan->getBufferSize() << endl;
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

int SampleInputStream::getFd() const
{
    if (iochan) return iochan->getFd();
    else return -1;
}

/**
 * Read a buffer of data and process all samples in the buffer.
 * This is typically used when a select has determined that there
 * is data available on our file descriptor. Process all available
 * data from the InputStream and distribute() samples to the receive()
 * method of my SampleClients.  This will perform only one physical
 * read of the underlying device.
 */
void SampleInputStream::readSamples() throw(atdUtil::IOException)
{
    static int nsamps = 0;

    iostream->read();		// read a buffer's worth

    SampleHeader header;
    map<unsigned long,DSMSensor*>::const_iterator mapend = sensor_map.end();
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
	    cerr << "read header, type=" << header.getType() <<
	    	" getTimeTag=" << header.getTimeTag() <<
	    	" getId=" << header.getId() <<
	    	" getDataByteLength=" << header.getDataByteLength() <<
		endl;
#endif
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
	    left = samp->getDataByteLength();
	    // cerr << "left=" << left << endl;
	    dptr = (char*) samp->getVoidDataPtr();
	}
	size_t len = iostream->available();
	// cerr << "left=" << left << " available=" << len << endl;
	if (left < len) len = left;
	len = iostream->read(dptr, len);
	// cerr << "read len=" << len << endl;
	dptr += len;
	left -= len;
	if (left == 0) {
	    if (!(nsamps++ % 100)) cerr << "read " << nsamps << " samples" << endl;
	    // if we're an input of raw samples, pass them to the
	    // appropriate sensor for distribution.

	    // todo: need to catch exceptions here and freeReference
	    if (isRaw()) {
		sensori = sensor_map.find(samp->getId());
		if (sensori != mapend) sensori->second->distributeRaw(samp);
		else unrecognizedSamples++;
	    }

	    // distribute them ourselvs too
	    distribute(samp);
	    samp->freeReference();
	    samp = 0;
	}
	else break;
    }
}

/**
 * Blocking read of the next sample from the buffer. The caller must
 * call freeReference on the sample when they're done with it.
 */
Sample* SampleInputStream::readSample() throw(atdUtil::IOException)
{
    // user shouldn't mix the two read methods on one stream, but if they
    // do, checking for non-null samp here should make things work.
    if (!samp) {
	SampleHeader header;
	if (iostream->available() < header.getSizeOf()) 
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
	left = samp->getDataByteLength();
	dptr = (char*) samp->getVoidDataPtr();
    }
    while (left > 0) {
	size_t len = iostream->available();
	if (left < len) len = left;
	len = iostream->read(dptr, len);
	dptr += len;
	left -= len;
	if (left == 0) break;
	iostream->read();
    }
    Sample* tmp = samp;
    samp = 0;
    return tmp;
}

/*
 * process <input> element
 */
void SampleInputStream::fromDOMElement(const xercesc::DOMElement* node)
        throw(atdUtil::InvalidParameterException)
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
            const std::string& aval = attr.getValue();
        }
    }

    // process <socket>, <fileset> child elements (should only be one)

    int niochan = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;

        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();

	iochan = IOChannel::createIOChannel(elname);

	iochan->setDSMConfig(getDSMConfig());
	iochan->setDSMService(getDSMService());

	iochan->fromDOMElement((xercesc::DOMElement*)child);

	if (++niochan > 1)
	    throw atdUtil::InvalidParameterException(
		    "SampleInputStream::fromDOMElement",
		    "input", "must have one child element");
    }
    if (!iochan)
        throw atdUtil::InvalidParameterException(
                "SampleInputStream::fromDOMElement",
		"input", "must have one child element");
    setName(string("SampleInputStream: ") + iochan->getName());
}
                                                           
xercesc::DOMElement* SampleInputStream::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
                        DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
                                                                                
xercesc::DOMElement* SampleInputStream::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

