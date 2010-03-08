//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

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
     * Create a stream socket connected to a remote address and port.
     */
    BluetoothRFCommSocket(const BluetoothAddress& addr,int port)
    	throw(IOException);

    /**
     * Create a stream socket connected to a remote host and port.
     */
    BluetoothRFCommSocket(const std::string& addr,int port)
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
     * Connect to a given remote host and port.
     * If the device has a pin which isn't matched, the
     * IOException will be EHOSTDOWN: Host is down
     *
     */
    void connect(const std::string& host,int port)
	throw(UnknownHostException,IOException);

    /**
     * Connect to a given remote host address and port.
     */
    void connect(const BluetoothAddress& addr, int port)
	throw(IOException);

    /**
     * Connect to a given remote socket address.
     */
    void connect(const SocketAddress& addr)
	throw(IOException);

    void bind(int port) throw(IOException);

    void bind(const BluetoothAddress& addr,int port)
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
     * Get remote port number of this socket.
     */
    int getRemotePort() const throw();

    /**
     * Get local address of this socket
     */
    const SocketAddress& getLocalSocketAddress() const throw();

    /**
     * Get local port number of this socket.
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
