//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifndef NIDAS_UTIL_INET4ADDRESS
#define NIDAS_UTIL_INET4ADDRESS

#include <nidas/util/UnknownHostException.h>
#include <nidas/util/ThreadSupport.h>

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
     */
    static std::list<Inet4Address> getAllByName(const std::string& hostname)
  	throw(UnknownHostException);

    /**
     * Return an address of a name, the first one found by getAllByName.
     */
    static Inet4Address getByName(const std::string& hostname)
  	throw(UnknownHostException);

    /**
     * Do reverse lookup of a name, given an address.
     * This is the address-to-name lookup method.
     * If a host name is not found for the address, returns
     * a string in dot notation: "x.x.x.x" using getHostAddress().
     */
    static std::string getHostName(const Inet4Address& addr) throw();

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
    const struct in_addr* getInAddrPtr() const { return &inaddr; }

    /**
     * Get the structure containing the 4 byte internet version 4 address.
     * To get the actual 4 byte integer value:
     * \code
     *   uint32_t addr = getInAddr().s_addr;
     * \endcode
     */
    const struct in_addr getInAddr() const { return inaddr; }

    /**
     * Comparator operator for addresses.
     */
    bool operator < (const Inet4Address& x) const {
    	return ntohl(inaddr.s_addr) < ntohl(x.inaddr.s_addr);
    }

    /**
     * Equality operator for addresses.
     */
    bool operator == (const Inet4Address& x) const {
    	return inaddr.s_addr == x.inaddr.s_addr;
    }

    /**
     * Inequality operator for addresses.
     */
    bool operator != (const Inet4Address& x) const {
    	return inaddr.s_addr != x.inaddr.s_addr;
    }

    /**
     * Is this address a multicast address?
     * Multicast addresses are in the range 224.0.0.1 to
     * 239.255.255.255, their first four bits are 1110=0xe
     */
    bool isMultiCastAddress() const {
        return (ntohl(inaddr.s_addr) & 0xf0000000L) == 0xe0000000L;
    }

    /**
     * How many leading bits match in the two addresses?
     */
    int bitsMatch(const Inet4Address& x) const throw();

protected:

    /**
     * The IP address, in network byte order.
     */
    struct in_addr inaddr;

#ifdef CACHE_DNS_LOOKUPS
    /**
     * Static cache of reverse name lookups.
     */
    static std::map<Inet4Address,std::string> addrToName;

    /**
     * Mutual exclusion lock to prevent simultaneous access
     * of addrToName map.
     */
    static Mutex addrToNameLock;

    /**
     * Static cache of name lookups.
     */
    static std::map<std::string,std::list<Inet4Address> > nameToAddrs;

    /**
     * Mutual exclusion lock to prevent simultaneous access of
     * nameToAddrs map.
     */
    static Mutex nameToAddrsLock;
#endif

};

}}	// namespace nidas namespace util

#endif
