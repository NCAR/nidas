/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-22 13:05:21 -0600 (Mon, 22 May 2006) $

    $LastChangedRevision: 3362 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/core/XMLParser.h $
 ********************************************************************

*/

#ifndef NIDAS_CORE_XMLWRITER_H
#define NIDAS_CORE_XMLWRITER_H

#include <nidas/core/XMLParser.h>


#ifdef NEEDED
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/IOException.h>

#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMErrorHandler.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xercesc/dom/DOMBuilder.hpp>
#endif

#include <string>

namespace nidas { namespace core {

/**
 * Wrapper class around xerces-c DOMWriter to write XML.
 */
class XMLWriter {
public:

    /**
     * Constructor.
     */
    XMLWriter() throw(nidas::core::XMLException);

    /**
     * Nuke the XMLWriter. This does a release() (delete) of the
     * associated DOMWriter.
     */
    ~XMLWriter();

    /**
     * Discard default content.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMWriterFeatures
     * @param val Boolean value specifying whether to discard default
     *      values of attributes and content.
     *   Default: true.
     */
    void setDiscardDefaultContent(bool val);

    /**
     * Control writing of entities.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMWriterFeatures
     * @param val true: entity references are serialized as &entityName;.
     *   If false, entity references are expanded.
     *   Default: true.
     */
    void setEntities(bool val);

    /**
     * Canonicalize the output.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMWriterFeatures
     * @param val true: is not supported in Xerces-c.
     *   Default: false.
     */
    void setCanonicalForm(bool val);

    /**
     * Format pretty-print.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMWriterFeatures
     * @param val true: add whitespace to produce an indented form.
     *   Default: false.
     */
    void setPrettyPrint(bool val);

    /**
     * Normalize characters.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMWriterFeatures
     * @param val true is not supported by Xerces-c.
     *   Default: false.
     */
    void setNormalizeCharacters(bool val);

    /**
     * Split CDATA sections.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMWriterFeatures
     * @param val true: split CDATA sections containing ']]>' or
     *    unrepresentable characters in the output encoding.
     *    false: signal an errror.
     *   Default: true.
     */
    void setSplitCDATASections(bool val);

    /**
     * Validation.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMWriterFeatures
     * @param val false: do not report validation errors.
     *     true: not supported by Xerces-c.
     *  Default: false.
     */
    void setValidation(bool val);

    /**
     * Whitespace in element content.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMWriterFeatures
     * @param val true: include text nodes that can be considered ignorable
     *     whitespace in the DOM tree.
     *     false: not supported by Xerces-c.
     *  Default: true.
     */
    void setWhitespaceInElement(bool val);

    /**
     * Include xml declaration.
     * See /link
     *	http://xml.apache.org/xerces-c/program-dom.html#DOMWriterFeatures
     * @param val true: include xml declaration.
     *  Default: true.
     */
    void setXMLDeclaration(bool val);

    void write(xercesc::DOMDocument*doc, const std::string& fileName)
    	throw(nidas::core::XMLException);

protected:
    
    xercesc::DOMImplementation *impl;
    xercesc::DOMWriter *writer;
    XMLErrorHandler errorHandler;

};

}}	// namespace nidas namespace core

#endif

