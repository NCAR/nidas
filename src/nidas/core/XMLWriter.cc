/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
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

#include "XMLWriter.h"
#include "XMLStringConverter.h"

#include <xercesc/framework/LocalFileFormatTarget.hpp>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

XMLWriter::XMLWriter():
    _impl(XMLImplementation::getImplementation()),
    _writer(((xercesc::DOMImplementationLS*)_impl)->createLSSerializer()),
    _errorHandler()
{
    // install error handler
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMErrorHandler, &_errorHandler);
}

XMLWriter::~XMLWriter() 
{
    //
    //  Delete the writer.
    //
    _writer->release();
}

void XMLWriter::setFilter(xercesc::DOMLSSerializerFilter* filter)
{
    _writer->setFilter(filter);
}

void XMLWriter::setDiscardDefaultContent(bool val) 
{
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTDiscardDefaultContent,val);
}

void XMLWriter::setEntities(bool val)
{
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTEntities,val);
}

void XMLWriter::setCanonicalForm(bool val)
{
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTCanonicalForm,val);
}

void XMLWriter::setPrettyPrint(bool val)
{
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint,val);
}

void XMLWriter::setNormalizeCharacters(bool val)
{
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTNormalizeCharacters,val);
}

void XMLWriter::setSplitCDATASections(bool val)
{
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTSplitCdataSections,val);
}

void XMLWriter::setValidation(bool val)
{
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTValidation,val);
}

void XMLWriter::setWhitespaceInElement(bool val)
{
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTWhitespaceInElementContent,val);
}

void XMLWriter::writeNode(xercesc::DOMLSOutput* const dest,
    const xercesc::DOMNode& node)
{
    //reset error count first
    _errorHandler.resetErrors();
    bool ok = _writer->write(&node,dest);
    const nidas::core::XMLException* xe = _errorHandler.getXMLException();
    if (xe) throw *xe;
    if (!ok) throw XMLException(string("writeNode failed"));
}

void XMLWriter::write(xercesc::DOMDocument*doc, const std::string& fileName)
{
    XMLStringConverter convname(fileName);
    xercesc::LocalFileFormatTarget xmlfile((const XMLCh*)convname);

    xercesc::DOMLSOutput *output;
    output = ((xercesc::DOMImplementationLS*)_impl)->createLSOutput();
    output->setByteStream(&xmlfile);
    output->setSystemId((const XMLCh*)convname);
    writeNode(output,*doc);
    output->release();
}
