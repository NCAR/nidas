/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SensorCatalog.h>
#include <XMLStringConverter.h>
#include <XDOM.h>

// #include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
// #include <xercesc/dom/DOMAttr.hpp>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SensorCatalog)

SensorCatalog::SensorCatalog()
{
}

SensorCatalog::~SensorCatalog()
{
}

void SensorCatalog::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    cerr << "element name=" << xnode.getNodeName() << endl;
    
    if (xnode.getNodeName().compare("sensorcatalog"))
	    throw atdUtil::InvalidParameterException(
		    "SensorCatalog::fromDOMElement","xml node name",
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

	if (!elname.compare("serialsensor")) {
	    pair<string,DOMElement*> p;
	    p.first = xchild.getAttributeValue("ID");
	    cerr << "serialsensor id=" << p.first << endl;
	    if(p.first.length() > 0) {
		if (operator[](p.first) != (DOMElement*)0)
		    throw atdUtil::InvalidParameterException(
			"SensorCatalog::fromDOMElement",
			"duplicate sensor in catalog, ID",
			p.first);
		p.second = (DOMElement*)child;
		insert(p);
	    }
	}
    }
}

DOMElement* SensorCatalog::toDOMParent(DOMElement* parent) throw(DOMException) {
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsm"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
DOMElement* SensorCatalog::toDOMElement(DOMElement* node) throw(DOMException) {
    return node;
}

