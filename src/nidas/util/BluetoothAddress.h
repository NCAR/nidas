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

#include <nidas/Config.h>

#ifdef HAVE_BLUETOOTH_RFCOMM_H

#ifndef NIDAS_UTIL_BLUETOOTHADDRESS
#define NIDAS_UTIL_BLUETOOTHADDRESS

#include <bluetooth/bluetooth.h>
#include <sys/types.h>
#include <regex.h>

#include <string>
#include <map>

#include "ThreadSupport.h"
#include "UnknownHostException.h"

namespace nidas { namespace util {

/**
 * Support for Bluetooth addresses.
 */
class BluetoothAddress {
public:

    /**
     * Return a BluetoothAddress given a string in the format 
     *  00:00:00:00:00:00, six pairs of hex digits, separated by colons.
     *
     * @throws UnknownHostException
     */
    static BluetoothAddress getByName(const std::string& addrstr);

    /**
     * Convert a BluetoothAddress to a string, in the format
     *  00:00:00:00:00:00, six pairs of hex digits, separated by colons.
     *
     * @throw()
     */
    static std::string getHostName(const BluetoothAddress& addr);

    /**
     * Default constructor. Creates address: 0:0:0:0:0:0, aka: BDADDR_ANY.
     */
    BluetoothAddress();

    /**
     * Construct an address from a pointer to the C bdaddr_t structure
     */
    BluetoothAddress(const bdaddr_t*);

    /**
     * Return string containing address in colon notation: a:b:c:d:e:f
     */
    std::string getHostName() const;

    /**
     * Return const pointer to struct in_addr.
     */
    const bdaddr_t* getBdAddrPtr() const { return &_bdaddr; }

    /**
     * Get the structure containing the 6 byte address.
     */
    const bdaddr_t getBdAddr() const { return _bdaddr; }

    /**
     * Comparator operator for addresses.
     */
    bool operator < (const BluetoothAddress& x) const;

    /**
     * Equality operator for addresses.
     */
    bool operator == (const BluetoothAddress& x) const;

    /**
     * Inequality operator for addresses.
     */
    bool operator != (const BluetoothAddress& x) const;

private:

    static ::regex_t* _addrPreg;

    static nidas::util::Mutex _staticMutex;

    static std::map<std::string,BluetoothAddress> _addrMap;

    /**
     * The bluetooth address, in network byte order.
     */
    bdaddr_t _bdaddr;

};

}}	// namespace nidas namespace util

#endif
#endif
