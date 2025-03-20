// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

#ifndef NIDAS_CORE_XMLCONFIGWRITER_H
#define NIDAS_CORE_XMLCONFIGWRITER_H

#include "DSMConfig.h"

#include "XMLParser.h"
#include "XMLWriter.h"

#include <nidas/util/IOException.h>

#include <xercesc/framework/XMLFormatter.hpp>

namespace nidas { namespace core {

class XMLConfigWriterFilter: public
xercesc::DOMLSSerializerFilter
{
public:

    /**
     * Only accept nodes for a certain dsm.
     */
    XMLConfigWriterFilter(const DSMConfig* dsm);

    xercesc::DOMNodeFilter::FilterAction
    acceptNode(const xercesc::DOMNode* node) const;

    unsigned long getWhatToShow() const;

    void setWhatToShow(unsigned long val);

    int getNumDSM() const { return _numDSM; }

private:

    const DSMConfig* _dsm;

    unsigned long _whatToShow;

    mutable int _numDSM;

    xercesc::DOMNodeFilter::FilterAction
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

    XMLConfigWriter();

    XMLConfigWriter(const DSMConfig* dsm);

    ~XMLConfigWriter();

    /**
     * How many <dsm> nodes were matched by the XMLConfigWriterFilter.
     * -1 means no <dsm>s were filtered out, i.e. all <dsm>s were written.
     */
    int getNumDSM() const { return _filter ? _filter->getNumDSM() : -1; }

private:
    
    XMLConfigWriterFilter* _filter;

    /** No copying. */
    XMLConfigWriter(const XMLConfigWriter&);

    /** No assignment. */
    XMLConfigWriter& operator=(const XMLConfigWriter&);
};

}}	// namespace nidas namespace core

#endif

