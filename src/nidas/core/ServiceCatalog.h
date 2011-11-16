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

#ifndef NIDAS_CORE_SERVICECATALOG_H
#define NIDAS_CORE_SERVICECATALOG_H

#include <nidas/core/DOMable.h>

#include <map>

namespace nidas { namespace core {

/**
 * A catalog of dsm DOMElements, implemented with
 * std::map, containing dsm DOMElements, keyed by
 * the ID attributes.
 */
class ServiceCatalog : public DOMable
{
public:
    ServiceCatalog();

    ServiceCatalog(const ServiceCatalog&);

    ~ServiceCatalog();

    ServiceCatalog& operator=(const ServiceCatalog&);

    xercesc::DOMElement*& operator[](const std::string& id)
    {
        return _services[id];
    }

    const xercesc::DOMElement* find(const std::string& id) const;

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);

private:

    std::map<std::string,xercesc::DOMElement*> _services;

};

}}	// namespace nidas namespace core

#endif

