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

#include <xercesc/framework/LocalFileFormatTarget.hpp>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

XMLWriter::XMLWriter() throw(nidas::core::XMLException):
    _impl(XMLImplementation::getImplementation()),
#if XERCES_VERSION_MAJOR < 3
    _writer(((xercesc::DOMImplementationLS*)_impl)->createDOMWriter()),
#else
    _writer(((xercesc::DOMImplementationLS*)_impl)->createLSSerializer()),
#endif
    _errorHandler()
{
    // install error handler
#if XERCES_VERSION_MAJOR < 3
    _writer->setErrorHandler(&_errorHandler);
#else
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMErrorHandler, &_errorHandler);
#endif
}

XMLWriter::~XMLWriter() 
{
    //
    //  Delete the writer.
    //
    _writer->release();
}

#if XERCES_VERSION_MAJOR < 3
void XMLWriter::setFilter(xercesc::DOMWriterFilter* filter)
#else
void XMLWriter::setFilter(xercesc::DOMLSSerializerFilter* filter)
#endif
{
    _writer->setFilter(filter);
}

void XMLWriter::setDiscardDefaultContent(bool val) 
{
#if XERCES_VERSION_MAJOR < 3
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTDiscardDefaultContent,val);
#else
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTDiscardDefaultContent,val);
#endif
}

void XMLWriter::setEntities(bool val)
{
#if XERCES_VERSION_MAJOR < 3
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTEntities,val);
#else
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTEntities,val);
#endif
}

void XMLWriter::setCanonicalForm(bool val)
{
#if XERCES_VERSION_MAJOR < 3
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTCanonicalForm,val);
#else
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTCanonicalForm,val);
#endif
}

void XMLWriter::setPrettyPrint(bool val)
{
#if XERCES_VERSION_MAJOR < 3
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint,val);
#else
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint,val);
#endif
}

void XMLWriter::setNormalizeCharacters(bool val)
{
#if XERCES_VERSION_MAJOR < 3
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTNormalizeCharacters,val);
#else
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTNormalizeCharacters,val);
#endif
}

void XMLWriter::setSplitCDATASections(bool val)
{
#if XERCES_VERSION_MAJOR < 3
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTSplitCdataSections,val);
#else
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTSplitCdataSections,val);
#endif
}

void XMLWriter::setValidation(bool val)
{
#if XERCES_VERSION_MAJOR < 3
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTValidation,val);
#else
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTValidation,val);
#endif
}

void XMLWriter::setWhitespaceInElement(bool val)
{
#if XERCES_VERSION_MAJOR < 3
    _writer->setFeature(xercesc::XMLUni::fgDOMWRTWhitespaceInElementContent,val);
#else
    _writer->getDomConfig()->setParameter(xercesc::XMLUni::fgDOMWRTWhitespaceInElementContent,val);
#endif
}

#if XERCES_VERSION_MAJOR < 3
void XMLWriter::writeNode(xercesc::XMLFormatTarget* const dest,
    const xercesc::DOMNode& node)
#else
void XMLWriter::writeNode(xercesc::DOMLSOutput* const dest,
    const xercesc::DOMNode& node)
#endif
    throw (nidas::core::XMLException,n_u::IOException)
{
    //reset error count first
    _errorHandler.resetErrors();
#if XERCES_VERSION_MAJOR < 3
    bool ok = _writer->writeNode(dest,node);
#else
    bool ok = _writer->write(&node,dest);
#endif
    const nidas::core::XMLException* xe = _errorHandler.getXMLException();
    if (xe) throw *xe;
    if (!ok) throw XMLException(string("writeNode failed"));
}

void XMLWriter::write(xercesc::DOMDocument*doc, const std::string& fileName)
        throw(nidas::core::XMLException,n_u::IOException)
{
    XMLStringConverter convname(fileName);
    xercesc::LocalFileFormatTarget xmlfile((const XMLCh*)convname);

#if XERCES_VERSION_MAJOR < 3
    writeNode(&xmlfile,*doc);
#else
    xercesc::DOMLSOutput *output;
    output = ((xercesc::DOMImplementationLS*)_impl)->createLSOutput();
    output->setByteStream(&xmlfile);
    output->setSystemId((const XMLCh*)convname);
    writeNode(output,*doc);
    output->release();
#endif
   
}
