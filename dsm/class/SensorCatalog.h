/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_SENSORCATALOG_H
#define DSM_SENSORCATALOG_H

#include <DOMable.h>

#include <map>

namespace dsm {

/**
 * A catalog of sensor DOMElements, implemented with
 * std::map, containing sensor DOMElements, keyed by
 * the ID attributes.
 */
class SensorCatalog : public DOMable,
	public std::map<std::string,xercesc::DOMElement*> {
public:
    SensorCatalog();
    virtual ~SensorCatalog();

    void fromDOMElement(const xercesc::DOMElement*)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

protected:

};

}

#endif

