// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

class XMLConfigWriterFilter: public
#if XERCES_VERSION_MAJOR < 3
	xercesc::DOMWriterFilter
#else
	xercesc::DOMLSSerializerFilter
#endif

{
public:

    /**
     * Only accept nodes for a certain dsm.
     */
    XMLConfigWriterFilter(const DSMConfig* dsm);

#if XERCES_VERSION_MAJOR < 3
    short
#else
    xercesc::DOMNodeFilter::FilterAction
#endif
    acceptNode(const xercesc::DOMNode* node) const;

    unsigned long getWhatToShow() const;

    void setWhatToShow(unsigned long val);

private:

    const DSMConfig* _dsm;

    unsigned long _whatToShow;

#if XERCES_VERSION_MAJOR < 3
    short
#else
    xercesc::DOMNodeFilter::FilterAction
#endif
    acceptDSMNode(const xercesc::DOMNode* node) const;

    /** No copying. */
    XMLConfigWriterFilter(const XMLConfigWriterFilter&);

    /** No assignment. */
    XMLConfigWriterFilter& operator=(const XMLConfigWriterFilter&);
};

/**
 * An XMLWriter which writes the NIDAS XML configuration.
 */
class XMLConfigWriter: public XMLWriter {
public:

    XMLConfigWriter() throw(nidas::core::XMLException);

    XMLConfigWriter(const DSMConfig* dsm) throw(nidas::core::XMLException);

    ~XMLConfigWriter();

private:
    
    XMLConfigWriterFilter* _filter;

    /** No copying. */
    XMLConfigWriter(const XMLConfigWriter&);

    /** No assignment. */
    XMLConfigWriter& operator=(const XMLConfigWriter&);
};

}}	// namespace nidas namespace core

#endif

