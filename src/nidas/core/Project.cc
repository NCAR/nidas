/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/Project.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/DOMObjectFactory.h>
#include <nidas/dynld/SampleArchiver.h>
#include <nidas/dynld/FileSet.h>

#include <nidas/util/Inet4Address.h>
#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

/* static */
Project* Project::instance = 0;

/* static */
Project* Project::getInstance() 
{
   if (!instance) instance = new Project();
   return instance;
}

Project::Project(): currentSite(0),sensorCatalog(0),dsmCatalog(0),
	serviceCatalog(0),
	maxSiteNumber(-1),minSiteNumber(INT_MAX - 10)

{
}

Project::~Project()
{

    delete sensorCatalog;
    delete dsmCatalog;
    delete serviceCatalog;
    // cerr << "deleting sites" << endl;
    for (list<Site*>::const_iterator it = sites.begin();
    	it != sites.end(); ++it) delete *it;

#ifdef DEBUG
    cerr << "~Project, deleting servers" << endl;
#endif
    for (list<DSMServer*>::const_iterator is = servers.begin();
	is != servers.end(); ++is) delete *is;
#ifdef DEBUG
    cerr << "~Project, deleted servers" << endl;
#endif

    for (list<Parameter*>::const_iterator pi = parameters.begin();
    	pi != parameters.end(); ++pi) delete *pi;

    instance = 0;
}

const string& Project::getFlightName() const
{
    n_u::Synchronized autolock(lookupLock);
    if (flightName.length() == 0) {
	const char* flightEnv = ::getenv("FLIGHT");
	if (flightEnv) flightName == string(flightEnv);
    }
    return flightName;
}


void Project::addSite(Site* val)
{
    sites.push_back(val);
    // station number 0 doesn't belong to a specific site
    if (val->getNumber() > 0) {
	lookupLock.lock();
        siteByStationNumber[val->getNumber()] = val;
	lookupLock.unlock();
    }
    maxSiteNumber = std::max(val->getNumber(),maxSiteNumber);
    minSiteNumber = std::min(val->getNumber(),minSiteNumber);
}

Site* Project::findSite(int stationNumber) const
{
    {
	n_u::Synchronized autolock(lookupLock);
	map<int,Site*>::const_iterator si =
	    siteByStationNumber.find(stationNumber);
	if (si != siteByStationNumber.end()) return si->second;
    }
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
    list<Site*>::const_iterator si = getSites().begin();
    for ( ; si != getSites().end(); ++si) {
	Site* ncsite = *si;
    	ncsite->initSensors();
    }
}

/**
 * Initialize all sensors for a Site.
 */
void Project::initSensors(const Site* site) throw(n_u::IOException)
{
    list<Site*>::const_iterator si = getSites().begin();
    for ( ; si != getSites().end(); ++si) {
	Site* ncsite = *si;
    	if (ncsite == site) ncsite->initSensors();
    }
}

/**
 * Initialize all sensors for a given dsm.
 */
void Project::initSensors(const DSMConfig* dsm) throw(n_u::IOException)
{
    list<Site*>::const_iterator si = getSites().begin();
    for ( ; si != getSites().end(); ++si) {
	Site* ncsite = *si;
	ncsite->initSensors(dsm);
    }
}

/**
 * Look for a server for this project that either has no name or whose
 * name matches hostname.  If none found, remove any domain names
 * and try again.
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
	if (servers.size() == 0) {
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

	if (servers.size() == 0) {
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

    // empty name
    DSMServerIterator sitr = getDSMServerIterator();
    for ( ; sitr.hasNext(); ) {
        DSMServer* srvr = sitr.next();
        if (srvr->getName().length() == 0) servers.push_back(srvr);
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
    cerr <<  "Checking sites" << endl;
    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	cerr <<  "Checking site " << site->getName() << " for dsm with address " << addr.getHostAddress() << endl;
	const DSMConfig* dsm = site->findDSM(addr);
	if (dsm) {
	    cerr <<  "Found dsm " << dsm->getName() <<
	    	"(" << addr.getHostAddress() << ") at site " << site->getName() << endl;
	    return dsm;
	}
    }
    return 0;
}

const DSMConfig* Project::findDSM(unsigned long id) const
{
    {
	n_u::Synchronized autolock(lookupLock);
	map<dsm_sample_id_t,const DSMConfig*>::const_iterator di =
	    dsmById.find(id);
	if (di != dsmById.end()) return di->second;
    }

    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	const DSMConfig* dsm = site->findDSM(id);
	if (dsm) {
	    lookupLock.lock();
	    dsmById[id] = dsm;
	    lookupLock.unlock();
	    return dsm;
	}
    }
    return 0;
}

const DSMConfig* Project::findDSM(const string& name) const
{
    cerr <<  "Checking sites" << endl;
    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	cerr <<  "Checking site " << site->getName() << " for dsm with name " << name << endl;
	const DSMConfig* dsm = site->findDSM(name);
	if (dsm) {
	    cerr <<  "Found dsm " << name <<
	    	" at site " << site->getName() << endl;
	    return dsm;
	}
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
    return 0;
}

list<nidas::dynld::FileSet*> Project::findSampleOutputStreamFileSets(
	const string& hostName) const
{
    list<nidas::dynld::FileSet*> filesets;
    if (hostName.length() > 0) {
        const DSMConfig* dsm = findDSM(hostName);
        if (dsm) filesets = dsm->findSampleOutputStreamFileSets();
    }
    
    list<DSMServer*> servers = findServers(hostName);

    list<DSMServer*>::const_iterator si = servers.begin();

    for ( ; si != servers.end(); ++si) {
        DSMServer* server = *si;
        ProcessorIterator pi = server->getProcessorIterator();
        for ( ; pi.hasNext(); ) {
            SampleIOProcessor* proc = pi.next();
            nidas::dynld::SampleArchiver* archiver =
                    dynamic_cast<nidas::dynld::SampleArchiver*>(proc);
            if (archiver) {
                const std::list<SampleOutput*> outputs =
                    proc->getOutputs();
                std::list<SampleOutput*>::const_iterator oi =
                    outputs.begin();
                for ( ; oi != outputs.end(); ++oi) {
                    SampleOutput* output = *oi;
                    IOChannel* ioc = output->getIOChannel();
                    nidas::dynld::FileSet* fset =
                            dynamic_cast<nidas::dynld::FileSet*>(ioc);
                    if (fset) filesets.push_back(fset);
                }
            }
        }
    }
    return filesets;
}

list<nidas::dynld::FileSet*> Project::findSampleOutputStreamFileSets() const
{
    return findSampleOutputStreamFileSets("");
}

#ifdef NEED_THESE
nidas::dynld::FileSet* Project::findSampleOutputStreamFileSet(
	const string& hostName,const n_u::UTime& t1, const n_u::UTime& t2)
{
    list<nidas::dynld::FileSet*> filesets =
        findSampleOutputStreamFileSets(hostName);
    list<nidas::dynld::FileSet*>::const_iterator fi = filesets.begin();
    for ( ; fi != filesets.end(); ++fi) {
        nidas::dynld::FileSet* fset = *fi;
	list<string> files = fset->matchFiles(t1,t2);
	if (files.size() > 0) return fset;
    }
    return 0;
}

nidas::dynld::FileSet* Project::findSampleOutputStreamFileSet(
	const n_u::UTime& t1, const n_u::UTime& t2)
{
    list<nidas::dynld::FileSet*> filesets = findSampleOutputStreamFileSets();
    list<nidas::dynld::FileSet*>::const_iterator fi = filesets.begin();
    for ( ; fi != filesets.end(); ++fi) {
        nidas::dynld::FileSet* fset = *fi;
	list<string> files = fset->matchFiles(t1,t2);
	if (files.size() > 0) return fset;
    }
    return 0;
}
#endif

DSMSensor* Project::findSensor(dsm_sample_id_t id) const
{
    {
	n_u::Synchronized autolock(sensorMapLock);
	map<dsm_sample_id_t,DSMSensor*>::const_iterator di =
	    sensorById.find(id);
	if (di != sensorById.end()) return di->second;
    }

    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	DSMSensor* sensor = site->findSensor(id);
	if (sensor) {
	    sensorMapLock.lock();
	    sensorById[id] = sensor;
	    sensorMapLock.unlock();
	    return sensor;
	}
    }
    return 0;
}

/* static */
string Project::expandEnvVars(const string& input)
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
	    string val = getEnvVar(token);
	    // cerr << "getTokenValue: token=" << token << " val=" << val << endl;
	    result.append(val);
	}
    }

    result.append(input.substr(lastpos));
    // cerr << "input: \"" << input << "\" expanded to \"" <<
    // 	result << "\"" << endl;
    return result;
}

/* static */
string Project::getEnvVar(const string& token)
{
    const char* val = ::getenv(token.c_str());
    if (val) return string(val);
    else return "unknown";
}

dsm_sample_id_t Project::getUniqueSampleId(unsigned long dsmid)
{
    n_u::Synchronized autolock(sensorMapLock);
    set<dsm_sample_id_t> ids;
    if (usedIds.size() == 0) {
	SampleTagIterator sti = getSampleTagIterator();
	for (; sti.hasNext(); ) {
	    const SampleTag* stag = sti.next();
	    dsm_sample_id_t id = stag->getId();
	    if (!usedIds.insert(id).second) 
		n_u::Logger::getInstance()->log(LOG_ERR,
			"sample id %d (dsm:%d, sample:%d) is not unique",
			id,GET_DSM_ID(id),GET_SHORT_ID(id));
	}
    }
    dsm_sample_id_t id = 0;
    id = SET_DSM_ID(id,dsmid);
    id = SET_SHORT_ID(id,32768);
    while(!usedIds.insert(id).second) id++;
    return id;
}

const Parameter* Project::getParameter(const string& name) const
{
    list<Parameter*>::const_iterator pi;
    for (pi = parameters.begin(); pi != parameters.end(); ++pi) {
        Parameter* param = *pi;
    	if (param->getName() == name) return param;
    }
    return 0;
}

void Project::fromDOMElement(const DOMElement* node)
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
	DOMNamedNodeMap *pAttributes = node->getAttributes();
	int nSize = pAttributes->getLength();
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    if (attr.getName() == "name") setName(attr.getValue());
	    else if (attr.getName() == "system")
	    	setSystemName(attr.getValue());
	    else if (attr.getName() == "version")
	    	setConfigVersion(attr.getValue());
	    else if (attr.getName() == "config")
	    	setConfigName(attr.getValue());
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
		site->fromDOMElement((DOMElement*)child);
	    }
	    catch(const n_u::InvalidParameterException& e) {
	        delete site;
		throw;
	    }
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
	    site->fromDOMElement((DOMElement*)child);
	    addSite(site);
	}
	else if (elname == "sensorcatalog") {
	    SensorCatalog* catalog = new SensorCatalog();
	    catalog->fromDOMElement((DOMElement*)child);
	    setSensorCatalog(catalog);
	}
	else if (elname == "dsmcatalog") {
	    DSMCatalog* catalog = new DSMCatalog();
	    catalog->fromDOMElement((DOMElement*)child);
	    setDSMCatalog(catalog);
	}
	else if (elname == "servicecatalog") {
	    ServiceCatalog* catalog = new ServiceCatalog();
	    catalog->fromDOMElement((DOMElement*)child);
	    setServiceCatalog(catalog);
	}
	else if (elname == "server") {
	    DSMServer* server = new DSMServer();
	    server->fromDOMElement((DOMElement*)child);
	    addServer(server);
	}
	else if (elname == "parameter")  {
	    Parameter* parameter =
	    	Parameter::createParameter((xercesc::DOMElement*)child);
	    addParameter(parameter);
	}
    }

    // loop over project-wide servers adding project sites.
    list<DSMServer*>::const_iterator si;
    for (si = getServers().begin(); si != getServers().end(); ++si) {
        DSMServer* server = *si;
	list<Site*>::const_iterator ti;
	for (ti = getSites().begin(); ti != getSites().end();
		++ti) {
	    Site* site = *ti;
	    server->addSite(site);
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

string Project::expandString(const string& input) const
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
    // cerr << "input: \"" << input << "\" expanded to \"" <<
    // 	result << "\"" << endl;
    return result;
}

string Project::getTokenValue(const string& token) const
{
    if (token == "PROJECT") return getName();

    if (token == "SYSTEM") return getSystemName();

    // if none of the above, try to get token value from UNIX environment
    return getEnvVar(token);
}

