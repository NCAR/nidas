/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/
#ifndef DSM_STATUSHANDLER_H
#define DSM_STATUSHANDLER_H

#include <xercesc/sax2/XMLReaderFactory.hpp>     // provides SAX2XMLReader
#include <xercesc/sax2/DefaultHandler.hpp>
#include <atdUtil/Exception.h>
#include <string>

using namespace xercesc;
using namespace std;

namespace dsm {

class StatusListener;

/**
 * This class implements handling routines for the SAX2 parser.
 */
class StatusHandler : public DefaultHandler
{
public:
  StatusHandler(StatusListener* lstn): _listener(lstn) {}

    // -----------------------------------------------------------------------
    //  Implementations of the SAX ErrorHandler interface
    // -----------------------------------------------------------------------
    void warning(const SAXParseException& exc);
    void error(const SAXParseException& exc);
    void fatalError(const SAXParseException& exc);


    // -----------------------------------------------------------------------
    //  Implementations of the SAX DocumentHandler interface
    // -----------------------------------------------------------------------
    void startElement(const XMLCh* const uri,
                      const XMLCh* const localname,
                      const XMLCh* const qname,
                      const Attributes&  attributes);

    void characters(const XMLCh* const chars, const unsigned int length);

    /// Call this prior to parsing to define the sources host name.
    void setSource(const string& val) { _src = val; }
  
    /// reference to listener thread
    StatusListener* _listener;

 protected:
    enum { TIME, STATUS } _element;

    /// host name of socket source
    string _src;
};

}

#endif
