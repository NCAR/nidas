/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <SampleOutput.h>
#include <DSMTime.h>
#include <Version.h>
#include <Project.h>

#include <atdUtil/Logger.h>

#include <iostream>


using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SampleOutputStream)

SampleOutputStream::SampleOutputStream(IOChannel* i):
	name("SampleOutputStream"),iochan(i),iostream(0),
	pseudoPort(0),service(0),connectionRequester(0),
	nextFileTime(0),nsamplesDiscarded(0)
{
}

/*
 * Copy constructor.
 */

SampleOutputStream::SampleOutputStream(const SampleOutputStream& x):
	name(x.name), iochan(x.iochan->clone()),iostream(0),
	pseudoPort(x.pseudoPort),
	dsms(x.dsms),service(x.service),
	connectionRequester(x.connectionRequester),
	nextFileTime(0),nsamplesDiscarded(0)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */

SampleOutputStream::SampleOutputStream(const SampleOutputStream& x,IOChannel* iochannel):
	name(x.name), iochan(iochannel),iostream(0),
	pseudoPort(x.pseudoPort),
	dsms(x.dsms),service(x.service),
	connectionRequester(x.connectionRequester),
	nextFileTime(0),nsamplesDiscarded(0)
{
}

SampleOutputStream::~SampleOutputStream()
{
#ifdef DEBUG
    cerr << "~SampleOutputStream()" << endl;
#endif
    delete iostream;
    delete iochan;
}

SampleOutputStream* SampleOutputStream::clone(IOChannel* iochannel) const
{
    // invoke copy constructor
    if (!iochannel) return new SampleOutputStream(*this);
    else return new SampleOutputStream(*this,iochannel);
}

void SampleOutputStream::setDSMConfigs(const list<const DSMConfig*>& val)
{
    dsms = val;
    if (iochan) iochan->setDSMConfigs(val);
}

const list<const DSMConfig*>& SampleOutputStream::getDSMConfigs() const
{
    return dsms;
}

void SampleOutputStream::addDSMConfig(const DSMConfig* val)
{
    dsms.push_back(val);
    if (iochan) iochan->addDSMConfig(val);
}

void SampleOutputStream::setIOChannel(IOChannel* val)
{
    delete iochan;
    iochan = val;
    setName(string("SampleOutputStream: ") + iochan->getName());
}

void SampleOutputStream::setPseudoPort(int val) { pseudoPort = val; }

int SampleOutputStream::getPseudoPort() const { return pseudoPort; }

void SampleOutputStream::requestConnection(SampleConnectionRequester* requester)
	throw(atdUtil::IOException)
{
    connectionRequester = requester;
    iochan->requestConnection(this,getPseudoPort());
}

void SampleOutputStream::connect()
	throw(atdUtil::IOException)
{
    IOChannel* ioc = iochan->connect(getPseudoPort());
    setIOChannel(ioc);
}

/*
 * We're connected.
 */
void SampleOutputStream::connected(IOChannel* iochannel) throw()
{

    if (!iochan) iochan = iochannel;
    else if (iochan != iochannel) {
	assert(connectionRequester);
	// This is a new iochannel - probably a connected socket.
	// Clone myself and report back to connectionRequester.
	SampleOutputStream* newout = clone(iochannel);
	connectionRequester->connected(newout);
	cerr << "SampleOutputStream::connected new channel" << endl;
    }
    else {
	assert(connectionRequester);
        connectionRequester->connected(this);
	setName(string("SampleOutputStream: ") + iochan->getName());
	cerr << "SampleOutputStream::connected old channel" << endl;
    }
#ifdef DEBUG
    cerr << "SampleOutputStream::connected, iochannel " <<
    	iochannel->getName() << " fd="  <<
    	iochannel->getFd() << endl;
#endif

}

void SampleOutputStream::init() throw()
{
    nextFileTime = 0;
    delete iostream;
#ifdef DEBUG
    cerr << "SampleOutputStream::init, buffer size=" <<
    	iochan->getBufferSize() << " fd=" << iochan->getFd() << endl;
#endif
    iostream = new IOStream(*iochan,iochan->getBufferSize());
}

void SampleOutputStream::close() throw(atdUtil::IOException)
{
#ifdef DEBUG
    cerr << "SampleOutputStream::close" << endl;
#endif
    delete iostream;
    iostream = 0;
    iochan->close();
}

int SampleOutputStream::getFd() const
{
    if (iochan) return iochan->getFd();
    else return -1;
}

void SampleOutputStream::flush() throw(atdUtil::IOException)
{
    if (iostream) iostream->flush();
}

bool SampleOutputStream::receive(const Sample *samp) throw()
{
    if (!iostream) return false;

    dsm_time_t tsamp = samp->getTimeTag();
    try {
	if (tsamp >= nextFileTime) {
	    // The very first file we use an exact time in the name,
	    // otherwise it is truncated down.
	    nextFileTime = iostream->createFile(tsamp,nextFileTime == 0);
	    if (connectionRequester)
		connectionRequester->sendHeader(tsamp,iostream);
	}
	bool success = write(samp);
	if (!success) {
	    if (!(nsamplesDiscarded++ % 1000)) 
		atdUtil::Logger::getInstance()->log(LOG_WARNING,
		    "%s: %d samples discarded due to output jambs\n",
		    getName().c_str(),nsamplesDiscarded);
	}
    }
    catch(const atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s",getName().c_str(),ioe.what());
	if (connectionRequester) connectionRequester->disconnected(this);
	else close();
	return false;
    }
    return true;
}

bool SampleOutputStream::write(const Sample* samp) throw(atdUtil::IOException)
{
#ifdef DEBUG
    static int nsamps = 0;
#endif
    const void* bufs[2];
    size_t lens[2];
    bufs[0] = samp->getHeaderPtr();
    lens[0] = samp->getHeaderLength();
    assert(samp->getHeaderLength() == 16);

    bufs[1] = samp->getConstVoidDataPtr();
    lens[1] = samp->getDataByteLength();

    // cerr << "iostream->write" << endl;
#ifdef DEBUG
    if (!(nsamps++ % 100)) cerr << "wrote " << nsamps << " samples" << endl;
#endif
    return iostream->write(bufs,lens,2);
}

void SampleOutputStream::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();
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

        IOChannel* iochannel = IOChannel::createIOChannel((DOMElement*)child);

        iochannel->setDSMConfigs(getDSMConfigs());

	iochannel->fromDOMElement((DOMElement*)child);

	if (++niochan > 1)
            throw atdUtil::InvalidParameterException(
                    "SampleOutputStream::fromDOMElement",
                    "output", "must have one child element");
	setIOChannel(iochannel);
    }
    if (!iochan)
        throw atdUtil::InvalidParameterException(
                "SampleOutputStream::fromDOMElement",
		"output", "must have one child element");
    setName(string("SampleOutputStream: ") + iochan->getName());
}

DOMElement* SampleOutputStream::toDOMParent(
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
                                                                                
DOMElement* SampleOutputStream::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

SortedSampleOutputStream::SortedSampleOutputStream():
	SampleOutputStream(),initialized(false),
	sorter(250,"SortedSampleOutputStream"),proxy(0,*this)
{
}
/*
 * Copy constructor.
 */
SortedSampleOutputStream::SortedSampleOutputStream(
	const SortedSampleOutputStream& x)
	: SampleOutputStream(x),initialized(false),
	sorter(x.sorter),proxy(0,*this)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */
SortedSampleOutputStream::SortedSampleOutputStream(
	const SortedSampleOutputStream& x,IOChannel* iochannel)
	: SampleOutputStream(x,iochannel),initialized(false),
	sorter(x.sorter),proxy(0,*this)
{
}

SortedSampleOutputStream::~SortedSampleOutputStream()
{
    if (initialized) {
	sorter.interrupt();
	sorter.join();
    }
}

SortedSampleOutputStream* SortedSampleOutputStream::clone(IOChannel* iochannel) const 
{
    if (iochannel) return new SortedSampleOutputStream(*this,iochannel);
    else return new SortedSampleOutputStream(*this);
}

void SortedSampleOutputStream::init() throw()
{
    SampleOutputStream::init();
    try {
	sorter.start();
    }
    catch(const atdUtil::Exception& e) {
    }
    sorter.addSampleClient(&proxy);
    initialized = true;
}
bool SortedSampleOutputStream::receive(const Sample *s) throw()
{
    return sorter.receive(s);
}

