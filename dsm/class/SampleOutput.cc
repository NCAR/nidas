/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SampleOutput.h>
#include <DSMTime.h>

#include <atdUtil/Logger.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SampleOutputStream)

SampleOutputStream::SampleOutputStream():
	name("SampleOutputStream"),iochan(0),iostream(0),
	pseudoPort(0),dsm(0),service(0),connectionRequester(0),
	type(TIMETAG_DEPENDENT),nextFileTime(0)
{
}

SampleOutputStream::SampleOutputStream(const SampleOutputStream& x):
	name(x.name), iochan(x.iochan->clone()),iostream(0),
	pseudoPort(x.pseudoPort),
	dsm(x.dsm),service(x.service),
	connectionRequester(x.connectionRequester),
	type(TIMETAG_DEPENDENT),nextFileTime(0)
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

SampleOutput* SampleOutputStream::clone() const
{
    // invoke copy constructor
    return new SampleOutputStream(*this);
}

void SampleOutputStream::requestConnection(SampleConnectionRequester* requester)
	throw(atdUtil::IOException)
{
    connectionRequester = requester;
    iochan->requestConnection(this,getPseudoPort());
}


void SampleOutputStream::setDSMConfig(const DSMConfig* val)
{
    dsm = val;
    if (iochan) iochan->setDSMConfig(val);
}

const DSMConfig* SampleOutputStream::getDSMConfig() const
{
    return dsm;
}

void SampleOutputStream::setDSMService(const DSMService* val)
{
    service = val;
    if (iochan) iochan->setDSMService(val);
}

const DSMService* SampleOutputStream::getDSMService() const
{
    return service;
}


void SampleOutputStream::setPseudoPort(int val) { pseudoPort = val; }

int SampleOutputStream::getPseudoPort() const { return pseudoPort; }

/*
 * We're connected.
 */
void SampleOutputStream::connected(IOChannel* iochannel) throw()
{

#ifdef DEBUG
    cerr << "SampleOutputStream::connected, iochannel " <<
    	iochannel->getName() << " fd="  <<
    	iochannel->getFd() << endl;
#endif
    assert(iochan == iochannel);
    assert(connectionRequester);
    setName(string("SampleOutputStream: ") + iochan->getName());
    connectionRequester->connected(this);
}

void SampleOutputStream::init() throw()
{
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

    if (nextFileTime == 0) nextFileTime = tsamp;
    if (tsamp >= nextFileTime) {
#ifdef DEBUG
	    cerr << "calling iostream->createFile, nextFileTime=" << nextFileTime << endl;
#endif

	dsm_time_t newFileTime = iostream->createFile(nextFileTime);
	if (connectionRequester)
	    connectionRequester->newFileCallback(nextFileTime);
	nextFileTime = newFileTime;
    }

    try {
	write(samp);
    }
    catch(const atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s",getName().c_str(),ioe.what());
	connectionRequester->disconnected(this);
	return false;
    }
    return true;
}

size_t SampleOutputStream::write(const Sample* samp) throw(atdUtil::IOException)
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
        XDOMElement xchild((DOMElement*) child);

	const string& elname = xchild.getNodeName();

        iochan = IOChannel::createIOChannel(elname);

        iochan->setDSMConfig(getDSMConfig());
        iochan->setDSMService(getDSMService());

	iochan->fromDOMElement((DOMElement*)child);

	if (++niochan > 1)
            throw atdUtil::InvalidParameterException(
                    "SampleOutputStream::fromDOMElement",
                    "output", "must have one child element");


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
	SampleOutputStream(),initialized(false),sorter(250),proxy(0,*this)
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

SortedSampleOutputStream::~SortedSampleOutputStream()
{
    if (initialized) {
	sorter.interrupt();
	sorter.join();
    }
}

SampleOutput* SortedSampleOutputStream::clone() const 
{
    return new SortedSampleOutputStream(*this);
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

