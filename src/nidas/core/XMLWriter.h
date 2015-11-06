// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_XMLWRITER_H
#define NIDAS_CORE_XMLWRITER_H

#include <nidas/core/XMLParser.h>

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
    virtual ~XMLWriter();

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

#if XERCES_VERSION_MAJOR < 3
    void setFilter(xercesc::DOMWriterFilter* filter);
#else
    void setFilter(xercesc::DOMLSSerializerFilter* filter);
#endif

    virtual void write(xercesc::DOMDocument*doc, const std::string& fileName)
    	throw(nidas::core::XMLException,nidas::util::IOException);

#if XERCES_VERSION_MAJOR < 3
    virtual void writeNode(xercesc::XMLFormatTarget* const dest, const xercesc::DOMNode& node)
        throw(nidas::core::XMLException,nidas::util::IOException);
#else
    virtual void writeNode(xercesc::DOMLSOutput* const dest, const xercesc::DOMNode& node)
        throw(nidas::core::XMLException,nidas::util::IOException);
#endif

private:
    
    xercesc::DOMImplementation *_impl;

#if XERCES_VERSION_MAJOR < 3
    xercesc::DOMWriter *_writer;
#else
    xercesc::DOMLSSerializer *_writer;
#endif

    XMLErrorHandler _errorHandler;

    /** No copying */
    XMLWriter(const XMLWriter&);

    /** No assignment */
    XMLWriter& operator=(const XMLWriter&);

};

}}	// namespace nidas namespace core

#endif

