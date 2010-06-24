/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#include <nidas/core/DOMable.h>

#include<xercesc/util/XMLString.hpp>

using namespace nidas::core;

/* static */
XMLCh* DOMable::namespaceURI = 0;

/**
 * Create a DOMElement and append it to the parent.
 */
xercesc::DOMElement*
    DOMable::toDOMParent(xercesc::DOMElement* parent,bool brief) const
            throw(xercesc::DOMException)
{

#if XERCES_VERSION_MAJOR < 3
    XMLStringConverter msg("toDOMParent not supported in this DOMable");
    throw xercesc::DOMException(xercesc::DOMException::NOT_SUPPORTED_ERR,msg);
#else
    throw xercesc::DOMException(xercesc::DOMException::NOT_SUPPORTED_ERR);
#endif

/*
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("name"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
*/
}

/**
 * Add my content into a DOMElement.
 */
xercesc::DOMElement*
    DOMable::toDOMElement(xercesc::DOMElement* node,bool brief) const
            throw(xercesc::DOMException)
{
#if XERCES_VERSION_MAJOR < 3
    XMLStringConverter msg("toDOMParent not supported in this DOMable");
    throw xercesc::DOMException(xercesc::DOMException::NOT_SUPPORTED_ERR,msg);
#else
    throw xercesc::DOMException(xercesc::DOMException::NOT_SUPPORTED_ERR);
#endif
}
