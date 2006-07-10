/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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
