/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <StatusThread.h>
#include <SampleOutput.h>
#include <DSMTime.h>
#include <Version.h>
#include <Project.h>

#include <atdUtil/Logger.h>

#include <iostream>

using namespace dsm;
using namespace std;

CREATOR_FUNCTION(SampleOutputStream)

SampleOutputBase::SampleOutputBase(IOChannel* i):
	name("SampleOutputBase"),
	iochan(i),
	connectionRequester(0),
	nextFileTime(LONG_LONG_MIN)
{
}

/*
 * Copy constructor.
 */

SampleOutputBase::SampleOutputBase(const SampleOutputBase& x):
	name(x.name),
	iochan(x.iochan->clone()),
	sampleTags(x.sampleTags),
	connectionRequester(x.connectionRequester),
	nextFileTime(LONG_LONG_MIN)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */

SampleOutputBase::SampleOutputBase(const SampleOutputBase& x,IOChannel* ioc):
	name(x.name),
	iochan(ioc),
	sampleTags(x.sampleTags),
	connectionRequester(x.connectionRequester),
	nextFileTime(LONG_LONG_MIN)
{
}

SampleOutputBase::~SampleOutputBase()
{
#ifdef DEBUG
    cerr << "~SampleOutputBase()" << endl;
#endif
    delete iochan;
}

void SampleOutputBase::init() throw()
{
    nextFileTime = LONG_LONG_MIN;
}

void SampleOutputBase::close() throw(atdUtil::IOException)
{
    if (iochan) iochan->close();
}

int SampleOutputBase::getFd() const
{
    if (iochan) return iochan->getFd();
    else return -1;
}

const set<const SampleTag*>& SampleOutputBase::getSampleTags() const
{
    return sampleTags;
}

void SampleOutputBase::addSampleTag(const SampleTag* val)
{
    sampleTags.insert(val);
    /*
    if (iochan) {
	cerr << getName() << " adding sample tag #" <<
		val->getId() << " to " << iochan->getName() << endl;
	iochan->addSampleTag(val);
    }
    */
}

void SampleOutputBase::setIOChannel(IOChannel* val)
{
    if (val != iochan) {
	delete iochan;
	iochan = val;
	if (iochan) setName(string("SampleOutputBase: ") + iochan->getName());
    }
}

void SampleOutputBase::requestConnection(SampleConnectionRequester* requester)
	throw(atdUtil::IOException)
{
    set<const SampleTag*>::const_iterator si = getSampleTags().begin();
    for ( ; si != getSampleTags().end(); ++si) {
#ifdef DEBUG
	cerr << "iochan=" << iochan->getName() << " add tag=" <<
		(*si)->getId() << endl;
#endif
	iochan->addSampleTag(*si);
    }

    connectionRequester = requester;
    iochan->requestConnection(this);
}

void SampleOutputBase::connect()
	throw(atdUtil::IOException)
{
    IOChannel* ioc = iochan->connect();
    setIOChannel(ioc);
}

/*
 * How an IOChannel notifies a SampleOutput that is it connected.
 */
void SampleOutputBase::connected(IOChannel* ioc) throw()
{

    if (!iochan) iochan = ioc;
    else if (iochan != ioc) {
	assert(connectionRequester);
	// This is a new ioc- probably a connected socket.
	// Clone myself and report back to connectionRequester.
	SampleOutput* newout = clone(ioc);
	connectionRequester->connected(newout);
	cerr << "SampleOutputBase::connected new channel" << endl;
    }
    else {
	assert(connectionRequester);
        connectionRequester->connected(this);
	setName(string("SampleOutputBase: ") + iochan->getName());
	cerr << "SampleOutputBase::connected old channel" << endl;
    }
#ifdef DEBUG
    cerr << "SampleOutputBase::connected, ioc" <<
    	ioc->getName() << " fd="  <<
    	ioc->getFd() << endl;
#endif

}


void SampleOutputBase::disconnect()
	throw(atdUtil::IOException)
{
    close();
    // warning: disconnected(this) ends doing a delete
    // of this SampleOutputBase, so don't do anything
    // other than return after the call.  That seems
    // to be OK.
    if (connectionRequester) connectionRequester->disconnected(this);
}

void SampleOutputBase::createNextFile(dsm_time_t tt)
	throw(atdUtil::IOException)
{
    // The very first file we use an exact time in the name,
    // otherwise it is truncated down.
    nextFileTime = getIOChannel()->createFile(tt,
    	nextFileTime == LONG_LONG_MIN);
    if (connectionRequester)
	connectionRequester->sendHeader(tt,this);
    else header.write(this);
}

void SampleOutputBase::write(const void* buf, size_t len)
	throw(atdUtil::IOException)
{
    // brute force it. This is typically only used for
    // writing the header.
    const char* cbuf = (const char*) buf;
    while (len > 0) {
	size_t l = iochan->write(cbuf,len);
	len -= l;
	cbuf += l;
    }
}

void SampleOutputBase::fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();
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

        IOChannel* ioc =
		IOChannel::createIOChannel((xercesc::DOMElement*)child);
	/*
	set<const SampleTag*>::const_iterator sti =
		getSampleTags().begin();
	for ( ; sti != getSampleTags().end(); ++sti)
	    ioc->addSampleTag(*sti);
	*/

	ioc->fromDOMElement((xercesc::DOMElement*)child);

	if (++niochan > 1)
            throw atdUtil::InvalidParameterException(
                    "SampleOutputBase::fromDOMElement",
                    "output", "must have one child element");
	setIOChannel(ioc);
    }
    if (!getIOChannel())
        throw atdUtil::InvalidParameterException(
                "SampleOutputBase::fromDOMElement",
		"output", "must have one child element");
    setName(string("SampleOutputBase: ") + getIOChannel()->getName());
}

xercesc::DOMElement* SampleOutputBase::toDOMParent(
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
                                                                                
xercesc::DOMElement* SampleOutputBase::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

SampleOutputStream::SampleOutputStream(IOChannel* i):
	SampleOutputBase(i),iostream(0),
	nsamplesDiscarded(0)
{
}

/*
 * Copy constructor.
 */
SampleOutputStream::SampleOutputStream(const SampleOutputStream& x):
	SampleOutputBase(x),iostream(0),
	nsamplesDiscarded(0)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */

SampleOutputStream::SampleOutputStream(const SampleOutputStream& x,IOChannel* ioc):
	SampleOutputBase(x,ioc),
	iostream(0),nsamplesDiscarded(0)
{
}

SampleOutputStream::~SampleOutputStream()
{
#ifdef DEBUG
    cerr << "~SampleOutputStream()" << endl;
#endif
    delete iostream;
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
    delete iostream;
#ifdef DEBUG
    cerr << "SampleOutputStream::init, buffer size=" <<
    	getIOChannel()->getBufferSize() << " fd=" << getIOChannel()->getFd() << endl;
#endif
    iostream = new IOStream(*getIOChannel(),getIOChannel()->getBufferSize());
}

void SampleOutputStream::close() throw(atdUtil::IOException)
{
#ifdef DEBUG
    cerr << "SampleOutputStream::close" << endl;
#endif
    delete iostream;
    iostream = 0;
    SampleOutputBase::close();
}

void SampleOutputStream::finish() throw()
{
    try {
	if (iostream) iostream->flush();
    }
    catch (atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: %s",getName().c_str(),ioe.what());
    }
}

bool SampleOutputStream::receive(const Sample *samp) throw()
{
    if (!iostream) return false;

    dsm_time_t tsamp = samp->getTimeTag();

    DSMServerStat::getInstance()->setSomeTime(tsamp);

    try {
	if (tsamp >= getNextFileTime()) {
	    iostream->flush();
	    createNextFile(tsamp);
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
	disconnect();
	return false;
    }
    return true;
}

void SampleOutputStream::write(const void* buf, size_t len)
	throw(atdUtil::IOException)
{
    iostream->write(buf,len);
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

SortedSampleOutputStream::SortedSampleOutputStream():
	SampleOutputStream(),
	sorter(0),proxy(0,*this),
	sorterLengthMsecs(250)
{
}
/*
 * Copy constructor.
 */
SortedSampleOutputStream::SortedSampleOutputStream(
	const SortedSampleOutputStream& x)
	: SampleOutputStream(x),
	sorter(0),proxy(0,*this),
	sorterLengthMsecs(x.sorterLengthMsecs)
{
}

/*
 * Copy constructor, with a new IOChannel.
 */
SortedSampleOutputStream::SortedSampleOutputStream(
	const SortedSampleOutputStream& x,IOChannel* ioc)
	: SampleOutputStream(x,ioc),
	sorter(0),proxy(0,*this),
	sorterLengthMsecs(x.sorterLengthMsecs)
{
}

SortedSampleOutputStream::~SortedSampleOutputStream()
{
    if (sorter) {
	sorter->interrupt();
	sorter->join();
	delete sorter;
    }
}

SortedSampleOutputStream* SortedSampleOutputStream::clone(IOChannel* ioc) const 
{
    if (ioc) return new SortedSampleOutputStream(*this,ioc);
    else return new SortedSampleOutputStream(*this);
}

void SortedSampleOutputStream::init() throw()
{
    if (getSorterLengthMsecs() > 0) {
	if (!sorter) sorter = new SampleSorter("SortedSampleOutputStream");
	sorter->setLengthMsecs(getSorterLengthMsecs());
	SampleOutputStream::init();
	try {
	    sorter->start();
	}
	catch(const atdUtil::Exception& e) {
	}
	sorter->addSampleClient(&proxy);
    }
}
bool SortedSampleOutputStream::receive(const Sample *s) throw()
{
    if (sorter) return sorter->receive(s);
    return SampleOutputStream::receive(s);
}

void SortedSampleOutputStream::fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    SampleOutputStream::fromDOMElement(node);
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
    // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (!aname.compare("sorterLength")) {
	        istringstream ist(aval);
		int len;
		ist >> len;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(
		    	"SortedSampleOutputStream",
			attr.getName(),attr.getValue());
		setSorterLengthMsecs(len);
	    }
	}
    }
}
