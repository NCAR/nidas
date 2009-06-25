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
#include <nidas/core/DSMTime.h>
#include <nidas/core/Version.h>
#include <nidas/core/Project.h>

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleOutputBase::SampleOutputBase(IOChannel* ioc):
	name("SampleOutputBase"),
	_iochan(0),
	_connectionRequester(0),
	_nextFileTime(LONG_LONG_MIN),
        _headerSource(0),_dsm(0),
        _nsamples(0),_nsamplesDiscarded(0),_lastTimeTag(0)
{
        setIOChannel(ioc);
}

/*
 * Copy constructor.
 */

SampleOutputBase::SampleOutputBase(const SampleOutputBase& x):
	name(x.name),
	_iochan(0),
	_sampleTags(x._sampleTags),
	_connectionRequester(x._connectionRequester),
	_nextFileTime(LONG_LONG_MIN),
        _headerSource(x._headerSource),_dsm(x._dsm),
        _nsamples(0),_nsamplesDiscarded(0),_lastTimeTag(0)
{
    if (x._iochan)
        setIOChannel(x._iochan->clone());
}

/*
 * Copy constructor, with a new IOChannel.
 */

SampleOutputBase::SampleOutputBase(const SampleOutputBase& x,IOChannel* ioc):
	name(x.name),
	_iochan(0),
	_sampleTags(x._sampleTags),
	_connectionRequester(x._connectionRequester),
	_nextFileTime(LONG_LONG_MIN),
        _headerSource(x._headerSource),_dsm(x._dsm),
        _nsamples(0),_nsamplesDiscarded(0),_lastTimeTag(0)
{
    setIOChannel(ioc);
}

SampleOutputBase::~SampleOutputBase()
{
#ifdef DEBUG
    cerr << "~SampleOutputBase(), this=" << this << endl;
#endif
    delete _iochan;

    map<std::string,Parameter*>::const_iterator pi;
    for (pi = _parameters.begin(); pi != _parameters.end(); ++pi)
	delete pi->second;

}

void SampleOutputBase::init() throw()
{
    _nextFileTime = LONG_LONG_MIN;
}

void SampleOutputBase::close() throw(n_u::IOException)
{
    if (_iochan) _iochan->close();
}

int SampleOutputBase::getFd() const
{
    if (_iochan) return _iochan->getFd();
    else return -1;
}

const list<const SampleTag*>& SampleOutputBase::getSampleTags() const
{
    return _sampleTags;
}

void SampleOutputBase::addSampleTag(const SampleTag* val)
{
    if (find(_sampleTags.begin(),_sampleTags.end(),val) == _sampleTags.end())
        _sampleTags.push_back(val);
}

void SampleOutputBase::setIOChannel(IOChannel* val)
{
    if (val != _iochan) {
	delete _iochan;
	_iochan = val;
    }
    if (_iochan) {
        _iochan->setDSMConfig(getDSMConfig());
	setName(string("SampleOutputBase: ") + _iochan->getName());
    }
}

void SampleOutputBase::requestConnection(SampleConnectionRequester* requester)
	throw(n_u::IOException)
{
    _connectionRequester = requester;
    _iochan->requestConnection(this);
}

/* implementation of SampleOutput::connect() */
void SampleOutputBase::connect()
	throw(n_u::IOException)
{
    IOChannel* ioc = _iochan->connect();
    setIOChannel(ioc);
}

/* implementation of SampleOutput::disconnect() */
void SampleOutputBase::disconnect()
	throw(n_u::IOException)
{
    close();
    // warning: disconnected(this) ends doing a delete
    // of this SampleOutputBase, so don't do anything
    // other than return after the call.  That seems
    // to be OK.
    if (_connectionRequester) _connectionRequester->disconnect(this);
}

/*
 * implementation of IOChannelRequester::connected().
 * How an IOChannel notifies a SampleOutput that is it connected.
 */
void SampleOutputBase::connected(IOChannel* ioc) throw()
{
    if (!_iochan) setIOChannel(ioc);
    else if (_iochan != ioc) {
	assert(_connectionRequester);
	// This is a new IOChannel, probably a connected socket.
	// Clone myself and report back to connectionRequester.
	SampleOutput* newout = clone(ioc);
	_connectionRequester->connect(this,newout);
    }
    else {
	assert(_connectionRequester);
	setName(string("SampleOutputBase: ") + _iochan->getName());
        _connectionRequester->connect(this,this);
    }
#ifdef DEBUG
    cerr << "SampleOutputBase::connected, ioc" <<
    	ioc->getName() << " fd="  <<
    	ioc->getFd() << endl;
#endif
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

const Parameter* SampleOutputBase::getParameter(const std::string& name) const
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
    if (_headerSource)
	_headerSource->sendHeader(tt,this);
    else HeaderSource::sendDefaultHeader(this);
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
	    Parameter::createParameter((xercesc::DOMElement*)child);
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

