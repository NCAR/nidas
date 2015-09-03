// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_XMLEXCEPTION_H
#define NIDAS_CORE_XMLEXCEPTION_H

#include <string>
#include <nidas/util/Exception.h>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/sax/SAXException.hpp>

namespace nidas { namespace core {

/**
 * Exception which can be built from an xerces::XMLException,
 * xercesc::SAXException, or xercesc::DOMException.
 * Simplifies XML exception handling.
 */
class XMLException : public nidas::util::Exception
{

  public:
 
    /**
     * Create an XMLException from a string;
     */
    XMLException(const std::string& msg):
    	nidas::util::Exception("XMLException",msg) 
    {
    }

    /**
     * Create an XMLException from a xercesc::XMLException.
     */
    XMLException(const xercesc::XMLException& e);

    /**
     * Create an XMLException from a xercesc::SAXException.
     */
    XMLException(const xercesc::SAXException& e);

    /**
     * Create an XMLException from a xercesc::DOMException.
     */
    XMLException(const xercesc::DOMException& e);

    /**
     * clone myself (a "virtual" constructor).
     */
    XMLException* clone() const {
	return new XMLException(*this);
    }
};

}}	// namespace nidas namespace core

#endif
