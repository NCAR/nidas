/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_SENSORCATALOG_H
#define NIDAS_CORE_SENSORCATALOG_H

#include <nidas/core/DOMable.h>

#include <map>

namespace nidas { namespace core {

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
	throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
    		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
    		throw(xercesc::DOMException);

protected:

};

}}	// namespace nidas namespace core

#endif

