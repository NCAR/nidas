//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifndef NIDAS_UTIL_UNIXSOCKETADDRESS
#define NIDAS_UTIL_UNIXSOCKETADDRESS

#include <nidas/util/SocketAddress.h>
#include <sys/un.h>

namespace nidas { namespace util {

/**
 * An AF_UNIX socket address.  Do "man 7 unix" from linux for info
 * on unix sockets.
 */
class UnixSocketAddress: public SocketAddress {
public:

    /**
     * Default constructor, path of "", empty string.
     */
    // UnixSocketAddress();

    /**
     * Constructor, with path string.
     */
    UnixSocketAddress(const std::string& path);

    /**
     * Constructor with pointer to sockaddr_un.
     */
    UnixSocketAddress(const struct sockaddr_un* sockaddr);

    /**
     * Copy constructor.
     */
    UnixSocketAddress(const UnixSocketAddress&);

    /**
     * Assignment operator.
     */
    UnixSocketAddress& operator=(const UnixSocketAddress& x);

    /**
     * Virtual constructor.
     */
    UnixSocketAddress* clone() const;

    /**
     * Return the address family, AF_UNIX.
     */
    int getFamily() const { return AF_UNIX; }

    /**
     * AF_UNIX addresses don't have ports, return -1.
     */
    int getPort() const { return -1; }

    /**
     * Provide non-const pointer to struct sockaddr_un.  This is
     * needed for recvfrom methods.  recvfrom updates
     * the struct sockaddr_un, so we can't cache the other
     * portions of the address.
     */
    struct sockaddr* getSockAddrPtr() { return (struct sockaddr*) &sockaddr; }

    /**
     * Provide const pointer to struct sockaddr_un.
     */
    const struct sockaddr* getConstSockAddrPtr() const
    {
        return (const struct sockaddr*) &sockaddr;
    }

    socklen_t getSockAddrLen() const { return sizeof(sockaddr); }

    /**
     * Java style toString: returns "unix:path"
     */
    std::string toString() const;

    /**
     * Comparator operator for addresses. Useful if this
     * address is a key in an STL map.
     */
    bool operator < (const SocketAddress& x) const;

    /**
     * Equality operator for addresses.
     */
    bool operator == (const SocketAddress& x) const;

protected:
    std::string path;

    struct sockaddr_un sockaddr;
};

}}	// namespace nidas namespace util

#endif
