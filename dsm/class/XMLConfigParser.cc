/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#ifdef NEED_ALL_THESE_INCLUDES

// ---------------------------------------------------------------------------
//  Includes
// ---------------------------------------------------------------------------
#include <xercesc/parsers/AbstractDOMParser.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMBuilder.hpp>
#include <xercesc/dom/DOMException.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMError.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/dom/DOMAttr.hpp>

#include <string.h>
#include <stdlib.h>

#endif

#include <XMLConfigParser.h>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMLocator.hpp>

#include <XMLStringConverter.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

XMLConfigParser::XMLConfigParser() 
    throw (DOMException)
{

    XMLPlatformUtils::Initialize();	// no exceptions thrown

    // Instantiate the DOM parser.

    // "LS" is the Load and Save feature.
    // See: http://www.w3.org/TR/DOM-Level-3-LS/load-save.html
    // no exceptions thrown, but may return null if no implementation
    static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };
    impl = DOMImplementationRegistry::getDOMImplementation(gLS);
    
    // Two kinds of builders: MODE_SYNCHRONOUS and MODE_ASYNCHRONOUS
    // ASYNC: The parseURI method returns null because the doc isn't
    //    necessarily parsed yet.  One registers for "load" and
    //	  "progress" events.
    // SYNC: We'll use this method. The parseURI method returns a fully
    //    parsed doc.
    // See: http://xml.apache.org/xerces-c/apiDocs/classDOMImplementationLS.html
    //
    // throws DOMException
    parser = ((DOMImplementationLS*)impl)->createDOMBuilder(
		DOMImplementationLS::MODE_SYNCHRONOUS, 0);
    cerr << "ctor done" << endl;
}

XMLConfigParser::~XMLConfigParser() 
{
    //
    //  Delete the parser itself.  Must be done prior to calling Terminate,
    //  below.
    //

    cerr << "release" << endl;
    parser->release();

    // cerr << "delete parser" << endl;
    // delete parser;

    // cerr << "delete impl" << endl;
    // delete impl;

    cerr << "Terminate" << endl;
    // And call the termination method
    XMLPlatformUtils::Terminate();
}


DOMDocument* XMLConfigParser::parse(const string& xmlFile) 
    throw (XMLException, DOMException)
{

    parser->setFeature(XMLUni::fgDOMNamespaces, true);
    parser->setFeature(XMLUni::fgXercesSchema, true);
    parser->setFeature(XMLUni::fgXercesSchemaFullChecking, true);
    parser->setFeature(XMLUni::fgDOMValidateIfSchema, true);

    // enable datatype normalization - default is off
    parser->setFeature(XMLUni::fgDOMDatatypeNormalization, true);

    // And create our error handler and install it
    XMLConfigErrorHandler errorHandler;
    parser->setErrorHandler(&errorHandler);

    //reset error count first
    errorHandler.resetErrors();

    /* 
	If the DOMBuilder is a synchronous DOMBuilder the newly
	created and populated DOMDocument is returned. If the
	DOMBuilder is asynchronous then null is returned since
	the document object is not yet parsed when this method returns. 
     */
    // throws XMLException, DOMException, SAXException
    doc = parser->parseURI(
    	(const XMLCh*)XMLStringConverter(xmlFile.c_str()));

    // check errors

    XMLCh xa[] = {chAsterisk, chNull};
    std::cerr << "length=" << doc->getElementsByTagName(xa)->getLength() <<
    	std::endl;

    return doc;
}

XMLConfigErrorHandler::XMLConfigErrorHandler()
{
}

XMLConfigErrorHandler::~XMLConfigErrorHandler()
{
}


// ---------------------------------------------------------------------------
//  XMLConfigErrorHandler interface
// ---------------------------------------------------------------------------
bool XMLConfigErrorHandler::handleError(const DOMError& domError)
{
    if (domError.getSeverity() == DOMError::DOM_SEVERITY_WARNING)
        std::cerr << "\nWarning at file ";
    else if (domError.getSeverity() == DOMError::DOM_SEVERITY_ERROR)
        std::cerr << "\nError at file ";
    else
        std::cerr << "\nFatal Error at file ";

    std::cerr << std::string(XMLStringConverter(
    	domError.getLocation()->getURI()))
         << ", line " << domError.getLocation()->getLineNumber()
         << ", char " << domError.getLocation()->getColumnNumber()
         << "\n  Message: " <<
	std::string(XMLStringConverter(domError.getMessage())) << std::endl;

    return true;
}

void XMLConfigErrorHandler::resetErrors()
{
}
