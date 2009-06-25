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

namespace n_u = nidas::util;

IOChannel::IOChannel(): _dsm(0)
{
}

/* static */
IOChannel* IOChannel::createIOChannel(const xercesc::DOMElement* node)
            throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();

    DOMable* domable;

    if (elname == "socket")
    	domable = Socket::createSocket(node);

    else if (elname == "fileset")
    	domable = DOMObjectFactory::createObject("FileSet");

    else if (elname == "postgresdb")
    	domable = DOMObjectFactory::createObject("psql.PSQLChannel");

    else if (elname == "ncserver")
    	domable = DOMObjectFactory::createObject("isff.NcServerRPC");

    else if (elname == "goes") {
	string classAttr = xnode.getAttributeValue("class");
	if (classAttr.length() == 0) classAttr = "isff.SE_GOESXmtr";
	domable = DOMObjectFactory::createObject(classAttr);
    }
    else throw n_u::InvalidParameterException(
	    "IOChannel::createIOChannel","unknown element",elname);

    IOChannel* channel;
    if (!(channel = dynamic_cast<IOChannel*>(domable)))
	throw n_u::InvalidParameterException(
	    "IOChannel::createIOChannel",elname,"is not an IOChannel");
    return channel;
}
