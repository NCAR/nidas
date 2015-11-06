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

    const std::map<std::string,xercesc::DOMElement*>& getMap() const 
        {return _sensors;}

private:

    std::map<std::string,xercesc::DOMElement*> _sensors;

};

}}	// namespace nidas namespace core

#endif

