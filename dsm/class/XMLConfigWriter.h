/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_XMLCONFIGWRITER_H
#define DSM_XMLCONFIGWRITER_H

#include <DSMConfig.h>

#include <XMLParser.h>

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

    // XMLConfigWriter() throw(atdUtil::Exception);

    XMLConfigWriter(const DSMConfig* dsm) throw(atdUtil::Exception);

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

