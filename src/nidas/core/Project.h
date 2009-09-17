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
#include <nidas/util/UTime.h>

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

    void setName(const std::string& val) { _name = val; }
    const std::string& getName() const { return _name; }

    void setSystemName(const std::string& val) { _sysname = val; }
    const std::string& getSystemName() const { return _sysname; }

    void setConfigVersion(const std::string& val) { _configVersion = val; }
    const std::string& getConfigVersion() const { return _configVersion; }

    void setConfigName(const std::string& val) { _configName = val; }
    const std::string& getConfigName() const { return _configName; }

    void setFlightName(const std::string& val) { _flightName = val; }

    const std::string& getFlightName() const;

    void addSite(Site* val);

    const std::list<Site*>& getSites() const { return _sites; }

    /**
     * Convenience function to return the maximum site number.
     */
    int getMaxSiteNumber() const { return _maxSiteNumber; }

    int getMinSiteNumber() const { return _minSiteNumber; }

    /**
     * A Project has one or more DSMServers.
     */
    void addServer(DSMServer* srvr) { _servers.push_back(srvr); }

    const std::list<DSMServer*>& getServers() const { return _servers; }

    /**
     * Look for a server for this project that either has no name or whose
     * name matches hostname.  If none found, remove any domain names
     * and try again. Then search the project Sites.
     */
    std::list<DSMServer*> findServers(const std::string& hostname) const;

    DSMServer* findServer(const nidas::util::Inet4Address& addr) const;

    /**
     * Find a DSM whose name corresponds to
     * a given IP address.
     */
    const DSMConfig* findDSM(const nidas::util::Inet4Address& addr) const;

    const DSMConfig* findDSM(const std::string& name) const;

    /**
     * Find a DSM matching id;
     */
    const DSMConfig* findDSM(unsigned int id) const;

    std::list<nidas::core::FileSet*> findSampleOutputStreamFileSets(
	const std::string& hostName) const;

    std::list<nidas::core::FileSet*> findSampleOutputStreamFileSets() const;

    DSMSensor* findSensor(dsm_sample_id_t id) const;

    DSMSensor* findSensor(const SampleTag* tag) const;

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
    dsm_sample_id_t getUniqueSampleId(unsigned int dsmid);

    void setSensorCatalog(SensorCatalog* val) { _sensorCatalog = val; }
    SensorCatalog* getSensorCatalog() const { return _sensorCatalog; }

    void setDSMCatalog(DSMCatalog* val) { _dsmCatalog = val; }
    DSMCatalog* getDSMCatalog() const { return _dsmCatalog; }

    void setServiceCatalog(ServiceCatalog* val) { _serviceCatalog = val; }
    ServiceCatalog* getServiceCatalog() const { return _serviceCatalog; }

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
    	toDOMParent(xercesc::DOMElement* parent,bool complete) const
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node,bool complete) const
    		throw(xercesc::DOMException);

    /**
     * Utility function to expand ${TOKEN} or $TOKEN fields
     * in a string.  If curly brackets are not
     * used, then the TOKEN should be delimited by a '/', a '.' or
     * the end of string, e.g.:  xxx/yyy/$ZZZ.dat
     * Token $PROJECT is replaced by getName() and $SYSTEM 
     * is replaced by getSystemName(). Other tokens are
     * looked up in the environment.
     */
    std::string expandString(std::string input) const;

    /**
     * Utility function to get the value of a token.
     * Token $PROJECT is replaced by getName() and $SYSTEM 
     * is replaced by getSystemName(). Other tokens are
     * looked up in the environment.
     * @return: token found
     */
    bool getTokenValue(const std::string& token,std::string& value) const;

protected:
    /**
     * Add a parameter to this Project. Project
     * will then own the pointer and will delete it
     * in its destructor.
     */
    void addParameter(Parameter* val)
    {
        _parameters.push_back(val);
    }

private:
    static Project* _instance;

    std::string _name;

    std::string _sysname;

    std::string _configVersion;

    /**
     * Name of XML file that this project was initialized from.
     */
    std::string _configName;

    mutable std::string _flightName;

    std::list<Site*> _sites;

    SensorCatalog* _sensorCatalog;

    DSMCatalog* _dsmCatalog;

    ServiceCatalog* _serviceCatalog;

    std::list<DSMServer*> _servers;

    mutable nidas::util::Mutex _lookupLock;

    mutable std::map<dsm_sample_id_t,const DSMConfig*> _dsmById;

    mutable nidas::util::Mutex _sensorMapLock;

    mutable std::map<dsm_sample_id_t,DSMSensor*> _sensorById;

    std::map<int,Site*> _siteByStationNumber;

    std::set<dsm_sample_id_t> _usedIds;

    dsm_sample_id_t _nextTempId;

    int _maxSiteNumber;

    int _minSiteNumber;

    /**
     * List of pointers to Parameters.
     */
    std::list<Parameter*> _parameters;

};

}}	// namespace nidas namespace core

#endif
