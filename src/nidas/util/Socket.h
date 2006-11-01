//
//              Copyright 2004 (C) by UCAR
//
// Description:
//

#ifndef NIDAS_UTIL_SOCKET_H
#define NIDAS_UTIL_SOCKET_H

#include <nidas/util/Inet4SocketAddress.h>
#include <nidas/util/UnixSocketAddress.h>
#include <nidas/util/Inet4Address.h>
#include <nidas/util/IOException.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/DatagramPacket.h>

#include <sys/types.h>
#include <sys/socket.h>
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
 * Because we're lazy, this class includes methods for
 * both stream (TCP) and datagram (UDP) sockets.
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

    SocketImpl(int domain,int type) throw(IOException);

    SocketImpl(int fd, const SocketAddress& remoteaddr) throw(IOException);

    /**
     * Copy constructor.
     */
    SocketImpl(const SocketImpl& x);

    /**
     * Assignment operator.
     */
    SocketImpl& operator = (const SocketImpl& rhs);

    ~SocketImpl();

    int getFd() const { return fd; }

    /**
     * Get the domain of this socket: PF_UNIX, PF_INET, etc,
     * from sys/socket.h.
     */
    int getDomain() const { return sockdomain; }

    void setBacklog(int val) { backlog = val; }

    /**
     * Get local socket address of this socket.
     */
    const SocketAddress& getLocalSocketAddress() const throw()
    {
	assert(localaddr);
    	return *localaddr;
    }

    /**
     * Get local port number of this socket.
     */
    int getLocalPort() const throw()
    {
	assert(localaddr);
        return localaddr->getPort();
    }

    /**
     * Get remote socket address of this socket.
     */
    const SocketAddress& getRemoteSocketAddress() const throw()
    {
	assert(remoteaddr);
    	return *remoteaddr;
    }

    /**
     * Get remote port number of this socket.
     */
    int getRemotePort() const throw()
    {
	assert(remoteaddr);
        return remoteaddr->getPort();
    }

    void setReuseAddress(bool val) { reuseaddr = val; }

    void setNonBlocking(bool val) throw(IOException);

    /**
     * Set the TCP_NODELAY (man 7 tcp) option on the socket.
     * This option is only appropriate for TCP sockets.
     * @param val: If true, set TCP_NODELAY. If false unset it.
     */
    void setTcpNoDelay(bool val) throw(IOException);

    /**
     * Get the TCP_NODELAY (man 7 tcp) option on the socket.
     * @return true if TCP_NODELAY is set on the socket.
     */
    bool getTcpNoDelay() throw(IOException);

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
     */
    void setKeepAlive(bool val) throw(IOException);

    /**
     * Get the current value of the SO_KEEPALIVE socket option.
     */
    bool getKeepAlive() const throw(IOException);

    /**
     * Set the number of seconds a connection needs to be idle before TCP
     * begins sending out keep-alive probes (TCP_KEEPIDLE).  Only appropriate
     * for stream (TCP) connections, not datagrams (UDP).
     * Calls setKeepAlive(true) if necessary so that the SO_KEEPALIVE option
     * is set.
     * @param val Number of seconds.
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
    void setKeepAliveIdleSecs(int val) throw(IOException);

    /**
     * Get the current value of TCP_KEEPIDLE on this socket.
     * @return Number of seconds a connection needs to be idle before TCP
     * begins sending out keep-alive probes (TCP_KEEPIDLE).
     */
    int getKeepAliveIdleSecs() const throw(IOException);

    /**
     * Get number of bytes currently unread in the local input queue.
     * This does a call to ioctl(fd, SIOCINQ, &num); See man 7 tcp or udp.
     */
    int getInQueueSize() const throw(IOException);

    /**
     * Get number of bytes currently unsent in the local output queue.
     * This does a call to ioctl(fd, SIOCOUTQ, &num); See man 7 tcp or udp.
     */
    int getOutQueueSize() const throw(IOException);

    void close() throw(IOException);

    void bind(int port)
	throw(IOException);

    void bind(const Inet4Address& addr,int port)
	throw(IOException);

    void bind(const SocketAddress& sockaddr)
	throw(IOException);

    void listen() throw(IOException);

    Socket* accept() throw(IOException);

    void connect(const std::string& dest,int port)
	throw(UnknownHostException,IOException);

    void connect(const Inet4Address& addr,int port)
	throw(IOException);

    void connect(const SocketAddress& addr)
	throw(IOException);

    void receive(DatagramPacketBase& packet) throw(IOException,IOTimeoutException);

    void send(const DatagramPacketBase& packet) throw(IOException);

    size_t recv(void* buf, size_t len, int flags=0)
	throw(IOException,IOTimeoutException);

    size_t recvfrom(void* buf, size_t len, int flags,
    	SocketAddress& from) throw(IOException,IOTimeoutException);

    /**
     * send data on socket. See send UNIX man page.
     * @param buf pointer to buffer.
     * @param len number of bytes to send.
     * @flags bitwise OR of flags for send. Default: 0.
     * @return Number of bytes written to socket.
     * If using non-blocking IO, either via setNonBlocking(true),
     * or by setting the MSG_DONTWAIT in flags, and the system
     * function returns EAGAIN, then the return value will be 0,
     * and no IOException is thrown.
     */
    size_t send(const void* buf, size_t len, int flags = 0)
	throw(IOException);

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
     */
    void sendall(const void* buf, size_t len, int flags = 0)
	throw(IOException);

    size_t sendto(const void* buf, size_t len, int flags,
    	const SocketAddress& to) throw(IOException);

    /**
     * Join a multicast group on all interfaces.
     */
    void joinGroup(Inet4Address groupAddr) throw(IOException);

    /**
     * Join a multicast group on a specific interface.
     * According to "man 7 ip", if the interface adddress "is equal to
     * INADDR_ANY an appropriate interface is chosen by the system",
     * which may not be what you want.
     * This was eth0 on a system with lo,eth0 and eth1.
     */
    void joinGroup(Inet4Address groupAddr,Inet4Address iaddr) throw(IOException);
    /**
     * Leave a multicast group on all interfaces.
     */
    void leaveGroup(Inet4Address groupAddr) throw(IOException);

    /**
     * Leave a multicast group on a given interface.
     */
    void leaveGroup(Inet4Address groupAddr,Inet4Address iaddr)
    	throw(IOException);

    void setReceiveBufferSize(int size) throw(IOException);

    int getReceiveBufferSize() throw(IOException);

    void setSendBufferSize(int size) throw(IOException);

    int getSendBufferSize() throw(IOException);

    void setTimeToLive(int val) throw(IOException);

    int getTimeToLive() const throw(IOException);

    void setInterface(Inet4Address maddr,Inet4Address iaddr) throw(IOException);

    void setInterface(Inet4Address iaddr) throw(IOException);

    Inet4Address getInterface() const throw(IOException);

    Inet4Address findInterface(const Inet4Address&) const throw(IOException);

    void setLoopbackEnable(bool val) throw(IOException);

    std::list<Inet4Address> getInterfaceAddresses() const throw(IOException);

    /**
     * Enable or disable SO_BROADCAST.  Note that broadcasting is generally
     * not advised, best to use multicast instead.
     */
    void setBroadcastEnable(bool val) throw(IOException);

    bool getBroadcastEnable() const throw(IOException);

protected:

    int sockdomain;
    int socktype;
    SocketAddress* localaddr;
    SocketAddress* remoteaddr;
    int fd;
    int backlog;
    bool reuseaddr;
    bool hasTimeout;
    struct timeval timeout;
    fd_set fdset;

    /**
     * Do system call to determine local address of this socket.
     */
    void getLocalAddr() throw(IOException);

    /**
     * Do system call to determine address of remote end.
     */
    void getRemoteAddr() throw(IOException);

};

/**
 * A stream (TCP) socket.  This class is patterned after the
 * java.net.Socket class. The Socket will be either a
 * PF_INET or PF_UNIX socket depending on the domain argument
 * to the constructor or the SocketAddress class that is passed
 * to bind or connnect. A bind or connect to an Inet4SocketAddress
 * will create a PF_INET socket.  A bind or connect to an
 * UnixSocketAddress will create a PF_UNIX socket.
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
     */
    Socket(int domain=PF_INET) throw(IOException);

    /**
     * Create a stream socket connected to a remote address and port.
     */
    Socket(const Inet4Address& addr,int port)
    	throw(IOException);

    /**
     * Create a stream socket connected to a remote host and port.
     */
    Socket(const std::string& host,int port)
    	throw(UnknownHostException,IOException);

    /**
     * Create a stream socket connected to a remote address.
     */
    Socket(const SocketAddress& addr) throw(IOException);

    /**
     * Called by ServerSocket after a connection is
     * accepted.
     */
    Socket(int fd, const SocketAddress& raddr) throw(IOException);

    ~Socket() throw() {
    }

    void close() throw(IOException)
    {
        impl.close();
    }

    /**
     * Create a pair of connnected unix sockets. See man page for
     * socketpair() system call.
     */
    static std::vector<Socket*> createSocketPair(int type=SOCK_STREAM)
    	throw(IOException);

    /**
     * Set the timeout for receive(), recv(), and recvfrom()
     * methods.
     * @param val timeout in milliseconds. 0=no timeout (infinite)
     * The receive methods will return IOTimeoutException
     * if an operation times out.
     */
    void setTimeout(int val) { impl.setTimeout(val); }

    /**
     * Do fcntl system call to set O_NONBLOCK file descriptor flag on 
     * the socket.
     * @param val true=set O_NONBLOCK, false=unset O_NONBLOCK.
     */
    void setNonBlocking(bool val) throw(IOException)
    {
	impl.setNonBlocking(val);
    }

    void setTcpNoDelay(bool val) throw(IOException)
    {
	impl.setTcpNoDelay(val);
    }

    bool getTcpNoDelay() throw(IOException)
    {
	return impl.getTcpNoDelay();
    }

    /**
     * Set or unset the SO_KEEPALIVE socket option.
     */
    void setKeepAlive(bool val) throw(IOException)
    {
        impl.setKeepAlive(val);
    }

    /**
     * Get the current value of the SO_KEEPALIVE socket option.
     */
    bool getKeepAlive() throw(IOException)
    {
        return impl.getKeepAlive();
    }

    /**
     * Set the number of seconds a connection needs to be idle before TCP
     * begins sending out keep-alive probes (TCP_KEEPIDLE).  Only appropriate
     * for stream (TCP) connections, not datagrams (UDP).
     * Calls setKeepAlive(true) if necessary so that the SO_KEEPALIVE option
     * is set.
     * @param val Number of seconds.
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
    void setKeepAliveIdleSecs(int val) throw(IOException)
    {
        impl.setKeepAliveIdleSecs(val);
    }

    /**
     * Get the current value of TCP_KEEPIDLE on this socket.
     * @return Number of seconds a connection needs to be idle before TCP
     * begins sending out keep-alive probes (TCP_KEEPIDLE).
     */
    int getKeepAliveIdleSecs() throw(IOException)
    {
        return impl.getKeepAliveIdleSecs();
    }

    int getInQueueSize() const throw(IOException)
    {
        return impl.getInQueueSize();
    }

    int getOutQueueSize() const throw(IOException)
    {
        return impl.getOutQueueSize();
    }


    /**
     * Connect to a given remote host and port.
     */
    void connect(const std::string& host,int port)
	throw(UnknownHostException,IOException)
    {
	impl.connect(host,port);
    }

    /**
     * Connect to a given remote host address and port.
     */
    void connect(const Inet4Address& addr, int port)
	throw(IOException)
    {
	impl.connect(addr,port);
    }

    /**
     * Connect to a given remote socket address.
     */
    void connect(const SocketAddress& addr)
	throw(IOException)
    {
	impl.connect(addr);
    }

    /**
     * Fetch the file descriptor associate with this socket.
     */
    int getFd() const { return impl.getFd(); }

    size_t recv(void* buf, size_t len, int flags = 0)
	throw(IOException) {
	return impl.recv(buf,len,flags);
    }

    /**
     * send data on socket, see man page for send system function.
     * @param buf pointer to buffer.
     * @param len number of bytes to send.
     * @flags bitwise OR of flags for send. Default: MSG_NOSIGNAL.
     */
    size_t send(const void* buf, size_t len, int flags=MSG_NOSIGNAL)
	throw(IOException) {
	return impl.send(buf,len,flags);
    }

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
	throw(IOException) {
	return impl.sendall(buf,len,flags);
    }

    void setReceiveBufferSize(int size) throw(IOException)
    {
    	impl.setReceiveBufferSize(size);
    }
    int getReceiveBufferSize() throw(IOException)
    {
    	return impl.getReceiveBufferSize();
    }

    void setSendBufferSize(int size) throw(IOException)
    {
    	impl.setSendBufferSize(size);
    }
    int getSendBufferSize() throw(IOException)
    {
    	return impl.getSendBufferSize();
    }

    /**
     * Get remote address of this socket
     */
    const SocketAddress& getRemoteSocketAddress() const throw()
    {
	return impl.getRemoteSocketAddress();
    }
    /**
     * Get remote port number of this socket.
     */
    int getRemotePort() const throw() {
        return impl.getRemotePort();
    }

    /**
     * Get local address of this socket
     */
    const SocketAddress& getLocalSocketAddress() const throw()
    {
	return impl.getLocalSocketAddress();
    }

    /**
     * Get local port number of this socket.
     */
    int getLocalPort() const throw() {
	return impl.getLocalPort();
    }

    int getDomain() const { return impl.getDomain(); }

protected:
    SocketImpl impl;
};

/**
 * A stream (TCP) socket that is used to listen for connections.
 * This class is patterned after the java.net.ServerSocket class.
 *
 * This class provides the public copy constructors and 
 * assignment operators.  Objects of this class can be copied and
 * assigned to without restriction.  However, because of this,
 * the destructor does not close the socket, so, in general, you
 * should make sure that the socket is closed once at some point.
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
 *          // Thread run metchod
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
 *	    Socket sock = ssock.accept();	// accept a connection
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
     * Create a PF_INET ServerSocket bound to a port on all local interfaces.
     * @param port Port number, 0<=port<=65535.  If zero, the system
     *        will select an available port number. To find out
     *        which port number was selected, use getLocalPort().
     *
     */
    ServerSocket(int port=0,int backlog=10) throw(IOException);

    /**
     * Create a ServerSocket bound to port on a given local address,
     * and set listen backlog parameter.
     */
    ServerSocket(const Inet4Address& bindAddr, int port,int backlog=10)
    	throw(IOException);

    /**
     * Create a ServerSocket bound to an address,
     * and set listen backlog parameter.
     */
    ServerSocket(const SocketAddress& bindAddr,int backlog=10)
    	throw(IOException);

    /**
     * Destructor. Does not close the socket. You must explicitly
     * close it with the close method.
     */
    ~ServerSocket() throw() {
    }

    int getFd() const { return impl.getFd(); }

    /**
     * close the socket.
     */
    void close() throw(IOException) 
    {
        impl.close();
    }

    /**
     * Accept connection, return a Socket instance.
     */
    Socket* accept() throw(IOException)
    {
        return impl.accept();
    }

    void setReceiveBufferSize(int size) throw(IOException)
    {
    	impl.setReceiveBufferSize(size);
    }
    int getReceiveBufferSize() throw(IOException)
    {
    	return impl.getReceiveBufferSize();
    }

    void setSendBufferSize(int size) throw(IOException)
    {
    	impl.setSendBufferSize(size);
    }
    int getSendBufferSize() throw(IOException)
    {
    	return impl.getSendBufferSize();
    }

    /**
     * Fetch the local address that this socket is bound to.
     */
    const SocketAddress& getLocalSocketAddress() const throw()
    {
	return impl.getLocalSocketAddress();
    }

    int getLocalPort() const throw() { return impl.getLocalPort(); }

    int getDomain() const { return impl.getDomain(); }

protected:
    SocketImpl impl;

};

/**
 * A socket for sending datagrams, either unicast, broadcast or multicast
 * (though the broadcast hasn't been tested).
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
 * \endcode
 * A unicast sender of datagrams on port 9000:
 * \code
 *	DatagramSocket sock;
 *      Inet4SocketAddress to(Inet4Address::getByName("128.117.80.99",9000));
 *	for (;;) {
 *	    sock.sendto("hello\n",6,0,to);
 *      }
 * \endcode
 * A multicast sender of datagrams on port 9000:
 * \code
 *	DatagramSocket sock;
 *      Inet4SocketAddress to(Inet4Address::getByName("239.0.0.1",9000));
 *	for (;;) {
 *	    sock.sendto("hello\n",6,0,to);
 *      }
 * \endcode
 * A broadcast sender of datagrams on port 9000 (untested):
 * \code
 *	DatagramSocket sock;
 *	sock.setBroadcastEnable(true);
 *      Inet4SocketAddress to(Inet4Address::getByName("255.255.255.255",9000));
 *	for (;;) {
 *	    sock.sendto("hello\n",6,0,to);
 *      }
 * \endcode
 */
class DatagramSocket {
public:

    /**
     * Create a DatagramSocket bound to a port on all local interfaces.
     * @param port Port number, 0<=port<=65535.  If zero, the system
     *        will select an available port number. To find out
     *        which port number was selected, use getLocalPort().
     */
    DatagramSocket(int port = 0) throw(IOException);

    /**
     * Creates a datagram socket and binds it to a port on
     * at the specified local address.
     */
    DatagramSocket(const Inet4Address& addr,int port)
    	throw(IOException);

    /**
     * Creates a datagram socket and binds it to the specified
     * local address.
     */
    DatagramSocket(const SocketAddress& addr)
    	throw(IOException);

    ~DatagramSocket() throw() {
    }

    void close() throw(IOException)
    {
        impl.close();
    }

    /**
     * Set the timeout for receive(), recv(), and recvfrom()
     * methods.
     * @param val timeout in milliseconds. 0=no timeout (infinite)
     * The receive methods will return IOTimeoutException
     * if an operation times out.
     */
    void setTimeout(int val) {
        impl.setTimeout(val);
    }

    /**
     * Datagrams are connectionless, so this doesn't establish
     * a true connection, it just sets the default destination
     * address for the send() calls.
     */
    void connect(const std::string& host,int port)
	throw(UnknownHostException,IOException)
    {
	impl.connect(host,port);
    }

    void connect(const Inet4Address& addr, int port)
	throw(IOException)
    {
	impl.connect(addr,port);
    }

    void connect(const SocketAddress& addr)
	throw(IOException)
    {
	impl.connect(addr);
    }

    int getFd() const { return impl.getFd(); }

    void receive(DatagramPacketBase& packet) throw(IOException)
    {
	impl.receive(packet);
    }

    void send(const DatagramPacketBase& packet) throw(IOException)
    {
	impl.send(packet);
    }

    size_t recv(void* buf, size_t len, int flags=0) throw(IOException)
    {
	return impl.recv(buf,len,flags);
    }

    size_t recvfrom(void* buf, size_t len, int flags,
	SocketAddress& from) throw(IOException)
    {
        return impl.recvfrom(buf,len,flags,from);
    }

    size_t send(const void* buf, size_t len, int flags = 0)
	throw(IOException) {
	return impl.send(buf,len,flags);
    }

    size_t sendto(const void* buf, size_t len, int flags,
	const SocketAddress& to) throw(IOException)
    {
        return impl.sendto(buf,len,flags,to);
    }

    /**
     * Get local address of this socket
     */
    const SocketAddress& getLocalSocketAddress() const throw()
    {
	return impl.getLocalSocketAddress();
    }

    /**
     * Get local port number of this socket.
     */
    int getLocalPort() const throw() {
	return impl.getLocalPort();
    }

    void setReceiveBufferSize(int size) throw(IOException)
    {
    	impl.setReceiveBufferSize(size);
    }

    int getReceiveBufferSize() throw(IOException)
    {
    	return impl.getReceiveBufferSize();
    }

    void setSendBufferSize(int size) throw(IOException)
    {
    	impl.setSendBufferSize(size);
    }
    int getSendBufferSize() throw(IOException)
    {
    	return impl.getSendBufferSize();
    }


    std::list<Inet4Address> getInterfaceAddresses() const throw(IOException) {
        return impl.getInterfaceAddresses();
    }

    void setBroadcastEnable(bool val) throw(IOException)
    {
        impl.setBroadcastEnable(val);
    }

    bool getBroadcastEnable() const throw(IOException)
    {
        return impl.getBroadcastEnable();
    }

protected:
    SocketImpl impl;
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
 *    msock.joinGroup(Inet4Address::getByName("239.0.0.1"));
 *    char buf[512];
 *    Inet4SocketAddress from;
 *    for (;;) {
 *        msock.recvfrom(buf,sizeof(buf),0,from);
 *        ...
 *    }
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
 *    msock.setInterface(Inet4Address::getByName("128.117.80.99"));
 *    msock.setTimeToLive(2);	// go through one router
 *    Inet4Address maddr = Inet4Address::getByName("239.0.0.1");
 *    Inet4SocketAddress msaddr = Inet4SocketAddress(maddr,9000);
 *    for (;;) {
 *        msock.sendto("hello\n",6,0,msaddr);
 *        ...
 *    }
 * \endcode
 */
class MulticastSocket: public DatagramSocket {
public:

    /**
     * Create an unbound multicast socket.  You must bind it to
     * a port if you want to receive packets.
     * If you are sending packets on this MulticastSocket, and do not
     * use getInterfaceAddresses() and setInterface() to choose a
     * specific interface, the system sends packets out on the first
     * non-loopback interface that it finds.  If a firewall is blocking
     * multicast packets on that interface the packets won't be sent.
     */
    MulticastSocket() throw(IOException)
    {
	setInterface(Inet4Address(INADDR_ANY),Inet4Address(INADDR_ANY));
    }

    /**
     * Create multicast socket, bind it to a local port.
     */
    MulticastSocket(int port) throw(IOException) : DatagramSocket(port)
    {
	setInterface(Inet4Address(INADDR_ANY),Inet4Address(INADDR_ANY));
    }

    /**
     * Creates a datagram socket and binds it to the specified
     * local address.
     */
    MulticastSocket(const Inet4SocketAddress& addr)
    	throw(IOException) : DatagramSocket(addr)
    {
	setInterface(Inet4Address(INADDR_ANY),Inet4Address(INADDR_ANY));
    }

    /**
     * Join a multicast group on all interfaces.
     */
    void joinGroup(Inet4Address groupAddr) throw(IOException) {
        impl.joinGroup(groupAddr);
    }

    /**
     * Join a multicast group on a given interface.
     */
    void joinGroup(Inet4Address groupAddr,Inet4Address iaddr) throw(IOException) {
        impl.joinGroup(groupAddr,iaddr);
    }

    /**
     * Leave a multicast group on all interfaces.
     */
    void leaveGroup(Inet4Address groupAddr) throw(IOException) {
        impl.leaveGroup(groupAddr);
    }

    /**
     * Leave a multicast group on a given interface.
     */
    void leaveGroup(Inet4Address groupAddr, Inet4Address iaddr)
    	throw(IOException)
    {
        impl.leaveGroup(groupAddr,iaddr);
    }

    void setTimeToLive(int val) throw(IOException)
    {
        impl.setTimeToLive(val);
    }

    int getTimeToLive() const throw(IOException)
    {
        return impl.getTimeToLive();
    }

    /**
     * Set the interface for a given multicast address.
     * If you are sending packets on this MulticastSocket, and do not
     * use setInterface() specific interface, the system will send packets
     * out on the first non-loopback interface that it finds. This
     * will also be the case if you do:
     * setInterface(Inet4Address(INADDR_ANY),Inet4Address(INADDR_ANY));
     */
    void setInterface(Inet4Address maddr,Inet4Address iaddr) throw(IOException) {
        impl.setInterface(maddr,iaddr);
    }

    void setInterface(Inet4Address iaddr) throw(IOException) {
        impl.setInterface(iaddr);
    }

    Inet4Address getInterface() const throw(IOException) {
        return impl.getInterface();
    }

    Inet4Address findInterface(const Inet4Address& iaddr) const
    	throw(IOException)
    {
        return impl.findInterface(iaddr);
    }

    void setLoopbackEnable(bool val) throw(IOException) {
        impl.setLoopbackEnable(val);
    }

    /**
     * Return the IP addresses of all my network interfaces.
     */
    std::list<Inet4Address> getInterfaceAddresses() const throw(IOException) {
        return impl.getInterfaceAddresses();
    }

protected:
};

}}	// namespace nidas namespace util

#endif
