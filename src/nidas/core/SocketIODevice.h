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

#ifndef NIDAS_CORE_SOCKETIODEVICE_H
#define NIDAS_CORE_SOCKETIODEVICE_H

#include <nidas/core/IODevice.h>
#include <nidas/util/Socket.h>
#include <nidas/util/ParseException.h>

#include <memory> // auto_ptr<>

namespace nidas { namespace core {

/**
 * A IODevice providing support for UDP and TCP sockets.
 */
class SocketIODevice : public IODevice {

public:

    /**
     * Create a SocketIODevice.  No IO operations
     * are performed in the constructor, hence no IOExceptions.
     */
    SocketIODevice();

    ~SocketIODevice();

    /**
     * Prepare to open the socket. This actually just parses the address from the
     * device name, and doesn't actually do a socket connection or bind.
     * See parseAddress().  The connect or bind must be done in the
     * TCPSocketIODevice or UDPSocketIODevice open method.
     * If the name contains a IP hostname and the
     * IP address of that host is not available, then an InvalidParameterException
     * is thrown.
     */
    void open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    /*
    * Perform an ioctl on the device. This is supported on sockets,
    * and will throw an IOException.
    */
    void ioctl(int /* request */, void* /* buf */, size_t /* len */) throw(nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),
		"ioctl","not supported on SocketIODevice");
    }

    /**
     * Parse the getName() string to extract a socket type, destination
     * address and optional port number.
     * The syntax of the set/getName() string is:
     * ( "inet" | "sock" | "unix" | "blue" | "btspp" ) ':' address [:port]
     * "inet" or "sock" indicate a TCP or UDP connection for use by
     * derived classes implementing a TCP or UDP socket. The address
     * field should then be a hostname which can be resolved to an IP
     * address, or an IP address in dot notation. A port number is required
     * for TCP/UDP socket connection.
     * "unix" indicates a Unix socket connection, where address is the
     * path name of the unix socket, for process to process communications.
     * The port value is not used by unix sockets.
     */
    static void parseAddress(const std::string& name, int& addrtype,std::string& hostname,
        int& port,std::string& bindaddr) throw(nidas::util::ParseException);

protected:

    /**
     * The type of the destination address, AF_INET or AF_UNIX.
     */
    int _addrtype;	

    /**
     * Destination host name from sensor name.
     */
    std::string _desthost;

    /**
     * Port number that is parsed from sensor name.
     * For a server socket this is the local port number to listen on.
     * For a remote connection socket this is the remote port number.
     */
    int _port;

    /**
     * The destination socket address.
     */
    std::auto_ptr<nidas::util::SocketAddress> _sockAddr;

    /**
     * The local bind socket address.
     */
    std::string _bindAddr;

};

}}	// namespace nidas namespace core

#endif
