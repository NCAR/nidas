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

DSMConfig::DSMConfig()
{
}

DSMConfig::~DSMConfig()
{
    for (list<DSMSensor*>::iterator si = sensors.begin();
    	si != sensors.end(); ++si) delete *si;
    for (list<SampleOutputStream*>::iterator oi = outputs.begin();
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
            const string& aname = attr.getName();
            const string& aval = attr.getValue();

            if (!aname.compare("name")) setName(aval);
            else if (!aname.compare("location")) setLocation(aval);
	    else throw atdUtil::InvalidParameterException(
		string("dsm") + ": " + getName(),
		"unrecognized attribute",aname);
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
	if (!elname.compare("sensor") ||
	    !elname.compare("serialSensor") ||
            !elname.compare("arincSensor") ||
            !elname.compare("irigSensor")) {
	    const string& idref = xchild.getAttributeValue("IDREF");
	    if (idref.length() > 0) {
		// cerr << "idref=" << idref << endl;
		Project* project = Project::getInstance();
		if (!project->getSensorCatalog())
		    throw atdUtil::InvalidParameterException(
			string("dsm") + ": " + getName(),
			"cannot find sensorcatalog for sensor with IDREF",
		    	idref);

		map<string,DOMElement*>::iterator mi;

		mi = project->getSensorCatalog()->find(idref);
		if (mi == project->getSensorCatalog()->end())
			throw atdUtil::InvalidParameterException(
		    string("dsm") + ": " + getName(),
		    "sensorcatalog does not contain a sensor with ID",
		    idref);
		DOMElement* snode = mi->second;
		XDOMElement sxnode(snode);
		const string& classattr = sxnode.getAttributeValue("class");
		if (classattr.length() == 0) 
		    throw atdUtil::InvalidParameterException(
			string("dsm") + ": " + getName(),
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
			string("dsm") + ": " + getName(),
			elname,
			"does not have a class attribute");
		cerr << "creating sensor, classattr=" << classattr << endl;
		try {
		    domable = DOMObjectFactory::createObject(classattr);
		}
		catch (const atdUtil::Exception& e) {
		    throw atdUtil::InvalidParameterException(
			string("dsm") + ": " + getName(),
		    	classattr, e.what());
		}
	    }
	    domable->fromDOMElement((DOMElement*)child);
	    DSMSensor* sensor = dynamic_cast<DSMSensor*>(domable);
	    if (!sensor) throw atdUtil::InvalidParameterException(
		    string("dsm") + ": " + getName(),
		    elname,"is not a DSMSensor");
	    addSensor(sensor);
	}
	else if (!elname.compare("output")) {
	    DOMable* domable;
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0) 
		throw atdUtil::InvalidParameterException(
		    string("dsm") + ": " + getName(),
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
	    if (!output) throw atdUtil::InvalidParameterException(
		string("dsm") + ": " + getName() + " output",
		    classattr,"is not a SampleOutputStream");
	    addOutput(output);
	    output->setDSMConfig(this);
	}
	else if (!elname.compare("config"));
	else throw atdUtil::InvalidParameterException(
		string("dsm") + ": " + getName(),
		    "unrecognized element",elname);
    }

    // check for sensor ids which have value 0, or are not unique.
    typedef map<unsigned short,DSMSensor*> sens_map_t;
    typedef map<unsigned short,DSMSensor*>::const_iterator sens_map_itr_t;
    sens_map_t sensorIdCheck;
    sens_map_t sampleIdCheck;

    for (list<DSMSensor*>::const_iterator si = sensors.begin();
    	si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;

	if (sensor->getId() == 0)
	    throw atdUtil::InvalidParameterException(sensor->getName(),
		    "id","must be non-zero");

	pair<sens_map_itr_t,bool> ins = sensorIdCheck.insert(
		make_pair<unsigned short,DSMSensor*>(sensor->getId(),sensor));
	if (!ins.second) {
	    ostringstream ost;
	    ost << sensor->getId();
	    DSMSensor* other = ins.first->second;
	    throw atdUtil::InvalidParameterException(
	    	sensor->getName() + " has same id (" + ost.str() + ") as " +
		    other->getName());
	}

	ins = sampleIdCheck.insert(
	    make_pair<unsigned short,DSMSensor*>(sensor->getId(),sensor));
	if (!ins.second) {
	    ostringstream ost;
	    ost << sensor->getId();
	    DSMSensor* other = ins.first->second;

	    if (other == sensor)
		throw atdUtil::InvalidParameterException(
		    sensor->getName() + " has duplicate sample ids: " +
		    ost.str());
	    else
		throw atdUtil::InvalidParameterException(
		    sensor->getName() + " & " + other->getName() +
		    " have equivalent sample ids: " + ost.str());
	}

	// check that sample ids are unique
	for (vector<const SampleTag*>::const_iterator ti =
		sensor->getSampleTags().begin();
			ti != sensor->getSampleTags().end(); ++ti) {
	    const SampleTag* stag = *ti;
	    if (stag->getId() == 0)
		throw atdUtil::InvalidParameterException(sensor->getName(),
			"sample id","must be non-zero");

	    ins = sampleIdCheck.insert(
		make_pair<unsigned short,DSMSensor*>(stag->getId(),sensor));
	    if (!ins.second) {
		ostringstream ost;
		ost << stag->getId();
		DSMSensor* other = ins.first->second;

		if (other == sensor)
		    throw atdUtil::InvalidParameterException(
		    	sensor->getName() + " has duplicate sample ids: " +
			ost.str());
		else
		    throw atdUtil::InvalidParameterException(
		    	sensor->getName() + " & " + other->getName() +
			" have equivalent sample ids: " + ost.str());
	    }
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

