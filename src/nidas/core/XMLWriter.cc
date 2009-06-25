/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/XMLWriter.h>
#include <nidas/core/XMLStringConverter.h>

// #include <xercesc/dom/DOMImplementationLS.hpp>
// #include <xercesc/dom/DOMNamedNodeMap.hpp>

#include <xercesc/framework/LocalFileFormatTarget.hpp>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

XMLWriter::XMLWriter()  throw(nidas::core::XMLException)
{
    _impl = XMLImplementation::getImplementation();
    _writer = ((xercesc::DOMImplementationLS*)_impl)->createDOMWriter();
    // install error handler
    _writer->setErrorHandler(&_errorHandler);
}

XMLWriter::~XMLWriter() 
{
    //
    //  Delete the writer.
    //
    _writer->release();
}

void XMLWriter::setFilter(xercesc::DOMWriterFilter* filter)
{
    _writer->setFilter(filter);
}

void XMLWriter::setDiscardDefaultContent(bool val) 
{
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTDiscardDefaultContent,val);
}

void XMLWriter::setEntities(bool val)
{
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTEntities,val);
}

void XMLWriter::setCanonicalForm(bool val)
{
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTCanonicalForm,val);
}

void XMLWriter::setPrettyPrint(bool val)
{
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint,val);
}

void XMLWriter::setNormalizeCharacters(bool val)
{
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTNormalizeCharacters,val);
}

void XMLWriter::setSplitCDATASections(bool val)
{
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTSplitCdataSections,val);
}

void XMLWriter::setValidation(bool val)
{
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTValidation,val);
}

void XMLWriter::setWhitespaceInElement(bool val)
{
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTWhitespaceInElementContent,val);
}

void XMLWriter::writeNode(xercesc::XMLFormatTarget* const dest,
    const xercesc::DOMNode& node)
    throw (nidas::core::XMLException)
{
    //reset error count first
    _errorHandler.resetErrors();
    bool ok = _writer->writeNode(dest,node);
    const nidas::core::XMLException* xe = _errorHandler.getXMLException();
    if (xe) throw *xe;
    if (!ok) throw XMLException(string("writeNode failed"));
}

void XMLWriter::write(xercesc::DOMDocument*doc, const std::string& fileName)
        throw(nidas::core::XMLException)
{
    xercesc::LocalFileFormatTarget xmlfile(
        (const XMLCh*)XMLStringConverter(fileName));
    writeNode(&xmlfile,*doc);
}
