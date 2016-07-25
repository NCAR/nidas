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
#include <xercesc/util/OutOfMemoryException.hpp>
#include <xercesc/framework/MemBufInputSource.hpp>

#include <nidas/util/Socket.h>

#include "Datagrams.h"
#include "XMLStringConverter.h"
#include "StatusListener.h"
#include "StatusHandler.h"

#include <nidas/util/Logger.h>

#include <iostream>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

StatusListener::StatusListener():Thread("StatusListener"),
    _clocksMutex(), _statusMutex(),
    _clocks(),_oldclk(),_nstale(),_status(),_samplePool(),
    _parser(0), _handler(new StatusHandler(this))
{
    unblockSignal(SIGUSR1);

    // initialize the mutex locks
    n_u::Synchronized clocks_autoLock(_clocksMutex);
    n_u::Synchronized status_autoLock(_statusMutex);

    // initialize the XML4C2 system for the SAX2 parser
    try {
        xercesc::XMLPlatformUtils::Initialize();
    }
    catch(const xercesc::XMLException & toCatch)
    {
        PLOG(("Error during XML initialization! :") 
            << (string) XMLStringConverter(toCatch.getMessage()));
        return;
    }
    // create a SAX2 parser object
    _parser = xercesc::XMLReaderFactory::createXMLReader();
    _parser->setContentHandler(_handler);
    _parser->setLexicalHandler(_handler);
    _parser->setErrorHandler(_handler);
}

StatusListener::~StatusListener()
{
    delete _handler;
    delete _parser;
    xercesc::XMLPlatformUtils::Terminate();
}

int StatusListener::run() throw(n_u::Exception)
{
    // create a socket to listen for the XML status messages
    n_u::MulticastSocket msock(NIDAS_STATUS_PORT_UDP);
    try {
  	// throws UnknownHostException, but shouldn't because
        // NIDAS_MULTICAST_ADDR is a string in dot notation.
        n_u::Inet4Address mcaddr =
            n_u::Inet4Address::getByName(NIDAS_MULTICAST_ADDR);
        list < n_u::Inet4NetworkInterface > interfaces = msock.getInterfaces();
        list < n_u::Inet4NetworkInterface >::const_iterator ii =
            interfaces.begin();
        for (; ii != interfaces.end(); ++ii) {
            n_u::Inet4NetworkInterface iface = *ii;
            int iflags = iface.getFlags();
            // join interfaces that support MULTICAST or LOOPBACK
            if (iflags & IFF_UP && iflags & (IFF_MULTICAST | IFF_LOOPBACK)) {
                try {
                    msock.joinGroup(mcaddr, iface);
                    ILOG(("") << "joined multicast group " <<
                            n_u::Inet4SocketAddress(mcaddr,msock.getLocalPort()).toAddressString() <<
                            " on interface " << iface.getName());
                }
                catch(const n_u::IOException& e) {
                    PLOG(("") << "joinGroup " <<
                            n_u::Inet4SocketAddress(mcaddr,msock.getLocalPort()).toAddressString() <<
                            " on interface " << iface.getName() << ": " << e.what());
                    return RUN_EXCEPTION;
                }
            }
        }
    }
    catch(const n_u::IOException& e) {
        PLOG(("") << msock.getLocalSocketAddress().toAddressString() << ": getInterfaces: " << e.what());
        return RUN_EXCEPTION;
    }
    n_u::Inet4SocketAddress from;
    char buf[8192];

    for (; !isInterrupted();) {
        // blocking read on multicast socket
        size_t l;
        try {
            l = msock.recvfrom(buf, sizeof(buf), 0, from);
            // if null terminated, subtract 1 from length
            if (l > 0 && buf[l-1] == 0) l--;
        }
        catch(const n_u::IOException& e) {
            PLOG(("%s: %s",msock.getLocalSocketAddress().toAddressString().c_str(),e.what()));
            msock.close();
            return RUN_EXCEPTION;
        }

        //    cerr << buf << endl;
        // convert char* buf into a parse-able memory stream
        try {
            xercesc::MemBufInputSource memBufIS((const XMLByte *) buf,l,"socket buffer", false);
            _parser->parse(memBufIS);
        }
        catch(const xercesc::OutOfMemoryException &)
        {
            PLOG(("OutOfMemoryException"));
            msock.close();
            return RUN_EXCEPTION;
        }
        catch(const xercesc::XMLException & e) {
            PLOG(("Error during parsing memory stream: ") <<
                (string) XMLStringConverter(e.getMessage()));
            msock.close();
            return RUN_EXCEPTION;
        }
    }
    return RUN_OK;
}

// ----------

void GetClocks::execute(XmlRpc::XmlRpcValue & /* params */,
                        XmlRpc::XmlRpcValue & result)
{
//cerr << "GetClocks" << endl;
    map < string, string >::iterator mi;
    _listener->_clocksMutex.lock();
    for (mi = _listener->_clocks.begin();
         mi != _listener->_clocks.end(); ++mi) {
        // only mark stalled numeric time changes as '-- stopped --'
        if (mi->second[0] == '2')       // as in 2006 ...
        {
            if (mi->second.compare(_listener->_oldclk[mi->first]) == 0) {
                if (_listener->_nstale[mi->first]++ > 3)
                    mi->second = "------ stopped ------";
            } else
                _listener->_nstale[mi->first] = 0;
        }

        _listener->_oldclk[mi->first] = mi->second;
        result[mi->first] = mi->second;
    }
    _listener->_clocksMutex.unlock();
}

void GetStatus::execute(XmlRpc::XmlRpcValue & params,
                        XmlRpc::XmlRpcValue & result)
{
    std::string & arg = params[0];
//cerr << "GetStatus for " << arg << endl;
    _listener->_statusMutex.lock();
    result = _listener->_status[arg];
    _listener->_statusMutex.unlock();
}
