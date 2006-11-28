/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLStringConverter.h>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>
#include <xercesc/dom/DOMLocator.hpp>

#include <iostream>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

DOMImplementation* XMLImplementation::impl = 0;
n_u::Mutex XMLImplementation::lock;

/* static */
DOMImplementation*
XMLImplementation::getImplementation() throw(nidas::core::XMLException)
{
    if (!impl) {
	n_u::Synchronized autosync(lock);
	if (!impl) {
	    XMLPlatformUtils::Initialize();

	    // Instantiate the DOM parser.

	    // "LS" is the Load and Save feature.
	    // See: http://www.w3.org/TR/DOM-Level-3-LS/load-save.html
	    // no exceptions thrown, but may return null if no implementation
	    static const XMLCh gLS[] = { chLatin_L, chLatin_S, chNull };
	    impl = DOMImplementationRegistry::getDOMImplementation(gLS);
	    if (!impl) throw nidas::core::XMLException(
	    	string("DOMImplementationRegistry::getDOMImplementation(gLS) failed"));
	}
    }
    return impl;
}
    
/* static */
void XMLImplementation::terminate()
{
    if (!impl) {
	n_u::Synchronized autosync(lock);
	if (impl) {
	    XMLPlatformUtils::Terminate();
	    impl = 0;
	}
    }
}
    

XMLParser::XMLParser() throw (nidas::core::XMLException)
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
    try {
	// throws DOMException
	parser = ((DOMImplementationLS*)impl)->createDOMBuilder(
		    DOMImplementationLS::MODE_SYNCHRONOUS, 0);
    }
    catch (const DOMException& e) {
        throw nidas::core::XMLException(e);
    }
    setXercesUserAdoptsDOMDocument(true);

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
    throw (nidas::core::XMLException)
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
    DOMDocument* doc = 0;
    try {
	doc = parser->parseURI(
	    (const XMLCh*)XMLStringConverter(xmlFile.c_str()));
	const XMLException* xe = errorHandler.getXMLException();
	if (xe) throw *xe;
    }
    catch (const xercesc::XMLException& e) { throw nidas::core::XMLException(e); }
    catch (const xercesc::SAXException& e) { throw nidas::core::XMLException(e); }
    catch (const xercesc::DOMException& e) { throw nidas::core::XMLException(e); }
    return doc;
}

DOMDocument* XMLParser::parse(xercesc::InputSource& source) 
    throw (nidas::core::XMLException)
{

    //reset error count first
    errorHandler.resetErrors();

    DOMDocument* doc = 0;
    try {
	xercesc::Wrapper4InputSource wrapper(&source,false);
        doc = parser->parse(wrapper);
	// throws SAXException, XMLException, DOMException
	const XMLException* xe = errorHandler.getXMLException();
	if (xe) throw *xe;
    }
    catch (const xercesc::XMLException& e) { throw nidas::core::XMLException(e); }
    catch (const xercesc::SAXException& e) { throw nidas::core::XMLException(e); }
    catch (const xercesc::DOMException& e) { throw nidas::core::XMLException(e); }
    return doc;
}

XMLErrorHandler::XMLErrorHandler(): xmlException(0)
{
}

XMLErrorHandler::~XMLErrorHandler()
{
    delete xmlException;
}


// ---------------------------------------------------------------------------
//  XMLErrorHandler interface
// ---------------------------------------------------------------------------
bool XMLErrorHandler::handleError(const DOMError& domError)
{

    string uri(XMLStringConverter(domError.getLocation()->getURI()));
    string msg(XMLStringConverter(domError.getMessage()));

    ostringstream ost;
    if (uri.length() > 0)
	ost << uri << ", line " << domError.getLocation()->getLineNumber() <<
         ", char " << domError.getLocation()->getColumnNumber() <<
         ": " << msg;
    else
	ost << msg;

    // cerr << ost.str() << endl;
    if (domError.getSeverity() == DOMError::DOM_SEVERITY_WARNING)
	warningMessages.push_back(ost.str());
    else if (domError.getSeverity() == DOMError::DOM_SEVERITY_ERROR)
        xmlException = new XMLException(ost.str());
    else 
        xmlException = new XMLException(ost.str());

    // true=proceed, false=give up
    return domError.getSeverity() == DOMError::DOM_SEVERITY_WARNING;
}

void XMLErrorHandler::resetErrors()
{
    warningMessages.clear();
    delete xmlException;
    xmlException = 0;
}

/* static */
XMLCachingParser* XMLCachingParser::instance = 0;

/* static */
n_u::Mutex XMLCachingParser::instanceLock;

/* static */
XMLCachingParser* XMLCachingParser::getInstance()
    throw(nidas::core::XMLException)
{
    if (!instance) {
        n_u::Synchronized autosync(instanceLock);
        if (!instance) instance = new XMLCachingParser();
    }
    return instance;
}

/* static */
void XMLCachingParser::destroyInstance()
{
    if (instance) {
        n_u::Synchronized autosync(instanceLock);
        if (instance) delete instance;
	instance = 0;
    }
}

XMLCachingParser::XMLCachingParser() throw(nidas::core::XMLException):
	XMLParser()
{
}

XMLCachingParser::~XMLCachingParser()
{
    n_u::Synchronized autosync(cacheLock);
    map<string,DOMDocument*>::const_iterator di;
    for (di = docCache.begin(); di != docCache.end(); ++di) {
	DOMDocument* doc = di->second;
	if (doc) doc->release();
    }
}

DOMDocument* XMLCachingParser::parse(const string& xmlFile) 
    throw (nidas::core::XMLException)
{
    // synchronize access to the cache
    n_u::Synchronized autosync(cacheLock);

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
time_t XMLCachingParser::getFileModTime(const string&  name) throw(n_u::IOException)
{
    struct stat filestat;
    if (::stat(name.c_str(),&filestat) < 0)
	throw n_u::IOException(name,"stat",errno);
    return filestat.st_mtime;
}
