/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_PROJECT_H
#define DSM_PROJECT_H

#include <DOMable.h>
#include <Aircraft.h>
#include <SensorCatalog.h>

#include <list>

namespace dsm {

/**
 */
class Project : public DOMable {
public:
    Project();
    virtual ~Project();

    static Project* getInstance();

    void addAircraft(Aircraft* val) { aircraft.push_back(val); }
    const std::list<Aircraft*>& getAircraft() const { return aircraft; }

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
    std::list<Aircraft*> aircraft;
    SensorCatalog* catalog;
};

}

#endif
