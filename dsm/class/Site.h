/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SITE_H
#define DSM_SITE_H

#include <DOMable.h>
#include <DSMConfig.h>
#include <Parameter.h>

#include <list>
#include <map>

namespace dsm {

class Project;
class DSMServer;

/**
 * A measurement site. It could be an Aircraft, or a grouping of
 * measurement systems (e.g. "meadow" site).
 */
class Site : public DOMable {
public:
    Site();
    virtual ~Site();

    /**
     * Set the name of the Site.
     */
    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    /**
     * Identify the Site by number. The site number
     * can be used for things like a NetCDF station
     * dimension. 
     * @param Site number, -1 means no number is associated with the site.
     */
    void setNumber(int val) { number = val; }

    const int getNumber() const { return number; }

    /**
     * Set the suffix for the Site. All variable names from this
     * site will have the suffix.
     */
    void setSuffix(const std::string& val) { suffix = val; }

    const std::string& getSuffix() const { return suffix; }

    /**
     * Provide pointer to Project.
     */
    const Project* getProject() const { return project; }

    /**
     * Set the current project for this Site.
     */
    void setProject(const Project* val) { project = val; }

    /**
     * A Site contains one or more DSMs.  Site will
     * own the pointer and will delete the DSMConfig in its
     * destructor.
     */
    void addDSMConfig(DSMConfig* dsm)
    {
        dsms.push_back(dsm);
        ncDsms.push_back(dsm);
    }

    const std::list<const DSMConfig*>& getDSMConfigs() const
    {
        return dsms;
    }

    /**
     * A Site has one or more DSMServers.
     */
    void addServer(DSMServer* srvr) { servers.push_back(srvr); }

    const std::list<DSMServer*>& getServers() const { return servers; }

    /**
     * Look for a server on this aircraft that either has no name or whose
     * name matches hostname.  If none found, remove any domain names
     * and try again.
     */
    DSMServer* findServer(const std::string& hostname) const;

    /**
     * Find a DSM whose name corresponds to
     * a given IP address.
     */
    const DSMConfig* findDSM(const atdUtil::Inet4Address& addr) const;

    /**
     * Find a DSM by id.
     */
    const DSMConfig* findDSM(unsigned long id) const;

    /**
     * Find a DSMSensor by the full id, both the DSM id and the sensor id.
     */
    DSMSensor* findSensor(unsigned long id) const;

    /**
     * Initialize all sensors for a Site.
     */
    void initSensors() throw(atdUtil::IOException);

    /**
     * Initialize all sensors for a given dsm.
     */
    void initSensors(const DSMConfig* dsm) throw(atdUtil::IOException);

    virtual const std::list<std::string> getAllowedParameterNames() const;

    /**
     * Add a parameter to this Site. Site
     * will then own the pointer and will delete it
     * in its destructor.
     */
    virtual void addParameter(Parameter* val);

    virtual const Parameter* getParameter(const std::string& name) const;

    virtual const std::list<const Parameter*>& getParameters() const;

    DSMConfigIterator getDSMConfigIterator() const;

    SensorIterator getSensorIterator() const;

    SampleTagIterator getSampleTagIterator() const;

    VariableIterator getVariableIterator() const;

    void fromDOMElement(const xercesc::DOMElement*)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

protected:

    const std::list<DSMConfig*>& getncDSMConfigs() const
    {
        return ncDsms;
    }

    /**
     * Pointer back to my project.
     */
    const Project* project;
	
    std::string name;

    int number;

    std::string suffix;

    std::list<const DSMConfig*> dsms;

    std::list<DSMConfig*> ncDsms;

    std::list<DSMServer*> servers;

    std::list<std::string> allowedParameterNames;

    /**
     * Mapping of Parameters, by name.
     */
    std::map<std::string,Parameter*> parameterMap;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> constParameters;
};

}

#endif
