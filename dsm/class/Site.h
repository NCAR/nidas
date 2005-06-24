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
#include <DSMServer.h>
#include <Project.h>
#include <Parameter.h>

#include <list>
#include <map>

namespace dsm {

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
    }

    const std::list<DSMConfig*>& getDSMConfigs() const
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

    virtual const std::list<std::string> getAllowedParameterNames() const;

    /**
     * Add a parameter to this Site. Site
     * will then own the pointer and will delete it
     * in its destructor.
     */
    virtual void addParameter(Parameter* val);

    virtual const Parameter* getParameter(const std::string& name) const;

    virtual const std::list<const Parameter*>& getParameters() const;

    void fromDOMElement(const xercesc::DOMElement*)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

protected:
    /**
     * Pointer back to my project.
     */
    const Project* project;
	
    std::string name;

    std::list<DSMConfig*> dsms;

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
