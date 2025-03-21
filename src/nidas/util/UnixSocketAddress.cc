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

#include "UnixSocketAddress.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>

using namespace nidas::util;
using namespace std;

UnixSocketAddress::UnixSocketAddress(const std::string& path):
    _path(path),_sockaddr()
{

    _sockaddr.sun_family = getFamily();

    string tmppath = _path;
    if (_path.length() > 5 && !_path.compare(0,5,"unix:"))
        tmppath = _path.substr(5);

    unsigned int l = tmppath.length();
    unsigned int lpath = sizeof(_sockaddr.sun_path);

    // If the path is empty or is not an absolute path,
    // create an "abstract" AF_UNIX address, as described
    // in "man 7 unix"
    if (l == 0 || tmppath[0] != '/') {
        lpath--;        // sun_path will have leading null
	if (l > lpath) l = lpath;
        _sockaddr.sun_path[0] = '\0';
	memcpy(_sockaddr.sun_path+1,tmppath.c_str(),l);
    }
    else {
        // copy trailing null
	if (++l > lpath) l = lpath;
	strncpy(_sockaddr.sun_path,tmppath.c_str(),l);
    }
}

UnixSocketAddress::UnixSocketAddress(const struct sockaddr_un* a):
	_path(),_sockaddr(*a)
{
    assert(a->sun_family == getFamily());
    if (_sockaddr.sun_path[0] == '\0') {
        // "abstract" AF_UNIX address
	int len;
	for (len = sizeof(_sockaddr.sun_path); len > 0; len--)
	    if (_sockaddr.sun_path[len-1] != '\0') break;

	ostringstream ost;
	if (len == 0) ost << "null";
	else
	    for (int i = 1; i < len; i++) {
		if (isprint(_sockaddr.sun_path[i])) ost << _sockaddr.sun_path[i];
		else ost << " 0x" << hex << setw(2) << setfill('0') <<
		    (unsigned int)_sockaddr.sun_path[i] << dec;
	    }
	// cerr << "abstract sizeof(_sockaddr.sun_path)=" <<
	//	sizeof(_sockaddr.sun_path) << " len=" << len << endl;
	_path = ost.str();
    }
    else {
	// _path = string(_sockaddr.sun_path);
	ostringstream ost;
	if (!strlen(_sockaddr.sun_path)) ost << "null";
	for (int i = 0; _sockaddr.sun_path[i] != '\0'; i++) {
	    // cerr << "i=" << i << " path=" << _sockaddr.sun_path[i] << endl;
	    if (isprint(_sockaddr.sun_path[i])) ost << _sockaddr.sun_path[i];
	    else ost << " 0x" << hex << setw(2) << setfill('0') <<
                 (unsigned int)_sockaddr.sun_path[i] << dec;
	}
	// cerr << "nonabstract, sizeof(_sockaddr.sun_path)=" <<
	//	sizeof(_sockaddr.sun_path) << endl;
	_path = ost.str();
    }
#ifdef DEBUG
    cerr << "_sockaddr.sun_path[0]=0x" <<
        hex << (int) _sockaddr.sun_path[0] << dec <<
        " path=" << _path << endl;
#endif
}

/* copy constructor */
UnixSocketAddress::UnixSocketAddress(const UnixSocketAddress& x):
    SocketAddress(),_path(x._path),_sockaddr(x._sockaddr)
{
}

/* clone */
UnixSocketAddress* UnixSocketAddress::clone() const 
{
    return new UnixSocketAddress(*this);
}

/* assignment operator */
UnixSocketAddress& UnixSocketAddress::operator=(const UnixSocketAddress& rhs)
{
    if (this != &rhs) {
        *(SocketAddress*) this = rhs;
        _path = rhs._path;
        _sockaddr = rhs._sockaddr;
    }
    return *this;
}

std::string UnixSocketAddress::toString() const
{
    std::ostringstream ost;
    ost << "unix:" << _path;
    return ost.str();
}

std::string UnixSocketAddress::toAddressString() const
{
    return toString();
}

/**
 * Comparator operator for addresses. Useful if this
 * address is a key in an STL map.
 */
bool UnixSocketAddress::operator < (const SocketAddress& x) const {
    if (getFamily() != x.getFamily()) return getFamily() < x.getFamily();
    const UnixSocketAddress& ux =
	    static_cast<const UnixSocketAddress&>(x);
    return _path.compare(ux._path) < 0;
}

/**
 * Equality operator for addresses.
 */
bool UnixSocketAddress::operator == (const SocketAddress& x) const {
    if (getFamily() != x.getFamily()) return false;
    const UnixSocketAddress& ux =
	    static_cast<const UnixSocketAddress&>(x);
    return _path.compare(ux._path) == 0;
}
