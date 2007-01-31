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
 * A catalog of dsm DOMElements, implemented with
 * std::map, containing dsm DOMElements, keyed by
 * the ID attributes.
 */
class DSMCatalog : public DOMable,
	public std::map<std::string,xercesc::DOMElement*> {
public:
    DSMCatalog();
    virtual ~DSMCatalog();

    void fromDOMElement(const xercesc::DOMElement*)
	throw(nidas::util::InvalidParameterException);

protected:

};

}}	// namespace nidas namespace core

#endif

