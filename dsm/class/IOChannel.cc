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
#include <McSocket.h>
#include <FileSet.h>

using namespace dsm;
using namespace std;

IOChannel::IOChannel(): dsm(0),service(0)
{
}

/* static */
IOChannel* IOChannel::createIOChannel(const string& type)
            throw(atdUtil::InvalidParameterException)
{
    IOChannel* channel = 0;

    if (!type.compare("socket")) channel = new McSocket();
    else if (!type.compare("fileset")) channel = new FileSet();
    else throw atdUtil::InvalidParameterException(
	    "IOChannel::fromIOChannelDOMElement","unknown element",type);
    return channel;
}

