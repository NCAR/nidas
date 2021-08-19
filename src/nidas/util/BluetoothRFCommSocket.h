// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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

#include <nidas/Config.h>   // HAVE_BLUETOOTH_RFCOMM_H

#ifdef HAVE_BLUETOOTH_RFCOMM_H

#ifndef NIDAS_UTIL_BLUETOOTHRFCOMMSOCKET_H
#define NIDAS_UTIL_BLUETOOTHRFCOMMSOCKET_H

#include "BluetoothRFCommSocketAddress.h"
#include "IOException.h"

namespace nidas { namespace util {

/**
 * A Bluetooth RFComm socket.
 */
class BluetoothRFCommSocket {
public:

    /**
     * Create an unconnected Bluetooth RFCOMM socket.
     * Caution: this no-arg constructor opens a file descriptor, and the
     * destructor does not close the file descriptor.
     * You must either do Socket::close() to close it, or
     * make a copy of Socket, and close the copy, since
     * the new copy will own the file descriptor.
     *
     * @throws IOException
     */
    BluetoothRFCommSocket();

    /**
     * Create a stream socket connected to a remote address and channel.
     *
     * @throws IOException
     */
    BluetoothRFCommSocket(const BluetoothAddress& addr, int channel);

    /**
     * Create a stream socket connected to a remote host and channel.
     *
     * @throws UnknownHostException
     * @throws IOException
     */
    BluetoothRFCommSocket(const std::string& addr, int channel);

    /**
     * Create a stream socket connected to a remote address.
     *
     * @throws IOException
     */
    BluetoothRFCommSocket(const SocketAddress& addr);

    /**
     * Called by ServerSocket after a connection is
     * accepted.
     *
     * @throws IOException
     */
    BluetoothRFCommSocket(int fd, const SocketAddress& raddr);

    /**
     * Copy constructor.
     */
    BluetoothRFCommSocket(const BluetoothRFCommSocket&);

    /**
     * Assignment operator.
     */
    BluetoothRFCommSocket& operator = (const BluetoothRFCommSocket& rhs);

    /**
     * Does not close the device.
     */
    ~BluetoothRFCommSocket() throw();

    /**
     * @throws IOException
     **/
    void close();

    /**
     * Set the timeout for receive(), recv(), and recvfrom()
     * methods.
     * @param val timeout in milliseconds. 0=no timeout (infinite)
     * The receive methods will return IOTimeoutException
     * if an operation times out.
     */
    void setTimeout(int val);

    int getTimeout() const;

    /**
     * Do fcntl system call to set O_NONBLOCK file descriptor flag on 
     * the socket.
     * @param val true=set O_NONBLOCK, false=unset O_NONBLOCK.
     *
     * @throws IOException
     */
    void setNonBlocking(bool val);

    /**
     * @throws IOException
     **/
    bool isNonBlocking() const;

    /**
     * Connect to a given remote address and channel.
     * If the remote device requires a PIN before associating
     * underlying code will look in /var/lib/bluetooth/<local_addr>
     * for files called linkkeys and pincodes. If a link key or
     * pin code is not found for the remote device, then an attempt
     * is make to contact a bluetooth desktop agent so that it can
     * prompt the user for a PIN code.
     * If the device has a pin which isn't matched, or an agent
     * is not available the IOException will be
     * ECONNREFUSED, connection refused.
     *
     * @throws UnknownHostException
     * @throws IOException
     */
    void connect(const std::string& addr, int channel);

    /**
     * Connect to a given remote host address and channel.
     *
     * @throws IOException
     */
    void connect(const BluetoothAddress& addr, int channel);

    /**
     * Connect to a given remote socket address.
     *
     * @throws IOException
     */
    void connect(const SocketAddress& addr);

    /**
     * Bind to a local Bluetooth device. For Bluetooth sockets, one must use
     * bind() in the following circumstances:
     *  1. before doing a connect() to a device that requires a PIN.
     *  2. before doing listen() and accept() of incoming connections.
     * In the first case, one can call bind(0) to bind to BDADDR_ANY, channel 0,
     * and the socket will be bound to the first bluetooth interface on the system.
     * This establishes an address for the local bluetooth socket so that the 
     * system can read pin codes and link keys on /var/lib/bluetooth that are
     * associated with the local interface.
     * If the remote device doesn't require a PIN, then the bind is not
     * necessary before doing a connect(), but doesn't hurt.
     *
     * @throws IOException
     */
    void bind(int channel);

    /**
     * @throws IOException
     **/
    void bind(const BluetoothAddress& addr, int channel);

    /**
     * @throws IOException
     **/
    void bind(const SocketAddress& sockaddr);

    /**
     * @throws IOException
     **/
    void listen();

    /**
     * @throws IOException
     **/
    BluetoothRFCommSocket* accept();

    /**
     * Fetch the file descriptor associated with this socket.
     */
    int getFd() const { return _fd; }

    /**
     * @throws IOException
     **/
    size_t recv(void* buf, size_t len, int flags = 0);

    /**
     * send data on socket, see man page for send system function.
     * @param buf pointer to buffer.
     * @param len number of bytes to send.
     * @flags bitwise OR of flags for send. Default: 0.
     *
     * @throws IOException
     */
    size_t send(const void* buf, size_t len, int flags=0);

    /**
     * @throws IOException
     */
    size_t send(const struct iovec* iov, int iovcnt, int flags=MSG_NOSIGNAL);

    /**
     * send all data in buffer on socket, repeating send()
     * as necessary, until all data is sent (or an exception
     * occurs).
     * 
     * @param buf pointer to buffer.
     * @param len number of bytes to send.
     * @flags bitwise OR of flags for send. Default: MSG_NOSIGNAL.
     * It is not recommended to use this method if using 
     * non-blocking IO.
     *
     * @throws IOException
     */
    void sendall(const void* buf, size_t len, int flags=MSG_NOSIGNAL);

    /**
     * Get remote address of this socket
     *
     * @throw()
     */
    const SocketAddress& getRemoteSocketAddress() const;

    /**
     * Get remote port number of this socket, which
     * for BluetoothRFCommSockets is the channel number.
     *
     * @throw()
     */
    int getRemotePort() const;

    /**
     * Get local address of this socket
     *
     * @throw()
     */
    const SocketAddress& getLocalSocketAddress() const;

    /**
     * Get local port number of this socket, which
     * for BluetoothRFCommSockets is the channel number.
     *
     * @throw()
     */
    int getLocalPort() const;

    int getDomain() const { return AF_BLUETOOTH; }

private:

    /**
     * Do system call to determine local address of this socket.
     *
     * @throws IOException
     */
    void getLocalAddr();

    /**
     * Do system call to determine address of remote end.
     *
     * @throws IOException
     */
    void getRemoteAddr();

    int _fd;

    SocketAddress* _localaddr;

    SocketAddress* _remoteaddr;

    bool _hasTimeout;

    struct timespec _timeout;

};


}}	// namespace nidas namespace util

#endif
#endif
