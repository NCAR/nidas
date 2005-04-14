/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_AIRCRAFT_H
#define DSM_AIRCRAFT_H

#include <DOMable.h>
#include <DSMConfig.h>
#include <DSMServer.h>
#include <Project.h>

#include <list>

namespace dsm {

/**
 * Here it is - a class which completely describes an aircraft!
 */
class Aircraft : public DOMable {
public:
    Aircraft();
    virtual ~Aircraft();

    /**
     * Set the name of the Aircraft.
     */
    void setName(const std::string& val) { name = val; }

    const std::string& getName() const { return name; }

    /**
     * Provide pointer to Project.
     */
    const Project* getProject() const { return project; }

    /**
     * Set the current project for this Aircraft.
     */
    void setProject(const Project* val) { project = val; }

    /**
     * An Aircraft contains one or more DSMs. 
     */
    void addDSMConfig(DSMConfig* dsm) { dsms.push_back(dsm); }

    const std::list<DSMConfig*>& getDSMConfigs() const { return dsms; }

    /**
     * An Aircraft has one or more DSMServers.
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
};

}

#endif
