// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
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

#ifndef NIDAS_CORE_PROJECT_H
#define NIDAS_CORE_PROJECT_H

#include "DOMable.h"
#include "Sample.h"
#include "NidsIterators.h"
#include "Dictionary.h"
#include "Datasets.h"
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/Inet4Address.h>
#include <nidas/util/IOException.h>

#include <list>
#include <set>
#include <map>

#define ACCESS_AS_SINGLETON

namespace nidas { namespace core {

class Site;
class DSMServer;
class DSMConfig;
class DSMSensor;
class SampleTag;
class SensorCatalog;
class DSMCatalog;
class ServiceCatalog;
class FileSet;
class Parameter;

/**
 */
class Project : public DOMable {
public:

    Project();

    virtual ~Project();

    /**
     * Convenient method to fill this Project instance from the
     * configuration in the XML file at @p xmlfilepath, using the
     * nidas::core::parseXMLConfigFile() function.
     *
     * @throws nidas::core::XMLException
     **/
    void
    parseXMLConfigFile(const std::string& xmlfilepath);

#ifdef ACCESS_AS_SINGLETON
    /**
     * Project is a singleton.
     */
    static Project* getInstance();

    /**
     * Destory the singleton.
     */
    static void destroyInstance();
#endif

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

    /**
     * Find a DSMConfig in any site in this project whose name matches the
     * given @p hostname.  If the hostname is fully qualified, then the
     * match is attempted against the long name first and then the
     * shortened name, with everything past the first '.' removed.
     *
     * If there is only one DSM in the project config, then that DSM
     * is always returned.
     *
     * DSMEngine uses this method to look up it's DSMConfig instance on
     * startup, based on the current hostname setting.
     **/
    const DSMConfig* findDSM(const std::string& name) const;

    /**
     * Find a DSM matching id;
     */
    const DSMConfig* findDSM(unsigned int id) const;

    /**
     * Find SampleOutputStreamFileSets belonging to SampleArchivers
     * of DSMServers whose name matches the argument name.
     * If no DSMServer names exactly match, then return the filesets
     * corresponding to a DSMServer with an empty name.
     */
    std::list<nidas::core::FileSet*> findServerSampleOutputStreamFileSets(const std::string& name) const;

    /**
     * Call findServerSampleOutputStreamFileSets(name) passing
     * the nodename returned by uname(2).
     *
     * @throws nidas::util::Exception
     */
    std::list<nidas::core::FileSet*> findServerSampleOutputStreamFileSets() const;

    /**
     * Find SampleOutputStreamFileSets belonging to the given DSM.
     */
    std::list<nidas::core::FileSet*> findSampleOutputStreamFileSets(
	const std::string& dsmName) const;

    /**
     * Find SampleOutputStreamFileSets of all DSMs.
     */
    std::list<nidas::core::FileSet*> findSampleOutputStreamFileSets() const;

    DSMSensor* findSensor(dsm_sample_id_t id) const;

    DSMSensor* findSensor(const SampleTag* tag) const;

    /**
     * Find a Site with the given station number.
     */
    Site* findSite(int stationNumber) const;

    /**
     * Find a Site with the given name.
     */
    Site* findSite(const std::string& name) const;

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
     *
     * @throws nidas::util::IOException
     */
    void initSensors();

    /**
     * Initialize all sensors for a given Site.
     *
     * @throws nidas::util::IOException
     */
    void initSensors(Site* site);

    /**
     * Initialize all sensors for a given dsm.
     *
     * @throws nidas::util::IOException
     */
    void initSensors(DSMConfig* dsm);

    const Parameter* getParameter(const std::string& name) const;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement*);

    /**
     * @throws xercesc::DOMException
     **/
    xercesc::DOMElement*
    toDOMParent(xercesc::DOMElement* parent,bool complete) const;

    /**
     * @throws xercesc::DOMException
     **/
    xercesc::DOMElement*
    toDOMElement(xercesc::DOMElement* node,bool complete) const;

    /**
     * Utility function to expand ${TOKEN} or $TOKEN fields
     * in a string with their value from getTokenValue().
     * If curly brackets are not used, then the TOKEN should
     * be delimited by a '/', a '.' or the end of string,
     * e.g.:  xxx/yyy/$ZZZ.dat
     */
    std::string expandString(const std::string& input) const
    {
        return _dictionary.expandString(input);
    }

    /**
     * Implement a lookup for tokens that I know about, like $PROJECT,
     * and $SYSTEM.  For other tokens, look them up in the process
     * environment.
     */
    bool getTokenValue(const std::string& token,std::string& value) const
    {
        return _dictionary.getTokenValue(token,value);
    }

    const Dictionary& getDictionary() const
    {
        return _dictionary;
    }

    void setDataset(const Dataset& val)
    {
        _dataset = val;
    }

    const Dataset& getDataset() const
    {
        return _dataset;
    }

    /**
     * When true, autoconfig elements in the config document are removed
     * before realizing the Project with fromDOMElement().
     **/
    void
    disableAutoconfig(bool disable)
    {
        _disableAutoconfig = disable;
    }

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

    /**
     * Scan the DOM and pull out any <autoconfig> tags, as well as change
     * the autoconfig classes back to DSMSerialSensor, or isff.PropVane as
     * needed, and remove porttype attributes.
     */
    void removeAutoConfig(xercesc::DOMNode* node, bool bumpRecursion=false);

#ifdef ACCESS_AS_SINGLETON
    static Project* _instance;
#endif

    std::string _name;

    std::string _sysname;

    std::string _configVersion;

    /**
     * Name of XML file that this project was initialized from.
     */
    std::string _configName;

    mutable std::string _flightName;

    class MyDictionary : public Dictionary {
    public:
        MyDictionary(Project* project): _project(project) {}
        MyDictionary(const MyDictionary& x): Dictionary(),_project(x._project) {}
        MyDictionary& operator=(const MyDictionary& rhs)
        {
            if (&rhs != this) {
                *(Dictionary*)this = rhs;
                _project = rhs._project;
            }
            return *this;
        }
        bool getTokenValue(const std::string& token, std::string& value) const;
    private:
        Project* _project;
    } _dictionary;

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

    std::map<std::string,Site*> _siteByName;

    std::set<dsm_sample_id_t> _usedIds;

    int _maxSiteNumber;

    int _minSiteNumber;

    /**
     * List of pointers to Parameters.
     */
    std::list<Parameter*> _parameters;

    /**
     * The current dataset.
     */
    Dataset _dataset;

    bool _disableAutoconfig;

    /**
     * Copy not supported. The main problem with a supporting
     * a copy constructor is that one would need a clone
     * for all the DSMSensors, which would be hard
     * to get and keep correct. Instead of copying/cloning
     * a project, create a new one and initialize it from
     * a DOMElement.
     */
    Project(const Project&);

    /**
     * Assignment not supported. See comments about copy constructor.
     */
    Project& operator=(const Project&);

};

}}	// namespace nidas namespace core

#endif
