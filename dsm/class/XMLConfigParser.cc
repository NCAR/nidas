/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <XMLConfigParser.h>
#include <XMLStringConverter.h>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>
#include <xercesc/dom/DOMLocator.hpp>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

xercesc::DOMImplementation* XMLImplementation::impl = 0;
atdUtil::Mutex XMLImplementation::lock;

/* static */
xercesc::DOMImplementation*
XMLImplementation::getImplementation() throw(atdUtil::Exception)
{
    if (!impl) {
	atdUtil::Synchronized autosync(lock);
	if (!impl) {
	    XMLPlatformUtils::Initialize();

	    // Instantiate the DOM parser.

	    // "LS" is the Load and Save feature.
	    // See: http://www.w3.org/TR/DOM-Level-3-LS/load-save.html
	    // no exceptions thrown, but may return null if no implementation
	    static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };
	    impl = DOMImplementationRegistry::getDOMImplementation(gLS);
	    if (!impl) throw atdUtil::Exception(
	    	" DOMImplementationRegistry::getDOMImplementation(gLS) failed");
	}
    }
    return impl;
}
    
/* static */
void XMLImplementation::terminate()
{
    if (!impl) {
	atdUtil::Synchronized autosync(lock);
	if (impl) {
	    XMLPlatformUtils::Terminate();
	    impl = 0;
	}
    }
}
    

XMLConfigParser::XMLConfigParser() throw (xercesc::DOMException,
	atdUtil::Exception)
{

    impl = XMLImplementation::getImplementation();
    
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

    // Create our error handler and install it
    parser->setErrorHandler(&errorHandler);
}

void XMLConfigParser::setDOMValidation(bool val) {
    parser->setFeature(XMLUni::fgDOMValidation, val);
}

void XMLConfigParser::setDOMValidateIfSchema(bool val) {
    parser->setFeature(XMLUni::fgDOMValidateIfSchema, val);
}

void XMLConfigParser::setDOMNamespaces(bool val) {
    parser->setFeature(XMLUni::fgDOMNamespaces, val);
}

void XMLConfigParser::setXercesSchema(bool val) {
    parser->setFeature(XMLUni::fgXercesSchema, val);
}

void XMLConfigParser::setXercesSchemaFullChecking(bool val) {
    parser->setFeature(XMLUni::fgXercesSchemaFullChecking, val);
}

void XMLConfigParser::setDOMDatatypeNormalization(bool val) {
    parser->setFeature(XMLUni::fgDOMDatatypeNormalization, val);
}

void XMLConfigParser::setXercesUserAdoptsDOMDocument(bool val) {
    parser->setFeature(XMLUni::fgXercesUserAdoptsDOMDocument, val);
}

XMLConfigParser::~XMLConfigParser() 
{
    //
    //  Delete the parser itself.  Must be done prior to calling Terminate.
    //  below.
    //  In xerces-c-src_2_6_0/src/xercesc/parsers/DOMBuilderImpl.cpp
    //  parser->release() is the same as delete parser.

    cerr << "parser release" << endl;
    parser->release();

}


DOMDocument* XMLConfigParser::parse(const string& xmlFile) 
    throw (XMLException, DOMException)
{

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
    return doc;
}

DOMDocument* XMLConfigParser::parse(xercesc::InputSource& source) 
    throw (XMLException, DOMException)
{

    //reset error count first
    errorHandler.resetErrors();

    // throws XMLException, DOMException, SAXException
    xercesc::Wrapper4InputSource wrapper(&source,false);
    doc = parser->parse(wrapper);
    // throws SAXException, XMLException, DOMException
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

    // true=proceed, false=give up
    return domError.getSeverity() == DOMError::DOM_SEVERITY_WARNING;
}

void XMLConfigErrorHandler::resetErrors()
{
}
