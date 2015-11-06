// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
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
    DOMable::toDOMParent(xercesc::DOMElement* /* parent */,bool /* brief */) const
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
    DOMable::toDOMElement(xercesc::DOMElement*,bool) const
            throw(xercesc::DOMException)
{
#if XERCES_VERSION_MAJOR < 3
    XMLStringConverter msg("toDOMParent not supported in this DOMable");
    throw xercesc::DOMException(xercesc::DOMException::NOT_SUPPORTED_ERR,msg);
#else
    throw xercesc::DOMException(xercesc::DOMException::NOT_SUPPORTED_ERR);
#endif
}
