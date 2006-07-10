/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-22 14:51:10 -0600 (Mon, 22 May 2006) $

    $LastChangedRevision: 3363 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/dynld/SampleOutput.cc $
 ********************************************************************

*/

#include <nidas/core/StatusThread.h>
#include <nidas/core/SampleOutput.h>
#include <nidas/core/DSMTime.h>
#include <nidas/core/Version.h>
#include <nidas/core/Project.h>

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;


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
    cerr << "~SampleOutputBase(), this=" << this << endl;
#endif
    delete iochan;
}

void SampleOutputBase::init() throw()
{
    nextFileTime = LONG_LONG_MIN;
}

void SampleOutputBase::close() throw(n_u::IOException)
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
	throw(n_u::IOException)
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
	throw(n_u::IOException)
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
	connectionRequester->connected(this,newout);
	cerr << "SampleOutputBase::connected new channel" << endl;
    }
    else {
	assert(connectionRequester);
        connectionRequester->connected(this,this);
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
	throw(n_u::IOException)
{
    close();
    // warning: disconnected(this) ends doing a delete
    // of this SampleOutputBase, so don't do anything
    // other than return after the call.  That seems
    // to be OK.
    if (connectionRequester) connectionRequester->disconnected(this);
}

void SampleOutputBase::createNextFile(dsm_time_t tt)
	throw(n_u::IOException)
{
    // The very first file we use an exact time in the name,
    // otherwise it is truncated down.
    nextFileTime = getIOChannel()->createFile(tt,
    	nextFileTime == LONG_LONG_MIN);
    if (connectionRequester)
	connectionRequester->sendHeader(tt,this);
}

void SampleOutputBase::write(const void* buf, size_t len)
	throw(n_u::IOException)
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
	throw(n_u::InvalidParameterException)
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
            throw n_u::InvalidParameterException(
                    "SampleOutputBase::fromDOMElement",
                    "output", "must have one child element");
	setIOChannel(ioc);
    }
    if (!getIOChannel())
        throw n_u::InvalidParameterException(
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

