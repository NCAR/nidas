/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_XMLCONFIGPARSER_H
#define DSM_XMLCONFIGPARSER_H

#ifdef NEED_ALL_THESE_INCLUDES
                                                                                
// ---------------------------------------------------------------------------
//  Includes
// ---------------------------------------------------------------------------
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMError.hpp>
#include <xercesc/dom/DOMLocator.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/dom/DOMAttr.hpp>
                                                                                
#include "DOMCount.hpp"
#include <string.h>
#include <stdlib.h>

#endif
                                                                                
#include <string>

// #include <xercesc/parsers/AbstractDOMParser.hpp>
#include <xercesc/dom/DOMBuilder.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>

namespace dsm {

/**
 * Wrapper class around xerces-c DOMBuilder to parse XML.
 */
class XMLConfigParser {
public:

    XMLConfigParser() throw(xercesc::DOMException);
    ~XMLConfigParser();

    xercesc::DOMDocument* parse(const std::string& xmlFile)
    	throw(xercesc::XMLException,
		xercesc::DOMException);


protected:
    
    xercesc::DOMImplementation *impl;
    xercesc::DOMBuilder *parser;
    xercesc::DOMDocument* doc;

};

class XMLConfigErrorHandler : public xercesc::DOMErrorHandler
{
    public:
    // -----------------------
    //  Constructors and Destructor
    // ----------------------
    XMLConfigErrorHandler();
    ~XMLConfigErrorHandler();

    // --------------------------
    //  Implementation of the DOM ErrorHandler interface
    // -------------------------
    bool handleError(const xercesc::DOMError& domError);
    void resetErrors();


    private :
    // -----------------------------
    //  Unimplemented constructors and operators
    // ----------------------------
    XMLConfigErrorHandler(const XMLConfigErrorHandler&);
    void operator=(const XMLConfigErrorHandler&);
};

}

#endif

