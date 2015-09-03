// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_CONNECTIONINFO_H
#define NIDAS_CORE_CONNECTIONINFO_H

#include <nidas/util/Inet4NetworkInterface.h>
#include <nidas/util/Inet4SocketAddress.h>

namespace nidas { namespace core {

/**
 * Extra information associated with an IOChannel concerning the connection.
 */
class ConnectionInfo
{
public:

    ConnectionInfo(): _remotesaddr(),_destAddr(),_iface() {}

    ConnectionInfo(const nidas::util::Inet4SocketAddress& remote,
        const nidas::util::Inet4Address& dest,
        const nidas::util::Inet4NetworkInterface iface):
        _remotesaddr(remote),_destAddr(dest),_iface(iface)
    {
    }

    /**
     * The remote address of connection.
     */
    nidas::util::Inet4SocketAddress getRemoteSocketAddress() const { return _remotesaddr; }

    void setRemoteSocketAddress(const nidas::util::Inet4SocketAddress& val) { _remotesaddr = val; }

    /**
     * The destination address of the packet. For a received unicast packet
     * it will be the address of the local interface.  Or the destination
     * address could be a multicast address or a broadcast address.
     */
    const nidas::util::Inet4Address& getDestinationAddress() const { return _destAddr; }

    void setDestinationAddress(const nidas::util::Inet4Address& val) { _destAddr = val; }

    /**
     * The interface that the packet was received on.
     */
    const nidas::util::Inet4NetworkInterface& getInterface() const { return _iface; }

    void setInterface(const nidas::util::Inet4NetworkInterface& val ) { _iface = val; }

private:
    nidas::util::Inet4SocketAddress _remotesaddr;

    nidas::util::Inet4Address _destAddr;

    nidas::util::Inet4NetworkInterface _iface;

};

}}	// namespace nidas namespace core

#endif
