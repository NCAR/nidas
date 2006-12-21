/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-12-13 15:22:53 -0700 (Wed, 13 Dec 2006) $

    $LastChangedRevision: 3589 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/core/XMLConfigWriter.cc $
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

XMLWriter::XMLWriter()
	throw (nidas::core::XMLException)
{
    impl = XMLImplementation::getImplementation();
    try {
        // throws DOMException
        writer = ((xercesc::DOMImplementationLS*)impl)->createDOMWriter();
    }
    catch (const xercesc::DOMException& e) {
        throw nidas::core::XMLException(e);
    }
    // Create our error handler and install it
    writer->setErrorHandler(&errorHandler);
}

XMLWriter::~XMLWriter() 
{
    //
    //  Delete the writer.
    //
    writer->release();
}

void XMLWriter::setDiscardDefaultContent(bool val) 
{
    writer->setFeature(xercesc::XMLUni::fgDOMWRTDiscardDefaultContent,val);
}

void XMLWriter::setEntities(bool val)
{
    writer->setFeature(xercesc::XMLUni::fgDOMWRTEntities,val);
}

void XMLWriter::setCanonicalForm(bool val)
{
    writer->setFeature(xercesc::XMLUni::fgDOMWRTCanonicalForm,val);
}

void XMLWriter::setPrettyPrint(bool val)
{
    writer->setFeature(xercesc::XMLUni::fgDOMWRTFormatPrettyPrint,val);
}

void XMLWriter::setNormalizeCharacters(bool val)
{
    writer->setFeature(xercesc::XMLUni::fgDOMWRTNormalizeCharacters,val);
}

void XMLWriter::setSplitCDATASections(bool val)
{
    writer->setFeature(xercesc::XMLUni::fgDOMWRTSplitCdataSections,val);
}

void XMLWriter::setValidation(bool val)
{
    writer->setFeature(xercesc::XMLUni::fgDOMWRTValidation,val);
}

void XMLWriter::setWhitespaceInElement(bool val)
{
    writer->setFeature(xercesc::XMLUni::fgDOMWRTWhitespaceInElementContent,val);
}

void XMLWriter::write(xercesc::DOMDocument*doc, const std::string& fileName)
        throw(nidas::core::XMLException)
{

    //reset error count first
    errorHandler.resetErrors();

    // XMLStringConverter fname(fileName);
    // LocalFileFormatTarget xmlfile((const XMLCh *) fname);
    xercesc::LocalFileFormatTarget xmlfile(
        (const XMLCh*)XMLStringConverter(fileName));
    try {
        writer->writeNode(&xmlfile,*doc);
        const XMLException* xe = errorHandler.getXMLException();
        if (xe) throw *xe;
    }
    catch (const xercesc::XMLException& e) {
        throw nidas::core::XMLException(e);
    }
    catch (const xercesc::SAXException& e) {
        throw nidas::core::XMLException(e);
    }
    catch (const xercesc::DOMException& e)
    {
        throw nidas::core::XMLException(e);
    }
}
