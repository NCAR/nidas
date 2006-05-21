/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <IOChannel.h>
#include <Socket.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

IOChannel::IOChannel()
	// : headerValidator(0)
{
}

/* static */
IOChannel* IOChannel::createIOChannel(const DOMElement* node)
            throw(atdUtil::InvalidParameterException)
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
    	domable = DOMObjectFactory::createObject("NcServerRPC");

    else throw atdUtil::InvalidParameterException(
	    "IOChannel::fromIOChannelDOMElement","unknown element",type);

    if (!(channel = dynamic_cast<IOChannel*>(domable)))
	throw atdUtil::InvalidParameterException(
	    "IOChannel::fromIOChannelDOMElement",type,"is not an IOChannel");
    return channel;
}
