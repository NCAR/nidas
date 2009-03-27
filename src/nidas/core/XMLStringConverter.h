/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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
private:
    std::string _str;
    const XMLCh *_cxstr;
    XMLCh *_xstr;
public:

    XMLStringConverter(const XMLCh* val) :
	_cxstr(val),_xstr(0)
    {
        char* cstr = xercesc::XMLString::transcode(val);
        _str = std::string(cstr ? cstr : "");
  	xercesc::XMLString::release(&cstr);
    }

    XMLStringConverter(const char* val) :
    	_str(val),
	_xstr(xercesc::XMLString::transcode(val))
    {
        _cxstr = _xstr;
    }

    XMLStringConverter(const std::string& val) :
    	_str(val),
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
};

inline std::ostream& operator<<(std::ostream& target,
	const XMLStringConverter& toDump)
{
    target << (std::string)toDump;
    return target;
}

}}	// namespace nidas namespace core

#endif
