
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <XMLConfigService.h>
#include <Aircraft.h>
// #include <DSMServer.h>

#include <Datagrams.h>

#include <XMLParser.h>
#include <XMLConfigWriter.h>
#include <XMLFdFormatTarget.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(XMLConfigService)

XMLConfigService::XMLConfigService():
	DSMService("XMLConfigService"),output(0)
{
}

XMLConfigService::~XMLConfigService()
{
    delete output;
}
/*
 * Copy constructor.
 */
XMLConfigService::XMLConfigService(const XMLConfigService& x):
        DSMService((const DSMService&)x),output(0)
{
    if (x.output) output = x.output->clone();
}

void XMLConfigService::schedule() throw(atdUtil::Exception)
{
    output->requestConnection(this,XML_CONFIG);
}

void XMLConfigService::offer(atdUtil::Socket* sock,int pseudoPort)
	throw(atdUtil::Exception)
{
    assert(pseudoPort == XML_CONFIG);
    // Figure out what DSM it came from
    atdUtil::Inet4Address remoteAddr = sock->getInet4Address();
    const DSMConfig* dsm = getAircraft()->findDSM(remoteAddr);
    if (!dsm)
	throw atdUtil::Exception(string("can't find DSM for address ") +
	    remoteAddr.getHostAddress());

    // make a copy of myself, assign it to a specific dsm
    XMLConfigService* newserv = new XMLConfigService();
    newserv->setDSMConfig(dsm);
    newserv->output->offer(sock);    // pass socket to new input
    newserv->start();
    getServer()->addThread(newserv);
}

int XMLConfigService::run() throw(atdUtil::Exception)
{
    XMLCachingParser* parser = XMLCachingParser::getInstance();

    xercesc::DOMDocument* doc = parser->parse(DSMServer::getXMLFileName());

    XMLFdFormatTarget formatter(output->getName(),output->getFd());

    XMLConfigWriter writer(getDSMConfig());
    writer.writeNode(&formatter,*doc);

    output->close();
    return RUN_OK;
}

void XMLConfigService::fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    int noutputs = 0;
    XDOMElement xnode(node);
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((DOMElement*) child);
        const string& elname = xchild.getNodeName();

        if (!elname.compare("output")) {
	    DOMNode* gkid;
	    for (gkid = child->getFirstChild(); gkid != 0;
		    gkid=gkid->getNextSibling())
	    {
		if (gkid->getNodeType() != DOMNode::ELEMENT_NODE) continue;
		output = Output::fromOutputDOMElement((xercesc::DOMElement*)gkid);
		if (++noutputs > 1)
		    throw atdUtil::InvalidParameterException(
			"XMLConfigService::fromDOMElement",
			"output", "one and only one output allowed");
	    }

        }
        else throw atdUtil::InvalidParameterException(
                "XMLConfigService::fromDOMElement",
                elname, "unsupported element");
    }
}

DOMElement* XMLConfigService::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* XMLConfigService::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

