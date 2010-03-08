//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifdef HAS_BLUETOOTHRFCOMM_H

#ifndef NIDAS_UTIL_BLUETOOTHRFCOMMSOCKETADDRESS
#define NIDAS_UTIL_BLUETOOTHRFCOMMSOCKETADDRESS

#include <nidas/util/SocketAddress.h>
#include <nidas/util/BluetoothAddress.h>
#include <bluetooth/rfcomm.h>

namespace nidas { namespace util {

/**
 * An AF_BLUETOOTH socket address containing a channel number.
 */
class BluetoothRFCommSocketAddress: public SocketAddress {
public:

    BluetoothRFCommSocketAddress();

    /**
     * Constructor, with an address string and channel number
     */
    BluetoothRFCommSocketAddress(int channel);

    /**
     * Constructor, with an address string and channel number
     */
    BluetoothRFCommSocketAddress(const std::string& host,int channel);

    /**
     * Constructor, with a BluetoothAddress and channel number
     */
    BluetoothRFCommSocketAddress(const BluetoothAddress& addr,int channel);

    /**
     * Constructor with pointer to sockaddr_rc.
     */
    BluetoothRFCommSocketAddress(const struct sockaddr_rc* sockaddr);

    /**
     * Copy constructor.
     */
    BluetoothRFCommSocketAddress(const BluetoothRFCommSocketAddress&);

    /**
     * Assignment operator.
     */
    BluetoothRFCommSocketAddress& operator=(const BluetoothRFCommSocketAddress& x);

    /**
     * Virtual constructor.
     */
    BluetoothRFCommSocketAddress* clone() const;

    /**
     * Return the address family, AF_BLUETOOTH.
     */
    int getFamily() const { return AF_BLUETOOTH; }

    /**
     * Return the Bluetooth address portion.
     */
    BluetoothAddress getBluetoothAddress() const {
    	return BluetoothAddress(&_sockaddr.rc_bdaddr);
    }

    /**
     * The "port" of an AF_BLUETOOTH addresses is the channel.
     */
    int getPort() const { return _sockaddr.rc_channel; }

    /**
     * Provide non-const pointer to struct sockaddr_rc.  This is
     * needed for recvfrom methods.  recvfrom updates
     * the struct sockaddr_rc, so we can't cache the other
     * portions of the address.
     */
    struct sockaddr* getSockAddrPtr() { return (struct sockaddr*) &_sockaddr; }

    /**
     * Provide const pointer to struct sockaddr_rc.
     */
    const struct sockaddr* getConstSockAddrPtr() const
    {
        return (const struct sockaddr*) &_sockaddr;
    }

    socklen_t getSockAddrLen() const { return sizeof(_sockaddr); }

    /**
     * Java style toString: returns "btspp:address:channel".
     */
    std::string toString() const;

    /**
     * Java style toString: returns "btspp:address:channel".
     */
    std::string toAddressString() const;

    /**
     * Comparator operator for addresses. Useful if this
     * address is a key in an STL map.
     */
    bool operator < (const BluetoothRFCommSocketAddress& x) const;

    /**
     * Equality operator for addresses.
     */
    bool operator == (const BluetoothRFCommSocketAddress& x) const;

protected:
    struct sockaddr_rc _sockaddr;
};

}}	// namespace nidas namespace util

#endif
#endif
