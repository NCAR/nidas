/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <DSMService.h>
#include <Aircraft.h>
#include <DSMServer.h>
#include <DOMObjectFactory.h>

using namespace dsm;
using namespace std;

DSMService::DSMService(const std::string& name): atdUtil::Thread(name),
	server(0),dsm(0)
{
    blockSignal(SIGHUP);
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
}

DSMService::~DSMService()
{
    cerr << "~DSMService" << endl;
}

const Aircraft* DSMService::getAircraft() const {
    return getServer()->getAircraft();
}

void DSMService::fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    if(node->hasAttributes()) {
	// get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	}
    }
}

xercesc::DOMElement* DSMService::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

xercesc::DOMElement* DSMService::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

