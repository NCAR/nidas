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

    void setVersion(const std::string& val) { version = val; }
    const std::string& getVersion() const { return version; }

    void setXMLName(const std::string& val) { xmlName = val; }
    const std::string& getXMLName() const { return xmlName; }

    void addSite(Site* val) { sites.push_back(val); }
    const std::list<Site*>& getSites() const { return sites; }

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

    SensorCatalog* catalog;
};

}

#endif
