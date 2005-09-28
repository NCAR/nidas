/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_XMLCONFIGWRITER_H
#define DSM_XMLCONFIGWRITER_H

#include <DSMConfig.h>

#include <XMLParser.h>
#include <XMLException.h>

#include <atdUtil/IOException.h>

#include <xercesc/framework/XMLFormatter.hpp>
#include <xercesc/dom/DOMWriter.hpp>


namespace dsm {

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

    // XMLConfigWriter() throw(dsm::XMLException);

    XMLConfigWriter(const DSMConfig* dsm) throw(dsm::XMLException);

    ~XMLConfigWriter();

    void writeNode(xercesc::XMLFormatTarget * dest,
    	const xercesc::DOMNode& node)
		throw(atdUtil::IOException);

protected:
    
    xercesc::DOMImplementation *impl;
    xercesc::DOMWriter *writer;
    XMLConfigWriterFilter* filter;
};

}

#endif

