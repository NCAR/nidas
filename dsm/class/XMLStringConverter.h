/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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
    	str(xercesc::XMLString::transcode(val)),
	xstr((XMLCh*)val),
	releaseChar(true)
    {}

    XMLStringConverter(const char* val) :
    	str(new char[strlen(val) + 1]),
	xstr(xercesc::XMLString::transcode(val)),
	releaseChar(false)
    {
        strcpy(str,val);
    }

    XMLStringConverter(const std::string& val) :
    	str(new char[strlen(val.c_str()) + 1]),
	xstr(xercesc::XMLString::transcode(val.c_str())),
	releaseChar(false)
    {
        strcpy(str,val.c_str());
    }

    ~XMLStringConverter() { 
  	if (releaseChar) xercesc::XMLString::release(&str);
	else {
	    delete [] str;
	    xercesc::XMLString::release(&xstr);
	}
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
