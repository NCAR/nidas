/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-03-03 12:46:08 -0700 (Fri, 03 Mar 2006) $

    $LastChangedRevision: 3299 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:5080/svn/nids/branches/ISFF_TREX/dsm/class/NetcdfRPCOutput.cc $
 ********************************************************************

*/

#include <NetcdfRPCOutput.h>
#include <NetcdfRPCChannel.h>

#ifdef NEEDED
#include <DSMConfig.h>
#include <DSMTime.h>
#include <Site.h>
#include <Project.h>
#endif

#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;

CREATOR_FUNCTION(NetcdfRPCOutput)

NetcdfRPCOutput::NetcdfRPCOutput(IOChannel* ioc):
	SampleOutputBase(ioc),ncChannel(0)
{
    if (getIOChannel()) {
        setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
	ncChannel = dynamic_cast<NetcdfRPCChannel*>(getIOChannel());
    }
}

/* copy constructor */
NetcdfRPCOutput::NetcdfRPCOutput(const NetcdfRPCOutput& x):
	SampleOutputBase(x),ncChannel(0)
{
    if (getIOChannel()) {
        setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
	ncChannel = dynamic_cast<NetcdfRPCChannel*>(getIOChannel());
    }
}

/* copy constructor */
NetcdfRPCOutput::NetcdfRPCOutput(const NetcdfRPCOutput& x,IOChannel*ioc):
	SampleOutputBase(x,ioc),ncChannel(0)
{
    if (getIOChannel()) {
        setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
	ncChannel = dynamic_cast<NetcdfRPCChannel*>(getIOChannel());
    }
}

NetcdfRPCOutput::~NetcdfRPCOutput()
{
}

void NetcdfRPCOutput::setIOChannel(IOChannel* val)
{
    SampleOutputBase::setIOChannel(val);
    if (getIOChannel()) {
        setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
	ncChannel = dynamic_cast<NetcdfRPCChannel*>(getIOChannel());
    }
    else ncChannel = 0;
}


bool NetcdfRPCOutput::receive(const Sample* samp) 
    throw()
{
    try {
	ncChannel->write(samp);
    }
    catch (const atdUtil::IOException& e) {
        atdUtil::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		getName().c_str(),e.what());
	return false;
    }
    return true;
}

void NetcdfRPCOutput::fromDOMElement(const xercesc::DOMElement* node)
        throw(atdUtil::InvalidParameterException)
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

        if (!elname.compare("ncserver")) {
	    IOChannel* ioc = new NetcdfRPCChannel();
	    ioc->fromDOMElement((xercesc::DOMElement*)child);
	    setIOChannel(ioc);
	}
	else throw atdUtil::InvalidParameterException(
                    "NetcdfRPCOutput::fromDOMElement",
		    "parse", "only supports ncserver elements");

        if (++niochan > 1)
            throw atdUtil::InvalidParameterException(
                    "NetcdfRPCOutput::fromDOMElement",
                    "parse", "must have only one child element");
    }
    if (!getIOChannel())
        throw atdUtil::InvalidParameterException(
                "NetcdfRPCOutput::fromDOMElement",
                "parse", "must have one child element");
    setName(string("NetcdfRPCOutput: ") + getIOChannel()->getName());
}
