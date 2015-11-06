// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

#include <list>

#include <nidas/util/Inet4NetworkInterface.h>
#include <nidas/util/Socket.h>

using namespace nidas::util;
using namespace std;

/* static */
Inet4NetworkInterface Inet4NetworkInterface::getInterface(const Inet4Address& addr)
        throw (IOException)
{
    list<Inet4NetworkInterface> ifaces;

    // The Socket constructor, getInterfaces() and close() can all
    // throw IOException.
    Socket tmpsock;
    try {
        ifaces = tmpsock.getInterfaces();
    }
    catch(const IOException& e) {
        tmpsock.close();
        throw;
    }
    tmpsock.close();

    list<Inet4NetworkInterface>::const_iterator ii = ifaces.begin();
    for ( ; ii != ifaces.end(); ++ii) {
        Inet4NetworkInterface iface = *ii;
        if (iface.getAddress() == addr) return iface;
    }

    // not one of my interfaces, return an interface with a negative index.
    Inet4NetworkInterface iface;
    iface._index = -1;
    return iface;
}
