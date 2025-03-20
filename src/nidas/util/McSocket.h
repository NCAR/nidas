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

#ifndef NIDAS_UTIL_MCSOCKET_H
#define NIDAS_UTIL_MCSOCKET_H

#include <nidas/Config.h>   // HAVE_PPOLL

#include "Socket.h"
#include "DatagramPacket.h"
#include "Thread.h"
#include "UTime.h"

#ifdef HAVE_PPOLL
#include <poll.h>
#else
#include <sys/select.h>
#endif

#include <list>

#include <unistd.h>   // need declaration of sleep()

namespace nidas { namespace util {

struct McSocketData {

    /**
     * Magic value that should be found at the beginning of all received datagrams.
     */
    int magic;
    /**
     * An integer which identifies the type of the request.
     * A McSocketListener on the server side must be doing
     * an accept for a McSocket with the same
     * requestType value. Stored in "network", big-endian order.
     */
    int _requestType;

    /**
     * Socket port that the remote host is listening on.
     * Stored in "network", big-endian order.
     */
    unsigned short _listenPort;

    /**
     * Either SOCK_STREAM=1, or SOCK_DGRAM=2
     */
    short _socketType;

    /**
     * Constructor.
     */
    McSocketData() : magic(0),_requestType(htonl((unsigned)-1)),_listenPort(0),_socketType(htons(SOCK_STREAM)) {}
};

/**
 * Datagram that is multicast by a host when it wants a service.
 * A McSocketDatagram contains the address of the sending host,
 * a port number that the sending host is listening on, the type
 * of connection required (SOCK_STREAM or SOCK_DGRAM), and the
 * type of the service requested.
 */
class McSocketDatagram: public DatagramPacketT<McSocketData>
{
public:
    McSocketDatagram(int requestType=-1);

    /**
     * Copy constructor.
     */
    McSocketDatagram(const McSocketDatagram& x);

    /**
     * Assignment operator.
     */
    McSocketDatagram& operator=(const McSocketDatagram& rhs);

    int getMagic() const { return ntohl(mcdata.magic); }

    void setMagic(int val) { mcdata.magic = htonl(val); }

    int getRequestType() const { return ntohl(mcdata._requestType); }

    void setRequestType(int val) { mcdata._requestType = htonl(val); }

    /**
     * What port is the requester listening on for the connection back?
     */
    int getRequesterListenPort() const { return ntohs(mcdata._listenPort); }

    void setRequesterListenPort(int val) { mcdata._listenPort = htons(val); }

    /**
     * What socket type does the requester want to establish?
     * SOCK_STREAM or SOCK_DGRAM.
     */
    int getSocketType() const { return ntohs(mcdata._socketType); }

    void setSocketType(int val) { mcdata._socketType = htons(val); }

    /**
     * Magic value that should be found at the beginning of
     * all received McSocketDatagrams.
     */
    static const int magicVal;

protected:
    struct McSocketData mcdata;
};


template <class SocketT>
class McSocket;

inline int getMcSocketType(McSocket<Socket>*)
{
    return SOCK_STREAM;
}

inline int getMcSocketType(McSocket<DatagramSocket>*)
{
    return SOCK_DGRAM;
}

/**
 * A McSocket provides a way to establish a TCP stream socket
 * connection, or a pair of UDP datagram sockets using a common
 * port number.  The communication is esablished by multicasting a
 * request for the service. Contained in the multicast datagram
 * is an integer value for the type of service requested, allowing
 * this protocol to be used for any type of service that can
 * be provided over TCP or UDP sockets.
 *
 * Use a McSocket<Socket> to establish a TCP SOCK_STREAM connection.
 * Use a McSocket<DatagramSocket> to establish a pair of UDP SOCK_DGRAM
 * sockets.
 *
 * McSocket is used to establish the connection at either end.
 * A McSocket can either listen for a connection (like a ServerSocket)
 * or connect to a remote McSocket.
 *
 * When McSocket does a connect(), it starts a McSocketMulticaster
 * thread. This thread creates either a ServerSocket (for McSocket<Socket>)
 * or a DatagramSocket (for McSocket<DatagramSocket>), using any available
 * port number. Then it multicasts McSocketDatagrams containing
 * the request type of the McSocket, the socket type (SOCK_STREAM or SOCK_DGRAM),
 * and the port number of the ServerSocket or DatagramSocket. When a select
 * indicates that data is readable on the created socket, then the
 * McSocketMulticaster passes the accepted socket back to the McSocket
 * as the return value of the connect() method. The McSocketMulticaster thread
 * then quits. This connection can also be established asynchronously
 * in a two-step sequence using request() and connected() instead of connect().
 *
 * When a McSocket does an accept(), it registers itself with
 * a McSocketListener, a thread which is listening for
 * McSocketDatagrams on a multicast address.
 * When the McSocketListener receives a McSocketDatagram, it
 * checks if a McSocket has requested a connection for the
 * given request type and socket type.
 * If so, then it creates a Socket or DatagramSocket and connects it
 * to the socket port on the requesting host. This Socket
 * is returned as the value of the accept() method.
 * This connection can also be established asynchronously
 * in a two-step sequence using listen() and connected() instead of accept().
 */
template <class SocketT>
class McSocket
{
    /**
     * McSocketListener and McSocketMulticaster are friends that setup
     * the socket connection and call the private offer() method.
     */
    friend class McSocketListener;

    template<class SocketTT>
    friend class McSocketMulticaster;
public:
    /**
     * Create a McSocket for requesting or accepting multicast
     * requests for a socket connection.
     * Typical usage:
     *
*      @code
*      Inet4Address mcastAddr = Inet4Address::getByName("239.0.0.10");
*      int mport = 10000;
*      Inet4SocketAddress mcastSockAddr(mcastAddr,mport);
*
*      int rtype = 99;
*
*      McSocket<Socket> server;
*      server.setInet4McastSocketAddress(mcastSockAddr);
*      server.setRequestType(rtype);
*
*      McSocket<Socket> client;
*      client.setInet4McastSocketAddress(mcastSockAddr);
*      client.setRequestType(rtype);
*
*      class McThread: public Thread {
*      public:
*         McThread(McSocket<Socket>& clnt):
*		Thread("McThread"),client(clnt) {}
*	int run() throw(Exception)
*	{
*           Socket* socket = client.connect();
*	    char buf[16];
*	    size_t l = socket->recv(buf,sizeof(buf));
*	    buf[l] = '\0';
*	    cerr << "requester read, l=" << l << endl;
*	    if (strcmp(buf,"hello\n"))
*	        throw Exception("McThread socket read not as expected");
*           // for this test, expect EOF on second recv.
*	    try {
*		l = socket->recv(buf,sizeof(buf));
*	    }
*	    catch(const EOFException& e) {
*		cerr << "requester EOF, closing socket" << endl;
*		socket->close();
*		delete socket;
*		return RUN_OK;
*	    }
*	    socket.close();
*	    delete socket;
*	    cerr << "requester no EOF, closing socket" << endl;
*	    throw Exception("McThread socket read not as expected");
*	}
*       private:
*	   McSocket<Socket>& client;
*       } cthread(client);
*
*       cthread.start();
*
*       Socket* socket = server.accept();
*       cerr << "server accepted, socket=" << hex << socket << endl;
*       socket->send("hello\n",7);
*       cerr << "server closing socket" << endl;
*       socket->close();
*       delete socket;
*
*       cerr << "joining client cthread" << endl;
*       cthread.join();
*
*       server.close();
*       client.close();
*
    * @endcode
     */
    McSocket();

    /**
     * Copy constructor.
     */
    McSocket(const McSocket<SocketT>&);

    /**
     * Assignment operator.
     */
    McSocket<SocketT>& operator=(const McSocket<SocketT>& rhs);

    virtual ~McSocket();

    /**
     * Set a specific interface for the multicasts.
     * If a request() or connect() is done, then requests will be sent on this
     * interface. If an listen() or accept() is done, then an
     * MulticastSocket::joinInterface() will be done on this interface to
     * listen for incoming datagrams.  If the interface is not set, or is left
     * at the default of INADDR_ANY, then McSocket will send on or join all
     * available * interfaces capable of multicast, including the loopback
     * interface.
     */
    void setInterface(Inet4NetworkInterface iaddr) {
        _iface = iaddr;
    }

    Inet4NetworkInterface getInterface() const { return _iface; }

    /**
     * Return all network interfaces on this system.
     *
     * @throws IOException
     **/
    std::list<Inet4NetworkInterface> getInterfaces() const;

    /**
     * Get the multicast address for listening to requests.
     */
    const Inet4SocketAddress& getInet4McastSocketAddress() const
    {
        return _mcastAddr;
    }

    /**
     * Set the multicast address for listening to requests.
     * @param val Multicast address to listen for McSocketDatagrams.
     */
    void setInet4McastSocketAddress(const Inet4SocketAddress& val) { _mcastAddr = val; }

    /**
     * Set the request type value.  This can be any number, agreed
     * upon by the McSocket sending requests and the McSocket listening
     * for requests.
     * @param val Request number
     */
    void setRequestType(int val) { _requestType = val; }

    /**
     * Get the request type number.
     */
    int getRequestType() const { return _requestType; }

    /**
     * Register with a McSocketListener to listen on the multicast
     * address. When a request is received on the socket port, with a
     * matching request type and socket type, McSocketListener will either
     * do a Socket::connect() to establish a TCP connection back to
     * the requesting host and port if the socket type is SOCK_STREAM,
     * or if the socket type is SOCK_DGRAM, will create a DatagramSocket
     * with a default destination address of the requesting host and port.
     * Once this has been done, McSocketListener will call the offer()
     * method of this McSocket. offer() then calls the virtual connected()
     * method, passing a pointer to the connected socket.
     * Use either listen() or accept() to wait for a connection.
     *
     * @throws IOException
     **/
    void listen();

    /**
     * Like ServerSocket::accept(), this method will return a connected socket.
     * As with listen(), it registers with a McSocketListener to listen
     * on the multicast address, then waits until a request is received with
     * a matching request type and socket type.  accept() will return with
     * a pointer to a connected TCP Socket for a McSocket<Socket>,
     * or to a DatagramSocket for a McSocket<DatagramSocket>,
     * with a default destination address of the requesting host and port.
     * The caller owns the pointer to the socket and is responsible for
     * closing and deleting it when done.
     *
     * @throws IOException
     **/
    SocketT* accept(Inet4PacketInfoX&);

    /**
     * Virtual method that is called when a socket connection is established.
     * This must be implemented in derived classes if the user wants
     * to implement an asynchronous, two-step connection using
     * listen() instead of accept() for incoming requests or
     * request() instead of connect() for outgoing requests.
     * @param sock The socket which is ready for I/O.
     * McSocket then owns the pointer to the socket and is responsible
     * for closing and deleting it when done.
     */
    virtual void connected(SocketT*,const Inet4PacketInfoX&) {}

    /**
     * Start issuing requests for a connection by multicasting McSocketDatagrams.
     * When a response is received, either in the form of a TCP socket
     * connection in the case of a socket type of SOCK_STREAM, or a datagram
     * is received in the case of a socket type of SOCK_DGRAM, then the
     * connected() method is called.
     *
     * @throws IOException
     **/
    void request();

    /**
     * Do a request(), and then wait until a TCP connection is established,
     * or a UDP datagram is received back.  Returns the pointer to the connected
     * TCP Socket or the DatagramSocket.
     * McSocket then owns the pointer to the socket and is responsible
     * for closing and deleting it when done.
     *
     * @throws IOException
     **/
    SocketT* connect(Inet4PacketInfoX&);

    /**
     * Unregister this McSocket from the multicasting and listening threads.
     *
     * @throws IOException
     **/
    virtual void close();

private:
    /**
     * How a McSocketListener passes back a connected TCP socket or
     * DatagramSocket. offer() calls the connected() virtual method.
     * McSocket will own the pointer to the socket and is responsible
     * for closing and deleting it when done.
     * @param sock A pointer to a Socket. May be null, in which case err
     *       will be a non-zero errno that occured.
     * @param err If sock is null, an errno.
     */
    void offer(SocketT* sock,const Inet4PacketInfoX& pktinfo);

    void offer(int err);

    void joinMulticaster();

    Inet4SocketAddress _mcastAddr;

    Inet4NetworkInterface _iface;

    int _requestType;

    Cond _connectCond;

    SocketT* _newsocket;

    Inet4PacketInfoX _newpktinfo;

    bool _socketOffered;

    int _offerErrno;

    /**
     * The thread we start which multicasts for connections.
     */
    Thread* _multicaster;

    Mutex _multicaster_mutex;

    // Inet4PacketInfoX _pktinfo;

};

/**
 * Class for listening on McSocket requests on a specific multicast address
 * and UDP port number.  Instances of this class are started as needed
 * as a result of McSocket::listen() or McSocket::accept(),
 * and shutdown as needed when a McSocket is closed. Methods of this
 * class are private and only accessed by McSocket.
 */
class McSocketListener: private Thread
{
    template<class SocketT>
    friend class McSocket;

public:

    /**
     * Public method to return the number of McSocketListeners that are active.
     */
    static int check() throw();

private:
    /**
     * How a McSocket<Socket> registers with a McSocketListener. If
     * a McSocketListener is not running on the multicast address
     * of the McSocket, then one is created and started.
     * When a McSocketDatagram arrives for the McSocket, matching the request type
     * and a socket type of SOCK_STREAM, then a TCP socket connection is made back
     * to the requesting host, and the connected socket is passed back to the
     * McSocket via McSocket::offer();
     *
     * @throws Exception
     **/
    static void accept(McSocket<Socket>* sock);

    /**
     * How a McSocket<DatagramSocket> registers with a McSocketListener. If
     * a McSocketListener is not running on the multicast address
     * of the McSocket, then one is created and started.
     * When a McSocketDatagram arrives for the McSocket, matching the request type
     * and a socket type of SOCK_DGRAM, then a DatagramSocket is created, with a default
     * destination address of the requesting host and port.
     * This DatagramSocket is passed back to the McSocket via McSocket::offer();
     *
     * @throws Exception
     **/
    static void accept(McSocket<DatagramSocket>* sock);

    /**
     * Remove the given McSocket from the list being served by this listener.
     * If there are no more McSockets that are listening on a given
     * multicast address and port, then the listener is shut down.
     *
     * @throws Exception
     **/
    static void close(McSocket<Socket>* sock);

    /**
     * Remove the given McSocket from the list being served by this listener.
     * If there are no more McSockets which are listening on a given
     * multicast address and port, then the listener is shut down.
     *
     * @throws Exception
     **/
    static void close(McSocket<DatagramSocket>* sock);

    /**
     * @throws Exception
     **/
    int run();

    void interrupt();

private:
    McSocketListener(const Inet4SocketAddress& addr);

    ~McSocketListener();

    void add(McSocket<Socket>* mcsocket);

    void add(McSocket<DatagramSocket>* mcsocket);

    int remove(McSocket<Socket>* mcsocket);

    int remove(McSocket<DatagramSocket>* mcsocket);

    /** No copying */
    McSocketListener(const McSocketListener&);

    /** No assignment */
    McSocketListener& operator=(const McSocketListener&);

private:

    Inet4SocketAddress _mcastAddr;

    Mutex _mcsocket_mutex;

    DatagramSocket* _readsock;

    std::map<int,McSocket<Socket>*> _tcpMcSockets;

    std::map<int,McSocket<DatagramSocket>*> _udpMcSockets;

};

/**
 * Thread which is started by McSocket to multicast requests
 * for connections.  An instance of this class is
 * started as a result of McSocket::request() or McSocket::connect(),
 * and shutdown when the McSocket is closed.  Methods of this
 * class are private and only accessed by McSocket.
 */
template<class SocketTT>
class McSocketMulticaster: private Thread
{
    template<class SocketT>
    friend class McSocket;

private:
    McSocketMulticaster(McSocket<SocketTT>* mcsocket);

    /** No copying */
    McSocketMulticaster(const McSocketMulticaster&);

    /** No assignment */
    McSocketMulticaster& operator=(const McSocketMulticaster&);

    virtual ~McSocketMulticaster();

    /**
     * @throws Exception
     **/
    int run();

    void interrupt();

    McSocket<SocketTT>* _mcsocket;

    /**
     * If multicasting for a TCP connection, then _serverSocket will
     * point to the TCP socket that listening for a connection.
     */
    ServerSocket* _serverSocket;

    /**
     * If multicasting for a UDP connection, then _datagramSocket will
     * point to the UDP socket that is waiting for incoming responses.
     */
    DatagramSocket* _datagramSocket;

    /**
     * The DatagramSocket that requests are sent on.
     */
    DatagramSocket* _requestSocket;

    Mutex _mcsocketMutex;
};

}}	// namespace nidas namespace util

#include "Logger.h"
#include <memory>

namespace nidas { namespace util {

template<class SocketT>
McSocket<SocketT>::McSocket(): _mcastAddr(),_iface(),_requestType(-1),
    _connectCond(),
    _newsocket(0),_newpktinfo(),
    _socketOffered(false),_offerErrno(0),
    _multicaster(0),_multicaster_mutex()
{
}

template<class SocketT>
McSocket<SocketT>::McSocket(const McSocket<SocketT>& x) :
    _mcastAddr(x._mcastAddr),_iface(x._iface),
    _requestType(x._requestType), _connectCond(),
    _newsocket(0),_newpktinfo(),
    _socketOffered(false),_offerErrno(0),
    _multicaster(0),_multicaster_mutex()
{
}

template<class SocketT>
McSocket<SocketT>& McSocket<SocketT>::operator=(const McSocket<SocketT>& rhs)
{
    if (&rhs != this) {
        _mcastAddr = rhs._mcastAddr;
        _iface = rhs._iface;
        _requestType = rhs._requestType;
        _newsocket = 0;
        _socketOffered = false;
        _multicaster = 0;
    }
    return *this;
}

template<class SocketT>
McSocket<SocketT>::~McSocket() {
}

template<class SocketT>
std::list<Inet4NetworkInterface> McSocket<SocketT>::getInterfaces() const
{
    MulticastSocket tmpsock(_mcastAddr.getPort());
    std::list<Inet4NetworkInterface> ifcs = tmpsock.getInterfaces();
    tmpsock.close();
    return ifcs;
}

template<class SocketT>
void McSocket<SocketT>::listen()
{
    if (getRequestType() < 0)
        throw IOException(_mcastAddr.toString(), "listen",
                          "request number has not been set");
    _connectCond.lock();
    _newsocket = 0;
    _socketOffered = false;
    _connectCond.unlock();
    try {
        McSocketListener::accept(this);
    }
    catch (const Exception& e) {
        throw IOException(_mcastAddr.toString(), "accept", e.what());
    }
}

/*
 * Does a listen and then waits for the connection.
 */
template<class SocketT>
SocketT* McSocket<SocketT>::accept(Inet4PacketInfoX& pktinfo)
{
    listen();
    _connectCond.lock();
    while(!_socketOffered) _connectCond.wait();
    SocketT* socket = _newsocket;
    pktinfo = _newpktinfo;
    _newsocket = 0;
    _connectCond.unlock();
    VLOG(("accept offerErrno=%d", _offerErrno));
    if (!socket) throw IOException("McSocket","accept",_offerErrno);
    return socket;
}

template<class SocketT>
void McSocket<SocketT>::request()
{
    if (getRequestType() < 0)
        throw IOException(_mcastAddr.toString(),"listen",
                          "request number has not been set");
    _connectCond.lock();
    _newsocket = 0;
    _socketOffered = false;
    _connectCond.unlock();
    // if (!_mcastAddr.getInet4Address().isMultiCastAddress())
    // 	throw IOException(_mcastAddr.toString(),"accept","is not a multicast address");

    _multicaster_mutex.lock();
    if (!_multicaster) {
        _multicaster = new McSocketMulticaster<SocketT>(this);
        try {
            _multicaster->start();
        }
        catch(const Exception& e) {
            throw IOException("McSocket","request",e.what());
        }
    }
    _multicaster_mutex.unlock();
}

/*
 * Does a request() and then waits for the connection.
 */
template<class SocketT>
SocketT* McSocket<SocketT>::connect(Inet4PacketInfoX& pktinfo)
{
    request();
    _connectCond.lock();
    while(!_socketOffered) _connectCond.wait();
    SocketT* socket = _newsocket;
    pktinfo = _newpktinfo;
    _newsocket = 0;
    _connectCond.unlock();
    VLOG(("connect offerErrno=%d",_offerErrno));
    if (!socket) throw IOException("McSocket","connect",_offerErrno);
    return socket;
}

template<class SocketT>
void McSocket<SocketT>::joinMulticaster()
{
    // can't do a multicaster->join here, that would deadlock
    // because we'd be waiting for ourselves,
    // so we spawn a detached thread to join and delete it.

    _multicaster_mutex.lock();

    if (_multicaster) {
        VLOG(("Mcsocket::offer creating/starting joiner"));
        ThreadJoiner* joiner = new ThreadJoiner(_multicaster);
        _multicaster = 0;
        try {
            joiner->start();
        }
        // shouldn't happen
        catch(const Exception& e) {
            Logger::getInstance()->log(LOG_ERR,"%s",e.what());
        }
        VLOG(("Mcsocket::offer joiner started"));
    }
    VLOG(("Mcsocket::offer doing connectCond.lock"));
    _multicaster_mutex.unlock();
}
/*
 * Method that executes in the thread of the McSocketListener
 * or the McSocketMulticaster, and notifies whoever did
 * the listen() or request() that the socket is connected.
 */
template<class SocketT>
void McSocket<SocketT>::offer(SocketT* socket,const Inet4PacketInfoX& pktinfo)
{
    joinMulticaster();

    if (socket) connected(socket,pktinfo);

    _connectCond.lock();
    _socketOffered = true;
    _offerErrno = 0;
    _newsocket = socket;
    _newpktinfo = pktinfo;
    _connectCond.signal();
    _connectCond.unlock();

    // note: don't access any class variables or virtual methods
    // after the above unlock.  The calling thread may have
    // deleted this object after it received the connected Socket.
}

template<class SocketT>
void McSocket<SocketT>::offer(int err)
{
    joinMulticaster();

    _connectCond.lock();
    _socketOffered = true;
    _offerErrno = err;
    _newsocket = 0;
    _connectCond.signal();
    _connectCond.unlock();

    // note: don't access any class variables or virtual methods
    // after the above unlock.  The calling thread may have
    // deleted this object after it received the connected Socket.
}

template<class SocketT>
void McSocket<SocketT>::close()
{
    VLOG(("Mcsocket::close"));
    _multicaster_mutex.lock();
    if (_multicaster && _multicaster->isRunning()) _multicaster->interrupt();
    _multicaster_mutex.unlock();

    offer(EINTR);

    try {
        McSocketListener::close(this);
    }
    catch (const Exception& e) {
        VLOG(("Mcsocket::close exception: %s", e.what()));
        throw IOException(_mcastAddr.toString(), "close", e.what());
    }
    VLOG(("Mcsocket::close done, this=0x%x", this));
}

template<class SocketTT>
McSocketMulticaster<SocketTT>::McSocketMulticaster(McSocket<SocketTT>* mcsock) :
    Thread("McSocketMulticaster"),
    _mcsocket(mcsock),_serverSocket(0),_datagramSocket(0),_requestSocket(0),
    _mcsocketMutex()
{
    switch (getMcSocketType(mcsock)) {
    case SOCK_STREAM:
        _serverSocket = new ServerSocket();
        _datagramSocket = 0;
        break;
    default:
        _datagramSocket = new DatagramSocket();
        _serverSocket = 0;
        break;
    }
    // block SIGUSR1, then unblock it in pselect/ppoll
    blockSignal(SIGUSR1);
}

template<class SocketT>
McSocketMulticaster<SocketT>::~McSocketMulticaster()
{
    if (_serverSocket) {
        _serverSocket->close();
        delete _serverSocket;
    }
    if (_datagramSocket) {
        _datagramSocket->close();
        delete _datagramSocket;
    }
    if (_requestSocket) {
        _requestSocket->close();
        delete _requestSocket;
    }
}

template<class SocketT>
void McSocketMulticaster<SocketT>::interrupt()
{
    _mcsocketMutex.lock();
    _mcsocket = 0;
    _mcsocketMutex.unlock();
    try {
        kill(SIGUSR1);
    }
    catch(const Exception& e) {
        PLOG(("%s",e.what()));
    }
}


void
listMulticastInterfaces(MulticastSocket* requestmsock,
                        std::vector<Inet4NetworkInterface>& ifaces);

template<class SocketT>
int McSocketMulticaster<SocketT>::run()
{
    MulticastSocket* requestmsock = 0;

    int sockfd = (_serverSocket ? _serverSocket->getFd() :
        _datagramSocket->getFd());

#ifdef HAVE_PPOLL
    struct pollfd fds;
    fds.fd =  sockfd;
#ifdef POLLRDHUP
    fds.events = POLLIN | POLLRDHUP;
#else
    fds.events = POLLIN;
#endif
#else
    fd_set fdset;
    FD_ZERO(&fdset);
#endif

    // wait for this amount of time after a datagram send to receive a
    // connection or receipt of UDP data
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = NSECS_PER_SEC / 4;             // portion of a second

    // get the existing signal mask
    sigset_t sigmask;
    pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
    // unblock SIGUSR1 in pselect/ppoll
    sigdelset(&sigmask,SIGUSR1);

    Inet4SocketAddress mcsockaddr =
	_mcsocket->getInet4McastSocketAddress();
    Inet4Address mcaddr = mcsockaddr.getInet4Address();

    std::vector<Inet4NetworkInterface> ifaces;

    if (mcaddr.isMultiCastAddress())
    {
        VLOG(("") << mcsockaddr.toString() << " is a multicast address.");
        _requestSocket = requestmsock = new MulticastSocket();
        if (_mcsocket->getInterface().getAddress() == Inet4Address(INADDR_ANY))
        {
            listMulticastInterfaces(requestmsock, ifaces);
        }
    }
    else
    {
        VLOG(("") << mcsockaddr.toString()
             << " is not a multicast address, using a datagram request socket.");
        _requestSocket = new DatagramSocket();
    }

    McSocketDatagram dgram;
    dgram.setMagic(dgram.magicVal);
    dgram.setSocketAddress(mcsockaddr);
    dgram.setRequestType(_mcsocket->getRequestType());
    dgram.setSocketType(getMcSocketType(_mcsocket));
    if (_serverSocket)
        dgram.setRequesterListenPort(_serverSocket->getLocalPort());
    else
        dgram.setRequesterListenPort(_datagramSocket->getLocalPort());

    for (int numCasts=0; !isInterrupted() ; numCasts++) {
        // If multicast, loop over interfaces
        try {
            if (requestmsock && ifaces.size() > 0) {
                Inet4NetworkInterface iface = ifaces[numCasts % ifaces.size()];
                requestmsock->setInterface(mcaddr,iface);
                _requestSocket->send(dgram);
            }
            else {
                _requestSocket->send(dgram);
            }
            if (!(numCasts % 300)) {
                ILOG(("") << "sent " << numCasts << " dgrams" <<
                     ", requestType=" << dgram.getRequestType() <<
                     ", port=" << dgram.getRequesterListenPort() <<
                     ", socketType=" << dgram.getSocketType() <<
                     ", len=" << dgram.getLength() <<
                     ", #mcifaces=" << ifaces.size());
            }
        }
        catch(const IOException& e)
        {
            // perhaps the interface has disappeared. Log the error.
            WLOG(("McSocketMulticaster: ")
                 << _requestSocket->getLocalSocketAddress().toString()
                 << ": " << e.what());
            sleep(10);
            if (requestmsock)
            {
                _requestSocket->close();
                delete _requestSocket;
                // close and re-create the socket
                _requestSocket = requestmsock = new MulticastSocket();
                // re-create the list of interfaces.
                listMulticastInterfaces(requestmsock, ifaces);
            }
            continue;
        }

        int res;
#ifdef HAVE_PPOLL
        res = ::ppoll(&fds,1,&timeout,&sigmask);

        if (res > 0) {

            if (fds.revents & POLLERR) {
                _mcsocketMutex.lock();
                if (_mcsocket) _mcsocket->offer(errno);
                _mcsocketMutex.unlock();
                if (_serverSocket) _serverSocket->close(); // OK to close twice
                if (_datagramSocket) _datagramSocket->close();
                _requestSocket->close();
                throw IOException("McSocket","ppoll",errno);
            }

#ifdef POLLRDHUP
            if (fds.revents & (POLLHUP | POLLRDHUP))
#else
            if (fds.revents & POLLHUP)
#endif
                WLOG(("%s POLLHUP","McSocket"));

        }

#else
        assert(sockfd >= 0 && sockfd < FD_SETSIZE);     // FD_SETSIZE=1024
        FD_SET(sockfd, &fdset);
        res = ::pselect(sockfd+1,&fdset,0,0,&timeout,&sigmask);
#endif
        if (res <= 0) {
            if (res == 0) continue;
            if (errno == EINTR) break;
            _mcsocketMutex.lock();
            if (_mcsocket) _mcsocket->offer(errno);
            _mcsocketMutex.unlock();
            if (_serverSocket) _serverSocket->close(); // OK to close twice
            if (_datagramSocket) _datagramSocket->close();
            _requestSocket->close();
            throw IOException("McSocket","poll/select",errno);
        }

        if (_serverSocket) {
            Socket* socket = _serverSocket->accept();
            DLOG(("accepted socket connection from ") <<
                socket->getRemoteSocketAddress().toString() << " on " <<
                _serverSocket->getLocalSocketAddress().toString());
            _serverSocket->close();
            nidas::util::Inet4PacketInfoX pktinfo;
            if (socket->getRemoteSocketAddress().getFamily() == AF_INET) {
                nidas::util::Inet4SocketAddress remoteAddr =
                    nidas::util::Inet4SocketAddress((const struct sockaddr_in*)
                    socket->getRemoteSocketAddress().getConstSockAddrPtr());
                pktinfo.setRemoteSocketAddress(remoteAddr);
            }
            if (socket->getLocalSocketAddress().getFamily() == AF_INET) {
                nidas::util::Inet4SocketAddress localAddr =
                    nidas::util::Inet4SocketAddress((const struct sockaddr_in*)
                    socket->getLocalSocketAddress().getConstSockAddrPtr());
                pktinfo.setLocalAddress(localAddr.getInet4Address());
                pktinfo.setDestinationAddress(localAddr.getInet4Address());
            }
            _mcsocketMutex.lock();
            if (_mcsocket) _mcsocket->offer((SocketT*)socket,pktinfo);
            _mcsocketMutex.unlock();
        }
        else {
            // If fishing for UDP responses, send out at least 3 multicasts
            // to see if we get more than one response.
            if (numCasts < 3) continue;

            // We know there is data at the socket, do a MSG_PEEK to get
            // information about the first packet.
            nidas::util::Inet4PacketInfoX pktinfo;
            _datagramSocket->receive(dgram,pktinfo,MSG_PEEK);
            if (dgram.getSocketAddress().getFamily() == AF_INET) {
                nidas::util::Inet4SocketAddress remoteAddr =
                    nidas::util::Inet4SocketAddress((const struct sockaddr_in*)
                    dgram.getSocketAddress().getConstSockAddrPtr());
                pktinfo.setRemoteSocketAddress(remoteAddr);
            }

            _mcsocketMutex.lock();
            if (_mcsocket) _mcsocket->offer((SocketT*)_datagramSocket,pktinfo);
            _datagramSocket = 0;    // we no longer own _datagramSocket
            _mcsocketMutex.unlock();
        }
        _requestSocket->close();
        return RUN_OK;
    }   // loop until receipt of connection or UDP packet

    VLOG(("McSocketMulticaster break"));
    _mcsocketMutex.lock();
    if (_mcsocket) _mcsocket->offer(EINTR);
    _mcsocketMutex.unlock();
    if (_serverSocket) _serverSocket->close();
    if (_datagramSocket) _datagramSocket->close();
    _requestSocket->close();
    VLOG(("McSocketMulticaster run method exiting"));
    return RUN_OK;
}

}}	// namespace nidas namespace util
#endif
