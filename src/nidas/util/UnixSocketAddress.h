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

#ifndef NIDAS_UTIL_UNIXSOCKETADDRESS
#define NIDAS_UTIL_UNIXSOCKETADDRESS

#include "SocketAddress.h"
#include <sys/un.h>

namespace nidas { namespace util {

/**
 * An AF_UNIX socket address.  Do "man 7 unix" from linux for info
 * on unix sockets.
 */
class UnixSocketAddress: public SocketAddress {
public:

    /**
     * Default constructor, path of "", empty string.
     */
    // UnixSocketAddress();

    /**
     * Constructor, with path string.
     */
    UnixSocketAddress(const std::string& path);

    /**
     * Constructor with pointer to sockaddr_un.
     */
    UnixSocketAddress(const struct sockaddr_un* sockaddr);

    /**
     * Copy constructor.
     */
    UnixSocketAddress(const UnixSocketAddress&);

    /**
     * Assignment operator.
     */
    UnixSocketAddress& operator=(const UnixSocketAddress& x);

    /**
     * Virtual constructor.
     */
    UnixSocketAddress* clone() const;

    /**
     * Return the address family, AF_UNIX.
     */
    int getFamily() const { return AF_UNIX; }

    /**
     * AF_UNIX addresses don't have ports, return -1.
     */
    int getPort() const { return -1; }

    /**
     * Provide non-const pointer to struct sockaddr_un.  This is
     * needed for recvfrom methods.  recvfrom updates
     * the struct sockaddr_un, so we can't cache the other
     * portions of the address.
     */
    struct sockaddr* getSockAddrPtr() { return (struct sockaddr*) &_sockaddr; }

    /**
     * Provide const pointer to struct sockaddr_un.
     */
    const struct sockaddr* getConstSockAddrPtr() const
    {
        return (const struct sockaddr*) &_sockaddr;
    }

    socklen_t getSockAddrLen() const { return sizeof(_sockaddr); }

    /**
     * Java style toString: returns "unix:path"
     */
    std::string toString() const;

    /**
     * Java style toString: also returns "unix:path"
     */
    std::string toAddressString() const;

    /**
     * Comparator operator for addresses. Useful if this
     * address is a key in an STL map.
     */
    bool operator < (const SocketAddress& x) const;

    /**
     * Equality operator for addresses.
     */
    bool operator == (const SocketAddress& x) const;

protected:
    std::string _path;

    struct sockaddr_un _sockaddr;
};

}}	// namespace nidas namespace util

#endif
