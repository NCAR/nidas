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

#include <nidas/util/Inet4SocketAddress.h>

#include <sstream>
#include <cstring>

using namespace nidas::util;
using namespace std;

Inet4SocketAddress::Inet4SocketAddress():
    _sockaddr()
{
    _sockaddr.sin_family = AF_INET;
}

Inet4SocketAddress::Inet4SocketAddress(int port):
    _sockaddr()
{
    _sockaddr.sin_family = AF_INET;
    _sockaddr.sin_port = htons(port);
    _sockaddr.sin_addr.s_addr = INADDR_ANY;
}

Inet4SocketAddress::Inet4SocketAddress(const Inet4Address& addr, int port):
    _sockaddr()
{
    _sockaddr.sin_family = AF_INET;
    _sockaddr.sin_port = htons(port);
    _sockaddr.sin_addr = *addr.getInAddrPtr();
}

Inet4SocketAddress::Inet4SocketAddress(const struct sockaddr_in* a):
	_sockaddr(*a)
{
}

/* copy constructor */
Inet4SocketAddress::Inet4SocketAddress(const Inet4SocketAddress& x):
    SocketAddress(),_sockaddr(x._sockaddr)
{
}

/* assignment operator */
Inet4SocketAddress& Inet4SocketAddress::operator=(const Inet4SocketAddress& rhs)
{
    if (this != &rhs) {
        *(SocketAddress*) this = rhs;
        _sockaddr = rhs._sockaddr;
    }
    return *this;
}

/* clone */
Inet4SocketAddress* Inet4SocketAddress::clone() const
{
    return new Inet4SocketAddress(*this);
}

std::string Inet4SocketAddress::toString() const
{
    std::ostringstream ost;
    ost << "inet:" << getInet4Address().getHostName() <<
	    ':' << getPort();
    return ost.str();
}

std::string Inet4SocketAddress::toAddressString() const
{
    std::ostringstream ost;
    ost << "inet:" << getInet4Address().getHostAddress() <<
	    ':' << getPort();
    return ost.str();
}

/**
 * Comparator operator for addresses. Useful if this
 * address is a key in an STL map.
 */
bool Inet4SocketAddress::operator < (const Inet4SocketAddress& x) const {
    if(ntohl(_sockaddr.sin_addr.s_addr) <
	    ntohl(x._sockaddr.sin_addr.s_addr)) return true;
    if (_sockaddr.sin_addr.s_addr == x._sockaddr.sin_addr.s_addr)
	return ntohs(_sockaddr.sin_port) < ntohs(x._sockaddr.sin_port);
    return false;
}

/**
 * Equality operator for addresses.
 */
bool Inet4SocketAddress::operator == (const Inet4SocketAddress& x) const {
    return _sockaddr.sin_addr.s_addr == x._sockaddr.sin_addr.s_addr &&
	_sockaddr.sin_port == x._sockaddr.sin_port;
}
