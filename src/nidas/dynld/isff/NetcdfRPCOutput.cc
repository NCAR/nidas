/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifdef HAS_NC_SERVER_RPC_H

#include <nidas/dynld/isff/NetcdfRPCOutput.h>
#include <nidas/dynld/isff/NetcdfRPCChannel.h>
#include <nidas/util/Logger.h>

using namespace nidas::dynld::isff;
using namespace nidas::dynld;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,NetcdfRPCOutput)

NetcdfRPCOutput::NetcdfRPCOutput():
	SampleOutputBase(),_ncChannel(0)
{
}

NetcdfRPCOutput::NetcdfRPCOutput(IOChannel* ioc):
	SampleOutputBase(ioc)
{
    setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
    _ncChannel = dynamic_cast<NetcdfRPCChannel*>(getIOChannel());
}

/* copy constructor */
NetcdfRPCOutput::NetcdfRPCOutput(NetcdfRPCOutput& x,IOChannel*ioc):
	SampleOutputBase(x,ioc)
{
    setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
    _ncChannel = dynamic_cast<NetcdfRPCChannel*>(getIOChannel());
}

NetcdfRPCOutput::~NetcdfRPCOutput()
{
}

void NetcdfRPCOutput::requestConnection(SampleConnectionRequester* requester)
	throw(n_u::IOException)
{
    // NetcdfRPCChannel needs to know the SampleTags before it connects.
    list<const SampleTag*> tags = getSourceSampleTags();
    for (list<const SampleTag*>::const_iterator ti = tags.begin();
        ti != tags.end(); ++ti) _ncChannel->addSampleTag(*ti);
    SampleOutputBase::requestConnection(requester);
}

SampleOutput* NetcdfRPCOutput::connected(IOChannel* ioc) throw()
{
    SampleOutput* so = SampleOutputBase::connected(ioc);
    if (so == this && !_ncChannel) setIOChannel(ioc);
    return so;
}


void NetcdfRPCOutput::setIOChannel(IOChannel* val)
{
    SampleOutputBase::setIOChannel(val);
    setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
    _ncChannel = dynamic_cast<NetcdfRPCChannel*>(getIOChannel());
}

bool NetcdfRPCOutput::receive(const Sample* samp) 
    throw()
{
    // cerr << "NetcdfRPCOutput::receive, samp=" << samp->getDSMId() << ',' << samp->getSpSId() << endl;
    try {
	_ncChannel->write(samp);
    }
    catch (const n_u::IOException& e) {
        n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		getName().c_str(),e.what());
        disconnect();
	return false;
    }
    return true;
}

void NetcdfRPCOutput::fromDOMElement(const xercesc::DOMElement* node)
        throw(n_u::InvalidParameterException)
{
    // process <ncserver> tag
    int niochan = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

        if (elname == "ncserver") {
	    IOChannel* ioc = new NetcdfRPCChannel();
	    ioc->fromDOMElement((xercesc::DOMElement*)child);
	    setIOChannel(ioc);
	}
	else throw n_u::InvalidParameterException(
                    "NetcdfRPCOutput::fromDOMElement",
		    "parse", "only supports ncserver elements");

        if (++niochan > 1)
            throw n_u::InvalidParameterException(
                    "NetcdfRPCOutput::fromDOMElement",
                    "parse", "must have only one child element");
    }
    if (!getIOChannel())
        throw n_u::InvalidParameterException(
                "NetcdfRPCOutput::fromDOMElement",
                "parse", "must have one child element");
    setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
}

#endif  // HAS_NC_SERVER_RPC_H
