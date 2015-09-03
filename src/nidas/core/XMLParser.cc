/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLStringConverter.h>
#include <nidas/util/Logger.h>

#include <xercesc/util/XMLUniDefs.hpp>
#include <xercesc/dom/DOMImplementationRegistry.hpp>
#include <xercesc/framework/Wrapper4InputSource.hpp>
#include <xercesc/dom/DOMLocator.hpp>

#include <iostream>
#include <sstream>
#include <memory>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

xercesc::DOMImplementation* XMLImplementation::_impl = 0;
n_u::Mutex XMLImplementation::_lock;

/* static */
xercesc::DOMImplementation*
XMLImplementation::getImplementation() throw(nidas::core::XMLException)
{
    if (!_impl) {
	n_u::Synchronized autosync(_lock);
	if (!_impl) {
	    xercesc::XMLPlatformUtils::Initialize();

	    // Instantiate the DOM parser.

	    // "LS" is the Load and Save feature.
	    // See: http://www.w3.org/TR/DOM-Level-3-LS/load-save.html
	    // no exceptions thrown, but may return null if no implementation
	    static const XMLCh gLS[] = { xercesc::chLatin_L, xercesc::chLatin_S, xercesc::chNull };
	    _impl = xercesc::DOMImplementationRegistry::getDOMImplementation(gLS);
	    if (!_impl) throw nidas::core::XMLException(
	    	string("DOMImplementationRegistry::getDOMImplementation(gLS) failed"));
	}
    }
    return _impl;
}
    
/* static */
void XMLImplementation::terminate()
{
    if (_impl) {
	n_u::Synchronized autosync(_lock);
	if (_impl) {
	    xercesc::XMLPlatformUtils::Terminate();
            // don't need to delete _impl, Terminate() does it.
	    _impl = 0;
	}
    }
}
    

XMLParser::XMLParser() throw (nidas::core::XMLException):
    _impl(XMLImplementation::getImplementation()),
    _parser(0),_errorHandler()
{

    
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
#if XERCES_VERSION_MAJOR < 3
	_parser = ((xercesc::DOMImplementationLS*)_impl)->createDOMBuilder(
		    xercesc::DOMImplementationLS::MODE_SYNCHRONOUS, 0);
#else
	XMLStringConverter schema ("http://www.w3.org/2001/XMLSchema");
	_parser = ((xercesc::DOMImplementationLS*)_impl)->createLSParser(
		    xercesc::DOMImplementationLS::MODE_SYNCHRONOUS, (const XMLCh*)schema);
#endif
    }
    catch (const xercesc::DOMException& e) {
        throw nidas::core::XMLException(e);
    }

    // User owns the DOMDocument, not the parser.
    setXercesUserAdoptsDOMDocument(true);

#if XERCES_VERSION_MAJOR < 3
    // Create our error handler and install it
    _parser->setErrorHandler(&_errorHandler);
#else
    _parser->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMErrorHandler, &_errorHandler);
#endif
}

void XMLParser::setDOMValidation(bool val) {
#if XERCES_VERSION_MAJOR < 3
    _parser->setFeature(xercesc::XMLUni::fgDOMValidation, val);
#else
    _parser->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMValidate, val);
#endif
}

void XMLParser::setDOMValidateIfSchema(bool val) {
#if XERCES_VERSION_MAJOR < 3
    _parser->setFeature(xercesc::XMLUni::fgDOMValidateIfSchema, val);
#else
    _parser->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMValidateIfSchema, val);
#endif
}

void XMLParser::setDOMNamespaces(bool val) {
#if XERCES_VERSION_MAJOR < 3
    _parser->setFeature(xercesc::XMLUni::fgDOMNamespaces, val);
#else
    _parser->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMNamespaces, val);
#endif
}

void XMLParser::setXercesSchema(bool val) {
#if XERCES_VERSION_MAJOR < 3
    _parser->setFeature(xercesc::XMLUni::fgXercesSchema, val);
#else
    _parser->getDomConfig()->setParameter(xercesc::XMLUni::fgXercesSchema, val);
#endif
}

void XMLParser::setXercesSchemaFullChecking(bool val) {
#if XERCES_VERSION_MAJOR < 3
    _parser->setFeature(xercesc::XMLUni::fgXercesSchemaFullChecking, val);
#else
    _parser->getDomConfig()->setParameter(xercesc::XMLUni::fgXercesSchemaFullChecking, val);
#endif
}

void XMLParser::setDOMDatatypeNormalization(bool val) {
#if XERCES_VERSION_MAJOR < 3
    _parser->setFeature(xercesc::XMLUni::fgDOMDatatypeNormalization, val);
#else
    _parser->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMDatatypeNormalization, val);
#endif
}

void XMLParser::setXercesUserAdoptsDOMDocument(bool val) {
#if XERCES_VERSION_MAJOR < 3
    _parser->setFeature(xercesc::XMLUni::fgXercesUserAdoptsDOMDocument, val);
#else
    _parser->getDomConfig()->setParameter(xercesc::XMLUni::fgXercesUserAdoptsDOMDocument, val);
#endif
}

void XMLParser::setXercesHandleMultipleImports(bool val)
{
    /* This feature is not supported before version 3. We're silently
     * ignoring that fact.
     */
#if XERCES_VERSION_MAJOR >= 3
    _parser->getDomConfig()->setParameter(xercesc::XMLUni::fgXercesHandleMultipleImports, val);
#endif
}

void XMLParser::setXercesDoXInclude(bool val)
{
    /* This feature is not supported before version 3. We're silently
     * ignoring that fact.
     */
#if XERCES_VERSION_MAJOR >= 3
    if (_parser->getDomConfig()->canSetParameter(xercesc::XMLUni::fgXercesDoXInclude, val)) {
        _parser->getDomConfig()->setParameter(xercesc::XMLUni::fgXercesDoXInclude, val);
    }
#endif
}

XMLParser::~XMLParser() 
{
    //  Delete the parser itself.  Must be done prior to calling Terminate.
    //  In xerces-c-src_2_6_0/src/xercesc/parsers/DOMBuilderImpl.cpp
    //  parser->release() is the same as delete parser.
    _parser->release();
}


xercesc::DOMDocument* XMLParser::parse(const string& xmlFile,bool verbose) 
    throw (nidas::core::XMLException)
{
    if (verbose) NLOG(("parsing: ") << xmlFile);

    //reset error count first
    _errorHandler.resetErrors();

    /* 
	If the DOMBuilder is a synchronous DOMBuilder the newly
	created and populated DOMDocument is returned. If the
	DOMBuilder is asynchronous then null is returned since
	the document object is not yet parsed when this method returns. 
     */
    // throws XMLException, DOMException, SAXException
    xercesc::DOMDocument* doc = 0;
    try {
	doc = _parser->parseURI(
	    (const XMLCh*)XMLStringConverter(xmlFile.c_str()));
	const XMLException* xe = _errorHandler.getXMLException();
	if (xe) throw *xe;
    }
    catch (const xercesc::XMLException& e) { throw nidas::core::XMLException(e); }
    catch (const xercesc::SAXException& e) { throw nidas::core::XMLException(e); }
    catch (const xercesc::DOMException& e) { throw nidas::core::XMLException(e); }
    return doc;
}

xercesc::DOMDocument* XMLParser::parse(xercesc::InputSource& source) 
    throw (nidas::core::XMLException)
{

    //reset error count first
    _errorHandler.resetErrors();

    xercesc::DOMDocument* doc = 0;
    try {
	xercesc::Wrapper4InputSource wrapper(&source,false);
#if XERCES_VERSION_MAJOR < 3
        doc = _parser->parse(wrapper);
#else
        doc = _parser->parse(&wrapper);
#endif
	// throws SAXException, XMLException, DOMException
	const XMLException* xe = _errorHandler.getXMLException();
	if (xe) throw *xe;
    }
    catch (const xercesc::XMLException& e) { throw nidas::core::XMLException(e); }
    catch (const xercesc::SAXException& e) { throw nidas::core::XMLException(e); }
    catch (const xercesc::DOMException& e) { throw nidas::core::XMLException(e); }
    return doc;
}

xercesc::DOMDocument* nidas::core::parseXMLConfigFile(const string& xmlFileName)
	throw(nidas::core::XMLException)
{
    // NLOG(("parsing: ") << xmlFileName);

    auto_ptr<XMLParser> parser(new XMLParser());
    // throws XMLException

    // If parsing a local file, turn on validation
    parser->setDOMValidation(true);
    parser->setDOMValidateIfSchema(true);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(true);
    parser->setXercesSchemaFullChecking(true);
    parser->setXercesHandleMultipleImports(true);
    parser->setXercesDoXInclude(true);

    parser->setDOMDatatypeNormalization(false);

    xercesc::DOMDocument* doc = parser->parse(xmlFileName);
    return doc;
}

XMLErrorHandler::XMLErrorHandler(): _warningMessages(),_xmlException(0)
{
}

XMLErrorHandler::~XMLErrorHandler()
{
    delete _xmlException;
}


// ---------------------------------------------------------------------------
//  XMLErrorHandler interface
// ---------------------------------------------------------------------------
bool XMLErrorHandler::handleError(const xercesc::DOMError& domError)
{

    string uri = XMLStringConverter(domError.getLocation()->getURI());
    string msg = XMLStringConverter(domError.getMessage());

    ostringstream ost;
    if (uri.length() > 0)
	ost << uri << ", line " << domError.getLocation()->getLineNumber() <<
         ", char " << domError.getLocation()->getColumnNumber() <<
         ": " << msg;
    else
	ost << msg;

    // cerr << ost.str() << endl;
    if (domError.getSeverity() == xercesc::DOMError::DOM_SEVERITY_WARNING)
	_warningMessages.push_back(ost.str());
    else if (domError.getSeverity() == xercesc::DOMError::DOM_SEVERITY_ERROR)
        _xmlException = new XMLException(ost.str());
    else 
        _xmlException = new XMLException(ost.str());

    // true=proceed, false=give up
    return domError.getSeverity() == xercesc::DOMError::DOM_SEVERITY_WARNING;
}

void XMLErrorHandler::resetErrors()
{
    _warningMessages.clear();
    delete _xmlException;
    _xmlException = 0;
}

/* static */
XMLCachingParser* XMLCachingParser::_instance = 0;

/* static */
n_u::Mutex XMLCachingParser::_instanceLock;

/* static */
XMLCachingParser* XMLCachingParser::getInstance()
    throw(nidas::core::XMLException)
{
    if (!_instance) {
        n_u::Synchronized autosync(_instanceLock);
        if (!_instance) _instance = new XMLCachingParser();
    }
    return _instance;
}

/* static */
void XMLCachingParser::destroyInstance()
{
    if (_instance) {
        n_u::Synchronized autosync(_instanceLock);
        if (_instance) delete _instance;
	_instance = 0;
    }
}

XMLCachingParser::XMLCachingParser() throw(nidas::core::XMLException):
	XMLParser(),_modTimeCache(),_docCache(),_cacheLock()
{
}

XMLCachingParser::~XMLCachingParser()
{
    n_u::Synchronized autosync(_cacheLock);
    map<string,xercesc::DOMDocument*>::const_iterator di;
    for (di = _docCache.begin(); di != _docCache.end(); ++di) {
	xercesc::DOMDocument* doc = di->second;
	if (doc) doc->release();
    }
}

xercesc::DOMDocument* XMLCachingParser::parse(const string& xmlFile) 
    throw (nidas::core::XMLException,nidas::util::IOException)
{
    // synchronize access to the cache
    n_u::Synchronized autosync(_cacheLock);

    // modification time of file when it was last parsed
    time_t lastModTime = _modTimeCache[xmlFile];

    // latest modification time of file
    time_t modTime = getFileModTime(xmlFile);

    // results of last parse (will be 0 if not previously parsed)
    xercesc::DOMDocument* doc = _docCache[xmlFile];

    if (modTime > lastModTime || !doc) {
	if (doc) doc->release();
        // set cached document to NULL in case we get a parse exception
	_docCache[xmlFile] = 0;
        doc = XMLParser::parse(xmlFile);
	_modTimeCache[xmlFile] = modTime;
	_docCache[xmlFile] = doc;
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
