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
#include <DSMServer.h>
#include <DOMObjectFactory.h>

#include <atdUtil/Logger.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

// CREATOR_FUNCTION(Project)

/* static */
Project* Project::instance = 0;

/* static */
Project* Project::getInstance() 
{
   if (!instance) instance = new Project();
   return instance;
}

Project::Project(): currentSite(0),catalog(0),
	maxSiteNumber(-1),minSiteNumber(INT_MAX - 10)
{
    const char* obsPer = 0;

    if (::getenv("ISFF") != 0)
	obsPer = ::getenv("OPS");
    else if(::getenv("ADS3_CONFIG") != 0)
	obsPer = getenv("ADS3_FLIGHT");

    if (obsPer == 0) obsPer = "unknown";

    currentObsPeriod.setName(obsPer);

    atdUtil::Logger::getInstance()->log(LOG_INFO,"currentObsPeriod=%s",
    	currentObsPeriod.getName().c_str());
}

Project::~Project()
{
    // cerr << "deleting catalog" << endl;
    delete catalog;
    // cerr << "deleting sites" << endl;
    for (std::list<Site*>::iterator it = sites.begin();
    	it != sites.end(); ++it) delete *it;

    for (list<DSMServer*>::iterator is = servers.begin();
	is != servers.end(); ++is) delete *is;

    instance = 0;
}

void Project::addSite(Site* val)
{
    sites.push_back(val);
    if (val->getNumber() >= 0) {
	lookupLock.lock();
        siteByStationNumber[val->getNumber()] = val;
	lookupLock.unlock();
	maxSiteNumber = std::max(val->getNumber(),maxSiteNumber);
	minSiteNumber = std::min(val->getNumber(),minSiteNumber);
    }
}

Site* Project::findSite(int stationNumber) const
{
    {
	atdUtil::Synchronized autolock(lookupLock);
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
void Project::initSensors() throw(atdUtil::IOException)
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
void Project::initSensors(const Site* site) throw(atdUtil::IOException)
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
void Project::initSensors(const DSMConfig* dsm) throw(atdUtil::IOException)
{
    list<Site*>::const_iterator si = getSites().begin();
    for ( ; si != getSites().end(); ++si) {
	Site* ncsite = *si;
	ncsite->initSensors(dsm);
    }
}

/* static */
string Project::getConfigName(const string& root, const string& projectsDir,
	const string& project, const string& site,const string& siteSubDir,
	const string& obsPeriod, const string& fileName)
		throw(atdUtil::InvalidParameterException)
{
    string rootName = root;
    if (root.length() > 0 && root[0] == '$') {
	const char* val = getenv(root.c_str()+1);
	if (!val) throw atdUtil::InvalidParameterException("environment",root,"null");
	rootName = val;
    }
    string projectName = project;
    if (project.length() > 0 && project[0] == '$') {
	const char* val = getenv(project.c_str()+1);
	if (!val) throw atdUtil::InvalidParameterException("environment",project,"null");
	projectName = val;
    }
    string siteName = site;
    if (site.length() > 0 && site[0] == '$') {
	const char* val = getenv(site.c_str()+1);
	if (!val) throw atdUtil::InvalidParameterException("environment",site,"null");
	siteName = val;
    }
    string obsName = obsPeriod;
    if (obsPeriod.length() > 0 && obsPeriod[0] == '$') {
	const char* val = getenv(obsPeriod.c_str()+1);
	if (!val) throw atdUtil::InvalidParameterException("environment",obsPeriod,"null");
	obsName = val;
    }

    return string(rootName) + '/' + projectsDir + '/' + projectName + '/' +
	siteName + '/' + siteSubDir + '/' + obsName + '/' + fileName;
}

/**
 * Look for a server on this aircraft that either has no name or whose
 * name matches hostname.  If none found, remove any domain names
 * and try again.
 */
DSMServer* Project::findServer(const string& hostname) const
{
    DSMServer* server = 0;
    for (list<DSMServer*>::const_iterator si=servers.begin();
        si != servers.end(); ++si) {
        DSMServer* srvr = *si;
        if (srvr->getName().length() == 0 ||
            !srvr->getName().compare(hostname)) {
            server = srvr;
            break;
        }
    }
    if (server) return server;

    // Not found, remove domain name, try again
    int dot = hostname.find('.');
    for (list<DSMServer*>::const_iterator si=servers.begin();
        si != servers.end(); ++si) {
        DSMServer* srvr = *si;
        const string& sname = srvr->getName();
        int sdot = sname.find('.');
        if (!sname.compare(0,sdot,hostname,0,dot)) {
            server = srvr;
            break;
        }
    }
    if (server) return server;

    for ( SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	server = site->findServer(hostname);
	if (server) return server;
    }
    return server;
}

const DSMConfig* Project::findDSM(const atdUtil::Inet4Address& addr) const
{
    cerr <<  "Checking sites" << endl;
    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	cerr <<  "Checking site " << site->getName() << endl;
	const DSMConfig* dsm = site->findDSM(addr);
	if (dsm) return dsm;
    }
    return 0;
}

const DSMConfig* Project::findDSM(unsigned long id) const
{
    cerr << "Project::findDSM, id=" << id << endl;
    {
	atdUtil::Synchronized autolock(lookupLock);
	map<dsm_sample_id_t,const DSMConfig*>::const_iterator di =
	    dsmById.find(id);
	if (di != dsmById.end()) return di->second;
    }

    for (SiteIterator si = getSiteIterator(); si.hasNext(); ) {
        const Site* site = si.next();
	cerr << "Project::findDSM, site=" << site->getName() << endl;
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

DSMSensor* Project::findSensor(dsm_sample_id_t id) const
{
    {
	atdUtil::Synchronized autolock(sensorMapLock);
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

dsm_sample_id_t Project::getUniqueSampleId(unsigned long dsmid)
{
    atdUtil::Synchronized autolock(sensorMapLock);
    set<dsm_sample_id_t> ids;
    if (usedIds.size() == 0) {
	SampleTagIterator sti = getSampleTagIterator();
	for (; sti.hasNext(); ) {
	    const SampleTag* stag = sti.next();
	    dsm_sample_id_t id = stag->getId();
	    if (!usedIds.insert(id).second) 
		atdUtil::Logger::getInstance()->log(LOG_ERR,
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
	for(int i=0;i<nSize;++i) {
	    XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	    if (!attr.getName().compare("name")) setName(attr.getValue());
	    else if (!attr.getName().compare("system")) setSystemName(attr.getValue());
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
	else if (!elname.compare("server")) {
	    DSMServer* server = new DSMServer();
	    server->fromDOMElement((DOMElement*)child);
	    addServer(server);
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

