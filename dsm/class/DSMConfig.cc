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
using namespace xercesc;

CREATOR_ENTRY_POINT(DSMConfig)

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
    
    if (xnode.getNodeName().compare("dsm"))
	    throw atdUtil::InvalidParameterException(
		    "DSMConfig::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();

	DOMable* sensor = 0;
	if (!elname.compare("serialsensor") ||
            !elname.compare("arincSensor")) {
	    const string& idref = xchild.getAttributeValue("IDREF");
	    if (idref.length() > 0) {
		// cerr << "idref=" << idref << endl;
		Project* project = Project::getInstance();
		if (!project->getSensorCatalog())
		    throw atdUtil::InvalidParameterException(
			"DSMConfig::fromDOMElement",
			"cannot find sensorcatalog for sensor with IDREF",
		    	idref);

		map<string,DOMElement*>::iterator mi;

		mi = project->getSensorCatalog()->find(idref);
		if (mi == project->getSensorCatalog()->end())
			throw atdUtil::InvalidParameterException(
		    "DSMConfig::fromDOMElement",
		    "sensorcatalog does not contain a sensor with ID",
		    idref);
		DOMElement* snode = mi->second;
		XDOMElement sxnode(snode);
		const string& classattr = sxnode.getAttributeValue("class");
		if (classattr.length() == 0) 
		    throw atdUtil::InvalidParameterException(
			"DSMConfig::fromDOMElement",
			string("sensor with ID ") + idref,
			"does not have a class attribute");
		cerr << "found sensor, idref=" << idref << " classattr=" <<
		    classattr << endl;
		sensor = DOMObjectFactory::createObject(classattr);
		sensor->fromDOMElement((DOMElement*)snode);
	    }
		    
	    if (!sensor) {
		const string& classattr = xchild.getAttributeValue("class");
		if (classattr.length() == 0) 
		    throw atdUtil::InvalidParameterException(
			"DSMConfig::fromDOMElement",
			elname,
			"does not have a class attribute");
		cerr << "creating sensor, classattr=" << classattr << endl;
	    	sensor = DOMObjectFactory::createObject(classattr);
	    }
	    sensor->fromDOMElement((DOMElement*)child);
	    ((DSMSensor*)sensor)->setId(sensors.size());	// unique id
	    addSensor((DSMSensor*) sensor);
	}
    }
}

DOMElement* DSMConfig::toDOMParent(DOMElement* parent) throw(DOMException) {
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsm"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
DOMElement* DSMConfig::toDOMElement(DOMElement* node) throw(DOMException) {
    return node;
}

