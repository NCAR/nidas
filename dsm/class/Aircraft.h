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

#include <list>

namespace dsm {

/**
 * Here it is - a class which completely describes an aircraft!
 */
class Aircraft : public DOMable {
public:
    Aircraft();
    virtual ~Aircraft();

    void addDSMConfig(DSMConfig* dsm) { dsms.push_back(dsm); }
    const std::list<DSMConfig*> getDSMConfigs() const { return dsms; }

    void fromDOMElement(const xercesc::DOMElement*)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

protected:
    std::list<DSMConfig*> dsms;

};

}

#endif
