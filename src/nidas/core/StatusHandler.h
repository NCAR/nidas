/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/
#ifndef NIDAS_CORE_STATUSHANDLER_H
#define NIDAS_CORE_STATUSHANDLER_H

#include <xercesc/sax2/XMLReaderFactory.hpp>    // provides SAX2XMLReader
#include <xercesc/sax2/DefaultHandler.hpp>
#include <nidas/util/Exception.h>
#include <string>

namespace nidas { namespace core {

class StatusListener;

/**
 * This class implements handling routines for the SAX2 parser.
 */
class StatusHandler:public xercesc::DefaultHandler
{
  public:
    StatusHandler(StatusListener * lstn):_listener(lstn),
        _element(NONE)
    {
    }

    // -----------------------------------------------------------------------
    //  Implementations of the SAX ErrorHandler interface
    // -----------------------------------------------------------------------
    void warning(const xercesc::SAXParseException & exc);
    void error(const xercesc::SAXParseException & exc);
    void fatalError(const xercesc::SAXParseException & exc);


    // -----------------------------------------------------------------------
    //  Implementations of the SAX DocumentHandler interface
    // -----------------------------------------------------------------------
    void startElement(const XMLCh * const uri,
                      const XMLCh * const localname,
                      const XMLCh * const qname,
                      const xercesc::Attributes & attributes);

    void endElement(const XMLCh * const uri,
                    const XMLCh * const localname,
                    const XMLCh * const qname);

    void characters(const XMLCh * const chars,
                    const unsigned int length);

    /// reference to listener thread
    StatusListener *_listener;

  protected:
    enum
    { SOURCE, TIME, STATUS, SAMPLEPOOL, NONE } _element;

    /// host name of socket source
     std::string _src;
};

}}  // namespace nidas namespace core

#endif
