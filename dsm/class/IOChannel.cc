/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <IOChannel.h>
#include <McSocket.h>
#include <FileSet.h>

using namespace dsm;
using namespace std;

/* static */
IOChannel* IOChannel::fromIOChannelDOMElement(const xercesc::DOMElement* node)
            throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode((xercesc::DOMElement*) node);
    const string& elname = xnode.getNodeName();
    IOChannel* channel = 0;

    if (!elname.compare("socket")) channel = new McSocket();
    else if (!elname.compare("fileset")) channel = new FileSet();
    else throw atdUtil::InvalidParameterException(
	    "IOChannel::fromIOChannelDOMElement","unknown element",elname);

    if (!channel) throw atdUtil::InvalidParameterException(
	    "IOChannel::fromIOChannelDOMElement","input/output",	
	    "no <socket> or <fileset> tags found");

    channel->fromDOMElement((xercesc::DOMElement*)node);
    return channel;
}

