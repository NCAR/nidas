/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_XMLPARSER_H
#define NIDAS_CORE_XMLPARSER_H

#include <nidas/core/XMLException.h>

#include <nidas/util/ThreadSupport.h>
#include <nidas/util/IOException.h>

#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xercesc/dom/DOMBuilder.hpp>

#include <string>
#include <map>
#include <list>

namespace nidas { namespace core {

class XMLImplementation {
public:
    static xercesc::DOMImplementation *getImplementation()
    	throw(nidas::core::XMLException);
    static void terminate();

private:
    static xercesc::DOMImplementation *impl;
    static nidas::util::Mutex lock;
};
    
class XMLErrorHandler : public xercesc::DOMErrorHandler
{
public:
    // -----------------------
    //  Constructors and Destructor
    // ----------------------
    XMLErrorHandler();
    ~XMLErrorHandler();

    // --------------------------
    //  Implementation of the DOM ErrorHandler interface
    // -------------------------
    bool handleError(const xercesc::DOMError& domError);

    void resetErrors();

    int getWarningCount() const { return warningMessages.size(); }

    const std::list<std::string>& getWarningMessages() const
    	{ return warningMessages; }

    const XMLException* getXMLException() const { return xmlException; }

private :

    // -----------------------------
    //  Unimplemented constructors and operators
    // ----------------------------
    XMLErrorHandler(const XMLErrorHandler&);

    void operator=(const XMLErrorHandler&);

    /**
     * Accumulated warning messages.
     */
    std::list<std::string> warningMessages;

    /**
     * Accumulated error messages.
     */
    XMLException* xmlException;

};
/**
 * Wrapper class around xerces-c DOMBuilder to parse XML.
 */
class XMLParser {
public:

    /**
     * Constructor. The default setting for
     * setXercesUserAdoptsDOMDocument(true) is true.
     */
    XMLParser() throw(nidas::core::XMLException);

    /**
     * Nuke the parser. This does a release() (delete) of the
     * associated DOMBuilder.
     */
    ~XMLParser();

    /**
     * DOMBuilder::setFilter is not yet implemented in xerces c++ 2.6.0 
    void setFilter(DOMBuilderFilter* filter)
     */

    /**
     * Enable/disable validation.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMBuilderFeatures
     * @param val Boolean value specifying whether to report all
     *   validation errors.
     *   Default: false.
     */
    void setDOMValidation(bool val);

    /**
     * Enable/disable schema validation.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMBuilderFeatures
     * @param val If true the parser will validate the
     *	 document only if a grammar is specified.
     *   If false validation is determined by the state of the
     *   validation feature, see setDOMValidation().  
     *   Default: false.
     */
    void setDOMValidateIfSchema(bool val);

    /**
     * Enable/disable namespace processing.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMBuilderFeatures
     * @param val If true perform namespace processing.
     *   Default: false.
     */
    void setDOMNamespaces(bool val);

    /**
     * Enable/disable schema support.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMBuilderFeatures
     * @param val If true enable the parser's schema support. 
     *   Default: false.
     */
    void setXercesSchema(bool val);

    /**
     * Enable/disable full schema constraint checking,
     * including checking which may be time-consuming or
     * memory intensive. Currently, particle unique
     * attribution constraint checking and particle derivation
     * restriction checking are controlled by this option.  
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMBuilderFeatures
     * @param val If true enable schema constraint checking.
     *   Default: false.
     */
    void setXercesSchemaFullChecking(bool val);

    /**
     * Enable/disable datatype normalization.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMBuilderFeatures
     * @param val If true let the validation process do its datatype
     *    normalization that is defined in the used schema language.
     *    If false disable datatype normalization. The XML 1.0 attribute
     *    value normalization always occurs though.  
     *   Default: false.
     */
    void setDOMDatatypeNormalization(bool val);

    /**
     * Control who owns DOMDocument pointer.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMBuilderFeatures
     * @param val If true the caller will adopt the DOMDocument that
     *     is returned from the parse method and thus is responsible
     *     to call DOMDocument::release() to release the associated memory.
     *     The parser will not release it. The ownership is transferred
     *     from the parser to the caller.
     *     If false the returned DOMDocument from the parse method is
     *     owned by the parser and thus will be deleted when the parser
     *     is released.
     */
    void setXercesUserAdoptsDOMDocument(bool val);

    xercesc::DOMDocument* parse(const std::string& xmlFile)
    	throw(nidas::core::XMLException);

    xercesc::DOMDocument* parse(xercesc::InputSource& source)
    	throw(nidas::core::XMLException);


protected:
    
    xercesc::DOMImplementation *impl;
    xercesc::DOMBuilder *parser;
    XMLErrorHandler errorHandler;

};

/**
 * Derived class of XMLParser that keeps its DOMDocuments
 * when parsing an XML disk file, and returns the cached
 * DOMDocument if the file hasn't changed.
 */
class XMLCachingParser : public XMLParser {
public:

    static XMLCachingParser* getInstance()
    	throw(nidas::core::XMLException);

    static void destroyInstance();

    /**
     * Parse from a file. This will return the DOMDocument
     * pointer of the a previous parse result if the file has
     * not been modified since the last time it was
     * parsed.
     */
    xercesc::DOMDocument* parse(const std::string& xmlFile)
    	throw(nidas::core::XMLException,nidas::util::IOException);

    /**
     * Parse from an InputSource. This is not cached.
     */
    xercesc::DOMDocument* parse(xercesc::InputSource& source)
    	throw(nidas::core::XMLException)
    {
        return XMLParser::parse(source);
    }

    static time_t getFileModTime(const std::string&  name) throw(nidas::util::IOException);

protected:
    XMLCachingParser() throw(nidas::core::XMLException);
    ~XMLCachingParser();

protected:
    static XMLCachingParser* instance;
    static nidas::util::Mutex instanceLock;


    std::map<std::string,time_t> modTimeCache;
    std::map<std::string,xercesc::DOMDocument*> docCache;

    nidas::util::Mutex cacheLock;
};

}}	// namespace nidas namespace core

#endif

