// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

#include "Project.h"
#include "Site.h"
#include "DSMServer.h"
#include "SampleTag.h"
#include "SensorCatalog.h"
#include "DSMCatalog.h"
#include "ServiceCatalog.h"
#include "DOMObjectFactory.h"
#include "SampleOutput.h"
#include "SampleArchiver.h"
#include "FileSet.h"

#include <nidas/util/Inet4Address.h>
#include <nidas/util/Logger.h>
#include <nidas/util/Socket.h>
#include <nidas/util/auto_ptr.h>

#include <iostream>

#include <sys/utsname.h>    // uname

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

#ifdef ACCESS_AS_SINGLETON

/* static */
Project* Project::_instance = 0;

/* static */
Project* Project::getInstance() 
{
   if (!_instance) _instance = new Project();
   return _instance;
}

/* static */
void Project::destroyInstance() 
{
   delete _instance;
   _instance = 0;
}
#endif

Project::Project():
    _name(),_sysname(),_configVersion(),_configName(),_flightName(),
    _dictionary(this),_sites(),
    _sensorCatalog(0),_dsmCatalog(0),_serviceCatalog(0),
    _servers(),_lookupLock(),_dsmById(),_sensorMapLock(),
    _sensorById(),_siteByStationNumber(),_siteByName(),
    _usedIds(),_maxSiteNumber(0),_minSiteNumber(0),
    _parameters(),_dataset()
{
#ifdef ACCESS_AS_SINGLETON
    _instance = this;
#endif
}

Project::~Project()
{
#ifdef DEBUG
    cerr << "~Project, deleting servers" << endl;
#endif
    for (list<DSMServer*>::const_iterator is = _servers.begin();
	is != _servers.end(); ++is) delete *is;
#ifdef DEBUG
    cerr << "~Project, deleted servers" << endl;
#endif

    delete _sensorCatalog;
    delete _dsmCatalog;
    delete _serviceCatalog;
    // cerr << "deleting sites" << endl;
    for (list<Site*>::const_iterator it = _sites.begin();
    	it != _sites.end(); ++it) delete *it;

    for (list<Parameter*>::const_iterator pi = _parameters.begin();
    	pi != _parameters.end(); ++pi) delete *pi;

#ifdef ACCESS_AS_SINGLETON
    _instance = 0;
#endif
}


void
Project::
parseXMLConfigFile(const std::string& xmlfilepath) 
    throw(nidas::core::XMLException)
{
    {
        n_u::auto_ptr<xercesc::DOMDocument> 
            doc(nidas::core::parseXMLConfigFile(xmlfilepath));
        this->fromDOMElement(doc->getDocumentElement());
    }
    XMLImplementation::terminate();
}


const string& Project::getFlightName() const
{
    n_u::Synchronized autolock(_lookupLock);
    if (_flightName.length() == 0) {
	const char* flightEnv = ::getenv("FLIGHT");
	if (flightEnv) _flightName = string(flightEnv);
    }
    return _flightName;
}


void Project::addSite(Site* val)
{
    _sites.push_back(val);
    // station number 0 doesn't belong to a specific site
    if (val->getNumber() > 0) {
	_lookupLock.lock();
        _siteByStationNumber[val->getNumber()] = val;
	_lookupLock.unlock();
	_maxSiteNumber = std::max(val->getNumber(),_maxSiteNumber);
	if (_minSiteNumber == 0)
	    _minSiteNumber = val->getNumber();
        else
	    _minSiteNumber = std::min(val->getNumber(),_minSiteNumber);
    }

    _lookupLock.lock();
    _siteByName[val->getName()] = val;
    _lookupLock.unlock();
}

Site* Project::findSite(int stationNumber) const
{
    n_u::Synchronized autolock(_lookupLock);
    map<int,Site*>::const_iterator si =
        _siteByStationNumber.find(stationNumber);
    if (si != _siteByStationNumber.end()) return si->second;
    return 0;
}

Site* Project::findSite(const std::string& name) const
{
    n_u::Synchronized autolock(_lookupLock);
    map<string,Site*>::const_iterator si =
        _siteByName.find(name);
    if (si != _siteByName.end()) return si->second;
    return 0;
}

DSMServerIterator Project::getDSMServerIterator() const
{
    return DSMServerIterator(this);
}

DSMServiceIterator Project::getDSMServiceIterator() const
{
    return DSMServiceIterator(this);
}

ProcessorIterator Project::getProcessorIterator() const
{
    return ProcessorIterator(this);
}

SiteIterator Project::getSiteIterator() const
{
    return SiteIterator(this);
}

DSMConfigIterator Project::getDSMConfigIterator() const
{
    return DSMConfigIterator(this);
}

SensorIterator Project::getSensorIterator() const
{
    return SensorIterator(this);
}

SampleTagIterator Project::getSampleTagIterator() const
{
    return SampleTagIterator(this);
}

VariableIterator Project::getVariableIterator() const
{
    return VariableIterator(this);
}

/**
 * Initialize all sensors for a Project.
 */
void Project::initSensors() throw(n_u::IOException)
{
    const list<Site*>& sites = getSites();
    list<Site*>::const_iterator si;
    for (si = sites.begin(); si != sites.end(); ++si) {
	Site* s = *si;
    	s->initSensors();
    }
}

/**
 * Initialize all sensors for a Site.
 */
void Project::initSensors(Site* site) throw(n_u::IOException)
{
    const list<Site*>& sites = getSites();
    list<Site*>::const_iterator si;
    for (si = sites.begin(); si != sites.end(); ++si) {
	Site* s = *si;
    	if (s == site) s->initSensors();
    }
}

/**
 * Initialize all sensors for a given dsm.
 */
void Project::initSensors(DSMConfig* dsm) throw(n_u::IOException)
{
    const list<Site*>& sites = getSites();
    list<Site*>::const_iterator si;
    for (si = sites.begin(); si != sites.end(); ++si) {
	Site* s = *si;
	s->initSensors(dsm);
    }
}

/**
 * Look for a server for this project that whose name matches my
 * hostname.  If none found, remove any domain names and try again.
 * If still none found return any configured servers with
 * no name.
 */
list<DSMServer*> Project::findServers(const string& hostname) const
{
    list<DSMServer*> servers;
    if (hostname.length() > 0) {
	DSMServerIterator sitr = getDSMServerIterator();
	for ( ; sitr.hasNext(); ) {
	    DSMServer* srvr = sitr.next();
	    if (srvr->getName() == hostname) {
		servers.push_back(srvr);
		break;
	    }
	}
	if (servers.empty()) {
	    // Not found, remove domain name, try again
	    int dot = hostname.find('.');
	    sitr = getDSMServerIterator();
	    for ( ; sitr.hasNext(); ) {
		DSMServer* srvr = sitr.next();
		const string& sname = srvr->getName();
		int sdot = sname.find('.');
		if (!sname.compare(0,sdot,hostname,0,dot)) {
		    servers.push_back(srvr);
		    break;
		}
	    }
	}

	if (servers.empty()) {
	    // look for address match
            try {
                list<n_u::Inet4Address> addrs =
                        n_u::Inet4Address::getAllByName(hostname);
                list<n_u::Inet4Address>::const_iterator ai = addrs.begin();
                for ( ; ai != addrs.end(); ++ai) {
                    DSMServer* srvr = findServer(*ai);
                    if (srvr) {
                        servers.push_back(srvr);
                        break;
                    }
                }
            }
            catch(const n_u::UnknownHostException& e) {}
	}
    }

    if (servers.empty()) {
        // empty name in the config is a wildcard, matching all hostnames
        DSMServerIterator sitr = getDSMServerIterator();
        for ( ; sitr.hasNext(); ) {
            DSMServer* srvr = sitr.next();
            if (srvr->getName().length() == 0) servers.push_back(srvr);
        }
    }
    return servers;
}

DSMServer* Project::findServer(const n_u::Inet4Address& addr) const
{
    DSMServerIterator sitr = getDSMServerIterator();
    for ( ; sitr.hasNext(); ) {
        DSMServer* srvr = sitr.next();
	if (srvr->getName().length() > 0) {
	    try {
		list<n_u::Inet4Address> saddrs =
		    n_u::Inet4Address::getAllByName(srvr->getName());
		list<n_u::Inet4Address>::const_iterator ai = saddrs.begin();
		for ( ; ai != saddrs.end(); ++ai)
		    if (addr == *ai) return srvr;
	    }
	    catch (n_u::UnknownHostException& e) {}
	}
    }
    return 0;
}

const DSMConfig* Project::findDSM(const n_u::Inet4Address& addr) const
{
    const DSMConfig* dsm = 0;
    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
        DLOG(("checking site ") << site->getName()
             << " for dsm with address " << addr.getHostAddress());
        dsm = site->findDSM(addr);
        if (dsm)
        {
            DLOG(("matched site ") << site->getName()
                 << " with dsm " << dsm->getName()
                 << " for address " << addr.getHostAddress());
            return dsm;
        }
    }

    // No match. Check if addr corresponds to one of my interfaces.
    try {
        n_u::Inet4NetworkInterface iface =
            n_u::Inet4NetworkInterface::getInterface(addr);
        if (iface.getIndex() < 0)
        {
            DLOG(("address ") << addr.getHostAddress()
                 << " does not have an interface.");
            return dsm;   // not one of my interfaces, return NULL
        }
    }
    catch(const n_u::IOException& e) {
        WLOG(("Cannot determine local interfaces: %s",e.what()));
        return dsm;
    }

    // Address is one of my interfaces.  Check if there is a 
    // DSM that also has an address of one of my interfaces.
    for (DSMConfigIterator di = getDSMConfigIterator(); !dsm && di.hasNext(); ) {
        const DSMConfig* dsm2 = di.next();
        try {
            list<n_u::Inet4Address> saddrs =
                n_u::Inet4Address::getAllByName(dsm2->getName());
            list<n_u::Inet4Address>::const_iterator ai = saddrs.begin();
            for ( ; !dsm && ai != saddrs.end(); ++ai) {
                n_u::Inet4NetworkInterface iface =
                    n_u::Inet4NetworkInterface::getInterface(*ai);
                if (iface.getIndex() >= 0)
                {
                    DLOG(("address ") << ai->getHostAddress()
                         << " of dsm " << dsm2->getName()
                         << " matches interface " << iface.getName());
                    dsm = dsm2;
                }
                else
                {
                    DLOG(("address ") << ai->getHostAddress()
                         << " of dsm " << dsm2->getName()
                         << " does not match any interfaces");
                }
            }
        }
        catch(const n_u::UnknownHostException& e)
        {
            WLOG(("cannot determine address for DSM named %s",dsm2->getName().c_str()));
        }
        catch(const n_u::IOException& e) {
            WLOG(("Cannot determine network interfaces on this host: %s",e.what()));
        }
    }
    if (!dsm) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
                "dsm with address %s not found in project configuration",
            addr.getHostAddress().c_str());
    }
    return dsm;
}

const DSMConfig* Project::findDSM(unsigned int id) const
{
    {
	n_u::Synchronized autolock(_lookupLock);
	map<dsm_sample_id_t,const DSMConfig*>::const_iterator di =
	    _dsmById.find(id);
	if (di != _dsmById.end()) return di->second;
    }

    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	const DSMConfig* dsm = site->findDSM(id);
	if (dsm) {
	    _lookupLock.lock();
	    _dsmById[id] = dsm;
	    _lookupLock.unlock();
	    return dsm;
	}
    }
    DLOG(("dsm with id %u not found",id));
    return 0;
}

const DSMConfig* Project::findDSM(const std::string& name) const
{
    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	const DSMConfig* dsm = site->findDSM(name);
	if (dsm) return dsm;
    }

    try {
        list<n_u::Inet4Address> saddrs =
            n_u::Inet4Address::getAllByName(name);
        list<n_u::Inet4Address>::const_iterator ai = saddrs.begin();
        for ( ; ai != saddrs.end(); ++ai) {
            const DSMConfig* dsm = findDSM(*ai);
            if (dsm) return dsm;
        }
    }
    catch(const n_u::UnknownHostException& e) {}
    WLOG(("dsm with name ") << name << "  not found in project configuration");
    return 0;
}


DSMConfig*
Project::
findDSMFromHostname(const std::string& hostname)
{
    // location of first dot of hostname, or if not found 
    // the host string match will be done against entire hostname
    string::size_type dot = hostname.find('.');

    DSMConfig* dsmConfig = 0;
    DSMConfig* dsm = 0;
    int ndsms = 0;

    const list<Site*>& sites = getSites();
    list<Site*>::const_iterator si;
    for (si = sites.begin(); !dsmConfig && si != sites.end(); ++si)
    {
        Site* site = *si;
        const list<DSMConfig*>& dsms = site->getDSMConfigs();

        list<DSMConfig*>::const_iterator di;
        for (di = dsms.begin(); !dsmConfig && di != dsms.end(); ++di)
        {
            dsm = *di;
            ndsms++;
            if (dsm->getName() == hostname ||
                dsm->getName() == hostname.substr(0, dot))
            {
                dsmConfig = dsm;
                n_u::Logger::getInstance()->log(LOG_INFO,
                    "Project: found <dsm> for %s", hostname.c_str());
                break;
            }
        }
    }
    if (ndsms == 1)
        dsmConfig = dsm;
    return dsmConfig;
}



list<nidas::core::FileSet*> Project::findSampleOutputStreamFileSets() const
{
    list<nidas::core::FileSet*> filesets;
    for (DSMConfigIterator di = getDSMConfigIterator(); di.hasNext(); ) {
	const DSMConfig* dsm = di.next();

        list<nidas::core::FileSet*> fsets = dsm->findSampleOutputStreamFileSets();
        filesets.splice(filesets.end(),fsets);

        // Newer libstdc++
        // filesets.insert(filesets.end(),dsm->findSampleOutputStreamFileSets());
    }
    return filesets;
}
    
list<nidas::core::FileSet*> Project::findSampleOutputStreamFileSets(const std::string& dsmName) const
{
    list<nidas::core::FileSet*> filesets;
    const DSMConfig* dsm = findDSM(dsmName);
    if (dsm) filesets = dsm->findSampleOutputStreamFileSets();
    return filesets;
}
    
list<nidas::core::FileSet*> Project::findServerSampleOutputStreamFileSets(const std::string& name) const
{
    list<nidas::core::FileSet*> filesets;
    // filesets corresponding to the "any" server, with name "".
    list<nidas::core::FileSet*> anysets;

    list<DSMServer*> servers = findServers(name);

    list<DSMServer*>::const_iterator si = servers.begin();

    for ( ; si != servers.end(); ++si ) {
        DSMServer* server = *si;
        ProcessorIterator pi = server->getProcessorIterator();
        for ( ; pi.hasNext(); ) {
            SampleIOProcessor* proc = pi.next();
            nidas::core::SampleArchiver* archiver =
                    dynamic_cast<nidas::core::SampleArchiver*>(proc);
            if (archiver) {
                const std::list<SampleOutput*> outputs =
                    proc->getOutputs();
                std::list<SampleOutput*>::const_iterator oi =
                    outputs.begin();
                for ( ; oi != outputs.end(); ++oi) {
                    SampleOutput* output = *oi;
                    IOChannel* ioc = output->getIOChannel();
                    nidas::core::FileSet* fset =
                            dynamic_cast<nidas::core::FileSet*>(ioc);
                    if (fset) {
                        if (server->getName().length() > 0)
                            filesets.push_back(fset);
                        else
                            anysets.push_back(fset);
                    }
                }
            }
        }
    }
    if (!filesets.empty()) return filesets;
    return anysets;
}

list<nidas::core::FileSet*> Project::findServerSampleOutputStreamFileSets() const throw(nidas::util::Exception)
{
    struct utsname utsbuf;
    if (::uname(&utsbuf) < 0)
        throw nidas::util::Exception("uname",errno);
    list<nidas::core::FileSet*> filesets =
        findServerSampleOutputStreamFileSets(utsbuf.nodename);
    if (filesets.empty())
        WLOG(("No filesets found for server %s", utsbuf.nodename));
    return filesets;
}

DSMSensor* Project::findSensor(dsm_sample_id_t id) const
{
    {
	n_u::Synchronized autolock(_sensorMapLock);
	map<dsm_sample_id_t,DSMSensor*>::const_iterator di =
	    _sensorById.find(id);
	if (di != _sensorById.end()) return di->second;
    }

    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	DSMSensor* sensor = site->findSensor(id);
	if (sensor) {
	    _sensorMapLock.lock();
	    _sensorById[id] = sensor;
	    _sensorMapLock.unlock();
	    return sensor;
	}
    }
    return 0;
}

DSMSensor* Project::findSensor(const SampleTag* tag) const
{
    dsm_sample_id_t id = tag->getId();
    return findSensor(id);
}

dsm_sample_id_t Project::getUniqueSampleId(unsigned int dsmid)
{
    n_u::Synchronized autolock(_sensorMapLock);
    set<dsm_sample_id_t> ids;

    if (_usedIds.empty()) {
        // initialize _usedIds
	SampleTagIterator sti = getSampleTagIterator();
	for (; sti.hasNext(); ) {
	    const SampleTag* stag = sti.next();
	    dsm_sample_id_t id = stag->getId();
	    if (!_usedIds.insert(id).second) 
		n_u::Logger::getInstance()->log(LOG_ERR,
			"sample %d,%d(%#x) is not unique",
			GET_DSM_ID(id),GET_SPS_ID(id),GET_SPS_ID(id));
	}
    }
    dsm_sample_id_t id = 0;
    id = SET_DSM_ID(id,dsmid);
    id = SET_SHORT_ID(id,32768);
    while(!_usedIds.insert(id).second) id++;
    return id;
}

const Parameter* Project::getParameter(const string& name) const
{
    list<Parameter*>::const_iterator pi;
    for (pi = _parameters.begin(); pi != _parameters.end(); ++pi) {
        Parameter* param = *pi;
    	if (param->getName() == name) return param;
    }
    return 0;
}

static void
LogSchemeFromDOMElement(const xercesc::DOMElement* node)
{
    XDOMElement xnode(node);
    const string& name = xnode.getAttributeValue("name");
    xercesc::DOMNode* child;
    n_u::LogScheme scheme;
    scheme.setName (name);
    for (child = node->getFirstChild(); child != 0;
	 child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();
	if (elname == "showfields")
	{
	    xercesc::DOMElement* text = (xercesc::DOMElement*)child->getFirstChild();
	    if (text->getNodeType() == xercesc::DOMNode::TEXT_NODE)
	    {
		std::string showfields = 
		    XMLStringConverter(text->getNodeValue());
		scheme.setShowFields(showfields);
	    }
	}
	else if (elname == "logconfig")
	{
	    n_u::LogConfig lc;
	    if (child->hasAttributes()) 
	    {
		xercesc::DOMNamedNodeMap *pAttributes = child->getAttributes();
		int nSize = pAttributes->getLength();
		for(int i=0;i<nSize;++i) {
		    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
		    if (attr.getName() == "filematch") 
			lc.filename_match = attr.getValue();
		    else if (attr.getName() == "functionmatch")
			lc.function_match = attr.getValue();
		    else if (attr.getName() == "tagmatch")
			lc.tag_match = attr.getValue();
		    else if (attr.getName() == "level")
			lc.level = n_u::stringToLogLevel(attr.getValue());
		    else if (attr.getName() == "line")
			lc.line = atoi(attr.getValue().c_str());
		    else if (attr.getName() == "activate")
		    {
			std::string value = attr.getValue();
			for (unsigned int c = 0; c < value.length(); ++c)
			    value[c] = std::tolower(value[c]);
			if (value == "true" ||
			    value == "1" ||
			    value == "yes")
			{
			    lc.activate = true;
			}
			else if (value == "false" ||
				 value == "0" ||
				 value == "no")
			{
			    lc.activate = false;
			}
			else
			{
			    throw n_u::InvalidParameterException
				("Project::LogSchemeFromDOMElement","activate",
				 name);
			}
		    }
		    
		}
	    }
	    scheme.addConfig (lc);
	}
    }
    n_u::Logger::getInstance()->updateScheme(scheme);
}


void Project::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
#ifdef XML_DEBUG
    cerr << "element name=" << xnode.getNodeName() << endl;
#endif
    
    if (xnode.getNodeName() != "project")
	    throw n_u::InvalidParameterException(
		    "Project::fromDOMElement","xml node name",
		    	xnode.getNodeName());
		    
    if(node->hasAttributes()) {
    // get all the attributes of the node
	xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
	    if (attr.getName() == "name") setName(attr.getValue());
	    else if (attr.getName() == "system")
	    	setSystemName(attr.getValue());
	    else if (attr.getName() == "version")
	    	setConfigVersion(attr.getValue());
	    else if (attr.getName() == "config")
	    	setConfigName(attr.getValue());
	    else if (attr.getName() == "maxStation") {
                istringstream ist(attr.getValue());
                int val;
                ist >> val;
                if (ist.fail()) 
                    throw n_u::InvalidParameterException("project",
                        attr.getName(),attr.getValue());
	    	if (val > 0) _maxSiteNumber = val;
            }
	}
    }

    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();
#ifdef XML_DEBUG
	cerr << "element name=" << elname << endl;
#endif

	if (elname == "site") {
	    DOMable* domable;
	    const string& classattr = xchild.getAttributeValue("class");
	    if (classattr.length() == 0)
		throw n_u::InvalidParameterException(
		    string("project") + ": " + getName(),
		    "site",
		    "does not have a class attribute");
	    try {
		domable = DOMObjectFactory::createObject(classattr);
	    }
	    catch (const n_u::Exception& e) {
		throw n_u::InvalidParameterException("site",
		    classattr,e.what());
	    }
	    Site* site = dynamic_cast<Site*>(domable);
	    if (!site)
		throw n_u::InvalidParameterException("project",
		    classattr,"is not a sub-class of Site");

	    site->setProject(this);
	    try {
		site->fromDOMElement((xercesc::DOMElement*)child);
	    }
	    catch(const n_u::InvalidParameterException& e) {
	        delete site;
		throw;
	    }
            site->validate();
	    addSite(site);
	}
	else if (elname == "aircraft") {
	    DOMable* domable;
	    // <aircraft> tag is the same as <site class="raf.Aircraft">
	    try {
                domable = DOMObjectFactory::createObject("raf.Aircraft");
	    }
	    catch (const n_u::Exception& e) {
		throw n_u::InvalidParameterException("aircraft",
		    "raf.Aircraft",e.what());
	    }
	    Site* site = dynamic_cast<Site*>(domable);
	    if (!site)
		throw n_u::InvalidParameterException("project",
		    "raf.Aircraft","is not a sub-class of Site");
	    site->setProject(this);
	    try {
		site->fromDOMElement((xercesc::DOMElement*)child);
	    }
	    catch(const n_u::InvalidParameterException& e) {
	        delete site;
		throw;
	    }
            site->validate();
	    addSite(site);
	}
	else if (elname == "sensorcatalog") {
	    SensorCatalog* catalog = new SensorCatalog();
	    catalog->fromDOMElement((xercesc::DOMElement*)child);
	    setSensorCatalog(catalog);
	}
	else if (elname == "dsmcatalog") {
	    DSMCatalog* catalog = new DSMCatalog();
	    catalog->fromDOMElement((xercesc::DOMElement*)child);
	    setDSMCatalog(catalog);
	}
	else if (elname == "servicecatalog") {
	    ServiceCatalog* catalog = new ServiceCatalog();
	    catalog->fromDOMElement((xercesc::DOMElement*)child);
	    setServiceCatalog(catalog);
	}
	else if (elname == "server") {
	    DSMServer* server = new DSMServer();
	    server->setProject(this);
	    server->fromDOMElement((xercesc::DOMElement*)child);
	    addServer(server);
	}
	else if (elname == "parameter")  {
	    Parameter* parameter =
	    	Parameter::createParameter((xercesc::DOMElement*)child,&_dictionary);
	    addParameter(parameter);
	}
	else if (elname == "logscheme")  {
	  LogSchemeFromDOMElement ((xercesc::DOMElement*)child);
	}
	else if (elname == "logger") {
	    const string& scheme = xchild.getAttributeValue("scheme");
	    n_u::Logger* logger = n_u::Logger::getInstance();
	    // If the current scheme is not the default, then don't
	    // override it.  This way the scheme can be set before an
	    // XML file is parsed, such as from a command line option,
	    // by giving that scheme a non-default name.
	    if (logger->getScheme().getName() == n_u::LogScheme().getName())
	    {
		logger->setScheme(scheme);
	    }
	}
    }

}

xercesc::DOMElement* Project::toDOMParent(xercesc::DOMElement* parent,bool complete) const
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
            DOMable::getNamespaceURI(),
            (const XMLCh*)XMLStringConverter("project"));
    parent->appendChild(elem);
    return toDOMElement(elem,complete);
}

xercesc::DOMElement* Project::toDOMElement(xercesc::DOMElement* elem,bool complete) const
    throw(xercesc::DOMException)
{
    if (complete) return 0; // not supported yet

    XDOMElement xelem(elem);
    xelem.setAttributeValue("name",getName());

    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        Site* site = si.next();
        site->toDOMParent(elem,complete);
    }
    return elem;
}

bool Project::MyDictionary::getTokenValue(const string& token,string& value) const
{
    if (token == "PROJECT") {
        value = _project->getName();
        return true;
    }

    if (token == "SYSTEM") {
        value = _project->getSystemName();
        return true;
    }

    // if none of the above, try to get token value from UNIX environment
    return n_u::Process::getEnvVar(token,value);
}

