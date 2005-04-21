/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <XMLParser.h>
#include <XMLStringConverter.h>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>
#include <xercesc/dom/DOMLocator.hpp>

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

DOMImplementation* XMLImplementation::impl = 0;
atdUtil::Mutex XMLImplementation::lock;

/* static */
DOMImplementation*
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
    

XMLParser::XMLParser() throw (DOMException,
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

void XMLParser::setDOMValidation(bool val) {
    parser->setFeature(XMLUni::fgDOMValidation, val);
}

void XMLParser::setDOMValidateIfSchema(bool val) {
    parser->setFeature(XMLUni::fgDOMValidateIfSchema, val);
}

void XMLParser::setDOMNamespaces(bool val) {
    parser->setFeature(XMLUni::fgDOMNamespaces, val);
}

void XMLParser::setXercesSchema(bool val) {
    parser->setFeature(XMLUni::fgXercesSchema, val);
}

void XMLParser::setXercesSchemaFullChecking(bool val) {
    parser->setFeature(XMLUni::fgXercesSchemaFullChecking, val);
}

void XMLParser::setDOMDatatypeNormalization(bool val) {
    parser->setFeature(XMLUni::fgDOMDatatypeNormalization, val);
}

void XMLParser::setXercesUserAdoptsDOMDocument(bool val) {
    parser->setFeature(XMLUni::fgXercesUserAdoptsDOMDocument, val);
}

XMLParser::~XMLParser() 
{
    //
    //  Delete the parser itself.  Must be done prior to calling Terminate.
    //  below.
    //  In xerces-c-src_2_6_0/src/xercesc/parsers/DOMBuilderImpl.cpp
    //  parser->release() is the same as delete parser.

    // cerr << "parser release" << endl;
    parser->release();

}


DOMDocument* XMLParser::parse(const string& xmlFile) 
    throw (SAXException, XMLException, DOMException)
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
    DOMDocument* doc = parser->parseURI(
    	(const XMLCh*)XMLStringConverter(xmlFile.c_str()));
    return doc;
}

DOMDocument* XMLParser::parse(xercesc::InputSource& source) 
    throw (SAXException, XMLException, DOMException)
{

    //reset error count first
    errorHandler.resetErrors();

    // throws XMLException, DOMException, SAXException
    xercesc::Wrapper4InputSource wrapper(&source,false);
    DOMDocument* doc = parser->parse(wrapper);
    // throws SAXException, XMLException, DOMException
    return doc;
}

XMLErrorHandler::XMLErrorHandler()
{
}

XMLErrorHandler::~XMLErrorHandler()
{
}


// ---------------------------------------------------------------------------
//  XMLErrorHandler interface
// ---------------------------------------------------------------------------
bool XMLErrorHandler::handleError(const DOMError& domError)
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

void XMLErrorHandler::resetErrors()
{
}

/* static */
XMLCachingParser* XMLCachingParser::instance = 0;

/* static */
atdUtil::Mutex XMLCachingParser::instanceLock;

/* static */
XMLCachingParser* XMLCachingParser::getInstance()
    throw(DOMException,atdUtil::Exception)
{
    if (!instance) {
        atdUtil::Synchronized autosync(instanceLock);
        if (!instance) instance = new XMLCachingParser();
    }
    return instance;
}

/* static */
void XMLCachingParser::destroyInstance()
{
    if (instance) {
        atdUtil::Synchronized autosync(instanceLock);
        if (instance) delete instance;
	instance = 0;
    }
}

XMLCachingParser::XMLCachingParser() throw(DOMException,atdUtil::Exception):
	XMLParser()
{
}

XMLCachingParser::~XMLCachingParser()
{
    atdUtil::Synchronized autosync(cacheLock);
    std::map<std::string,DOMDocument*>::const_iterator di;
    for (di = docCache.begin(); di != docCache.end(); ++di) {
	DOMDocument* doc = di->second;
	cerr << "releasing doc" << endl;
	doc->release();
    }
}

DOMDocument* XMLCachingParser::parse(const string& xmlFile) 
    throw (SAXException, XMLException, DOMException)
{
    // synchronize access to the cache
    atdUtil::Synchronized autosync(cacheLock);

    // modification time of file when it was last parsed
    time_t lastModTime = modTimeCache[xmlFile];

    // latest modification time of file
    time_t modTime = getFileModTime(xmlFile);

    // results of last parse (will be 0 if not previously parsed)
    DOMDocument* doc = docCache[xmlFile];

    if (modTime > lastModTime || !doc) {
	if (doc) doc->release();
        doc = XMLParser::parse(xmlFile);
	modTimeCache[xmlFile] = modTime;
	docCache[xmlFile] = doc;
    }
    return doc;
}

/* static */
time_t XMLCachingParser::getFileModTime(const std::string&  name) throw(atdUtil::IOException)
{
    struct stat filestat;
    if (stat(name.c_str(),&filestat) < 0)
	throw atdUtil::IOException(name,"stat",errno);
    return filestat.st_mtime;
}
