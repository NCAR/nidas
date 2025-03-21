// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_UTIL_INETSOCKETADDRESS
#define NIDAS_UTIL_INETSOCKETADDRESS

#include "SocketAddress.h"
#include "Inet4Address.h"

namespace nidas { namespace util {

/**
 * A IP version 4 socket address, containing a host address,
 * and a port number. The only data element is a struct sockaddr_in,
 * so the default copy constructor and assignment operators 
 * work fine.
 */
class Inet4SocketAddress: public SocketAddress {
public:

    /**
     * Default constructor, address of 0.0.0.0 (INADDR_ANY), port 0
     */
    Inet4SocketAddress();

    /**
     * Address of 0.0.0.0 (INADDR_ANY), with a given port number.
     */
    Inet4SocketAddress(int port);

    Inet4SocketAddress(const Inet4Address&, int port);

    Inet4SocketAddress(const struct sockaddr_in* _sockaddr);

    /**
     * Copy constructor.
     */
    Inet4SocketAddress(const Inet4SocketAddress&);

    /**
     * Assignment operator.
     */
    Inet4SocketAddress& operator=(const Inet4SocketAddress& x);

    /**
     * Virtual constructor.
     */
    Inet4SocketAddress* clone() const;

    /**
     * Return the address family, AF_INET.
     */
    int getFamily() const { return _sockaddr.sin_family; }

    /**
     * Return the port number.
     */
    int getPort() const { return ntohs(_sockaddr.sin_port); }

    /**
     * Set the port number.
     */
    void setPort(int val) { _sockaddr.sin_port = htons((short)val); }

    /**
     * Return the IP address portion.
     */
    Inet4Address getInet4Address() const {
	return Inet4Address(&_sockaddr.sin_addr);
    }

    /**
     * Provide non-const pointer to struct sockaddr_in.  This is
     * needed for recvfrom methods.  recvfrom updates
     * the struct sockaddr_in, so we can't cache the other
     * portions of the address.
     */
    struct sockaddr* getSockAddrPtr() { return (struct sockaddr*) &_sockaddr; }

    /**
     * Provide const pointer to struct sockaddr_in.
     */
    const struct sockaddr* getConstSockAddrPtr() const
    {
        return (const struct sockaddr*) &_sockaddr;
    }

    socklen_t getSockAddrLen() const { return sizeof(_sockaddr); }

    /**
     * Java style toString: returns "inet:hostname:port"
     */
    std::string toString() const;

    /**
     * Java style toString, but no DNS lookup: returns "inet:w.x.y.z:port"
     */
    std::string toAddressString() const;

    /**
     * Comparator operator for addresses. Useful if this
     * address is a key in an STL map.
     */
    bool operator < (const Inet4SocketAddress& x) const;

    /**
     * Equality operator for addresses.
     */
    bool operator == (const Inet4SocketAddress& x) const;

protected:
    struct sockaddr_in _sockaddr;
};

}}	// namespace nidas namespace util

#endif
