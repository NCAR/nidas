/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_XMLSTRINGCONVERTER_H
#define DSM_XMLSTRINGCONVERTER_H

#include<xercesc/util/XMLString.hpp>

namespace dsm {

/**
 * Class providing conversions between char* and XMLCh*
 * using the Xerces-c transcode and release methods.
 */
class XMLStringConverter {
private:
  char *str;
  XMLCh *xstr;
  bool releaseChar;
public:

    XMLStringConverter(const XMLCh* val) :
    	str(XERCES_CPP_NAMESPACE::XMLString::transcode(val)),
	xstr((XMLCh*)val),
	releaseChar(true)
    {}

    XMLStringConverter(const char* val) :
    	str((char*)val),
	xstr(XERCES_CPP_NAMESPACE::XMLString::transcode(val)),
	releaseChar(false)
    {}

    ~XMLStringConverter() { 
  	if (releaseChar) XERCES_CPP_NAMESPACE::XMLString::release(&str);
  	else XERCES_CPP_NAMESPACE::XMLString::release(&xstr);
    }

    /**
     * Conversion to const char*
     */
    operator const char*() const { return str; }
    operator const XMLCh*() const { return xstr; }
};

inline std::ostream& operator<<(std::ostream& target,
	const XMLStringConverter& toDump)
{
    target << (const char*)toDump;
    return target;
}

}

#endif
