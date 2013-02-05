// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_SITE_H
#define NIDAS_CORE_SITE_H

#include <nidas/core/DOMable.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/Parameter.h>
#include <nidas/core/Dictionary.h>

#include <list>
#include <map>

namespace nidas { namespace core {

class Project;
class DSMServer;
class DSMConfig;
class DSMSensor;

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
    void setName(const std::string& val) { _name = val; }

    const std::string& getName() const { return _name; }

    /**
     * Identify the Site by number. The site number
     * can be used for things like a NetCDF station
     * dimension. 
     * @param Site number, 0 means no number is associated with the site.
     */
    void setNumber(int val) { _number = val; }

    int getNumber() const { return _number; }

    /**
     * Equivalence operator for Site, checks name.
     */
    bool operator == (const Site& x) const
    {
        if (this == &x) return true;
        return _name == x._name;
    }

    /**
     * Non-equivalence operator for Site.
     */
    bool operator != (const Site& x) const
    {
        return !operator == (x);
    }

    /**
     * Less than operator for Site, compares the names.
     */
    bool operator < (const Site& x) const
    {
        if (operator == (x)) return false;
        return _name.compare(x._name) < 0;
    }

    /**
     * Set the suffix for the Site. All variable names from this
     * site will have the suffix.
     */
    void setSuffix(const std::string& val) { _suffix = val; }

    const std::string& getSuffix() const { return _suffix; }

    /**
     * Provide pointer to Project.
     */
    const Project* getProject() const { return _project; }

    /**
     * Set the current project for this Site.
     */
    void setProject(const Project* val) { _project = val; }

    /**
     * A Site contains one or more DSMs.  Site will
     * own the pointer and will delete the DSMConfig in its
     * destructor.
     */
    void addDSMConfig(DSMConfig* dsm)
    {
        _dsms.push_back(dsm);
        _ncDsms.push_back(dsm);
    }

    void removeDSMConfig(DSMConfig* dsm)
    {
        std::list<const DSMConfig*>::iterator di;
        for (di = _dsms.begin(); di != _dsms.end(); ) 
            if (dsm == *di) di = _dsms.erase(di);
            else ++di;
        for (std::list<DSMConfig*>::iterator di = _ncDsms.begin();
             di != _ncDsms.end(); )
            if (dsm == *di) {
                DSMConfig* deletableDSMConfig =  *di;
                di = _ncDsms.erase(di);
                // The DSM configuration has been removed from both lists, now delete the object.
                delete deletableDSMConfig;
            }
            else ++di;
    }

    const std::list<const DSMConfig*>& getDSMConfigs() const
    {
        return _dsms;
    }

    /**
     * A Site has one or more DSMServers.
     */
    void addServer(DSMServer* srvr) { _servers.push_back(srvr); }

    const std::list<DSMServer*>& getServers() const { return _servers; }

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
    const DSMConfig* findDSM(const nidas::util::Inet4Address& addr) const;

    /**
     * Find a DSM by id.
     */
    const DSMConfig* findDSM(unsigned int id) const;

    /**
     * Find a DSM by name.
     */
    const DSMConfig* findDSM(const std::string& name) const;

    /**
     * Find a DSMSensor by the full id, both the DSM id and the sensor id.
     */
    DSMSensor* findSensor(unsigned int id) const;

    /**
     * Initialize all sensors for a Site.
     */
    void initSensors() throw(nidas::util::IOException);

    /**
     * Initialize all sensors for a given dsm.
     */
    void initSensors(const DSMConfig* dsm) throw(nidas::util::IOException);

    /**
     * Add a parameter to this Site. Site
     * will then own the pointer and will delete it
     * in its destructor.
     */
    virtual void addParameter(Parameter* val);

    virtual const Parameter* getParameter(const std::string& name) const;

    virtual const std::list<const Parameter*>& getParameters() const;

    /**
     * Do we want DSMSensor::process methods at this site to apply
     * variable conversions?  Currently on raf.Aircraft we don't
     * want process methods to apply the conversions.
     */
    virtual bool getApplyVariableConversions() const
    {
        return _applyCals;
    }

    /**
     * Utility function to expand ${TOKEN} or $TOKEN fields
     * in a string.
     */
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
     * Implement a lookup for tokens that I know about, like $SITE, and
     * $AIRCRAFT.  For other tokens, call getProject()->getTokenValue(token,value);
     */
    bool getTokenValue(const std::string& token,std::string& value) const
    {
        return _dictionary.getTokenValue(token,value);
    }

    const Dictionary& getDictionary() const
    {
        return _dictionary;
    }

    DSMServerIterator getDSMServerIterator() const;

    DSMServiceIterator getDSMServiceIterator() const;

    ProcessorIterator getProcessorIterator() const;

    DSMConfigIterator getDSMConfigIterator() const;

    SensorIterator getSensorIterator() const;

    SampleTagIterator getSampleTagIterator() const;

    VariableIterator getVariableIterator() const;

    void validate()
	throw(nidas::util::InvalidParameterException);

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent,bool complete) const
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node,bool complete) const
    		throw(xercesc::DOMException);

private:

    const std::list<DSMConfig*>& getncDSMConfigs() const
    {
        return _ncDsms;
    }

    /**
     * Pointer back to my project.
     */
    const Project* _project;
	
    std::string _name;

    int _number;

    std::string _suffix;

    class MyDictionary : public Dictionary {
    public:
        MyDictionary(Site* site): _site(site) {}
        MyDictionary(const MyDictionary& x): Dictionary(),_site(x._site) {}
        MyDictionary& operator=(const MyDictionary& rhs)
        {
            if (&rhs != this) {
                *(Dictionary*) this = rhs;
                _site = rhs._site;
            }
            return *this;
        }
        bool getTokenValue(const std::string& token, std::string& value) const;
    private:
        Site* _site;
    } _dictionary;

    std::list<const DSMConfig*> _dsms;

    std::list<DSMConfig*> _ncDsms;

    std::list<DSMServer*> _servers;

    /**
     * Mapping of Parameters, by name.
     */
    std::map<std::string,Parameter*> _parameterMap;

    /**
     * List of const pointers to Parameters for providing via
     * getParameters().
     */
    std::list<const Parameter*> _constParameters;

    /**
     * Copy not supported. See Project copy constructor: Project(const Project&);
     */
    Site(const Site&);

    /**
     * Assignment not supported.
     */
    Site& operator=(const Site&);

protected:
    /**
     * Should NIDAS apply calibrations, or defer them to other processing.
     */
    bool _applyCals;

};

}}	// namespace nidas namespace core

#endif
