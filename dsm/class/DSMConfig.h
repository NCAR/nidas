/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_DSMCONFIG_H
#define DSM_DSMCONFIG_H

#include <DOMable.h>

namespace dsm {

/**
 * Class that should include all that is configurable about a
 * DSM.  It should be able to initialize itself from a
 * <dsm> XML element, and provide get methods to access
 * its essential pieces, like sensors.
 */
class DSMConfig : public DOMable {
public:
    DSMConfig();
    virtual ~DSMConfig();

    void fromDOMElement(const XERCES_CPP_NAMESPACE::DOMElement*)
	throw(atdUtil::InvalidParameterException);

    XERCES_CPP_NAMESPACE::DOMElement*
    	toDOMParent(XERCES_CPP_NAMESPACE::DOMElement* parent)
    		throw(XERCES_CPP_NAMESPACE::DOMException);

    XERCES_CPP_NAMESPACE::DOMElement*
    	toDOMElement(XERCES_CPP_NAMESPACE::DOMElement* node)
    		throw(XERCES_CPP_NAMESPACE::DOMException);

protected:

};

}

#endif
