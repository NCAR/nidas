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

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

// CREATOR_ENTRY_POINT(SensorCatalog)

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
    
    if (xnode.getNodeName().compare("sensorcatalog"))
	    throw atdUtil::InvalidParameterException(
		    "SensorCatalog::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();
	// cerr << "SensorCatalog: child element name=" << elname << endl;

	if (!elname.compare("serialSensor") ||
            !elname.compare("arincSensor") ||
            !elname.compare("irigSensor") ||
            !elname.compare("sensor")) {
	    const string& id = xchild.getAttributeValue("ID");
	    if(id.length() > 0) {
		map<string,DOMElement*>::iterator mi =
			find(id);
		if (mi != end() && mi->second != (DOMElement*)child)
		    throw atdUtil::InvalidParameterException(
			"SensorCatalog::fromDOMElement",
			"duplicate sensor in catalog, ID",id);
		insert(make_pair<string,DOMElement*>(id,(DOMElement*)child));

		/*
		cerr << "sensorCatalog.size=" << size() << endl;
		for (mi = begin(); mi != end(); ++mi)
		    cerr << "map:" << mi->first << " " << hex << mi->second <<
		    	dec << endl;
		*/
	    }
        }
	else throw atdUtil::InvalidParameterException(
			"SensorCatalog::fromDOMElement",
			"unrecognized element",elname);
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

