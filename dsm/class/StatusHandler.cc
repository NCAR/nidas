/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/
#include <xercesc/sax2/Attributes.hpp>

#include <DSMSensor.h>
#include <StatusHandler.h>
#include <StatusListener.h>
#include <XMLStringConverter.h>

#include <iostream> // cerr
#include <fstream>  // ofstream
#include <map>

using namespace std;
using namespace dsm;

// ---------------------------------------------------------------------------
//  StatusHandler: Overrides of the SAX ErrorHandler interface
// ---------------------------------------------------------------------------
void StatusHandler::warning(const SAXParseException& e)
{
  cerr << "\nWarning at file " << XMLStringConverter(e.getSystemId())
       << ", line " << e.getLineNumber()
       << ", char " << e.getColumnNumber()
       << "\n  Message: " << XMLStringConverter(e.getMessage()) << endl;
}

void StatusHandler::error(const SAXParseException& e)
{
  cerr << "\nError at file " << XMLStringConverter(e.getSystemId())
       << ", line " << e.getLineNumber()
       << ", char " << e.getColumnNumber()
       << "\n  Message: " << XMLStringConverter(e.getMessage()) << endl;
}

void StatusHandler::fatalError(const SAXParseException& e)
{
  cerr << "\nFatal Error at file " << XMLStringConverter(e.getSystemId())
       << ", line " << e.getLineNumber()
       << ", char " << e.getColumnNumber()
       << "\n  Message: " << XMLStringConverter(e.getMessage()) << endl;
}


// ---------------------------------------------------------------------------
//  StatusHandler: Overrides of the SAX DocumentHandler interface
// ---------------------------------------------------------------------------
void StatusHandler::startElement(const XMLCh* const uri,
                                 const XMLCh* const localname,
                                 const XMLCh* const qname,
                                 const Attributes&  attributes)
{
//   cerr << "qname: " << XMLStringConverter(qname) << endl;
//   unsigned int len = attributes.getLength();
//   for (unsigned int index = 0; index < len; index++) {
//     cerr << "attributes.getQName(" << index << "): ";
//     cerr << XMLStringConverter(attributes.getQName(index)) << endl;
//     cerr << "attributes.getValue(" << index << "): ";
//     cerr << XMLStringConverter(attributes.getValue(index)) << endl;
//   }
  if (!strcmp(XMLStringConverter(qname),"clock"))       _element = TIME;
  else if (!strcmp(XMLStringConverter(qname),"status")) _element = STATUS;
}

void StatusHandler::characters(const     XMLCh* const    chars,
                               const   unsigned int    length)
{
  switch (_element) {
  case TIME:
    _listener->_clocks[_src] = XMLStringConverter(chars);
    break;

  case STATUS:
    _listener->_status[_src] = XMLStringConverter(chars);
    break;
  }
}
