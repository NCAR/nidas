//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifndef NIDAS_UTIL_INET4NETWORKINTERFACE_H
#define NIDAS_UTIL_INET4NETWORKINTERFACE_H

#include <nidas/util/Inet4Address.h>

namespace nidas { namespace util {

class Inet4NetworkInterface
{
public:
    Inet4NetworkInterface():_index(0),_addr(INADDR_ANY),
        _baddr(INADDR_BROADCAST),_netmask(INADDR_BROADCAST),_mtu(0),_flags(0) {}

    Inet4NetworkInterface(const std::string& name,Inet4Address addr, Inet4Address brdcastAddr, Inet4Address netmask,int mtu,int index,short flags):
        _name(name),_index(index),_addr(addr),_baddr(brdcastAddr),_netmask(netmask),_mtu(mtu),_flags(flags) {}

    /**
     * The name of the interface: like "lo", "eth0", etc.
     */
    const std::string& getName() const { return _name; }

    /**
     * The index of the interface.
     */
    int getIndex() const { return _index; }

    /**
     * The IPV4 address of the interface.
     */
    Inet4Address getAddress() const { return _addr; }

    /**
     * The IPV4 broadcast address of the interface.
     */
    Inet4Address getBroadcastAddress() const { return _baddr; }

    /**
     * The IPV4 network mask of the interface.
     */
    Inet4Address getNetMask() const { return _netmask; }

    /**
     * The mtu of the interface.
     */
    int getMTU() const { return _mtu; }

    /**
     * The interface flags. Use macros like IFF_UP, IFF_BROADCAST, IFF_POINTOPOINT,
     * IFF_MULTICAST, IFF_LOOPBACK from net/if.h to check for capabilities.
     * See man netdevice.
     */
    short getFlags() const { return _flags; }
private:
    std::string _name;
    int _index;
    Inet4Address _addr;
    Inet4Address _baddr;
    Inet4Address _netmask;
    int _mtu;
    short _flags;
};

}}	// namespace nidas namespace util

#endif
