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
#include <xercesc/sax2/Attributes.hpp>

#include <nidas/core/DSMSensor.h>
#include <nidas/core/StatusHandler.h>
#include <nidas/core/StatusListener.h>
#include <nidas/core/XMLStringConverter.h>

#include <nidas/util/Logger.h>

#include <iostream>             // cerr
#include <fstream>              // ofstream
#include <map>

using namespace std;
using namespace nidas::core;

// ---------------------------------------------------------------------------
//  StatusHandler: Overrides of the SAX ErrorHandler interface
// ---------------------------------------------------------------------------
void StatusHandler::warning(const xercesc::SAXParseException & e)
{
    WLOG(("Warning at file ") << (string) XMLStringConverter(e.getSystemId())
        << ", line " << e.getLineNumber()
        << ", char " << e.getColumnNumber()
        << ": " << (string) XMLStringConverter(e.getMessage()));
}

void StatusHandler::error(const xercesc::SAXParseException & e)
{
    PLOG(("Error at file ") << (string) XMLStringConverter(e.getSystemId())
        << ", line " << e.getLineNumber()
        << ", char " << e.getColumnNumber()
        << ": " << (string) XMLStringConverter(e.getMessage()));
}

void StatusHandler::fatalError(const xercesc::SAXParseException & e)
{
    PLOG(("Fatal Error at file ") << (string) XMLStringConverter(e.getSystemId())
        << ", line " << e.getLineNumber()
        << ", char " << e.getColumnNumber()
        << ": " << (string) XMLStringConverter(e.getMessage()));
}


// ---------------------------------------------------------------------------
//  StatusHandler: Overrides of the SAX DocumentHandler interface
// ---------------------------------------------------------------------------
void StatusHandler::startElement(const XMLCh * const /* uri */,
        const XMLCh * const /* localname */,
        const XMLCh * const qname,
        const xercesc::Attributes & /* attributes */)
{
    //   cerr << "qname: " << XMLStringConverter(qname) << endl;
    //   unsigned int len = attributes.getLength();
    //   for (unsigned int index = 0; index < len; index++) {
    //     cerr << "attributes.getQName(" << index << "): ";
    //     cerr << XMLStringConverter(attributes.getQName(index)) << endl;
    //     cerr << "attributes.getValue(" << index << "): ";
    //     cerr << XMLStringConverter(attributes.getValue(index)) << endl;
    //   }
    if ((string) XMLStringConverter(qname) == "name")
        _element = SOURCE;
    else if ((string) XMLStringConverter(qname) == "clock")
        _element = TIME;
    else if ((string) XMLStringConverter(qname) == "status")
        _element = STATUS;
    else if ((string) XMLStringConverter(qname) == "samplepool")
        _element = SAMPLEPOOL;
}

void StatusHandler::endElement(const XMLCh * const /* uri */,
        const XMLCh * const /* localname */,
        const XMLCh * const /* qname */)
{
    _element = NONE;
}

void StatusHandler::characters(const XMLCh * const chars,
#if XERCES_VERSION_MAJOR < 3
        const unsigned int /* length */)
#else
const XMLSize_t /* length */)
#endif
{
    switch (_element) {
    case SOURCE:
        _src = XMLStringConverter(chars);
        break;

    case TIME:
        _listener->_clocksMutex.lock();
        _listener->_clocks[_src] = XMLStringConverter(chars);
        _listener->_clocksMutex.unlock();
        break;

    case STATUS:
        _listener->_statusMutex.lock();
        _listener->_status[_src] = XMLStringConverter(chars);
        _listener->_statusMutex.unlock();
        break;

    case SAMPLEPOOL:
        _listener->_samplePool[_src] = XMLStringConverter(chars);
        break;

    case NONE:
        break;
    }
}
