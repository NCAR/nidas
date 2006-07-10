/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/IOChannel.h>
#include <nidas/core/Socket.h>

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

IOChannel::IOChannel()
	// : headerValidator(0)
{
}

/* static */
IOChannel* IOChannel::createIOChannel(const DOMElement* node)
            throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& type = xnode.getNodeName();

    IOChannel* channel = 0;
    DOMable* domable;

    if (!type.compare("socket"))
    	domable = Socket::createSocket(node);

    else if (!type.compare("fileset"))
    	domable = DOMObjectFactory::createObject("FileSet");

    else if (!type.compare("postgresdb"))
    	domable = DOMObjectFactory::createObject("PSQLChannel");

    else if (!type.compare("ncserver"))
    	domable = DOMObjectFactory::createObject("isff.NcServerRPC");

    else if (!type.compare("goes")) {
	string classAttr = xnode.getAttributeValue("class");
	if (classAttr.length() == 0) classAttr = "isff.SE_GOESXmtr";
	domable = DOMObjectFactory::createObject(classAttr);
    }

    else throw n_u::InvalidParameterException(
	    "IOChannel::fromIOChannelDOMElement","unknown element",type);

    if (!(channel = dynamic_cast<IOChannel*>(domable)))
	throw n_u::InvalidParameterException(
	    "IOChannel::fromIOChannelDOMElement",type,"is not an IOChannel");
    return channel;
}
