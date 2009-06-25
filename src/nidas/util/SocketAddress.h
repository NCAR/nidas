//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifndef NIDAS_UTIL_SOCKETADDRESS
#define NIDAS_UTIL_SOCKETADDRESS

#include <nidas/util/Inet4Address.h>

namespace nidas { namespace util {

/**
 * An interface for a socket address.
 */
class SocketAddress {
public:

    /**
     * Virtual constructor.
     */
    virtual SocketAddress* clone() const = 0;

    virtual ~SocketAddress() {}

    /**
     * Get the family of this SocketAddress, one of the values from
     * /usr/include/sys/socket.h: AF_UNIX, AF_INET, AF_INET6, etc.
     */
    virtual int getFamily() const = 0;

    /**
     * return the port number of this address, or -1 if
     * there is no associated port number, e.g. AF_UNIX.
     */
    virtual int getPort() const = 0;

    /**
     * Provide non-const pointer to struct sockaddr_in.  This is
     * needed for recvfrom methods.  recvfrom updates
     * the struct sockaddr, so we can't cache the other
     * portions of the address.
     */
    virtual struct sockaddr* getSockAddrPtr() = 0;

    virtual const struct sockaddr* getConstSockAddrPtr() const = 0;

    /**
     * Return the length of the struct sockaddr_XX for
     * this address family.
     */
    virtual socklen_t getSockAddrLen() const = 0;

    /**
     * Java style toString.
     */
    virtual std::string toString() const = 0;

    /**
     * Java style toString, but no DNS lookup.
     */
    virtual std::string toAddressString() const = 0;

    /**
     * Comparator operator for addresses. Useful if this
     * address is a key in an STL map.
     */
    // virtual bool operator < (const SocketAddress& x) const = 0;

    /**
     * Equality operator for addresses.
     */
    // virtual bool operator == (const SocketAddress& x) const = 0;

};

}}	// namespace nidas namespace util

#endif
