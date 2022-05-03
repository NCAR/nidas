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

#ifndef NIDAS_UTIL_INET4ADDRESS
#define NIDAS_UTIL_INET4ADDRESS

#include "UnknownHostException.h"
#include "ThreadSupport.h"

#include <arpa/inet.h>

#include <string>
#include <list>
#include <map>

namespace nidas { namespace util {

/**
 * Support for IP version 4 host address.  This class provides
 * by-name and by-address name service lookup, and
 * caches the names and addresses in static maps.
 */
class Inet4Address {
public:
    /**
     * Static method returning a list of addresses for a name.
     * This is the name-to-address lookup method.
     * @param hostname: either a local hostname, like "linus",
     *        or a fully qualified name, "linus.atd.ucar.edu",
     *		or an address in dot notation: "128.117.80.208".
     *
     * @throws UnknownHostException;
     */
    static std::list<Inet4Address> getAllByName(const std::string& hostname);

    /**
     * Return an address of a name, the first one found by getAllByName.
     *
     * @throws UnknownHostException;
     */
    static Inet4Address getByName(const std::string& hostname);

    /**
     * Do reverse lookup of a name, given an address.
     * This is the address-to-name lookup method.
     * If a host name is not found for the address, returns
     * a string in dot notation: "x.x.x.x" using getHostAddress().
     *
     * @throw()
     */
    static std::string getHostName(const Inet4Address& addr);

    /**
     * Default constructor. Creates address: 0.0.0.0, aka: INADDR_ANY.
     */
    Inet4Address();

    /**
     * Construct an address from a pointer to the C in_addr structure
     */
    Inet4Address(const struct in_addr*);

    /**
     * Construct an address.
     */
    Inet4Address(unsigned int addr);

    /**
     * Return string containing address in dot notation: w.x.y.z
     */
    std::string getHostAddress() const;

    /**
     * Return a hostname for this address. Calls static getHostName()
     * method.
     */
    std::string getHostName() const throw();

    /**
     * Return const pointer to struct in_addr.
     */
    const struct in_addr* getInAddrPtr() const { return &_inaddr; }

    /**
     * Get the structure containing the 4 byte internet version 4 address.
     * To get the actual 4 byte integer value:
     * \code
     *   uint32_t addr = getInAddr().s_addr;
     * \endcode
     */
    const struct in_addr getInAddr() const { return _inaddr; }

    /**
     * Comparator operator for addresses.
     */
    bool operator < (const Inet4Address& x) const {
    	return ntohl(_inaddr.s_addr) < ntohl(x._inaddr.s_addr);
    }

    /**
     * Equality operator for addresses.
     */
    bool operator == (const Inet4Address& x) const {
    	return _inaddr.s_addr == x._inaddr.s_addr;
    }

    /**
     * Inequality operator for addresses.
     */
    bool operator != (const Inet4Address& x) const {
    	return _inaddr.s_addr != x._inaddr.s_addr;
    }

    /**
     * Is this address a multicast address?
     * Multicast addresses are in the range 224.0.0.1 to
     * 239.255.255.255, their first four bits are 1110=0xe
     */
    bool isMultiCastAddress() const {
        return (ntohl(_inaddr.s_addr) & 0xf0000000L) == 0xe0000000L;
    }

    /**
     * How many leading bits match in the two addresses?
     */
    int bitsMatch(const Inet4Address& x) const throw();

protected:

    /**
     * The IP address, in network byte order.
     */
    struct in_addr _inaddr;

};

}}	// namespace nidas namespace util

#endif
