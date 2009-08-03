/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/DSMConfig.h>
#include <nidas/core/Project.h>
#include <nidas/core/SensorHandler.h>

#include <nidas/util/Logger.h>

#include <nidas/core/DOMObjectFactory.h>
#include <nidas/dynld/SampleOutputStream.h>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

DSMConfig::DSMConfig(): _site(0),_id(0),_remoteSerialSocketPort(0),
    _derivedDataSocketAddr(new n_u::Inet4SocketAddress()),
    _statusSocketAddr(new n_u::Inet4SocketAddress())
{
}

DSMConfig::~DSMConfig()
{
    // delete the sensors I own
    for (list<DSMSensor*>::const_iterator si = _ownedSensors.begin();
    	si != _ownedSensors.end(); ++si) delete *si;

    list<SampleOutput*>::const_iterator oi = getOutputs().begin();
    for ( ; oi != getOutputs().end(); ++oi) delete *oi;

    list<SampleIOProcessor*>::const_iterator pi = _processors.begin();
    for ( ; pi != _processors.end(); ++pi) delete *pi;

    delete _derivedDataSocketAddr;
}

ProcessorIterator DSMConfig::getProcessorIterator() const
{
    return ProcessorIterator(this);
}


SensorIterator DSMConfig::getSensorIterator() const
{
    return SensorIterator(this);
}

SampleTagIterator DSMConfig::getSampleTagIterator() const
{
    return SampleTagIterator(this);
}

VariableIterator DSMConfig::getVariableIterator() const
{
    return VariableIterator(this);
}

void DSMConfig::addSensor(DSMSensor* sensor)
{
    _ownedSensors.push_back(sensor);
    _allSensors.push_back(sensor);
}

void DSMConfig::removeSensor(DSMSensor* sensor)
{
    for (list<DSMSensor*>::iterator si = _ownedSensors.begin();
    	si != _ownedSensors.end(); ) {
	if (sensor == *si) si = _ownedSensors.erase(si);
	else ++si;
    }
    for (list<DSMSensor*>::iterator si = _allSensors.begin();
    	si != _allSensors.end(); ) {
	if (sensor == *si) si = _allSensors.erase(si);
	else ++si;
    }
}


void DSMConfig::initSensors()
	throw(n_u::IOException)
{
    list<DSMSensor*>::iterator si;
    for (si = _ownedSensors.begin(); si != _ownedSensors.end(); ++si) {
	DSMSensor* sensor = *si;
	sensor->init();
    }
}

void DSMConfig::openSensors(SensorHandler* selector)
{
    list<DSMSensor*>::const_iterator si;
    for (si = _ownedSensors.begin(); si != _ownedSensors.end(); ++si) {
        DSMSensor* sensor = *si;
        selector->addSensor(sensor);
    }
    _ownedSensors.clear();
}

list<nidas::core::FileSet*> DSMConfig::findSampleOutputStreamFileSets() const 
{
    list<nidas::core::FileSet*> filesets;
    const list<SampleOutput*>& outputs = getOutputs();
    list<SampleOutput*>::const_iterator oi = outputs.begin();
    for ( ; oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
	nidas::dynld::SampleOutputStream* outstream =
		dynamic_cast<nidas::dynld::SampleOutputStream*>(output);
	if (outstream) {
	    IOChannel* iochan = outstream->getIOChannel();
	    nidas::core::FileSet* fset =
	    	dynamic_cast<nidas::core::FileSet*>(iochan);
	    if (fset) filesets.push_back(fset);
	}
    }
    return filesets;
}

void DSMConfig::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    
    if (xnode.getNodeName() != "dsm")
	    throw n_u::InvalidParameterException(
		    "DSMConfig::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    

    const string& idstr = xnode.getAttributeValue("id");
    if (idstr.length() > 0) {
	unsigned int id;
	istringstream ist(idstr);
	ist >> id;
	if (ist.fail()) throw n_u::InvalidParameterException(
	    string("dsm") + ": " + getName(),"id",idstr);
	setId(id);
    }
    const string& dname = xnode.getAttributeValue("name");
    if (dname.length() > 0) setName(dname);

    const string& idref = xnode.getAttributeValue("IDREF");
    // then parse catalog entry, then main entry
    if (idref.length() > 0) {
	// cerr << "idref=" << idref << endl;
	Project* project = Project::getInstance();
	if (!project->getDSMCatalog())
	    throw n_u::InvalidParameterException(
		string("dsm") + ": " + getName(),
		"cannot find a dsmcatalog for dsm with IDREF",
		idref);

	map<string,xercesc::DOMElement*>::const_iterator mi;

	mi = project->getDSMCatalog()->find(idref);
	if (mi == project->getDSMCatalog()->end())
		throw n_u::InvalidParameterException(
	    string("dsm") + ": " + getName(),
	    "dsmcatalog does not contain a dsm with ID",
	    idref);
        fromDOMElement(mi->second);
    }

    if(node->hasAttributes()) {
    // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const string& aname = attr.getName();
            const string& aval = attr.getValue();

            if (aname == "name") setName(aval);
            else if (aname == "location") setLocation(aval);
            else if (aname == "id");	// already scanned
            else if (aname == "rserialPort") {
		unsigned short port;
		istringstream ist(aval);
		ist >> port;
		if (ist.fail()) throw n_u::InvalidParameterException(
		    string("dsm") + ": " + getName(), aname,aval);
	        setRemoteSerialSocketPort(port);
	    }
            else if (aname == "derivedData") {
                // format:  sock:addr:port or  sock::port
                bool valOK = false;
                if (aval.length() > 5 && aval.substr(0,5) == "sock:") {
                    string::size_type colon = aval.find(':',5);

                    if (colon < string::npos) {
                        string straddr = aval.substr(5,colon-5);
                        n_u::Inet4Address addr;
                        // If no address part, it defaults to INADDR_ANY (0.0.0.0)
                        if (straddr.length() > 0) {
                            try {
                                addr = n_u::Inet4Address::getByName(straddr);
                            }
                            catch(const n_u::UnknownHostException& e) {
                                throw n_u::InvalidParameterException("dsm",aname,e.what());
                            }
                        }
                            
                        unsigned short port;
                        istringstream ist(aval.substr(colon+1));
                        ist >> port;
                        if (!ist.fail()) {
                            n_u::Inet4SocketAddress saddr(addr,port);
                            setDerivedDataSocketAddr(saddr);
                            valOK = true;
                        }
                    }
                }
                if (!valOK) throw n_u::InvalidParameterException(
                        string("dsm") + ": " + getName(), aname,aval);
	    }
            else if (aname == "statusAddr") {
                // format:  sock:addr:port or  sock::port
                bool valOK = false;
                if (aval.length() > 5 && aval.substr(0,5) == "sock:") {
                    string::size_type colon = aval.find(':',5);

                    if (colon < string::npos) {
                        string straddr = aval.substr(5,colon-5);
                        n_u::Inet4Address addr;
                        // If no address part, it defaults to INADDR_ANY (0.0.0.0)
                        if (straddr.length() > 0) {
                            try {
                                addr = n_u::Inet4Address::getByName(straddr);
                            }
                            catch(const n_u::UnknownHostException& e) {
                                throw n_u::InvalidParameterException("dsm",aname,e.what());
                            }
                        }
                            
                        unsigned short port;
                        istringstream ist(aval.substr(colon+1));
                        ist >> port;
                        if (!ist.fail()) {
                            n_u::Inet4SocketAddress saddr(addr,port);
                            setStatusSocketAddr(saddr);
                            valOK = true;
                        }
                    }
                }
                if (!valOK) throw n_u::InvalidParameterException(
                        string("dsm") + ": " + getName(), aname,aval);
	    }
            else if (aname == "ID");	// catalog entry
            else if (aname == "IDREF");	// already scanned
	    else throw n_u::InvalidParameterException(
		string("dsm") + ": " + getName(),
		"unrecognized attribute",aname);
	}
    }

    list<DSMSensor*> tmpSensorList;

    xercesc::DOMNode* child;
    DOMable* domable;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (elname == "sensor" ||
	    elname == "serialSensor" ||
            elname == "arincSensor" ||
            elname == "irigSensor" ||
            elname == "lamsSensor" ||
            elname == "socketSensor") {

	    string classattr =
	    	DSMSensor::getClassName((xercesc::DOMElement*)child);
	    if (classattr.length() == 0)
		throw n_u::InvalidParameterException("sensor",
		    getName(),"no class attribute");

	    // look for a previous definition of a sensor on this device
	    DSMSensor* sensor = 0;
	    bool newsensor = false;
	    const string& devname = xchild.getAttributeValue("devicename");
	    if (devname.length() > 0) {
		for (list<DSMSensor*>::iterator si = _ownedSensors.begin();
			si != _ownedSensors.end(); ++si) {
		    DSMSensor* snsr = *si;
		    if (snsr->getDeviceName() == devname) sensor = snsr;
		}
	    }
	    if (sensor && sensor->getClassName() != classattr) 
		    throw n_u::InvalidParameterException("sensor", sensor->getName(),
		    string("conflicting class names: ") + sensor->getClassName() +
		    	" " + classattr);

	    if (!sensor) {
		newsensor = true;
		try {
		    domable = DOMObjectFactory::createObject(classattr);
		}
		catch (const n_u::Exception& e) {
		    throw n_u::InvalidParameterException("sensor",
			classattr,e.what());
		}
		sensor = dynamic_cast<DSMSensor*>(domable);
		if (!sensor) {
                    throw n_u::InvalidParameterException(
                        string("dsm") + ": " + getName(),
                        elname,"is not a DSMSensor");
                    delete domable;
                }
	    }

	    // do setDSMConfig before fromDOMElement, because
	    // some sensors may want to know their DSMConfig
	    // within their fromDOMElement
	    sensor->setDSMConfig(this);
	    try {
		sensor->fromDOMElement((xercesc::DOMElement*)child);
	    }
	    catch (const n_u::InvalidParameterException& e) {
	        delete sensor;
		throw;
	    }
	    if (newsensor) {
	        addSensor(sensor);
		tmpSensorList.push_back(sensor);
	    }
	}
	else if (elname == "output") {
	    const string& classattr = xchild.getAttributeValue("class");
            if (classattr.length() == 0)
                throw n_u::InvalidParameterException(
		    string("dsm") + ": " + getName(),
                    elname,
		    "does not have a class attribute");
            try {
                domable = DOMObjectFactory::createObject(classattr);
            }
            catch (const n_u::Exception& e) {
                throw n_u::InvalidParameterException("service",
                    classattr,e.what());
            }
            SampleOutput* output = dynamic_cast<SampleOutput*>(domable);
            if (!output) {
                delete domable;
		throw n_u::InvalidParameterException(
		    string("dsm") + ": " + getName() + " output",
		    classattr,"is not a SampleOutput");
            }
	    try {
                output->setDSMConfig(this);
		output->fromDOMElement((xercesc::DOMElement*)child);
	    }
	    catch (const n_u::InvalidParameterException& e) {
	        delete output;
		throw;
	    }
	    addOutput(output);
        }
        else if (elname == "processor") {
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw n_u::InvalidParameterException(
		    "DSMService::fromDOMElement",
		    elname, "class not specified");
            try {
                domable = DOMObjectFactory::createObject(classattr);
            }
            catch (const n_u::Exception& e) {
                n_u::InvalidParameterException ipe("dsm",classattr,e.what());
		n_u::Logger::getInstance()->log(LOG_WARNING,"%s",ipe.what());
		continue;
            }
	    SampleIOProcessor* processor = dynamic_cast<SampleIOProcessor*>(domable);
            if (!processor) {
		delete domable;
                throw n_u::InvalidParameterException("dsm",
                    classattr,"is not of type SampleIOProcessor");
	    }
            processor->setDSMId(this->getId());
            processor->fromDOMElement((xercesc::DOMElement*)child);
	    addProcessor(processor);
        }
	else throw n_u::InvalidParameterException(
		string("dsm") + ": " + getName(),
		    "unrecognized element",elname);
    }

    // Warn if no outputs
    if (getOutputs().size() == 0) {
	ostringstream ost;
        n_u::Logger::getInstance()->log(LOG_WARNING,
		"dsm id %d has no configured outputs",getId());
    }

    // check for sensor ids which have value less than 0, or are not unique.
    typedef map<unsigned int,DSMSensor*> sens_map_t;
    typedef map<unsigned int,DSMSensor*>::const_iterator sens_map_itr_t;
    sens_map_t sensorIdCheck;
    sens_map_t dupSensorIdCheck;
    sens_map_t sampleIdCheck;
    sens_map_t dupSampleIdCheck;
    pair<sens_map_itr_t,bool> ins;
    sens_map_itr_t it;

    for (list<DSMSensor*>::const_iterator si = tmpSensorList.begin();
    	si != tmpSensorList.end(); ++si) {
	DSMSensor* sensor = *si;

	if (sensor->getId() < 0)
	    throw n_u::InvalidParameterException(sensor->getName(),
		"id","must be non-zero");

        /* if it is OK for this sensor to have duplicate IDs with another
         * then the other sensor must also agree.
         */
        if (sensor->getDuplicateIdOK()) {
            it = sensorIdCheck.find(sensor->getId());
            if (it != sensorIdCheck.end()) {
                ostringstream ost;
                ost << sensor->getId() <<
                    "(dsm id=" << sensor->getDSMId() <<
                    ",sensor id=" << sensor->getShortId() << ')';
                DSMSensor* other = it->second;
                throw n_u::InvalidParameterException(
                    sensor->getName(),string(" has same id=") + ost.str() + " as " +
                        other->getName()," (both must set duplicatID=true, if that is what you want)");
            }
            dupSensorIdCheck.insert(make_pair<unsigned int,DSMSensor*>(sensor->getId(),sensor));
        }
        else {
            ins = sensorIdCheck.insert(
                    make_pair<unsigned int,DSMSensor*>(sensor->getId(),sensor));
            it = dupSensorIdCheck.find(sensor->getId());
            if (!ins.second || it != dupSensorIdCheck.end()) {
                ostringstream ost;
                ost << sensor->getId() <<
                    "(dsm id=" << sensor->getDSMId() <<
                    ",sensor id=" << sensor->getShortId() << ')';
                DSMSensor* other;
                if (!ins.second) other = ins.first->second;
                else other = it->second;
                throw n_u::InvalidParameterException(
                    sensor->getName() + " has same id=" + ost.str() + " as " +
                        other->getName());
            }
        }

	// check the sensor ids (which become the ids of the raw samples)
	// against the sample ids of all other sensors.
        if (sensor->getDuplicateIdOK()) {
            it = sampleIdCheck.find(sensor->getId());
            if (it != sampleIdCheck.end()) {
                ostringstream ost;
                ost << sensor->getId();
                DSMSensor* other = it->second;
                throw n_u::InvalidParameterException(
                    sensor->getName() + " id=" + ost.str() +
                    " is equal to a sensor or sample id belonging to " + other->getName());
            }
            dupSampleIdCheck.insert(make_pair<unsigned int,DSMSensor*>(sensor->getId(),sensor));
        }
        else {
            ins = sampleIdCheck.insert(
                make_pair<unsigned int,DSMSensor*>(sensor->getId(),sensor));
            it = dupSampleIdCheck.find(sensor->getId());
            if (!ins.second || it != dupSampleIdCheck.end()) {
                ostringstream ost;
                ost << sensor->getId();
                DSMSensor* other;
                if (!ins.second) other = ins.first->second;
                else other = it->second;
                throw n_u::InvalidParameterException(
                    sensor->getName() + " id=" + ost.str() +
                    " is equal to a sensor or sample id belonging to " + other->getName());
            }
        }

	// check that sample ids are unique
	for (list<const SampleTag*>::const_iterator ti =
		sensor->getSampleTags().begin();
			ti != sensor->getSampleTags().end(); ++ti) {
	    const SampleTag* stag = *ti;
	    if (stag->getId() == 0)
		throw n_u::InvalidParameterException(sensor->getName(),
			"sample id","must be non-zero");

            if (sensor->getDuplicateIdOK()) {
                it = sampleIdCheck.find(stag->getId());
                if (it != sampleIdCheck.end()) {
                    DSMSensor* other = it->second;
                    ostringstream ost;
                    ost << stag->getId();
                    throw n_u::InvalidParameterException(
                        sensor->getName() + " & " + other->getName() +
                        " have equivalent sample ids: " + ost.str());
                }
                dupSampleIdCheck.insert(make_pair<unsigned int,DSMSensor*>(stag->getId(),sensor));
            }
            else {
                ins = sampleIdCheck.insert(
                    make_pair<unsigned int,DSMSensor*>(stag->getId(),sensor));
                it = dupSampleIdCheck.find(stag->getId());
                if (!ins.second || it != dupSampleIdCheck.end()) {
                    ostringstream ost;
                    ost << stag->getId();
                    DSMSensor* other;
                    if (!ins.second) other = ins.first->second;
                    else other = it->second;

                    if (other == sensor)
                        throw n_u::InvalidParameterException(
                            sensor->getName() + " has duplicate sample ids: " +
                            ost.str());
                    else
                        throw n_u::InvalidParameterException(
                            sensor->getName() + " & " + other->getName() +
                            " have equivalent sample ids: " + ost.str());
                }
            }
	}
	list<SampleOutput*>::const_iterator oi =  getOutputs().begin();
	for ( ; oi != getOutputs().end(); ++oi) {
	    SampleOutput* output = *oi;
	    SampleTagIterator sti = sensor->getSampleTagIterator();
	    for ( ; sti.hasNext(); ) output->addSampleTag(sti.next());
	}
    }
    for (SensorIterator si = getSensorIterator(); si.hasNext(); ) {
	DSMSensor* sensor = si.next();
        sensor->validate();
    }
}

xercesc::DOMElement* DSMConfig::toDOMParent(xercesc::DOMElement* parent,bool complete) const
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
            DOMable::getNamespaceURI(),
            (const XMLCh*)XMLStringConverter("dsm"));
    parent->appendChild(elem);
    return toDOMElement(elem,complete);
}

xercesc::DOMElement* DSMConfig::toDOMElement(xercesc::DOMElement* elem,bool complete) const
    throw(xercesc::DOMException)
{

    if (complete) return 0; // not supported yet

    XDOMElement xelem(elem);
    xelem.setAttributeValue("name",getName());
    if (getLocation().length() > 0)
        xelem.setAttributeValue("location",getLocation());
    ostringstream ost;
    ost << getId();
    xelem.setAttributeValue("id",ost.str());
    for (SensorIterator ssi = getSensorIterator(); ssi.hasNext(); ) {
        DSMSensor* sensor = ssi.next();
        sensor->toDOMParent(elem,complete);
    }
    return elem;
}

string DSMConfig::expandString(const string& input) const
{
    string::size_type lastpos = 0;
    string::size_type dollar;

    string result;

    while ((dollar = input.find('$',lastpos)) != string::npos) {

        result.append(input.substr(lastpos,dollar-lastpos));
	lastpos = dollar;

	string::size_type openparen = input.find('{',dollar);
	string token;

	if (openparen == dollar + 1) {
	    string::size_type closeparen = input.find('}',openparen);
	    if (closeparen == string::npos) break;
	    token = input.substr(openparen+1,closeparen-openparen-1);
	    lastpos = closeparen + 1;
	}
	else {
	    string::size_type endtok = input.find_first_of("/.",dollar + 1);
	    if (endtok == string::npos) endtok = input.length();
	    token = input.substr(dollar+1,endtok-dollar-1);
	    lastpos = endtok;
	}
	if (token.length() > 0) {
	    string val = getTokenValue(token);
	    // cerr << "getTokenValue: token=" << token << " val=" << val << endl;
	    result.append(val);
	}
    }

    result.append(input.substr(lastpos));
#ifdef DEBUG
    cerr << "input: \"" << input << "\" expanded to \"" <<
    	result << "\"" << endl;
#endif
    return result;
}

string DSMConfig::getTokenValue(const string& token) const
{
    if (token == "PROJECT") return Project::getInstance()->getName();

    if (token == "SYSTEM") return Project::getInstance()->getSystemName();

    if (token == "AIRCRAFT" || token == "SITE") return getSite()->getName();
        
    if (token == "DSM") return getName();
        
    if (token == "LOCATION") return getLocation();

    // if none of the above, try to get token value from UNIX environment
    const char* val = ::getenv(token.c_str());
    if (val) return string(val);
    else return string("${") + token + "}";      // unknown value, return original token
}

