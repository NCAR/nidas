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

#ifndef NIDAS_CORE_XMLSTRINGCONVERTER_H
#define NIDAS_CORE_XMLSTRINGCONVERTER_H

#include<xercesc/util/XMLString.hpp>

namespace nidas { namespace core {

/**
 * Class providing conversions between string and XMLCh*
 * using the Xerces-c transcode and release methods.
 */
class XMLStringConverter {
public:

    XMLStringConverter(const XMLCh* val) :
	_str(),_cxstr(val),_xstr(0)
    {
        char* cstr = xercesc::XMLString::transcode(val);
        _str = std::string(cstr ? cstr : "");
  	xercesc::XMLString::release(&cstr);
    }

    XMLStringConverter(const char* val) :
    	_str(val),_cxstr(0),
	_xstr(xercesc::XMLString::transcode(val))
    {
        _cxstr = _xstr;
    }

    XMLStringConverter(const std::string& val) :
    	_str(val),_cxstr(0),
	_xstr(xercesc::XMLString::transcode(val.c_str()))
    {
        _cxstr = _xstr;
    }

    ~XMLStringConverter() { 
  	if (_xstr) xercesc::XMLString::release(&_xstr);
    }

    /**
     * Conversion to const XMLCh*
     */
    operator const XMLCh*() const { return _cxstr; }

    /**
     * Conversion to string.
     */
    operator std::string() const
    {
        return _str;
    }

private:
    std::string _str;
    const XMLCh *_cxstr;
    XMLCh *_xstr;

    /** No copying */
    XMLStringConverter(const XMLStringConverter&);

    /** No assignment */
    XMLStringConverter& operator=(const XMLStringConverter&);

};

inline std::ostream& operator<<(std::ostream& target,
	const XMLStringConverter& toDump)
{
    target << (std::string)toDump;
    return target;
}

}}	// namespace nidas namespace core

#endif
