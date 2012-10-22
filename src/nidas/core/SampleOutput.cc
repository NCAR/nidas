// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/StatusThread.h>
#include <nidas/core/SampleOutput.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Version.h>
#include <nidas/core/Project.h>

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleOutputBase::SampleOutputBase():
    _name("SampleOutputBase"),
    _tagsMutex(),_requestedTags(),_constRequestedTags(),
    _iochan(0),
    _connectionRequester(0),
    _nextFileTime(LONG_LONG_MIN),
    _headerSource(0),_dsm(0),
    _nsamplesDiscarded(0),_parameters(),_constParameters(),
    _sourceTags(),
    _original(this), _latency(0.25),_reconnectDelaySecs(-2)
{
}

SampleOutputBase::SampleOutputBase(IOChannel* ioc,SampleConnectionRequester* rqstr):
    _name("SampleOutputBase"),
    _tagsMutex(),_requestedTags(),_constRequestedTags(),
    _iochan(ioc),
    _connectionRequester(rqstr),
    _nextFileTime(LONG_LONG_MIN),
    _headerSource(0),_dsm(0),
    _nsamplesDiscarded(0),_parameters(),_constParameters(),
    _sourceTags(),
    _original(this), _latency(0.25),_reconnectDelaySecs(-2)
{
}

/*
 * Copy constructor, with a new, connected IOChannel.
 */
SampleOutputBase::SampleOutputBase(SampleOutputBase& x,IOChannel* ioc):
    _name(x._name),
    _tagsMutex(),_requestedTags(),_constRequestedTags(),
    _iochan(ioc),
    _connectionRequester(x._connectionRequester),
    _nextFileTime(LONG_LONG_MIN),
    _headerSource(x._headerSource),_dsm(x._dsm),
    _nsamplesDiscarded(0),_parameters(),_constParameters(),
    _sourceTags(),
    _original(&x),_latency(x._latency),_reconnectDelaySecs(x._reconnectDelaySecs)
{
    _iochan->setDSMConfig(getDSMConfig());

    list<const SampleTag*> tags = x.getRequestedSampleTags();
    list<const SampleTag*>::const_iterator si = tags.begin();
    for ( ; si != tags.end(); ++si) {
        const SampleTag *tag = *si;
        addRequestedSampleTag(new SampleTag(*tag));
    }

    tags = x.getSourceSampleTags();
    for (si = tags.begin(); si != tags.end(); ++si)
        addSourceSampleTag(*si);
}

SampleOutputBase::~SampleOutputBase()
{
#ifdef DEBUG
    cerr << "~SampleOutputBase(), this=" << this << endl;
#endif
    delete _iochan;

    map<string,Parameter*>::const_iterator pi;
    for (pi = _parameters.begin(); pi != _parameters.end(); ++pi)
	delete pi->second;

    list<SampleTag*>::const_iterator si = _requestedTags.begin();
    for ( ; si != _requestedTags.end(); ++si)
        delete *si;

}

int SampleOutputBase::getReconnectDelaySecs() const
{
    if (_reconnectDelaySecs >= -1) return _reconnectDelaySecs;
    if (_iochan) return _iochan->getReconnectDelaySecs();
    return 10;
}

void SampleOutputBase::setReconnectDelaySecs(int val)
{
    _reconnectDelaySecs = val;
}

void SampleOutputBase::addRequestedSampleTag(SampleTag* tag)
    throw(n_u::InvalidParameterException)
{
    n_u::Autolock autolock(_tagsMutex);
    if (find(_requestedTags.begin(),_requestedTags.end(),tag) ==
        _requestedTags.end()) {
        _requestedTags.push_back(tag);
        _constRequestedTags.push_back(tag);
    }
}

std::list<const SampleTag*> SampleOutputBase::getRequestedSampleTags() const
{
    n_u::Autolock autolock(_tagsMutex);
    return _constRequestedTags;
}

void SampleOutputBase::addSourceSampleTag(const SampleTag* tag)
    throw(n_u::InvalidParameterException)
{
    n_u::Autolock autolock(_tagsMutex);
    if (find(_sourceTags.begin(),_sourceTags.end(),tag) == _sourceTags.end())
        _sourceTags.push_back(tag);
}

void SampleOutputBase::addSourceSampleTags(const list<const SampleTag*>& tags)
    throw(n_u::InvalidParameterException)
{
    list<const SampleTag*>::const_iterator ti = tags.begin();
    for ( ; ti != tags.end(); ++ti) {
        const SampleTag* tag = *ti;
        // want to use the virtual addSampleTag() method here, even if it
        // means repeated  locks, so that derived classes only have to
        // re-implement addSampleTag. 
        addSourceSampleTag(tag);
    }
}

list<const SampleTag*> SampleOutputBase::getSourceSampleTags() const
{
    n_u::Autolock autolock(_tagsMutex);
    return list<const SampleTag*>(_sourceTags);
}

void SampleOutputBase::close() throw(n_u::IOException)
{
#ifdef DEBUG
    DLOG(("closing: ") << getName());
#endif
    _nextFileTime = LONG_LONG_MIN;
    if (_iochan) _iochan->close();
}

int SampleOutputBase::getFd() const
{
    if (_iochan) return _iochan->getFd();
    else return -1;
}

void SampleOutputBase::setIOChannel(IOChannel* val)
{
    if (val != _iochan) {
	delete _iochan;
	_iochan = val;
    }
    _iochan->setDSMConfig(getDSMConfig());
}

void SampleOutputBase::requestConnection(SampleConnectionRequester* requester)
	throw(n_u::IOException)
{
    _connectionRequester = requester;
    _iochan->requestConnection(this);
}

/*
 * implementation of IOChannelRequester::connected().
 * How an IOChannel notifies a SampleOutput that it is connected.
 */
SampleOutput* SampleOutputBase::connected(IOChannel* ioc) throw()
{
    setName(string("SampleOutputBase: ") + ioc->getName());
    ILOG(("%s: has connected",getName().c_str()));
    if (_iochan && _iochan != ioc) {
	// This is a new IOChannel, probably a connected socket.
	// Clone myself and report back to connectionRequester.
	if (_connectionRequester) {
            SampleOutput* newout = clone(ioc);
            _connectionRequester->connect(newout);
            return newout;
        }
        else {
            // If no requester, set the iochan.
            _iochan->close();
            _nextFileTime = LONG_LONG_MIN;
	    setIOChannel(ioc);
        }
    }
    else {
        if (!_iochan) setIOChannel(ioc);
        _nextFileTime = LONG_LONG_MIN;
	if (_connectionRequester) _connectionRequester->connect(this);
    }
    return this;
}

/* implementation of SampleOutput::disconnect() */
void SampleOutputBase::disconnect()
	throw(n_u::IOException)
{
    ILOG(("%s: disconnecting",getName().c_str()));

    // _connectionRequester::disconnect() may delete this object.
    // Don't access any member of the SampleOutput object after
    // the call to disconnect.
    if (_connectionRequester) _connectionRequester->disconnect(this);
    else close();
}

/*
 * Add a parameter to my map, and list.
 */
void SampleOutputBase::addParameter(Parameter* val)
{
    map<string,Parameter*>::iterator pi = _parameters.find(val->getName());
    if (pi == _parameters.end()) {
        _parameters[val->getName()] = val;
	_constParameters.push_back(val);
    }
    else {
	// parameter with name exists. If the pointers aren't equal
	// delete the old parameter.
	Parameter* p = pi->second;
	if (p != val) {
	    // remove it from constParameters list
	    list<const Parameter*>::iterator cpi = _constParameters.begin();
	    for ( ; cpi != _constParameters.end(); ) {
		if (*cpi == p) cpi = _constParameters.erase(cpi);
		else ++cpi;
	    }
	    delete p;
	    pi->second = val;
	    _constParameters.push_back(val);
	}
    }
}

const Parameter* SampleOutputBase::getParameter(const string& name) const
{
    map<string,Parameter*>::const_iterator pi = _parameters.find(name);
    if (pi == _parameters.end()) return 0;
    return pi->second;
}

void SampleOutputBase::createNextFile(dsm_time_t tt)
	throw(n_u::IOException)
{
    // The very first file we use an exact time in the name,
    // otherwise it is truncated down.
    _nextFileTime = getIOChannel()->createFile(tt,
    	_nextFileTime == LONG_LONG_MIN);
    if (getIOChannel()->writeNidasHeader()) {
        if (_headerSource)
            _headerSource->sendHeader(tt,this);
        else HeaderSource::sendDefaultHeader(this);
    }
}

size_t SampleOutputBase::write(const void* buf, size_t len)
	throw(n_u::IOException)
{
    // brute force it. This is typically only used for
    // writing the header.
    const char* cbuf = (const char*) buf;
    size_t lout = 0;
    while (len > 0) {
	size_t l = _iochan->write(cbuf+lout,len);
	len -= l;
        lout += l;
    }
    return lout;
}

void SampleOutputBase::fromDOMElement(const xercesc::DOMElement* node)
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
            const std::string& aval = attr.getValue();
            // Sample sorter length in seconds
	    if (aname == "class");
	    else if (aname == "sorterLength") {
                WLOG(("SampleOutputBase: attribute ") << aname << " is deprecated");
	    }
	    else if (aname == "heapMax") {
                WLOG(("SampleOutputBase: attribute ") << aname << " is deprecated");
	    }
	    else if (aname == "latency") {
		istringstream ist(aval);
		float val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),
		    	aname,aval);
		setLatency(val);
	    }
	    else throw n_u::InvalidParameterException(
	    	string("SampleOutputBase: unrecognized attribute: ") + aname);
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

	if (elname == "parameter") {
	    Parameter* parameter =
            Parameter::createParameter((xercesc::DOMElement*)child,&Project::getInstance()->getDictionary());
	    addParameter(parameter);
	}
        else {
            // nidas.xsd schema currently allows these elements which are all
            // IOChannels: socket,fileset,postgresdb,ncserver,goes
            IOChannel* ioc =
                    IOChannel::createIOChannel((xercesc::DOMElement*)child);

            ioc->setDSMConfig(getDSMConfig());
            ioc->fromDOMElement((xercesc::DOMElement*)child);

            if (++niochan > 1)
                throw n_u::InvalidParameterException(
                        "SampleOutputBase::fromDOMElement",
                        "output", "must have one child element");
            setIOChannel(ioc);
        }
    }
    if (!getIOChannel())
        throw n_u::InvalidParameterException(
                "SampleOutputBase::fromDOMElement",
		"output", "must have one child element");
    setName(string("SampleOutputBase: ") + getIOChannel()->getName());

}

