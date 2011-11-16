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

#ifndef NIDAS_UTIL_BLUETOOTHADDRESS
#define NIDAS_UTIL_BLUETOOTHADDRESS

#include <bluetooth/bluetooth.h>
#include <sys/types.h>
#include <regex.h>

#include <string>
#include <map>

#include <nidas/util/ThreadSupport.h>
#include <nidas/util/UnknownHostException.h>

namespace nidas { namespace util {

/**
 * Support for Bluetooth addresses.
 */
class BluetoothAddress {
public:

    /**
     * Return a BluetoothAddress given a string in the format 
     *  00:00:00:00:00:00, six pairs of hex digits, separated by colons.
     */
    static BluetoothAddress getByName(const std::string& addrstr)
  	throw(UnknownHostException);

    /**
     * Convert a BluetoothAddress to a string, in the format
     *  00:00:00:00:00:00, six pairs of hex digits, separated by colons.
     */
    static std::string getHostName(const BluetoothAddress& addr) throw();

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
