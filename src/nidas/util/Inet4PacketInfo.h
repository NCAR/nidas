//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifndef NIDAS_UTIL_INET4PACKETINFO_H
#define NIDAS_UTIL_INET4PACKETINFO_H

#include <nidas/util/Inet4NetworkInterface.h>
#include <nidas/util/Inet4SocketAddress.h>

namespace nidas { namespace util {

/**
 * Ancillary information that can be determined about an incoming UDP packet.
 * Do "man 7 ip" for information.
 */
class Inet4PacketInfo
{
public:
    Inet4PacketInfo(): _flags(0) {}

    /**
     * The local address of the packet. For a received packet,
     * it is the address of the local interface that the packet
     * was received on.
     */
    Inet4Address getLocalAddress() const { return _localaddr; }

    void setLocalAddress(const Inet4Address& val) { _localaddr = val; }

    /**
     * The destination address of the packet. For a received unicast packet
     * it will be the address of the local interface.  Or the destination
     * address could be a multicast address or a broadcast address.
     */
    const Inet4Address& getDestinationAddress() const { return _destaddr; }

    void setDestinationAddress(const Inet4Address& val) { _destaddr = val; }

    /**
     * The interface that the packet was received on.
     */
    const Inet4NetworkInterface& getInterface() const { return _iface; }

    void setInterface(const Inet4NetworkInterface& val ) { _iface = val; }

    /**
     * The flags on the received packet. See "man 7 ip". It can contain several
     * flags: MSG_EOR, MSG_TRUNC, MSG_CTRUNC, MSG_OOB, MSG_ERRQUEUE.
     */
    int getFlags() const { return _flags; }

    void setFlags(int val) { _flags = val; }

private:
    Inet4Address _localaddr;

    Inet4Address _destaddr;

    Inet4NetworkInterface _iface;

    int _flags;
};

class Inet4PacketInfoX : public Inet4PacketInfo
{
public:

    /**
     * The remote address of the packet. For a received packet,
     * it is the address of the local interface that the packet
     * was sent from.
     */
    Inet4SocketAddress getRemoteSocketAddress() const { return _remotesaddr; }

    void setRemoteSocketAddress(const Inet4SocketAddress& val) { _remotesaddr = val; }

private:
    Inet4SocketAddress _remotesaddr;
};

}}	// namespace nidas namespace util

#endif
