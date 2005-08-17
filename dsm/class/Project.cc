/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <Project.h>
#include <Aircraft.h>
#include <DOMObjectFactory.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

// CREATOR_ENTRY_POINT(Project)

/* static */
Project* Project::instance = 0;

/* static */
Project* Project::getInstance() 
{
   if (!instance) instance = new Project();
   return instance;
}

Project::Project(): currentSite(0),catalog(0)
{
    currentObsPeriod.setName("test");
}

Project::~Project()
{
    // cerr << "deleting catalog" << endl;
    delete catalog;
    // cerr << "deleting sites" << endl;
    for (std::list<Site*>::iterator it = sites.begin();
    	it != sites.end(); ++it) delete *it;
    instance = 0;
}

void Project::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
#ifdef XML_DEBUG
    cerr << "element name=" << xnode.getNodeName() << endl;
#endif
    
    if (xnode.getNodeName().compare("project"))
	    throw atdUtil::InvalidParameterException(
		    "Project::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    if(node->hasAttributes()) {
    // get all the attributes of the node
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
#ifdef XML_DEBUG
	cerr <<"\tAttributes" << endl;
	cerr <<"\t----------" << endl;
#endif
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
#ifdef XML_DEBUG
	    cerr << "attrname=" << attr.getName() << endl;
	    cerr << "\tattrval=" << attr.getValue() << endl;
#endif
	    if (!attr.getName().compare("name")) setName(attr.getValue());
	    else if (!attr.getName().compare("version")) setVersion(attr.getValue());
	    else if (!attr.getName().compare("xmlname")) setXMLName(attr.getValue());
	}
    }

    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();
#ifdef XML_DEBUG
	cerr << "element name=" << elname << endl;
#endif

	if (!elname.compare("site")) {
	    DOMable* domable;
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw atdUtil::InvalidParameterException(
		    string("project") + ": " + getName(),
		    "site",
		    "does not have a class attribute");
	    try {
		domable = DOMObjectFactory::createObject(classattr);
	    }
	    catch (const atdUtil::Exception& e) {
		throw atdUtil::InvalidParameterException("site",
		    classattr,e.what());
	    }
	    Site* site = dynamic_cast<Site*>(domable);
	    if (!site)
		throw atdUtil::InvalidParameterException("project",
                    classattr,"is not a sub-class of Site");

	    site->setProject(this);
	    site->fromDOMElement((DOMElement*)child);
	    addSite(site);
	}
	else if (!elname.compare("aircraft")) {
	    // <aircraft> tag is the same as <site class="Aircraft">
	    Aircraft* site = new Aircraft();
	    site->setProject(this);
	    site->fromDOMElement((DOMElement*)child);
	    addSite(site);
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

