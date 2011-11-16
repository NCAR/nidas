// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
class SensorCatalog : public DOMable
{
public:
    SensorCatalog();

    SensorCatalog(const SensorCatalog&);

    ~SensorCatalog();

    SensorCatalog& operator=(const SensorCatalog&);

    xercesc::DOMElement*& operator[](const std::string& id)
    {
        return _sensors[id];
    }

    const xercesc::DOMElement* find(const std::string& id) const;

    /**
     * Build this SensorCatalog from a catalog element.
     * The SensorCatalog does not own the DOM elements.
     */
    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);

private:

    std::map<std::string,xercesc::DOMElement*> _sensors;

};

}}	// namespace nidas namespace core

#endif

