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
    Inet4PacketInfo():
        _localaddr(), _destaddr(), _iface(), _flags(0)
    {
    }

    virtual ~Inet4PacketInfo() {}

    /**
     * The local address of the packet. For a received packet,
     * it is the address of the local interface that the packet
     * was received on.
     */
    virtual Inet4Address getLocalAddress() const { return _localaddr; }

    virtual void setLocalAddress(const Inet4Address& val) { _localaddr = val; }

    /**
     * The destination address of the packet. For a received unicast packet
     * it will be the address of the local interface.  Or the destination
     * address could be a multicast address or a broadcast address.
     */
    virtual const Inet4Address& getDestinationAddress() const { return _destaddr; }

    virtual void setDestinationAddress(const Inet4Address& val) { _destaddr = val; }

    /**
     * The interface that the packet was received on.
     */
    virtual const Inet4NetworkInterface& getInterface() const { return _iface; }

    virtual void setInterface(const Inet4NetworkInterface& val ) { _iface = val; }

    /**
     * The flags on the received packet. See "man 7 ip". It can contain several
     * flags: MSG_EOR, MSG_TRUNC, MSG_CTRUNC, MSG_OOB, MSG_ERRQUEUE.
     */
    virtual int getFlags() const { return _flags; }

    virtual void setFlags(int val) { _flags = val; }

private:
    Inet4Address _localaddr;

    Inet4Address _destaddr;

    Inet4NetworkInterface _iface;

    int _flags;
};

class Inet4PacketInfoX : public Inet4PacketInfo
{
public:
    Inet4PacketInfoX(): _remotesaddr() {}

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
