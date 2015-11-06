/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/core/DSMConfig.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/SensorHandler.h>
#include <nidas/core/DSMCatalog.h>
#include <nidas/core/SampleIOProcessor.h>
#include <nidas/core/SampleOutput.h>
#include <nidas/core/FileSet.h>

#include <nidas/util/Logger.h>

#include <nidas/core/DOMObjectFactory.h>

#include <iostream>
#include <sstream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

DSMConfig::DSMConfig():
    _site(0),_name(),_suffix(),_location(),_dictionary(this),
    _id(0),_ownedSensors(),_allSensors(),_outputs(),
    _remoteSerialSocketPort(0),
    _rawSorterLength(0.0), _procSorterLength(0.0),
    _rawHeapMax(5000000), _procHeapMax(5000000),
    _rawLateSampleCacheSize(0), _procLateSampleCacheSize(0),
    _derivedDataSocketAddr(new n_u::Inet4SocketAddress()),
    _processors(),
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
    delete _statusSocketAddr;
}

const Project* DSMConfig::getProject() const
{
    if (getSite()) return getSite()->getProject();
    return 0;
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
    DSMSensor * deleteableSensor = NULL;
    for (list<DSMSensor*>::iterator si = _ownedSensors.begin();
    	si != _ownedSensors.end(); ) {
	if (sensor == *si) {
             deleteableSensor = *si;
             si = _ownedSensors.erase(si);
        }
	else ++si;
    }
 
    for (list<DSMSensor*>::iterator si = _allSensors.begin();
    	si != _allSensors.end(); ) {
	if (sensor == *si) {
            si = _allSensors.erase(si);
        }
	else ++si;
    }
 
    // Sensor was owned and has been removed from both lists, now delete it.
    if (deleteableSensor) delete deleteableSensor;
}


void DSMConfig::initSensors()
	throw(n_u::IOException)
{
    list<DSMSensor*>::iterator si;
    for (si = _allSensors.begin(); si != _allSensors.end(); ++si) {
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
        IOChannel* iochan = output->getIOChannel();
        nidas::core::FileSet* fset =
            dynamic_cast<nidas::core::FileSet*>(iochan);
        if (fset) filesets.push_back(fset);
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
		    
    const Project* project = getProject();
    assert(project);

    string idstr = xnode.getAttributeValue("id");
    if (idstr.length() > 0) {
	unsigned int id;
	idstr = project->expandString(idstr);
	istringstream ist(idstr);
	ist >> id;
	if (ist.fail()) throw n_u::InvalidParameterException(
	    string("dsm") + ": " + getName(),"id",idstr);
	setId(id);
    }
    const string& dname = xnode.getAttributeValue("name");
    if (dname.length() > 0) setName(project->expandString(dname));

    string idref = project->expandString(xnode.getAttributeValue("IDREF"));
    // then parse catalog entry, then main entry
    if (idref.length() > 0) {
	// cerr << "idref=" << idref << endl;
	if (!project->getDSMCatalog())
	    throw n_u::InvalidParameterException(
		string("dsm") + ": " + getName(),
		"cannot find a dsmcatalog for dsm with IDREF",
		idref);

	const xercesc::DOMElement* cnode =
            project->getDSMCatalog()->find(idref);
	if (!cnode)
		throw n_u::InvalidParameterException(
	    string("dsm") + ": " + getName(),
	    "dsmcatalog does not contain a dsm with ID",
	    idref);
        fromDOMElement(cnode);
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

            if (aname == "name") setName(project->expandString(aval));
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
                        // If no address part, it defaults to NIDAS_MULTICAST_ADDR
                        if (straddr.length() == 0) straddr = NIDAS_MULTICAST_ADDR;
                        try {
                            addr = n_u::Inet4Address::getByName(straddr);
                        }
                        catch(const n_u::UnknownHostException& e) {
                            throw n_u::InvalidParameterException(
                                string("dsm: ") + getName() + ": " + aname,straddr,e.what());
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
                        string("dsm: ") + getName(), aname,aval);
	    }
            else if (aname == "ID");	// catalog entry
            else if (aname == "IDREF");	// already scanned
            else if (aname == "rawSorterLength" || aname == "procSorterLength") {
		float val;
		istringstream ist(aval);
		ist >> val;
		if (ist.fail()) throw n_u::InvalidParameterException(
		    string("dsm") + ": " + getName(), aname,aval);
                if (aname[0] == 'r') setRawSorterLength(val);
                else setProcSorterLength(val);
	    }
            else if (aname == "rawHeapMax" || aname == "procHeapMax") {
		int val;
		istringstream ist(aval);
		ist >> val;
		if (ist.fail()) throw n_u::InvalidParameterException(
		    string("dsm") + ": " + getName(), aname,aval);
                string smult;
		ist >> smult;
                int mult = 1;
                if (smult.length() > 0) {
                    if (smult[0] == 'K') mult = 1000;
                    else if (smult[0] == 'M') mult = 1000000;
                    else if (smult[0] == 'G') mult = 1000000000;
                }
                if (aname[0] == 'r') setRawHeapMax((size_t)val*mult);
                else setProcHeapMax((size_t)val*mult);
	    }
            else if (aname == "rawLateSampleCacheSize" || aname == "procLateSampleCacheSize") {
		unsigned int val;
		istringstream ist(aval);
		ist >> val;
		if (ist.fail()) throw n_u::InvalidParameterException(
		    string("dsm") + ": " + getName(), aname,aval);
                if (aname[0] == 'r') setRawLateSampleCacheSize(val);
                else setProcLateSampleCacheSize(val);
	    }
            else if (aname == "xml:base" || aname == "xmlns");
	    else throw n_u::InvalidParameterException(
		string("dsm") + ": " + getName(),
		"unrecognized attribute",aname);
	}
    }

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
            elname == "irigSensor" ||   // not needed, identical to <sensor> in schema
            elname == "lamsSensor" ||   // not needed, identical to <sensor> in schema
            elname == "socketSensor") {

            if (elname == "irigSensor") WLOG(("%s: <irigSensor> element is obsolete. Use a <sensor> element instead",getName().c_str()));
            else if (elname == "lamsSensor") WLOG(("%s: <lamsSensor> element is obsolete. Use a <sensor> element instead",getName().c_str()));

            /*
             * This may not return a new DSMSensor, if there is a DSMCatalog,
             * and this sensor element matches by devicename a sensor element from the
             * entry for this DSMConfig in the DSMCatalog.
             */
            DSMSensor* sensor = sensorFromDOMElement((xercesc::DOMElement*)child);

            // check if this is a new DSMSensor for this DSMConfig.
            const std::list<DSMSensor*>& sensors = getSensors();
            list<DSMSensor*>::const_iterator si = std::find(sensors.begin(),sensors.end(),sensor);
	    if (si == sensors.end()) addSensor(sensor);
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
            processor->fromDOMElement((xercesc::DOMElement*)child);
	    addProcessor(processor);
        }
	else throw n_u::InvalidParameterException(
		string("dsm") + ": " + getName(),
		    "unrecognized element",elname);
    }

    // Warn if no outputs
    if (getOutputs().empty()) {
	ostringstream ost;
        n_u::Logger::getInstance()->log(LOG_WARNING,
		"dsm id %d has no configured outputs",getId());
    }
}

void DSMConfig::validate()
	throw(n_u::InvalidParameterException)
{
    for (SensorIterator si = getSensorIterator(); si.hasNext(); ) {
	DSMSensor* sensor = si.next();
        sensor->validate();
    }
    validateSensorAndSampleIds();
}

DSMSensor* DSMConfig::sensorFromDOMElement(const xercesc::DOMElement* node)
    throw(n_u::InvalidParameterException)
{
    assert(getProject());
    string classattr =
        DSMSensor::getClassName(node,getProject());
    XDOMElement xnode(node);
    /*
     * Look for a previous definition of a sensor with same devicename.
     * This is necessary to support surface systems which also
     * have a DSM catalog. <sensor> elements in the actual <dsm> section
     * override <sensor> elements from the <dsm> entry in the catalog,
     * where the <sensors> are matched by device name.
     */
    DSMSensor* sensor = 0;
    DSMSensor* mysensor = 0;
    DOMable* domable;
    const string& elname = xnode.getNodeName();
    const string& devname = xnode.getAttributeValue("devicename");

    if (devname.length() > 0) {
        const std::list<DSMSensor*>& sensors = getSensors();
        for (list<DSMSensor*>::const_iterator si = sensors.begin(); si != sensors.end(); ++si) {
            DSMSensor* snsr = *si;
            if (snsr->getDeviceName() == devname) sensor = snsr ;
        }
    }
    if (sensor && classattr.length() > 0 && sensor->getClassName() != classattr) 
            throw n_u::InvalidParameterException("sensor", sensor->getName(),
            string("conflicting class names: ") + sensor->getClassName() +
                " " + classattr);

    if (!sensor) {
        if (classattr.length() == 0)
            throw n_u::InvalidParameterException("sensor in dsm ",
            getName(),"has no class attribute");

        try {
            domable = DOMObjectFactory::createObject(classattr);
        }
        catch (const n_u::Exception& e) {
            throw n_u::InvalidParameterException("sensor",
                classattr,e.what());
        }
        mysensor = sensor = dynamic_cast<DSMSensor*>(domable);
        if (!sensor) {
            delete domable;
            throw n_u::InvalidParameterException(
                string("dsm") + ": " + getName(),
                elname,"is not a DSMSensor");
        }
    }

    // do setDSMConfig before fromDOMElement, because
    // some sensors may want to know their DSMConfig
    // within their fromDOMElement
    sensor->setDSMConfig(this);
    try {
        sensor->fromDOMElement(node);
    }
    catch (const n_u::InvalidParameterException& e) {
        delete mysensor;
        throw;
    }
    return sensor;
}

void DSMConfig::validateSensorAndSampleIds()
	throw(n_u::InvalidParameterException)
{

    // check for sensor ids which have value less than 0, or are not unique.
    typedef map<unsigned int,DSMSensor*> sens_map_t;
    typedef map<unsigned int,DSMSensor*>::const_iterator sens_map_itr_t;
    sens_map_t sensorIdCheck;
    sens_map_t dupSensorIdCheck;
    sens_map_t sampleIdCheck;
    sens_map_t dupSampleIdCheck;
    pair<sens_map_itr_t,bool> ins;
    sens_map_itr_t it;

    const std::list<DSMSensor*>& sensors = getSensors();
    for (list<DSMSensor*>::const_iterator si = sensors.begin();
    	si != sensors.end(); ++si) {
	DSMSensor* sensor = *si;

	if (sensor->getId() == 0)
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
                    ",sensor id=" << sensor->getSensorId() << ')';
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
                    ",sensor id=" << sensor->getSensorId() << ')';
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
                ost << sensor->getDSMId() << ',' << sensor->getSensorId();
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
                ost << sensor->getDSMId() << ',' << sensor->getSensorId();
                DSMSensor* other;
                if (!ins.second) other = ins.first->second;
                else other = it->second;
                throw n_u::InvalidParameterException(
                    sensor->getName() + " id=" + ost.str() +
                    " is equal to a sensor or sample id belonging to " + other->getName());
            }
        }

	// check that sample ids are unique
	const list<SampleTag*>& tags = sensor->getSampleTags();
	for (list<SampleTag*>::const_iterator ti = tags.begin();
			ti != tags.end(); ++ti) {
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

bool DSMConfig::MyDictionary::getTokenValue(const string& token,string& value) const
{
    if (token == "DSM") {
        value = _dsm->getName();
        return true;
    }

    if (token == "DSMID") {
        ostringstream ost;
        ost << _dsm->getId();
        value = ost.str();
        return true;
    }
        
    if (token == "LOCATION") {
        value = _dsm->getLocation();
        return true;
    }

    if (_dsm->getSite()) {
        return _dsm->getSite()->getTokenValue(token,value);
    }

    return false;
}

