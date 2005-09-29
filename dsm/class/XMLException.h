/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_XMLEXCEPTION_H
#define DSM_XMLEXCEPTION_H

#include <string>
#include <atdUtil/Exception.h>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/sax/SAXException.hpp>

namespace dsm {

/**
 * Exception which can be built from an xerces::XMLException,
 * xercesc::SAXException, or xercesc::DOMException.
 * Simplifies XML exception handling.
 */
class XMLException : public atdUtil::Exception {

  public:
 
    /**
     * Create an XMLException from a string;
     */
    XMLException(const std::string& msg):
    	atdUtil::Exception("XMLException",msg) 
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
}

#endif
