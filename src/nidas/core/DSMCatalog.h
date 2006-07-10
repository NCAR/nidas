/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-22 12:05:21 -0700 (Mon, 22 May 2006) $

    $LastChangedRevision: 3362 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/core/DSMCatalog.h $
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

