/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

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

    void setName(const std::string& val) { name = val; }
    const std::string& getName() const { return name; }

    /**
     * Provide pointer to Project.
     */
    Project* getProject() { return project; }

    void setProject(Project* val) { project = val; }

    void addDSMConfig(DSMConfig* dsm) { dsms.push_back(dsm); }
    const std::list<DSMConfig*>& getDSMConfigs() const { return dsms; }

    void addServer(DSMServer* srvr) { servers.push_back(srvr); }
    const std::list<DSMServer*>& getServers() const { return servers; }

    /**
     * Look for a server on this aircraft that either has no name or whose
     * name matches hostname.  If none found, remove any domain names
     * and try again.
     */
    DSMServer* findServer(const std::string& hostname) const;

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
     * Pointer back to my project. Aircraft does not own this
     * pointer, just is able to pass it along to anyone who
     * wants to know general project info.
     */
    Project* project;
	
    std::string name;

    std::list<DSMConfig*> dsms;
    std::list<DSMServer*> servers;
};

}

#endif
