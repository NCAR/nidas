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

#ifndef NIDAS_CORE_DSMCATALOG_H
#define NIDAS_CORE_DSMCATALOG_H

#include <nidas/core/DOMable.h>

#include <map>

namespace nidas { namespace core {

/**
 * A catalog of DSM DOMElements, implemented with
 * std::map, containing dsm DOMElements, keyed by
 * the ID attributes.
 */
class DSMCatalog : public DOMable
{
public:
    DSMCatalog();

    DSMCatalog(const DSMCatalog&);

    ~DSMCatalog();

    DSMCatalog& operator=(const DSMCatalog&);

    /**
     * Get or set a DOMElement in this catalog by the DSM name.
     * This catalog does not own the DOMElement.
     */
    xercesc::DOMElement* & operator[](const std::string& id)
    {
        return _dsms[id];
    }

    const xercesc::DOMElement* find(const std::string& id) const;

    /**
     * Build this DSMCatalog from a catalog element.
     * The DSMCatalog does not own the DOM elements.
     */
    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);

private:

    std::map<std::string,xercesc::DOMElement*> _dsms;
};

}}	// namespace nidas namespace core

#endif

