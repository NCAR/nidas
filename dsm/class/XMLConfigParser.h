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

                                                                                
#include <string>

#include <atdUtil/ThreadSupport.h>

#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xercesc/dom/DOMBuilder.hpp>

namespace dsm {

class XMLImplementation {
public:
    static xercesc::DOMImplementation *getImplementation()
    	throw(atdUtil::Exception);
    static void terminate();

private:
    static xercesc::DOMImplementation *impl;
    static atdUtil::Mutex lock;
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
/**
 * Wrapper class around xerces-c DOMBuilder to parse XML.
 */
class XMLConfigParser {
public:

    XMLConfigParser() throw(xercesc::DOMException,atdUtil::Exception);

    /**
     * Nuke the parser. This does a release() (delete) of the
     * associated DOMBuilder.
     */
    ~XMLConfigParser();

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
    	throw(xercesc::XMLException,
		xercesc::DOMException);

    xercesc::DOMDocument* parse(xercesc::InputSource& source)
    	throw(xercesc::XMLException,
		xercesc::DOMException);


protected:
    
    xercesc::DOMImplementation *impl;
    xercesc::DOMBuilder *parser;
    xercesc::DOMDocument* doc;
    XMLConfigErrorHandler errorHandler;

};


}

#endif

