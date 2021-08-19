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
/*

    C++ classes supporting TCP, UDP and Unix sockets.

*/

#ifndef NIDAS_UTIL_SOCKET_H
#define NIDAS_UTIL_SOCKET_H

#include "Inet4SocketAddress.h"
#include "Inet4NetworkInterface.h"
#include "Inet4PacketInfo.h"
#include "UnixSocketAddress.h"
#include "Inet4Address.h"
#include "IOException.h"
#include "EOFException.h"
#include "DatagramPacket.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <net/if.h>   // IFF_* bits for network interface flags

#include <netinet/in.h>
#include <ctime>
#include <cassert>

#include <vector>

namespace nidas { namespace util {

class Socket;

/**
 * Implementation of a socket, providing a C++ interface to
 * system socket calls: socket,bind,listen,accept,setsockopt, etc.
 *
 * This is patterned after java.net.SocketImpl.
 * This class includes methods for both stream (TCP) and datagram (UDP) sockets.
 * We also haven't implemented the socket implementation factory from
 * Java.
 *
 * This class provides the public copy constructors and 
 * assignment operators.  Objects of this class can be copied and
 * assigned to without restriction.  However, because of this,
 * the destructor does not close the socket file descriptor, so,
 * in general, you should make sure that the socket is closed
 * once at some point.
 */
class SocketImpl {
public:

    /**
     * @throws IOException
     **/
    SocketImpl(int domain,int type);

    /**
     * @throws IOException
     **/
    SocketImpl(int fd, const SocketAddress& remoteaddr);

    /**
     * Copy constructor.
     */
    SocketImpl(const SocketImpl& x);

    /**
     * Assignment operator.
     */
    SocketImpl& operator = (const SocketImpl& rhs);

    ~SocketImpl();

    int getFd() const { return _fd; }

    /**
     * Get the domain of this socket: AF_UNIX, AF_INET, etc,
     * from sys/socket.h.
     */
    int getDomain() const { return _sockdomain; }

    void setBacklog(int val) { _backlog = val; }

    /**
     * Get local socket address of this socket.
     *
     * @throw()
     */
    const SocketAddress& getLocalSocketAddress() const
    {
    	return *_localaddr;
    }

    /**
     * Get local port number of this socket.
     *
     * @throw()
     */
    int getLocalPort() const
    {
        return _localaddr->getPort();
    }

    /**
     * Get remote socket address of this socket.
     *
     * @throw()
     */
    const SocketAddress& getRemoteSocketAddress() const
    {
    	return *_remoteaddr;
    }

    /**
     * Get remote port number of this socket.
     *
     * @throw()
     */
    int getRemotePort() const
    {
        return _remoteaddr->getPort();
    }

    void setReuseAddress(bool val) { _reuseaddr = val; }

    /**
     * @throws IOException
     **/
    void setNonBlocking(bool val);

    /**
     * @throws IOException
     **/
    bool isNonBlocking() const;

    /**
     * Set the TCP_NODELAY (man 7 tcp) option on the socket.
     * This option is only appropriate for TCP sockets.
     * @param val: If true, set TCP_NODELAY. If false unset it.
     *
     * @throws IOException
     */
    void setTcpNoDelay(bool val);

    /**
     * Get the TCP_NODELAY (man 7 tcp) option on the socket.
     * @return true if TCP_NODELAY is set on the socket.
     *
     * @throws IOException
     */
    bool getTcpNoDelay();

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
     * Set or unset the SO_KEEPALIVE socket option.
     *
     * @throws IOException
     */
    void setKeepAlive(bool val);

    /**
     * Get the current value of the SO_KEEPALIVE socket option.
     *
     * @throws IOException
     */
    bool getKeepAlive() const;

    /**
     * Set the number of seconds a connection needs to be idle before TCP
     * begins sending out keep-alive probes (TCP_KEEPIDLE).  Only appropriate
     * for stream (TCP) connections, not datagrams (UDP).
     * Calls setKeepAlive(true) if necessary so that the SO_KEEPALIVE option
     * is set.
     * @param val Number of seconds.
     *
     * @throws IOException
     *
     * man 7 tcp:
        tcp_keepalive_time
              The  number of seconds a connection needs to be idle before TCP begins send-
              ing out keep-alive probes.  Keep-alives are only sent when the  SO_KEEPALIVE
              socket  option is enabled.  The default value is 7200 seconds (2 hours).  An
              idle connection is terminated after approximately an additional  11  minutes
              (9 probes an interval of 75 seconds apart) when keep-alive is enabled.

              Note that underlying connection tracking mechanisms and application timeouts
              may be much shorter.
    */
    void setKeepAliveIdleSecs(int val);

    /**
     * Get the current value of TCP_KEEPIDLE on this socket.
     * @return Number of seconds a connection needs to be idle before TCP
     * begins sending out keep-alive probes (TCP_KEEPIDLE).
     *
     * @throws IOException
     */
    int getKeepAliveIdleSecs() const;

    /**
     * Get number of bytes currently unread in the local input queue.
     * This does a call to ioctl(fd, SIOCINQ, &num); See man 7 tcp or udp.
     *
     * @throws IOException
     */
    int getInQueueSize() const;

    /**
     * Get number of bytes currently unsent in the local output queue.
     * This does a call to ioctl(fd, SIOCOUTQ, &num); See man 7 tcp or udp.
     *
     * @throws IOException
     */
    int getOutQueueSize() const;

    /**
     * @throws IOException
     **/
    void close();

    /**
     * @throws IOException
     **/
    void bind(int port);

    /**
     * @throws IOException
     **/
    void bind(const Inet4Address& addr, int port);

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
    Socket* accept();

    /**
     * @throws UnknownHostException
     * @throws IOException
     **/
    void connect(const std::string& dest, int port);

    /**
     * @throws IOException
     **/
    void connect(const Inet4Address& addr, int port);

    /**
     * @throws IOException
     **/
    void connect(const SocketAddress& addr);

    /**
     * @throws IOException
     **/
    void receive(DatagramPacketBase& packet);

    /**
     * @throws IOException
     **/
    void receive(DatagramPacketBase& packet, Inet4PacketInfo& info, int flags=0);

    /**
     * Receive data on a socket. See "man 2 recv" for values of the
     * flags parameter (none of which have been tested).
     * An EOFException is returned if the remote host does
     * an orderly shutdown of the socket.
     *
     * @throws IOException
     **/
    size_t recv(void* buf, size_t len, int flags=0);

    /**
     * @throws IOException
     **/
    size_t recvfrom(void* buf, size_t len, int flags, SocketAddress& from);

    /**
     * @throws IOException
     **/
    void send(const DatagramPacketBase& packet,int flags=0);

    /**
     * send data on socket. See send UNIX man page.
     * @param buf pointer to buffer.
     * @param len number of bytes to send.
     * @flags bitwise OR of flags for send. Default: 0.
     * @return Number of bytes written to socket.
     * If using non-blocking IO, either via setNonBlocking(true),
     * or by setting the MSG_DONTWAIT in flags, and the system
     * function returns EAGAIN or EWOULDBLOCK, then the return value will be 0,
     * and no IOException is thrown.
     *
     * @throws IOException
     */
    size_t send(const void* buf, size_t len, int flags = 0);

    /**
     * @throws IOException
     **/
    size_t send(const struct iovec* iov, int iovcnt, int flags = 0);

    /**
     * @throws IOException
     **/
    size_t sendto(const void* buf, size_t len, int flags,
                  const SocketAddress& to);

    /**
     * @throws IOException
     **/
    size_t sendto(const struct iovec* iov, int iovcnt, int flags,
                  const SocketAddress& to);

    /**
     * send all data in buffer on socket, repeating send()
     * as necessary, until all data is sent (or an exception
     * occurs).
     * @param buf pointer to buffer.
     * @param len number of bytes to send.
     * @flags bitwise OR of flags for send. Default: 0.
     * @return Number of bytes written to socket.
     * It is not recommended to use this method if using 
     * non-blocking IO.
     *
     * @throws IOException
     **/
    void sendall(const void* buf, size_t len, int flags = 0);

    /**
     * Join a multicast group on all interfaces.
     *
     * @throws IOException
     */
    void joinGroup(Inet4Address groupAddr);

    /*
    void joinGroup(Inet4Address groupAddr,Inet4Address iaddr) throw(IOException);
    */

    /**
     * Join a multicast group on a specific interface.
     * According to "man 7 ip", if the interface adddress "is equal to
     * INADDR_ANY an appropriate interface is chosen by the system",
     * which may not be what you want.
     * This was eth0 on a system with lo,eth0 and eth1.
     *
     * @throws IOException
     */
    void joinGroup(Inet4Address groupAddr, const Inet4NetworkInterface&);

    /**
     * Leave a multicast group on all interfaces.
     *
     * @throws IOException
     */
    void leaveGroup(Inet4Address groupAddr);

    /**
     * Leave a multicast group on a given interface.
     *
     * @throws IOException
     */
    void leaveGroup(Inet4Address groupAddr, const Inet4NetworkInterface& iaddr);

    /**
     * @throws IOException
     **/
    void setReceiveBufferSize(int size);

    /**
     * @throws IOException
     **/
    int getReceiveBufferSize();

    /**
     * @throws IOException
     **/
    void setSendBufferSize(int size);

    /**
     * @throws IOException
     **/
    int getSendBufferSize();

    /**
     * @throws IOException
     **/
    void setTimeToLive(int val);

    /**
     * @throws IOException
     **/
    int getTimeToLive() const;

    /**
     * Control whether a IP_PKTINFO ancillary message is received with
     * each datagram.  Only supported on DatagramSockets.  The
     * IP_PKTINFO message is converted to an Inet4PacketInfo object
     * which is available via the getInet4PacketInfo() method.
     * @param val: if true enable the IP_PKTINFO message, if false, disable.
     *
     * @throws IOException
     **/
    void setPktInfo(bool val);

    bool getPktInfo() const
    {
        return _pktInfo;
    }

    /**
     * Whether to set the IP_MULTICAST_LOOP socket option. According
     * to "man 7 ip", IP_MULTICAST_LOOP controls "whether sent multicast
     * packets should be looped back to the local sockets."
     * This behaviour seems to be the default in Linux in that setting
     * this does not seem to be necessary for a process on a host
     * to receive multicast packets that are sent out on one of its
     * interfaces, providing the multicast reader has joined that
     * interface, and a firewall is not blocking them.
     *
     * @throws IOException
     */
    void setMulticastLoop(bool val);

    /**
     * @throws IOException
     **/
    bool getMulticastLoop() const;

    /**
     * @throws IOException
     **/
    void setInterface(Inet4Address maddr, const Inet4NetworkInterface& iaddr);

    /**
     * @throws IOException
     **/
    void setInterface(Inet4Address iaddr);

    /**
     * @throws IOException
     **/
    Inet4NetworkInterface getInterface() const;

    /**
     * @throws IOException
     **/
    Inet4NetworkInterface getInterface(const std::string& name) const;

    /**
     * @throws IOException
     **/
    Inet4NetworkInterface findInterface(const Inet4Address&) const;

    /**
     * @throws IOException
     **/
    std::list<Inet4NetworkInterface> getInterfaces() const;

    /**
     * Enable or disable SO_BROADCAST.  Note that broadcasting is generally
     * not advised, best to use multicast instead.
     *
     * @throws IOException
     **/
    void setBroadcastEnable(bool val);

    /**
     * @throws IOException
     **/
    bool getBroadcastEnable() const;

protected:

    int _sockdomain;
    int _socktype;
    SocketAddress* _localaddr;
    SocketAddress* _remoteaddr;
    int _fd;
    int _backlog;
    bool _reuseaddr;
    bool _hasTimeout;
    struct timespec _timeout;

    bool _pktInfo;

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
};

/**
 * A stream (TCP) socket.  This class is patterned after the
 * java.net.Socket class. The Socket will be either a
 * AF_INET or AF_UNIX socket depending on the domain or
 * address family of the SocketAddressess argument to the constructor.
 * 
 * This class provides the public copy constructors and 
 * assignment operators.  Objects of this class can be copied and
 * assigned to without restriction.  However, because of this,
 * the destructor does not close the socket file descriptor, so,
 * in general, you should make sure that the socket is closed
 * once at some point.
 *
 * Usage scenario:
 * \code
 *	// create a connection to host big_server, on port 5000
 *	Socket sock("big_server",5000);
 *	for (int i = 0; i < 10; i++) {
 *	    sock.send("hello\n",6,0);
 *      }
 *      sock.close();
 * \endcode
 * 
 */
class Socket {
public:

    /**
     * Create an unconnected stream socket. Caution: this no-arg
     * constructor opens a file descriptor, and the
     * destructor does not close the file descriptor.
     * You must either do Socket::close() to close it, or
     * make a copy of Socket, and close the copy, since
     * the new copy will own the file descriptor.
     *
     * @throws IOException
     */
    Socket(int domain=AF_INET);

    /**
     * Create an Internet domain stream socket connected to a
     * remote address and port.
     *
     * @throws IOException
     */
    Socket(const Inet4Address& addr,int port);

    /**
     * Create an Internet domain stream socket connected to a
     * remote host and port.
     *
     * @throws UnknownHostException
     * @throws IOException
     */
    Socket(const std::string& host, int port);

    /**
     * Create a stream socket connected to a remote address.
     * The socket domain will match the domain of the
     * SocketAddress.
     *
     * @throws IOException
     */
    Socket(const SocketAddress& addr);

    /**
     * Called by ServerSocket after a connection is
     * accepted.  The socket domain will match the
     * domain of the SocketAddress.
     *
     * @throws IOException
     */
    Socket(int fd, const SocketAddress& raddr);

    ~Socket() throw() {
    }

    /**
     * @throws IOException
     **/
    void close()
    {
        _impl.close();
    }

    /**
     * Create a pair of connnected unix sockets. See man page for
     * socketpair() system call.
     *
     * @throws IOException
     */
    static std::vector<Socket*> createSocketPair(int type=SOCK_STREAM);

    /**
     * Set the timeout for receive(), recv(), and recvfrom()
     * methods.
     * @param val timeout in milliseconds. 0=no timeout (infinite)
     * The receive methods will return IOTimeoutException
     * if an operation times out.
     */
    void setTimeout(int val) { _impl.setTimeout(val); }

    /**
     * Do fcntl system call to set O_NONBLOCK file descriptor flag on 
     * the socket.
     * @param val true=set O_NONBLOCK, false=unset O_NONBLOCK.
     *
     * @throws IOException
     */
    void setNonBlocking(bool val)
    {
        _impl.setNonBlocking(val);
    }

    /**
     * @throws IOException
     **/
    bool isNonBlocking() const
    {
	return _impl.isNonBlocking();
    }

    /**
     * @throws IOException
     **/
    void setTcpNoDelay(bool val)
    {
	_impl.setTcpNoDelay(val);
    }

    /**
     * @throws IOException
     **/
    bool getTcpNoDelay()
    {
	return _impl.getTcpNoDelay();
    }

    /**
     * Set or unset the SO_KEEPALIVE socket option.
     *
     * @throws IOException
     **/
    void setKeepAlive(bool val)
    {
        _impl.setKeepAlive(val);
    }

    /**
     * Get the current value of the SO_KEEPALIVE socket option.
     *
     * @throws IOException
     */
    bool getKeepAlive()
    {
        return _impl.getKeepAlive();
    }

    /**
     * Set the number of seconds a connection needs to be idle before TCP
     * begins sending out keep-alive probes (TCP_KEEPIDLE).  Only appropriate
     * for stream (TCP) connections, not datagrams (UDP).
     * Calls setKeepAlive(true) if necessary so that the SO_KEEPALIVE option
     * is set.
     * @param val Number of seconds.
     *
     * @throws IOException
     *
     * man 7 tcp:
        tcp_keepalive_time
              The  number of seconds a connection needs to be idle before TCP begins send-
              ing out keep-alive probes.  Keep-alives are only sent when the  SO_KEEPALIVE
              socket  option is enabled.  The default value is 7200 seconds (2 hours).  An
              idle connection is terminated after approximately an additional  11  minutes
              (9 probes an interval of 75 seconds apart) when keep-alive is enabled.

              Note that underlying connection tracking mechanisms and application timeouts
              may be much shorter.
    */
    void setKeepAliveIdleSecs(int val)
    {
        _impl.setKeepAliveIdleSecs(val);
    }

    /**
     * Get the current value of TCP_KEEPIDLE on this socket.
     * @return Number of seconds a connection needs to be idle before TCP
     * begins sending out keep-alive probes (TCP_KEEPIDLE).
     *
     * @throws IOException
     */
    int getKeepAliveIdleSecs()
    {
        return _impl.getKeepAliveIdleSecs();
    }

    /**
     * @throws IOException
     **/
    int getInQueueSize() const
    {
        return _impl.getInQueueSize();
    }

    /**
     * @throws IOException
     **/
    int getOutQueueSize() const
    {
        return _impl.getOutQueueSize();
    }

    /**
     * Get a list of all my network interfaces.
     *
     * @throws IOException
     **/
    std::list<Inet4NetworkInterface> getInterfaces() const
    {
        return _impl.getInterfaces();
    }

    /**
     * Connect to a given remote host and port.
     *
     * @throws IOException
     * @throws UnknownHostException
     */
    void connect(const std::string& host,int port)
    {
        _impl.connect(host,port);
    }

    /**
     * Connect to a given remote host address and port.
     *
     * @throws IOException
     */
    void connect(const Inet4Address& addr, int port)
    {
        _impl.connect(addr,port);
    }

    /**
     * Connect to a given remote socket address.
     *
     * @throws IOException
     */
    void connect(const SocketAddress& addr)
    {
        _impl.connect(addr);
    }

    /**
     * Fetch the file descriptor associate with this socket.
     */
    int getFd() const
    {
        return _impl.getFd();
    }

    /**
     * @throws IOException
     **/
    size_t recv(void* buf, size_t len, int flags = 0)
    {
        return _impl.recv(buf,len,flags);
    }

    /**
     * send data on socket, see man page for send system function.
     * @param buf pointer to buffer.
     * @param len number of bytes to send.
     * @flags bitwise OR of flags for send. Default: MSG_NOSIGNAL.
     *
     * @throws IOException
     */
    size_t send(const void* buf, size_t len, int flags=MSG_NOSIGNAL)
    {
        return _impl.send(buf,len,flags);
    }

    /**
     * @throws IOException
     **/
    size_t send(const struct iovec* iov, int iovcnt, int flags=MSG_NOSIGNAL)
    {
        return _impl.send(iov,iovcnt,flags);
    }

    /**
     * send all data in buffer on socket, repeating send()
     * as necessary, until all data is sent (or an exception
     * occurs).
     * 
     * @param buf pointer to buffer.
     * @param len number of bytes to send.
     * @flags bitwise OR of flags for send. Default: MSG_NOSIGNAL.
     * @throws IOException
     *
     * It is not recommended to use this method if using non-blocking IO.
     */
    void sendall(const void* buf, size_t len, int flags=MSG_NOSIGNAL)
    {
        return _impl.sendall(buf,len,flags);
    }

    /**
     * @throws IOException
     **/
    void setReceiveBufferSize(int size)
    {
    	_impl.setReceiveBufferSize(size);
    }

    /**
     * @throws IOException
     **/
    int getReceiveBufferSize()
    {
    	return _impl.getReceiveBufferSize();
    }

    /**
     * @throws IOException
     **/
    void setSendBufferSize(int size)
    {
    	_impl.setSendBufferSize(size);
    }

    /**
     * @throws IOException
     **/
    int getSendBufferSize()
    {
    	return _impl.getSendBufferSize();
    }

    /**
     * Get remote address of this socket
     *
     * @throw()
     */
    const SocketAddress& getRemoteSocketAddress() const
    {
        return _impl.getRemoteSocketAddress();
    }

    /**
     * Get remote port number of this socket.
     *
     * @throw()
     */
    int getRemotePort() const {
        return _impl.getRemotePort();
    }

    /**
     * Get local address of this socket
     *
     * @throw()
     */
    const SocketAddress& getLocalSocketAddress() const
    {
        return _impl.getLocalSocketAddress();
    }

    /**
     * Get local port number of this socket.
     *
     * @throw()
     */
    int getLocalPort() const {
        return _impl.getLocalPort();
    }

    int getDomain() const
    {
        return _impl.getDomain();
    }

protected:
    SocketImpl _impl;
};

/**
 * A stream (TCP) socket that is used to listen for connections.
 * This class is patterned after the java.net.ServerSocket class.
 *
 * This class provides the public copy constructors and 
 * assignment operators.  Objects of this class can be copied and
 * assigned to without restriction.  However, because of this,
 * the destructor does not close the socket, so it is the user's
 * responsibility to call Socket::close() when finished with
 * the connection.
 *
 * Usage scenario of a server which listens for connections
 * on port 5000, and spawns a thread to handle each connection.
 *
 * \code
 *      using namespace nidas::util;
 *      class DetachedSocketThread: public DetachedThread {
 *      public:
 *          // Thread constructor
 *          DetachedSocketThread(Socket s) throw(IOException):
 *		DetachedThread("reader"),sock(s) {}
 *
 *          // Thread run method
 *          int run() throw(nidas::util::Exception) {
 *              for (;;) {
 *                  char buf[512];
 *                  if (recv(buf,sizeof(buf),0) == 0) break;
 *                  ...
 *              }
 *              sock.close();
 *              return RUN_OK;
 *          }
 *          // my socket
 *          Socket sock;
 *      };
 *
 *	ServerSocket ssock(5000);	// bind and listen on port 5000
 *	for (;;) {
 *	    Socket* sock = ssock.accept();	// accept a connection
 *	    DetachedSockThread* thrd = new DetachedSockThread(sock);
 *	    thrd->start();
 *          // DetachedThreads delete themselves when they're done.
 *      }
 * \endcode
 * 
 */
class ServerSocket {
public:

    /**
     * Create a AF_INET ServerSocket bound to a port on all local interfaces.
     * @param port Port number, 0<=port<=65535.  If zero, the system
     *        will select an available port number. To find out
     *        which port number was selected, use getLocalPort().
     *
     * @throws IOException
     */
    ServerSocket(int port=0,int backlog=10);

    /**
     * Create a ServerSocket bound to port on a given local address,
     * and set listen backlog parameter.
     *
     * @throws IOException
     */
    ServerSocket(const Inet4Address& bindAddr, int port, int backlog=10);

    /**
     * Create a ServerSocket bound to an address,
     * and set listen backlog parameter.
     *
     * @throws IOException
     */
    ServerSocket(const SocketAddress& bindAddr, int backlog=10);

    /**
     * Destructor. Does not close the socket. You must explicitly
     * close it with the close method.
     *
     * @throw()
     */
    ~ServerSocket() {
    }

    int getFd() const {
        return _impl.getFd();
    }

    /**
     * Close the socket.
     *
     * @throws IOException
     */
    void close();

    /**
     * Accept connection, return a connected Socket instance.
     * This method does the following in addition to the basic accept() system call.
     * 1. The pselect or ppoll system calls are used to wait on the socket file
     *    descriptor, with SIGUSR1 unset in the signal mask. In order
     *    to catch a SIGUSR1 signal with this accept() method, SIGUSR1
     *    should first be blocked in the thread before calling accept().
     *    If SIGUSR1 or any other signal is caught by this method, it will
     *    throw an IOException with a getErrno() of EINTR. The actual value
     *    of the signal is not available.
     * 2. If the ::accept() system call returns EAGAIN, EWOULDBLOCK
     *    or ECONNABORTED, the pselect()/ppoll() and accept() system calls are retried.
     *
     * @throws IOException
     */
    Socket* accept()
    {
        return _impl.accept();
    }

    /**
     * @throws IOException
     **/
    void setReceiveBufferSize(int size)
    {
    	_impl.setReceiveBufferSize(size);
    }

    /**
     * @throws IOException
     **/
    int getReceiveBufferSize()
    {
    	return _impl.getReceiveBufferSize();
    }

    /**
     * @throws IOException
     **/
    void setSendBufferSize(int size)
    {
    	_impl.setSendBufferSize(size);
    }

    /**
     * @throws IOException
     **/
    int getSendBufferSize()
    {
    	return _impl.getSendBufferSize();
    }

    /**
     * Fetch the local address that this socket is bound to.
     *
     * @throw()
     */
    const SocketAddress& getLocalSocketAddress() const
    {
        return _impl.getLocalSocketAddress();
    }

    /**
     * @throw()
     **/
    int getLocalPort() const {
        return _impl.getLocalPort();
    }

    int getDomain() const {
        return _impl.getDomain();
    }

    /**
     * @throws IOException
     **/
    void setNonBlocking(bool val)
    {
    	_impl.setNonBlocking(val);
    }

    /**
     * @throws IOException
     **/
    bool isNonBlocking() const
    {
    	return _impl.isNonBlocking();
    }

protected:
    SocketImpl _impl;

};

/**
 * A socket for sending or receiving datagrams, either unicast,
 * broadcast or multicast.
 *
 * This class provides the default public copy constructors and 
 * assignment operators.  Objects of this class can be copied and
 * assigned to without restriction.  However, because of this,
 * the destructor does not close the socket, so, in general, you
 * should make sure that the socket is closed once at some point.
 *
 * Typical usage:
 * A receiver of datagrams on port 9000:
 * \code
 *	DatagramSocket sock(9000);
 *	char buf[512];
 *	Inet4SocketAddress from;
 *	for (;;) {
 *	    sock.recvfrom(buf,sizeof(buf),0,from);
 *	}
 *	sock.close();
 * \endcode
 * A unicast sender of datagrams on port 9000:
 * \code
 *	DatagramSocket sock;
 *      Inet4SocketAddress to(Inet4Address::getByName("128.117.80.99"),9000);
 *	for (;;) {
 *	    sock.sendto("hello\n",6,0,to);
 *      }
 *	sock.close();
 * \endcode
 * A multicast sender of datagrams on port 9000:
 * \code
 *	DatagramSocket sock;
 *      Inet4SocketAddress to(Inet4Address::getByName("239.0.0.1"),9000);
 *	for (;;) {
 *	    sock.sendto("hello\n",6,0,to);
 *      }
 *	sock.close();
 * \endcode
 * A broadcast sender of datagrams on port 9000 to the limited
 * broadcast address of 255.255.255.255. This will fail with a 
 * "Network is unreachable" error if there are no external network
 * interfaces UP with an assigned address. Use a broadcast address of
 * 127.255.255.255 to "broadcast" on the loopback interface.
 * \code
 *	DatagramSocket sock;
 *	sock.setBroadcastEnable(true);
 *      Inet4SocketAddress to(Inet4Address(INADDR_BROADCAST),9000);
 *	for (;;) {
 *	    sock.sendto("hello\n",6,0,to);
 *      }
 *	sock.close();
 * \endcode
 */
class DatagramSocket {
public:

    /**
     * Create a DatagramSocket not bound to a port.
     *
     * @throws IOException
     **/
    DatagramSocket();

    /**
     * Create a DatagramSocket bound to a port on all local interfaces
     * (a local address of INADDR_ANY).
     * @param port Port number, 0<=port<=65535.  If zero, the system
     *        will select an available port number. To find out
     *        which port number was selected, use getLocalPort().
     *
     * @throws IOException
     */
    DatagramSocket(int port);

    /**
     * Creates a datagram socket and binds it to a port
     * at the specified local address.
     * The address should correspond to the address of a local
     * interface.  Only packets sent the given port and interface,
     * by unicast, broadcast or multicast will be received.
     *
     * @throws IOException
     */
    DatagramSocket(const Inet4Address& addr, int port);

    /**
     * Creates a DatagramSocket and binds it to the specified
     * local address.  The address should correspond to the
     * address of a local interface.  Only packets sent
     * the given port and interface, by unicast, broadcast
     * or multicast will be received.
     *
     * @throws IOException
     */
    DatagramSocket(const SocketAddress& addr);

    /**
     * MulticastSocket is derived from DatagramSocket, so
     * we provide a virtual destructor. Currently there
     * are no other virtual methods, may need some more thought.
     */
    virtual ~DatagramSocket() throw()
    {
    }

    /**
     * @throws IOException
     **/
    void close()
    {
        _impl.close();
    }

    /**
     * Set the timeout for receive(), recv(), and recvfrom()
     * methods.
     * @param val timeout in milliseconds. 0=no timeout (infinite)
     * The receive methods will return IOTimeoutException
     * if an operation times out.
     */
    void setTimeout(int val) {
        _impl.setTimeout(val);
    }

    /**
     * Datagrams are connectionless, so this doesn't establish
     * a true connection, it just sets the default destination
     * address for the send() calls.
     *
     * @throws UnknownHostException
     * @throws IOException
     */
    void connect(const std::string& host, int port)
    {
        _impl.connect(host,port);
    }

    /**
     * Datagrams are connectionless, so this doesn't establish
     * a true connection, it just sets the default destination
     * address for the send() calls.
     *
     * @throws IOException
     */
    void connect(const Inet4Address& addr, int port)
    {
        _impl.connect(addr,port);
    }

    /**
     * Datagrams are connectionless, so this doesn't establish
     * a true connection, it just sets the default destination
     * address for the send() calls.
     *
     * @throws IOException
     */
    void connect(const SocketAddress& addr)
    {
        _impl.connect(addr);
    }

    /**
     * Bind the DatagramSocket to the specified local address.
     * The address should correspond to the address of a local
     * interface.  Only packets sent to the given port and interface,
     * by unicast, broadcast or multicast will be received.
     *
     * @throws IOException
     */
    void bind(const Inet4Address& addr, int port)
    {
        _impl.bind(addr,port);
    }

    /**
     * Bind the DatagramSocket to the specified local address.
     * The address should correspond to the address of a local
     * interface.  Only packets sent to the given port and interface,
     * by unicast, broadcast or multicast will be received.
     *
     * @throws IOException
     */
    void bind(const SocketAddress& addr)
    {
        _impl.bind(addr);
    }

    int getFd() const {
        return _impl.getFd();
    }

    /**
     * Read a packet from the DatagramSocket. On return, packet.getLength()
     * will contain the number bytes received, and packet.getSocketAddress() will
     * contain the address of the sender.
     *
     * @throws IOException
     */
    void receive(DatagramPacketBase& packet)
    {
        _impl.receive(packet);
    }

    /**
     * Read a packet from the DatagramSocket. On return, packet.getLength()
     * will contain the number bytes received, and packet.getSocketAddress() will
     * contain the address of the sender. info will contain the information
     * on the local and destination addresses of the packet, and the
     * interface it was received on.
     * If setPktInfo(true) has not be set on this socket, then it is
     * set prior to receiving the packet, and then unset after
     * receipt of the packet.
     *
     * @throws IOException
     */
    void receive(DatagramPacketBase& packet, Inet4PacketInfo& info, int flags=0)
    {
        _impl.receive(packet,info,flags);
    }

    /**
     * @throws IOException
     **/
    void send(const DatagramPacketBase& packet, int flags=0)
    {
        _impl.send(packet,flags);
    }

    /**
     * @throws IOException
     **/
    size_t recv(void* buf, size_t len, int flags=0)
    {
        return _impl.recv(buf,len,flags);
    }

    /**
     * @throws IOException
     **/
    size_t recvfrom(void* buf, size_t len, int flags,
                    SocketAddress& from)
    {
        return _impl.recvfrom(buf,len,flags,from);
    }

    /**
     * @throws IOException
     **/
    size_t send(const void* buf, size_t len, int flags = 0)
    {
        return _impl.send(buf,len,flags);
    }

    /**
     * @throws IOException
     **/
    size_t send(const struct iovec* iov, int iovcnt, int flags=MSG_NOSIGNAL)
    {
        return _impl.send(iov,iovcnt,flags);
    }

    /**
     * @throws IOException
     **/
    size_t sendto(const void* buf, size_t len, int flags,
                  const SocketAddress& to)
    {
        return _impl.sendto(buf,len,flags,to);
    }

    /**
     * @throws IOException
     **/
    size_t sendto(const struct iovec* iov, int iovcnt, int flags,
                  const SocketAddress& to)
    {
        return _impl.sendto(iov,iovcnt,flags,to);
    }

    /**
     * Get local address of this socket
     *
     * @throw()
     */
    const SocketAddress& getLocalSocketAddress() const
    {
        return _impl.getLocalSocketAddress();
    }

    /**
     * Get local port number of this socket.
     *
     * @throw()
     */
    int getLocalPort() const {
        return _impl.getLocalPort();
    }

    /**
     * Do fcntl system call to set O_NONBLOCK file descriptor flag on 
     * the socket.
     * @param val true=set O_NONBLOCK, false=unset O_NONBLOCK.
     *
     * @throws IOException
     */
    void setNonBlocking(bool val)
    {
        _impl.setNonBlocking(val);
    }

    /**
     * @throws IOException
     **/
    bool isNonBlocking() const
    {
        return _impl.isNonBlocking();
    }

    /**
     * @throws IOException
     **/
    void setReceiveBufferSize(int size)
    {
    	_impl.setReceiveBufferSize(size);
    }

    /**
     * @throws IOException
     **/
    int getReceiveBufferSize()
    {
    	return _impl.getReceiveBufferSize();
    }

    /**
     * @throws IOException
     **/
    void setSendBufferSize(int size)
    {
    	_impl.setSendBufferSize(size);
    }

    /**
     * @throws IOException
     **/
    int getSendBufferSize()
    {
    	return _impl.getSendBufferSize();
    }

    /**
     * @throws IOException
     **/
    std::list<Inet4NetworkInterface> getInterfaces() const
    {
        return _impl.getInterfaces();
    }

    /**
     * @throws IOException
     **/
    void setBroadcastEnable(bool val)
    {
        _impl.setBroadcastEnable(val);
    }

    /**
     * @throws IOException
     **/
    bool getBroadcastEnable() const
    {
        return _impl.getBroadcastEnable();
    }

    /**
     * Control whether a IP_PKTINFO ancillary message is received with
     * each datagram.  Only supported on DatagramSockets.  The
     * IP_PKTINFO message is converted to an Inet4PacketInfo object
     * which is available via the getInet4PacketInfo() method.
     * @param val: if true enable the IP_PKTINFO message, if false, disable.
     *
     * @throws IOException
     **/
    void setPktInfo(bool val)
    {
        _impl.setPktInfo(val);
    }

protected:
    SocketImpl _impl;
};

/**
 * A datagram socket to be used for multicasts.
 *
 * This class provides the default public copy constructors and 
 * assignment operators.  Objects of this class can be copied and
 * assigned to without restriction.  However, because of this,
 * the destructor does not close the socket, so, in general, you
 * should make sure that the socket is closed once at some point.
 *
 * Usage example:
 * \code
 *    // host A listens for multicasts to address 239.0.0.1, port 9000:
 *    MulticastSocket msock(9000);
 *    // join the multicast group in whatever interface the system chooses,
 *    // typically the first ethernet interface.
 *    msock.joinGroup(Inet4Address::getByName("239.0.0.1"));
 *    char buf[512];
 *    Inet4SocketAddress from;
 *    for (;;) {
 *        msock.recvfrom(buf,sizeof(buf),0,from);
 *        ...
 *    }
 *
 *    // how to join on all multicast and loopback interfaces
 *    // note that multicast works over the loopback interface
 *    list<Inet4NetworkInterface> ifaces = msock.getInterfaces();
 *    for (list<Inet4NetworkInterface>::const_iterator ii = ifaces.begin();
 *          ii != ifaces.end(); ++ii)
 *    {
 *          if ((ii->getFlags() & (IFF_LOOPBACK | IFF_MULTICAST)) && (ii->getFlags() & IFF_UP))
 *              msock.joinGroup(Inet4Address::getByName("239.0.0.1"),*ii);
 *    }
 *
 *
 *    // host B multicasts to address 239.0.0.1, port 9000:
 *    DatagramSocket msock;
 *    Inet4Address maddr = Inet4Address::getByName("239.0.0.1");
 *    Inet4SocketAddress msaddr = Inet4SocketAddress(maddr,9000);
 *    for (;;) {
 *        msock.sendto("hello\n",6,0,msaddr);
 *        ...
 *    }
 *
 *    // host C also does similar multicasts, but uses MulticastSocket
 *    // methods to send packets out a specific interface, and
 *    // to set the time to live (TTL) parameter.
 *    MulticastSocket msock;
 *    Inet4Address maddr = Inet4Address::getByName("239.0.0.1");
 *    list<Inet4NetworkInterfaces> ifaces = msock.getInterfaces();
 *    list<Inet4NetworkInterfaces>::const_iterator ii; 
 *    for (ii = ifaces.begin(); ii != ifaces.end(); ++ii) {
 *      Inet4NetworkInterface ni = *ii;
 *      if (ni.getName() == "eth1")
 *          msock.setInterface(maddr,ni);
 *    }
 *    msock.setTimeToLive(2);	// go through one router
 *    Inet4SocketAddress msaddr = Inet4SocketAddress(maddr,9000);
 *    for (;;) {
 *        msock.sendto("hello\n",6,0,msaddr);
 *        ...
 *    }
 * \endcode
 * Note: if the above examples are not working for you, check your
 * firewall settings. They might be blocking multicasts.
 */
class MulticastSocket: public DatagramSocket {
public:

    /**
     * Create an unbound multicast socket.  You must bind it to
     * a port if you want to receive packets.
     * If you are sending packets on this MulticastSocket, and do not
     * use getInterfaces() and setInterface() to choose a
     * specific interface, the system sends packets out on the first
     * non-loopback interface that it finds.  If a firewall is blocking
     * multicast packets on that interface the packets won't be sent.
     *
     * @throws IOException
     */
    MulticastSocket() : DatagramSocket()
    {
	// setInterface(Inet4Address(INADDR_ANY),Inet4Address(INADDR_ANY));
    }

    /**
     * Create multicast socket, bind it to a local port.
     *
     * @throws IOException
     */
    MulticastSocket(int port) : DatagramSocket(port)
    {
	// setInterface(Inet4Address(INADDR_ANY),Inet4Address(INADDR_ANY));
    }

    /**
     * Creates a datagram socket and binds it to the specified
     * local address.
     *
     * @throws IOException
     */
    MulticastSocket(const Inet4SocketAddress& addr) : DatagramSocket(addr)
    {
	// setInterface(Inet4Address(INADDR_ANY),Inet4Address(INADDR_ANY));
    }

    /**
     * Join a multicast group on all interfaces.
     *
     * @throws IOException
     */
    void joinGroup(Inet4Address groupAddr) {
        _impl.joinGroup(groupAddr);
    }

    /**
     * Join a multicast group on a given interface.
     */
    /*
    void joinGroup(Inet4Address groupAddr,Inet4Address iaddr) throw(IOException) {
        _impl.joinGroup(groupAddr,iaddr);
    }
    */

    /**
     * @throws IOException
     **/
    void joinGroup(Inet4Address groupAddr, const Inet4NetworkInterface &iface) {
        _impl.joinGroup(groupAddr,iface);
    }

    /**
     * Leave a multicast group on all interfaces.
     *
     * @throws IOException
     */
    void leaveGroup(Inet4Address groupAddr) {
        _impl.leaveGroup(groupAddr);
    }

    /**
     * Leave a multicast group on a given interface.
     *
     * @throws IOException
     */
    void leaveGroup(Inet4Address groupAddr, const Inet4NetworkInterface& iface)
    {
        _impl.leaveGroup(groupAddr,iface);
    }

    /**
     * @throws IOException
     **/
    void setTimeToLive(int val)
    {
        _impl.setTimeToLive(val);
    }

    /**
     * @throws IOException
     **/
    int getTimeToLive() const
    {
        return _impl.getTimeToLive();
    }

    /**
     * Whether to set the IP_MULTICAST_LOOP socket option. According
     * to "man 7 ip", IP_MULTICAST_LOOP controls "whether sent multicast
     * packets should be looped back to the local sockets."
     * This is the default in Linux in that setting
     * it does not seem to be necessary for a process on a host
     * to receive multicast packets that are sent out on one of its
     * interfaces, providing the multicast reader has joined that
     * interface, and a firewall is not blocking them (a source of
     * frustration!).
     *
     * @throws IOException
     */
    void setMulticastLoop(bool val)
    {
        _impl.setTimeToLive(val);
    }

    /**
     * @throws IOException
     **/
    bool getMulticastLoop() const
    {
        return _impl.getTimeToLive();
    }

    /**
     * Set the interface for a given multicast address.
     * If you are sending packets on this MulticastSocket, and do not
     * use setInterface() specific interface, the system will send packets
     * out on the first interface that it finds that is capable of MULTICAST.
     * See setInterface(Inet4Address maddr);
     *  
     * If there are no multicast interfaces that are UP, then the system may
     * not choose the loopback interface by default.
     * So doing multicast on a DHCP laptop that isn't connected to the net
     * may give problems.
     *
     * @throws IOException
     */
    void setInterface(Inet4Address maddr, const Inet4NetworkInterface& iface) {
        _impl.setInterface(maddr,iface);
    }

    /**
     * Set the interface for a given multicast address. This uses the default
     * value of INADDR_ANY for the specific interface, which causes
     * the system to choose what it thinks is the most appropriate interface,
     * typically the first ethernet interface on the system.
     * If you have more than one candidate interface, you can add an entry
     * in the routing table to indicate which you want to use:
     * route add -host 239.0.0.10 dev eth1
     *
     * @throws IOException
     */
    void setInterface(Inet4Address iaddr) {
        _impl.setInterface(iaddr);
    }

    /**
     * @throws IOException
     **/
    Inet4NetworkInterface getInterface() const {
        return _impl.getInterface();
    }

    /**
     * @throws IOException
     **/
    Inet4NetworkInterface findInterface(const Inet4Address& iaddr) const
    {
        return _impl.findInterface(iaddr);
    }

    /**
     * Return the IP addresses of all my network interfaces.
     *
     * @throws IOException
     **/
    std::list<Inet4NetworkInterface> getInterfaces() const
    {
        return _impl.getInterfaces();
    }

protected:
};

}}	// namespace nidas namespace util

#endif
