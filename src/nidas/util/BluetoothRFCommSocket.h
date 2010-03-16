//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifdef HAS_BLUETOOTHRFCOMM_H

#ifndef NIDAS_UTIL_BLUETOOTHRFCOMMSOCKET_H
#define NIDAS_UTIL_BLUETOOTHRFCOMMSOCKET_H

#include <nidas/util/BluetoothRFCommSocketAddress.h>
#include <nidas/util/IOException.h>

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
     */
    BluetoothRFCommSocket() throw(IOException);

    /**
     * Create a stream socket connected to a remote address and channel.
     */
    BluetoothRFCommSocket(const BluetoothAddress& addr,int channel)
    	throw(IOException);

    /**
     * Create a stream socket connected to a remote host and channel.
     */
    BluetoothRFCommSocket(const std::string& addr,int channel)
    	throw(UnknownHostException,IOException);

    /**
     * Create a stream socket connected to a remote address.
     */
    BluetoothRFCommSocket(const SocketAddress& addr) throw(IOException);

    /**
     * Called by ServerSocket after a connection is
     * accepted.
     */
    BluetoothRFCommSocket(int fd, const SocketAddress& raddr) throw(IOException);

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

    void close() throw(IOException);

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
     */
    void setNonBlocking(bool val) throw(IOException);

    bool isNonBlocking() const throw(IOException);

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
     */
    void connect(const std::string& addr,int channel)
	throw(UnknownHostException,IOException);

    /**
     * Connect to a given remote host address and channel.
     */
    void connect(const BluetoothAddress& addr, int channel)
	throw(IOException);

    /**
     * Connect to a given remote socket address.
     */
    void connect(const SocketAddress& addr)
	throw(IOException);

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
     */
    void bind(int channel) throw(IOException);

    void bind(const BluetoothAddress& addr,int channel)
        throw(IOException);

    void bind(const SocketAddress& sockaddr) throw(IOException);

    void listen() throw(IOException);

    BluetoothRFCommSocket* accept() throw(IOException);

    /**
     * Fetch the file descriptor associated with this socket.
     */
    int getFd() const { return _fd; }

    size_t recv(void* buf, size_t len, int flags = 0)
	throw(IOException);

    /**
     * send data on socket, see man page for send system function.
     * @param buf pointer to buffer.
     * @param len number of bytes to send.
     * @flags bitwise OR of flags for send. Default: 0.
     */
    size_t send(const void* buf, size_t len, int flags=0)
	throw(IOException);

    size_t send(const struct iovec* iov, int iovcnt, int flags=MSG_NOSIGNAL)
	throw(IOException);

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
     */
    void sendall(const void* buf, size_t len, int flags=MSG_NOSIGNAL)
	throw(IOException);

    /**
     * Get remote address of this socket
     */
    const SocketAddress& getRemoteSocketAddress() const throw();

    /**
     * Get remote port number of this socket, which
     * for BluetoothRFCommSockets is the channel number.
     */
    int getRemotePort() const throw();

    /**
     * Get local address of this socket
     */
    const SocketAddress& getLocalSocketAddress() const throw();

    /**
     * Get local port number of this socket, which
     * for BluetoothRFCommSockets is the channel number.
     */
    int getLocalPort() const throw();

    int getDomain() const { return AF_BLUETOOTH; }

private:

    /**
     * Do system call to determine local address of this socket.
     */
    void getLocalAddr() throw(IOException);

    /**
     * Do system call to determine address of remote end.
     */
    void getRemoteAddr() throw(IOException);

    int _fd;

    SocketAddress* _localaddr;

    SocketAddress* _remoteaddr;

    bool _hasTimeout;

    struct timeval _timeout;

    fd_set _fdset;
};


}}	// namespace nidas namespace util

#endif
#endif
