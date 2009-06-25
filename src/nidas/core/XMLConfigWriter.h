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
#include <nidas/core/XMLWriter.h>

#include <nidas/util/IOException.h>

#include <xercesc/framework/XMLFormatter.hpp>

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

private:

    const DSMConfig* _dsm;

    unsigned long _whatToShow;

    short acceptDSMNode(const xercesc::DOMNode* node) const;
};

/**
 * An XMLWriter which writes the NIDAS XML configuration.
 */
class XMLConfigWriter: public XMLWriter {
public:

    XMLConfigWriter(const DSMConfig* dsm) throw(nidas::core::XMLException);

    ~XMLConfigWriter();

private:
    
    XMLConfigWriterFilter* _filter;
};

}}	// namespace nidas namespace core

#endif

