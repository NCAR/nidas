/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_PROJECT_H
#define DSM_PROJECT_H

#include <Site.h>
#include <DSMServer.h>
#include <DOMable.h>
#include <SensorCatalog.h>
#include <ObsPeriod.h>

#include <atdUtil/ThreadSupport.h>

#include <list>

namespace dsm {

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

    void setCurrentObsPeriod(const ObsPeriod& val) { currentObsPeriod = val; }
    const ObsPeriod& getCurrentObsPeriod() const { return currentObsPeriod; }

    void setVersion(const std::string& val) { version = val; }
    const std::string& getVersion() const { return version; }

    void setXMLName(const std::string& val) { xmlName = val; }
    const std::string& getXMLName() const { return xmlName; }

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
    DSMServer* findServer(const std::string& hostname) const;

    /**
     * Find a DSM whose name corresponds to
     * a given IP address.
     */
    const DSMConfig* findDSM(const atdUtil::Inet4Address& addr) const;

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

    void setSensorCatalog(SensorCatalog* val) { catalog = val; }
    SensorCatalog* getSensorCatalog() const { return catalog; }

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
    void initSensors() throw(atdUtil::IOException);

    /**
     * Initialize all sensors for a given Site.
     */
    void initSensors(const Site* site) throw(atdUtil::IOException);

    /**
     * Initialize all sensors for a given dsm.
     */
    void initSensors(const DSMConfig* dsm) throw(atdUtil::IOException);

    void fromDOMElement(const xercesc::DOMElement*)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

    /**
     * Create the full path name of a configuration file from
     * pieces. The path name is created by simply putting
     * a '/' between the value of each of the arguments:
     *      root/projectsDir/project/site/siteSubDir/obsPeriod/fileName
     *
     * This supports configurations in a tree structure looking like so
     * (where directories have a trailing slash):
     *
     *	/root/
     *	  projects/
     *      PROJECT1/
     *        aircraft1/
     *          flights/
     *            flight1/
     *              config.xml
     *            flight2/
     *              config.xml
     *        valley_site_1/
     *          configs/
     *            test/
     *              met.xml
     *            may_june/
     *              met.xml
     *      PROJECT2/
     *        aircraft1/
     *	    ...
     *
     *
     * If root,project,site or obsPeriod begin with a dollar sign,
     * they are treated as environment variables, and their
     * value is fetched from the process environment.
     * If they are not found in the environment, an
     * InvalidParameterException is thrown.
     *
     * @param root Root path of configuration tree, typically
     *    a directory path starting with a '/', like
     *	  "/home/data_sys", or an environment variable
     *    like  $DATASYS_CONFIG, with a value of "/home/data_sys".
     * @param projectsDir Directory path under root where
     *    project configuration files are stored, typically
     *	  something like "projects".
     * @param project Name of project.
     * @param site Name of measurement site, like an aircraft name,
     *    or surface measurement site.
     * @param siteSubDir Directory under the site name where
     *    config files for the observation periods are kept,
     *    like "flights", or "configs".
     * @param obsPeriod  Name of observation period, like
     *    "flight1", "joes_calibration" or "operations".
     * @param fileName  Finally, the name of the file, e.g. "config.xml".
     *     
     */
    static std::string getConfigName(const std::string& root,
    	const std::string& projectsDir,
    	const std::string& project, const std::string& site,
	const std::string& siteSubDir,const std::string& obsPeriod,
	const std::string& fileName)
	throw(atdUtil::InvalidParameterException);

protected:
    static Project* instance;

    std::string name;

    std::string sysname;

    std::string version;

    /**
     * Name of XML file that this project was initialized from.
     */
    std::string xmlName;

    std::list<Site*> sites;

    const Site* currentSite;

    SensorCatalog* catalog;

    ObsPeriod currentObsPeriod;

    std::list<DSMServer*> servers;

    mutable atdUtil::Mutex lookupLock;

    mutable std::map<dsm_sample_id_t,const DSMConfig*> dsmById;

    mutable atdUtil::Mutex sensorMapLock;

    mutable std::map<dsm_sample_id_t,DSMSensor*> sensorById;

    std::map<int,Site*> siteByStationNumber;

    std::set<dsm_sample_id_t> usedIds;

    dsm_sample_id_t nextTempId;

    int maxSiteNumber;

    int minSiteNumber;

};

}

#endif
