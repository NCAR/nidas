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
#include <Project.h>

#include <DOMObjectFactory.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

DSMConfig::DSMConfig(): sensorId(0x0010),sensorIncrement(0x0010)
{
}

DSMConfig::~DSMConfig()
{
    for (std::list<DSMSensor*>::iterator si = sensors.begin();
    	si != sensors.end(); ++si) delete *si;
    for (std::list<SampleOutputStream*>::iterator oi = outputs.begin();
    	oi != outputs.end(); ++oi) delete *oi;
}

void DSMConfig::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);

    
    if (xnode.getNodeName().compare("dsm"))
	    throw atdUtil::InvalidParameterException(
		    "DSMConfig::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    if(node->hasAttributes()) {
    // get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
            if (!aname.compare("name")) setName(aval);
	}
    }

    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();

	DOMable* domable = 0;
	if (!elname.compare("serialsensor") ||
            !elname.compare("arincSensor") ||
            !elname.compare("irigsensor")) {
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
		try {
		    domable = DOMObjectFactory::createObject(classattr);
		}
		catch (const atdUtil::Exception& e) {
		    throw atdUtil::InvalidParameterException("sensor",
		    	classattr,e.what());
		}
		domable->fromDOMElement((DOMElement*)snode);
	    }
		    
	    if (!domable) {
		const string& classattr = xchild.getAttributeValue("class");
		if (classattr.length() == 0) 
		    throw atdUtil::InvalidParameterException(
			"DSMConfig::fromDOMElement",
			elname,
			"does not have a class attribute");
		cerr << "creating sensor, classattr=" << classattr << endl;
		try {
		    domable = DOMObjectFactory::createObject(classattr);
		}
		catch (const atdUtil::Exception& e) {
		    throw atdUtil::InvalidParameterException("sensor",
		    	classattr,e.what());
		}
	    }
	    domable->fromDOMElement((DOMElement*)child);
	    DSMSensor* sensor = dynamic_cast<DSMSensor*>(domable);
	    if (!sensor) throw atdUtil::InvalidParameterException("sensor",
		    elname,"is not a DSMSensor");
	    sensor->setId(sensorId);	// unique id
	    sensorId += sensorIncrement;
	    addSensor(sensor);
	}
	else if (!elname.compare("output")) {
	    DOMable* domable;
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0) 
		throw atdUtil::InvalidParameterException(
		    "DSMConfig::fromDOMElement",
		    elname,
		    "does not have a class attribute");
	    try {
		domable = DOMObjectFactory::createObject(classattr);
	    }
	    catch (const atdUtil::Exception& e) {
		throw atdUtil::InvalidParameterException("output",
		    classattr,e.what());
	    }
	    domable->fromDOMElement((DOMElement*)child);
	    SampleOutputStream* output =
	    	dynamic_cast<SampleOutputStream*>(domable);
	    if (!output) throw atdUtil::InvalidParameterException("output",
		    classattr,"is not a SampleOutputStream");
	    addOutput(output);
	    output->setDSMConfig(this);
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

