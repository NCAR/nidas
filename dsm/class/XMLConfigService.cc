
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
#include <XMLParser.h>

#include <DSMServer.h>
#include <SocketAddress.h>
#include <XMLConfigWriter.h>
#include <XMLFdFormatTarget.h>
#include <Datagrams.h>


#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(XMLConfigService)

XMLConfigService::XMLConfigService():DSMService("XMLConfigService")
{
}

atdUtil::ServiceListenerClient* XMLConfigService::clone()
{
    return new XMLConfigService(*this);
}

int XMLConfigService::run() throw(atdUtil::Exception)
{

    XMLCachingParser* parser = XMLCachingParser::getInstance();

    xercesc::DOMDocument* doc = parser->parse(DSMServer::getXMLFileName());

    XMLFdFormatTarget formatter(
    	socket.getInet4SocketAddress().toString(),socket.getFd());

    XMLConfigWriter writer(
    	socket.getInet4SocketAddress().getInet4Address());

    writer.writeNode(&formatter,*doc);

    socket.close();
    return RUN_OK;
}

void XMLConfigService::fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((DOMElement*) child);
        const string& elname = xchild.getNodeName();

	if (!elname.compare("output"))
	    fromDOMElementOutput((DOMElement*) child);
    }
}

void XMLConfigService::fromDOMElementOutput(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((DOMElement*) child);
        const string& elname = xchild.getNodeName();

	if (!elname.compare("socket")) {
	    SocketAddress saddr;
	    saddr.fromDOMElement((DOMElement*)child);
	    cerr << "XMLConfigService saddr=" << saddr.toString() << endl;
	    setListenSocketAddress(saddr);
	}
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

