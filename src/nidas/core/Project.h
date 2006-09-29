/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_PROJECT_H
#define NIDAS_CORE_PROJECT_H

#include <nidas/core/Site.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/DOMable.h>
#include <nidas/core/SensorCatalog.h>
#include <nidas/core/DSMCatalog.h>
#include <nidas/core/ServiceCatalog.h>

#include <nidas/util/ThreadSupport.h>

#include <list>

namespace nidas { namespace core {

/**
 */
class Project : public DOMable {
public:
    Project();
    virtual ~Project();

    static Project* getInstance();

    void setName(const std::string& val) { name = val; }
    const std::string& getName() const { return name; }

    void setSystemName(const std::string& val) { sysname = val; }
    const std::string& getSystemName() const { return sysname; }

    void setConfigVersion(const std::string& val) { configVersion = val; }
    const std::string& getConfigVersion() const { return configVersion; }

    void setConfigName(const std::string& val) { configName = val; }
    const std::string& getConfigName() const { return configName; }

    void setFlightName(const std::string& val) { flightName = val; }

    const std::string& getFlightName() const;

    void addSite(Site* val);

    const std::list<Site*>& getSites() const { return sites; }

    /**
     * Convenience function to return the maximum site number.
     */
    int getMaxSiteNumber() const { return maxSiteNumber; }

    int getMinSiteNumber() const { return minSiteNumber; }

    /**
     * A Project has one or more DSMServers.
     */
    void addServer(DSMServer* srvr) { servers.push_back(srvr); }

    const std::list<DSMServer*>& getServers() const { return servers; }

    /**
     * Look for a server for this project that either has no name or whose
     * name matches hostname.  If none found, remove any domain names
     * and try again. Then search the project Sites.
     */
    DSMServer* findServer(const std::string& hostname) const
	throw(nidas::util::UnknownHostException);

    DSMServer* findServer(const nidas::util::Inet4Address& addr) const;

    /**
     * Find a DSM whose name corresponds to
     * a given IP address.
     */
    const DSMConfig* findDSM(const nidas::util::Inet4Address& addr) const;

    const DSMConfig* findDSM(const std::string& name) const
	throw(nidas::util::UnknownHostException);

    /**
     * Find a DSM matching id;
     */
    const DSMConfig* findDSM(unsigned long id) const;

    DSMSensor* findSensor(dsm_sample_id_t id) const;

    /**
     * Find a Site with the given station number.
     */
    Site* findSite(int stationNumber) const;

    /**
     * Get a temporary unique sample id for a given DSM id.
     * This id can be used for identifying derived samples
     * during processing, when they are passed between
     * SampleSources and SampleClients.  The value of these
     * ids is not saved anywhere, and so they are meant to be
     * used for temporary connections, not for archived samples.
     */
    dsm_sample_id_t getUniqueSampleId(unsigned long dsmid);

    void setSensorCatalog(SensorCatalog* val) { sensorCatalog = val; }
    SensorCatalog* getSensorCatalog() const { return sensorCatalog; }

    void setDSMCatalog(DSMCatalog* val) { dsmCatalog = val; }
    DSMCatalog* getDSMCatalog() const { return dsmCatalog; }

    void setServiceCatalog(ServiceCatalog* val) { serviceCatalog = val; }
    ServiceCatalog* getServiceCatalog() const { return serviceCatalog; }

    DSMServerIterator getDSMServerIterator() const;

    DSMServiceIterator getDSMServiceIterator() const;

    ProcessorIterator getProcessorIterator() const;

    SiteIterator getSiteIterator() const;

    DSMConfigIterator getDSMConfigIterator() const;

    SensorIterator getSensorIterator() const;

    SampleTagIterator getSampleTagIterator() const;

    VariableIterator getVariableIterator() const;

    /**
     * Initialize all sensors for a Project.
     */
    void initSensors() throw(nidas::util::IOException);

    /**
     * Initialize all sensors for a given Site.
     */
    void initSensors(const Site* site) throw(nidas::util::IOException);

    /**
     * Initialize all sensors for a given dsm.
     */
    void initSensors(const DSMConfig* dsm) throw(nidas::util::IOException);

    const Parameter* getParameter(const std::string& name) const;

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

    static std::string expandEnvVars(const std::string& input);
    
    static std::string getEnvVar(const std::string& token);

protected:
    /**
     * Add a parameter to this SampleTag. SampleTag
     * will then own the pointer and will delete it
     * in its destructor.
     */
    void addParameter(Parameter* val)
    {
        parameters.push_back(val);
    }

private:
    static Project* instance;

    std::string name;

    std::string sysname;

    std::string configVersion;

    /**
     * Name of XML file that this project was initialized from.
     */
    std::string configName;

    mutable std::string flightName;

    std::list<Site*> sites;

    const Site* currentSite;

    SensorCatalog* sensorCatalog;

    DSMCatalog* dsmCatalog;

    ServiceCatalog* serviceCatalog;

    std::list<DSMServer*> servers;

    mutable nidas::util::Mutex lookupLock;

    mutable std::map<dsm_sample_id_t,const DSMConfig*> dsmById;

    mutable nidas::util::Mutex sensorMapLock;

    mutable std::map<dsm_sample_id_t,DSMSensor*> sensorById;

    std::map<int,Site*> siteByStationNumber;

    std::set<dsm_sample_id_t> usedIds;

    dsm_sample_id_t nextTempId;

    int maxSiteNumber;

    int minSiteNumber;

    /**
     * List of pointers to Parameters.
     */
    std::list<Parameter*> parameters;

};

}}	// namespace nidas namespace core

#endif
