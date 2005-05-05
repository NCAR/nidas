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

using namespace dsm;
using namespace std;

IOChannel::IOChannel()
{
}

/* static */
IOChannel* IOChannel::createIOChannel(const string& type)
            throw(atdUtil::InvalidParameterException)
{
    IOChannel* channel = 0;
    DOMable* domable;

    if (!type.compare("socket"))
    	domable = DOMObjectFactory::createObject("McSocket");

    else if (!type.compare("fileset"))
    	domable = DOMObjectFactory::createObject("FileSet");

    else if (!type.compare("postgresdb"))
    	domable = DOMObjectFactory::createObject("PSQLChannel");

    else throw atdUtil::InvalidParameterException(
	    "IOChannel::fromIOChannelDOMElement","unknown element",type);

    if (!(channel = dynamic_cast<IOChannel*>(domable)))
	throw atdUtil::InvalidParameterException(
	    "IOChannel::fromIOChannelDOMElement",type,"is not an IOChannel");
    return channel;
}
