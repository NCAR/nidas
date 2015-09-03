/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2012, Copyright University Corporation for Atmospheric Research
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
#include <nidas/core/requestXMLConfig.h>
#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLConfigInput.h>
#include <nidas/core/XMLFdInputSource.h>
#include <nidas/util/Logger.h>

namespace n_c = nidas::core;
namespace n_u = nidas::util;

extern xercesc::DOMDocument* n_c::requestXMLConfig(bool all,
  const n_u::Inet4SocketAddress& mcastAddr, sigset_t* signalMask)
 throw(n_u::Exception)
{
    std::auto_ptr<n_c::XMLParser> parser(new n_c::XMLParser());
    // throws XMLException

    // If parsing xml received from a server over a socket,
    // turn off validation - assume the server has validated the XML.
    parser->setDOMValidation(false);
    parser->setDOMValidateIfSchema(false);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(false);
    parser->setXercesSchemaFullChecking(false);
    parser->setXercesHandleMultipleImports(true);
    parser->setXercesDoXInclude(true);
    parser->setDOMDatatypeNormalization(false);

    n_c::XMLConfigInput xmlRequestSocket;
    if (all) xmlRequestSocket.setRequestType(XML_ALL_CONFIG);
    xmlRequestSocket.setInet4McastSocketAddress(mcastAddr);

    std::auto_ptr<n_u::Socket> configSock;
    n_u::Inet4PacketInfoX pktinfo;

    try {
        if ( signalMask != (sigset_t*)0 )
            pthread_sigmask(SIG_UNBLOCK,signalMask,0);
        configSock.reset(xmlRequestSocket.connect(pktinfo));
        if ( signalMask != (sigset_t*)0 )
            pthread_sigmask(SIG_BLOCK,signalMask,0);
    }
    catch(...) {
        if ( signalMask != (sigset_t*)0 )
            pthread_sigmask(SIG_BLOCK,signalMask,0);
        xmlRequestSocket.close();
        throw;
    }
    xmlRequestSocket.close();

    xercesc::DOMDocument* doc = 0;
    try {
        std::string sockName = configSock->getRemoteSocketAddress().toAddressString();
        DLOG(("requestXMLConfig: sockName: ") << sockName);

        n_c::XMLFdInputSource sockSource(sockName,configSock->getFd());
        doc = parser->parse(sockSource);
        configSock->close();
    }
    catch(const n_u::IOException& e) {
        PLOG(("requestXMLConfig:") << e.what());
        configSock->close();
        throw e;
    }
    catch(const n_c::XMLException& xe) {
        PLOG(("requestXMLConfig:") << xe.what());
        configSock->close();
        throw xe;
    }
    catch(...) {
        configSock->close();
        throw;
    }
    return doc;
}
