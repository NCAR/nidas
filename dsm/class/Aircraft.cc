/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <Aircraft.h>
#include <XMLStringConverter.h>
#include <XDOM.h>
#include <DOMObjectFactory.h>

#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(Aircraft)

Aircraft::Aircraft()
{
}

Aircraft::~Aircraft()
{
    for (std::list<DSMConfig*>::iterator it = dsms.begin();
    	it != dsms.end(); ++it) delete *it;
}

void Aircraft::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    cerr << "Aircraft fromDOMElement element name=" <<
    	xnode.getNodeName() << endl;
    
    if (xnode.getNodeName().compare("aircraft"))
	    throw atdUtil::InvalidParameterException(
		    "Aircraft::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	cerr <<"\tAttributes" << endl;
	cerr <<"\t----------" << endl;
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    // get attribute name
	    cerr << "attrname=" << attr.getName() << endl;
	    
	    // get attribute type
	    cerr << "\tattrval=" << attr.getValue() << endl;
	}
    }

    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();
	cerr << "element name=" << elname << endl;

	if (!elname.compare("dsm")) {
	    DSMConfig* dsm = new DSMConfig();
	    dsm->fromDOMElement((DOMElement*)child);
	    addDSMConfig(dsm);
	}
    }
}

DOMElement* Aircraft::toDOMParent(DOMElement* parent) throw(DOMException) {
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("aircraft"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
DOMElement* Aircraft::toDOMElement(DOMElement* node) throw(DOMException) {
    return node;
}

