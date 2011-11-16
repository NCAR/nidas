// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

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
    _sockaddr(x._sockaddr)
{
}

/* assignment operator */
Inet4SocketAddress& Inet4SocketAddress::operator=(const Inet4SocketAddress& rhs)
{
    if (this != &rhs) {
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
