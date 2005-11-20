/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <DSMConfig.h>
#include <Project.h>
#include <PortSelector.h>

#include <atdUtil/Logger.h>

#include <DOMObjectFactory.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

DSMConfig::DSMConfig(): site(0),remoteSerialSocketPort(0)
{
}

DSMConfig::~DSMConfig()
{
    // cerr << "deleting sensors" << endl;
    for (list<DSMSensor*>::const_iterator si = ownedSensors.begin();
    	si != ownedSensors.end(); ++si) delete *si;

    // cerr << "deleting output" << endl;
    list<SampleOutput*>::const_iterator oi;
    for (oi = getOutputs().begin(); oi != getOutputs().end(); ++oi)
    	delete *oi;
}

void DSMConfig::addSensor(DSMSensor* sensor)
{
    // sensors.push_back(sensor);
    ownedSensors.push_back(sensor);
}

void DSMConfig::openSensors(PortSelector* selector)
	throw(atdUtil::IOException)
{
    list<DSMSensor*>::iterator si;
    for (si = ownedSensors.begin(); si != ownedSensors.end(); ) {
	DSMSensor* sensor = *si;
	try {
	    sensor->open(sensor->getDefaultMode());
	    selector->addDSMSensor(sensor);
	    si = ownedSensors.erase(si);
	}
	catch(const atdUtil::IOException& e) {
	    atdUtil::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    sensor->getName().c_str(),e.what());
            ++si;
	}
	catch(const atdUtil::InvalidParameterException& e) {
	    atdUtil::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		    sensor->getName().c_str(),e.what());
            ++si;
	}
    }
}

void DSMConfig::initSensors()
	throw(atdUtil::IOException)
{
    list<DSMSensor*>::iterator si;
    for (si = ownedSensors.begin(); si != ownedSensors.end(); ++si) {
	DSMSensor* sensor = *si;
	sensor->init();
    }
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
            else if (!aname.compare("id")) {
		unsigned short id;
		istringstream ist(aval);
		ist >> id;
		if (ist.fail()) throw atdUtil::InvalidParameterException(
		    string("dsm") + ": " + getName(), aname,aval);
	        setId(id);
	    }
            else if (!aname.compare("rserialPort")) {
		unsigned short port;
		istringstream ist(aval);
		ist >> port;
		if (ist.fail()) throw atdUtil::InvalidParameterException(
		    string("dsm") + ": " + getName(), aname,aval);
	        setRemoteSerialSocketPort(port);
	    }
	    else throw atdUtil::InvalidParameterException(
		string("dsm") + ": " + getName(),
		"unrecognized attribute",aname);
	}
    }

    list<DSMSensor*> tmpSensorList;

    DOMNode* child;
    DOMable* domable;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (!elname.compare("sensor") ||
	    !elname.compare("serialSensor") ||
            !elname.compare("arincSensor") ||
            !elname.compare("irigSensor") ||
            !elname.compare("socketSensor")) {
	    const string& idref = xchild.getAttributeValue("IDREF");

	    DSMSensor* sensor = 0;

	    if (idref.length() > 0) {
		// cerr << "idref=" << idref << endl;
		Project* project = Project::getInstance();
		if (!project->getSensorCatalog())
		    throw atdUtil::InvalidParameterException(
			string("dsm") + ": " + getName(),
			"cannot find sensorcatalog for sensor with IDREF",
		    	idref);

		map<string,DOMElement*>::const_iterator mi;

		mi = project->getSensorCatalog()->find(idref);
		if (mi == project->getSensorCatalog()->end())
			throw atdUtil::InvalidParameterException(
		    string("dsm") + ": " + getName(),
		    "sensorcatalog does not contain a sensor with ID",
		    idref);
		DOMElement* catnode = mi->second;
		XDOMElement xcatnode(catnode);
		const string& classattr = xcatnode.getAttributeValue("class");
		if (classattr.length() == 0) 
		    throw atdUtil::InvalidParameterException(
			string("dsm") + ": " + getName(),
			string("sensor with ID ") + idref,
			"does not have a class attribute");
		// cerr << "found sensor, idref=" << idref << " classattr=" <<
		  //   classattr << endl;
		try {
		    domable = DOMObjectFactory::createObject(classattr);
		}
		catch (const atdUtil::Exception& e) {
		    throw atdUtil::InvalidParameterException("sensor",
		    	classattr,e.what());
		}
		sensor = dynamic_cast<DSMSensor*>(domable);
		if (!sensor) throw atdUtil::InvalidParameterException(
		    string("dsm") + ": " + getName(),
		    elname,"is not a DSMSensor");

		// do setDSMConfig before fromDOMElement, because
		// some sensors may want to know their DSMConfig
		// within their fromDOMElement
		sensor->setDSMConfig(this);

		// get the id from the "real" entry, and set it before
		// doing a fromDOMElement of the catalog entry. Then when
		// addSampleTag is done from the catalog entry, the samples
		// have their real ids.
		const string& idstr = xchild.getAttributeValue("id");
		if (idstr.length() > 0) {
		    istringstream ist(idstr);
		    // If you unset the dec flag, then a leading '0' means
		    // octal, and 0x means hex.
		    ist.unsetf(ios::dec);
		    unsigned short val;
		    ist >> val;
		    if (ist.fail())
			throw atdUtil::InvalidParameterException("sensor","id",idstr);
		    sensor->setShortId(val);
		}

		sensor->fromDOMElement((DOMElement*)catnode);
	    }
		    
	    if (!sensor) {
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
		    throw atdUtil::InvalidParameterException(
			string("dsm") + ": " + getName(),
		    	classattr, e.what());
		}
		sensor = dynamic_cast<DSMSensor*>(domable);
		if (!sensor) throw atdUtil::InvalidParameterException(
		    string("dsm") + ": " + getName(),
		    elname,"is not a DSMSensor");

		// do setDSMConfig before fromDOMElement, because
		// some sensors may want to know their DSMConfig
		// within their fromDOMElement
		sensor->setDSMConfig(this);
	    }
	    sensor->fromDOMElement((DOMElement*)child);
	    addSensor(sensor);
	    tmpSensorList.push_back(sensor);
	}
	else if (!elname.compare("output")) {
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
                throw atdUtil::InvalidParameterException("service",
                    classattr,e.what());
            }
            SampleOutput* output = dynamic_cast<SampleOutput*>(domable);
            if (!output) {
                delete domable;
		throw atdUtil::InvalidParameterException(
		    string("dsm") + ": " + getName() + " output",
		    classattr,"is not a SampleOutput");
            }
	    output->addDSMConfig(this);
            output->fromDOMElement((DOMElement*)child);
	    addOutput(output);
        }
	else if (!elname.compare("config"));
	else throw atdUtil::InvalidParameterException(
		string("dsm") + ": " + getName(),
		    "unrecognized element",elname);
    }

    // Warn if no outputs
    if (getOutputs().size() == 0) {
	ostringstream ost;
        atdUtil::Logger::getInstance()->log(LOG_WARNING,
		"dsm id %d has no configured outputs",getId());
    }

    // check for sensor ids which have value less than 10, or are not unique.
    typedef map<unsigned short,DSMSensor*> sens_map_t;
    typedef map<unsigned short,DSMSensor*>::const_iterator sens_map_itr_t;
    sens_map_t sensorIdCheck;
    sens_map_t sampleIdCheck;

    for (list<DSMSensor*>::const_iterator si = tmpSensorList.begin();
    	si != tmpSensorList.end(); ++si) {
	DSMSensor* sensor = *si;

	if (sensor->getId() < 0)
	    throw atdUtil::InvalidParameterException(sensor->getName(),
		    "id","must be non-zero");

	pair<sens_map_itr_t,bool> ins = sensorIdCheck.insert(
		make_pair<unsigned short,DSMSensor*>(sensor->getId(),sensor));
	if (!ins.second) {
	    ostringstream ost;
	    ost << sensor->getId() <<
	    	"(dsm id=" << sensor->getDSMId() <<
		",sensor id=" << sensor->getShortId() << ')';
	    DSMSensor* other = ins.first->second;
	    throw atdUtil::InvalidParameterException(
	    	sensor->getName() + " has same id=" + ost.str() + " as " +
		    other->getName());
	}

	// check the sensor ids (which become the ids of the raw samples)
	// against the sample ids of all other sensors.
	ins = sampleIdCheck.insert(
	    make_pair<unsigned short,DSMSensor*>(sensor->getId(),sensor));
	if (!ins.second) {
	    ostringstream ost;
	    ost << sensor->getId();
	    DSMSensor* other = ins.first->second;

	    throw atdUtil::InvalidParameterException(
		sensor->getName() + " id=" + ost.str() +
		" is equal to a sensor or sample id belonging to " + other->getName());
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

