// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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

#include <nidas/util/BluetoothRFCommSocketAddress.h>

#ifdef HAVE_BLUETOOTH_RFCOMM_H

#include <sstream>
#include <cstring>
#include <iostream>

using namespace nidas::util;
using namespace std;

BluetoothRFCommSocketAddress::BluetoothRFCommSocketAddress(): _sockaddr()
{
    _sockaddr.rc_family = AF_BLUETOOTH;
}

BluetoothRFCommSocketAddress::BluetoothRFCommSocketAddress(int channel):
    _sockaddr()
{
    _sockaddr.rc_family = AF_BLUETOOTH;
    _sockaddr.rc_channel = channel;
}

BluetoothRFCommSocketAddress::BluetoothRFCommSocketAddress(const string& addrstr,
    int channel): _sockaddr()
{
    _sockaddr.rc_family = AF_BLUETOOTH;
    ::str2ba(addrstr.c_str(),&_sockaddr.rc_bdaddr);
    _sockaddr.rc_channel = channel;
}

BluetoothRFCommSocketAddress::BluetoothRFCommSocketAddress(const BluetoothAddress& addr, int channel): _sockaddr()
{
    _sockaddr.rc_family = AF_BLUETOOTH;
    ::bacpy(&_sockaddr.rc_bdaddr,addr.getBdAddrPtr());
    // _sockaddr.rc_bdaddr = *addr.getBdAddrPtr();
    _sockaddr.rc_channel = channel;
}

BluetoothRFCommSocketAddress::BluetoothRFCommSocketAddress(const struct sockaddr_rc* a):
	_sockaddr(*a)
{
}

/* copy constructor */
BluetoothRFCommSocketAddress::BluetoothRFCommSocketAddress(const BluetoothRFCommSocketAddress& x):
    SocketAddress(),_sockaddr(x._sockaddr)
{
}

/* assignment operator */
BluetoothRFCommSocketAddress& BluetoothRFCommSocketAddress::operator=(const BluetoothRFCommSocketAddress& rhs)
{
    if (this != &rhs) {
        *(SocketAddress*) this = rhs;
        _sockaddr = rhs._sockaddr;
    }
    return *this;
}

/* clone */
BluetoothRFCommSocketAddress* BluetoothRFCommSocketAddress::clone() const
{
    return new BluetoothRFCommSocketAddress(*this);
}

std::string BluetoothRFCommSocketAddress::toString() const
{
    std::ostringstream ost;
    ost << "btspp:" << getBluetoothAddress().getHostName() <<
	    ':' << getPort();
    return ost.str();
}

std::string BluetoothRFCommSocketAddress::toAddressString() const
{
    std::ostringstream ost;
    ost << "btspp:" << getBluetoothAddress().getHostName() <<
	    ':' << getPort();
    return ost.str();
}

/**
 * Comparator operator for addresses. Useful if this
 * address is a key in an STL map.
 */
bool BluetoothRFCommSocketAddress::operator < (const BluetoothRFCommSocketAddress& x) const {
    int i = ::bacmp(&_sockaddr.rc_bdaddr,&x._sockaddr.rc_bdaddr);
    if (i < 0) return true;
    if (i == 0) return _sockaddr.rc_channel < x._sockaddr.rc_channel;
    return false;
}

/**
 * Equality operator for addresses.
 */
bool BluetoothRFCommSocketAddress::operator == (const BluetoothRFCommSocketAddress& x) const {
    return ::bacmp(&_sockaddr.rc_bdaddr,&x._sockaddr.rc_bdaddr) == 0 &&
	_sockaddr.rc_channel == x._sockaddr.rc_channel;
}
#endif
