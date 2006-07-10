/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_XMLCONFIGWRITER_H
#define NIDAS_CORE_XMLCONFIGWRITER_H

#include <nidas/core/DSMConfig.h>

#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLException.h>

#include <nidas/util/IOException.h>

#include <xercesc/framework/XMLFormatter.hpp>
#include <xercesc/dom/DOMWriter.hpp>

namespace nidas { namespace core {

class XMLConfigWriterFilter: public xercesc::DOMWriterFilter {
public:

    /**
     * Only accept nodes for a certain dsm.
     */
    XMLConfigWriterFilter(const DSMConfig* dsm);

    short acceptNode(const xercesc::DOMNode* node) const;

    unsigned long getWhatToShow() const;

    void setWhatToShow(unsigned long val);

protected:
    const DSMConfig* dsm;
    unsigned long whatToShow;

    short acceptDSMNode(const xercesc::DOMNode* node) const;
};

/**
 * Wrapper class around xerces-c DOMWriter to write XML.
 */
class XMLConfigWriter {
public:

    // XMLConfigWriter() throw(nidas::core::XMLException);

    XMLConfigWriter(const DSMConfig* dsm) throw(nidas::core::XMLException);

    ~XMLConfigWriter();

    void writeNode(xercesc::XMLFormatTarget * dest,
    	const xercesc::DOMNode& node)
		throw(nidas::util::IOException);

protected:
    
    xercesc::DOMImplementation *impl;
    xercesc::DOMWriter *writer;
    XMLConfigWriterFilter* filter;
};

}}	// namespace nidas namespace core

#endif

