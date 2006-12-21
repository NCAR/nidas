
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/dynld/XMLConfigService.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMServer.h>

#include <nidas/core/Datagrams.h>

#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLConfigWriter.h>
#include <nidas/core/XMLFdFormatTarget.h>

#include <nidas/util/Logger.h>

// #include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMNodeList.hpp>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(XMLConfigService)

XMLConfigService::XMLConfigService():
	DSMService("XMLConfigService"),iochan(0),dsm(0)
{
}

/*
 * Copy constructor.
 */
XMLConfigService::XMLConfigService(const XMLConfigService& x,IOChannel* ioc):
        DSMService((const DSMService&)x),iochan(ioc),dsm(x.dsm)
{
}

XMLConfigService::~XMLConfigService()
{
    if (iochan) {
        iochan->close();
	delete iochan;
    }
}

void XMLConfigService::schedule() throw(n_u::Exception)
{
    iochan->setRequestNumber(XML_CONFIG);
    iochan->requestConnection(this);
}

void XMLConfigService::interrupt() throw()
{
    iochan->close();
    DSMService::interrupt();
}
void XMLConfigService::connected(IOChannel* iochan) throw()
{
    // Figure out what DSM it came from
    n_u::Inet4Address remoteAddr = iochan->getRemoteInet4Address();
    cerr << "findDSM, addr=" << remoteAddr.getHostAddress() << endl;
    const DSMConfig* dsm = Project::getInstance()->findDSM(remoteAddr);
    if (!dsm) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
	    "can't find DSM for address %s" ,
	    remoteAddr.getHostAddress().c_str());
	return;
    }

    cerr << "findDSM, dsm=" << dsm->getName() << endl;
    // make a copy of myself, assign it to a specific dsm
    XMLConfigService* newserv = new XMLConfigService(*this,iochan);
    newserv->setDSMConfig(dsm);
    newserv->start();

    addSubService(newserv);
}

int XMLConfigService::run() throw(n_u::Exception)
{
    XMLCachingParser* parser = XMLCachingParser::getInstance();

    xercesc::DOMDocument* doc = parser->parse(
        Project::expandEnvVars(DSMServer::getInstance()->getXMLFileName()));

    xercesc::DOMNodeList* projnodes =
        doc->getElementsByTagName((const XMLCh*)XMLStringConverter("project"));

    for (unsigned int i = 0; i < projnodes->getLength(); i++) {
        xercesc::DOMNode* proj = projnodes->item(i);
        XDOMElement xnode((xercesc::DOMElement*)proj);
        xnode.setAttributeValue("config",
            DSMServer::getInstance()->getXMLFileName());
    }
    // delete projnodes;

    XMLFdFormatTarget formatter(iochan->getName(),iochan->getFd());

    XMLConfigWriter writer(getDSMConfig());
    writer.writeNode(&formatter,*doc);

    iochan->close();
    return RUN_OK;
}

void XMLConfigService::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    int niochan = 0;
    XDOMElement xnode(node);
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();

        if (!elname.compare("output")) {
	    xercesc::DOMNode* gkid;
	    for (gkid = child->getFirstChild(); gkid != 0;
		    gkid=gkid->getNextSibling())
	    {
		if (gkid->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;

		iochan = IOChannel::createIOChannel((xercesc::DOMElement*)gkid);
		iochan->fromDOMElement((xercesc::DOMElement*)gkid);

		if (++niochan > 1)
		    throw n_u::InvalidParameterException(
			"XMLConfigService::fromDOMElement",
			"output", "must have one child element");
	    }

        }
        else throw n_u::InvalidParameterException(
                "XMLConfigService::fromDOMElement",
                elname, "unsupported element");
    }
    if (iochan == 0)
	throw n_u::InvalidParameterException(
	    "XMLConfigService::fromDOMElement",
	    "output", "one output required");
}

