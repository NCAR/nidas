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

#include <DOMable.h>
#include <SensorCatalog.h>
#include <ObsPeriod.h>

#include <list>

namespace dsm {

class Site;

/**
 */
class Project : public DOMable {
public:
    Project();
    virtual ~Project();

    static Project* getInstance();

    void setName(const std::string& val) { name = val; }
    const std::string& getName() const { return name; }

    void setCurrentObsPeriod(const ObsPeriod& val) { currentObsPeriod = val; }
    const ObsPeriod& getCurrentObsPeriod() const { return currentObsPeriod; }

    void setVersion(const std::string& val) { version = val; }
    const std::string& getVersion() const { return version; }

    void setXMLName(const std::string& val) { xmlName = val; }
    const std::string& getXMLName() const { return xmlName; }

    void addSite(Site* val) { sites.push_back(val); }
    const std::list<Site*>& getSites() const { return sites; }

    /**
     * When the Project configuration is being used
     * for a specific Site, the value of the current site
     * can be set with setCurrentSite().  Other objects can
     * then have access to the current Site via the
     * getCurrentSite() method of the Project singleton.
     * This may not be the best way to implement this -
     * perhaps we could provide access to the current Site
     * via static methods in the Site class.
     */
    void setCurrentSite(const Site* val) { currentSite = val; }

    const Site* getCurrentSite() const { return currentSite; }

    void setSensorCatalog(SensorCatalog* val) { catalog = val; }
    SensorCatalog* getSensorCatalog() const { return catalog; }

    void fromDOMElement(const xercesc::DOMElement*)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

protected:
    static Project* instance;

    std::string name;

    std::string version;

    /**
     * Name of XML file that this project was initialized from.
     */
    std::string xmlName;

    std::list<Site*> sites;

    const Site* currentSite;

    SensorCatalog* catalog;

    ObsPeriod currentObsPeriod;
};

}

#endif
