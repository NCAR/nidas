/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <Project.h>
#include <XMLStringConverter.h>
#include <XDOM.h>
#include <DOMObjectFactory.h>

#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(Project)

/* static */
Project* Project::instance = 0;

/* static */
Project* Project::getInstance() 
{
   if (!instance) instance = new Project();
   return instance;
}

Project::Project(): catalog(0)
{
}

Project::~Project()
{
    delete catalog;
    instance = 0;
}

void Project::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    cerr << "element name=" << xnode.getNodeName() << endl;
    
    if (xnode.getNodeName().compare("project"))
	    throw atdUtil::InvalidParameterException(
		    "Project::fromDOMElement","xml node name",
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

	if (!elname.compare("aircraft")) {
	    Aircraft* aircraft = new Aircraft();
	    aircraft->fromDOMElement((DOMElement*)child);
	    addAircraft(aircraft);
	}
	else if (!elname.compare("sensorcatalog")) {
	    SensorCatalog* catalog = new SensorCatalog();
	    catalog->fromDOMElement((DOMElement*)child);
	    setSensorCatalog(catalog);
	}
    }
}

DOMElement* Project::toDOMParent(DOMElement* parent) throw(DOMException) {
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("project"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
DOMElement* Project::toDOMElement(DOMElement* node) throw(DOMException) {
    return node;
}

