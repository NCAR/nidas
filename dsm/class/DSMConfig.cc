/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <DSMConfig.h>
#include <XMLStringConverter.h>
#include <XDOM.h>
#include <DOMObjectFactory.h>

// #include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
// #include <xercesc/dom/DOMAttr.hpp>

#include <iostream>

using namespace dsm;
using namespace std;
XERCES_CPP_NAMESPACE_USE

DSMConfig::DSMConfig()
{
}

DSMConfig::~DSMConfig()
{
}

void DSMConfig::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    cerr << "element name=" << xnode.getNodeName() << endl;
    
    if (xnode.getNodeName().compare("dsm"))
	    throw atdUtil::InvalidParameterException(
		    "DSMConfig::fromDOMElement","xml node name",
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
	    const string& classattr = xchild.getAttributeValue("class");
	    cerr << "classattr=" << classattr << endl;
	    DOMable* xx = DOMObjectFactory::createObject(classattr);
	    xx->fromDOMElement((DOMElement*)child);
	}
    }
}

DOMElement* DSMConfig::toDOMParent(DOMElement* parent) throw(DOMException) {
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
DOMElement* DSMConfig::toDOMElement(DOMElement* node) throw(DOMException) {
    return node;
}

