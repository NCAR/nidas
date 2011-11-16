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

#ifdef HAS_BLUETOOTHRFCOMM_H

#include <nidas/util/BluetoothRFCommSocketAddress.h>

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
    _sockaddr(x._sockaddr)
{
}

/* assignment operator */
BluetoothRFCommSocketAddress& BluetoothRFCommSocketAddress::operator=(const BluetoothRFCommSocketAddress& x)
{
    if (this != &x) {
        _sockaddr = x._sockaddr;
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
