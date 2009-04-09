/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_UTIL_MCSOCKET_H
#define NIDAS_UTIL_MCSOCKET_H

#include <nidas/util/Socket.h>
#include <nidas/util/DatagramPacket.h>
#include <nidas/util/Thread.h>
#include <nidas/util/UTime.h>

#include <list>

namespace nidas { namespace util {

struct McSocketData {

    /**
     * Magic value that should be found at the beginning of all received datagrams.
     */
    int magic;
    /**
     * Pseudo port number of request which identifies the request.
     * A McSocketListener on the server side must be doing
     * an accept for a McSocket with the same
     * requestNum value. Stored in "network", big-endian order.
     */
    int _requestNum;

    /**
     * TCP stream socket port that the remote host is listening on.
     * Stored in "network", big-endian order.
     */
    unsigned short _listenPort;

    /**
     * Either SOCK_STREAM=1, or SOCK_DGRAM=2
     */
    short _socketType;

    /**
     * How many multicasts has it sent.
     * Stored in "network", big-endian order.
     */
    int _numMulticasts;

    /**
     * Constructor.
     */
    McSocketData() : _requestNum(0),_listenPort(0),_socketType(htons(SOCK_STREAM)),_numMulticasts(0) {}
};

/**
 * Datagram that is multicast by a host when it wants a service.
 */
class McSocketDatagram: public DatagramPacketT<McSocketData>
{
public:
    McSocketDatagram(int requestNum=0);

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


    int getRequestNumber() const { return ntohl(mcdata._requestNum); }
    void setRequestNumber(int val) { mcdata._requestNum = htonl(val); }

    /**
     * What TCP port is the requester listening on for the connection back?
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
     * How patiently has the client been waiting?
     */
    int getNumMulticasts() const { return ntohl(mcdata._numMulticasts); }
    void setNumMulticasts(int val) { mcdata._numMulticasts = htonl(val); }
    
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
inline int getMcSocketType(McSocket<Socket>* ptr)
{
    return SOCK_STREAM;
}

inline int getMcSocketType(McSocket<DatagramSocket>* ptr)
{
    return SOCK_DGRAM;
}

/**
 * A McSocket provides a way to establish a TCP stream socket
 * connection, or a pair of UDP datagram sockets using a common
 * port number.  The communication is esablished by multicasting a
 * request for the service.
 *
 * Use McSocket<Socket> to establish a TCP SOCK_STREAM connection,
 * and McSocket<DatagramSocket> to establish a pair of UDP SOCK_DGRAM
 * sockets.
 *
 * McSocket is used to establish the connection at either end.
 * A McSocket can either listen for a connection (like a ServerSocket)
 * or connect to a remote McSocket.
 *
 * When a McSocket does an accept(), it registers itself with
 * a McSocketListener, a thread which is listening for
 * McSocketDatagrams on a multicast address.
 *
 * A McSocketDatagram contains the address of the sending host,
 * a port number that the sending host is listening on, the type
 * of connection required (SOCK_STREAM or SOCK_DGRAM), and a
 * request number.
 * The McSocketListener checks if a McSocket has requested
 * a connection with the given request number and socket type.
 * If so, then it creates a Socket or DatagramSocket and connects it
 * to the socket port on the requesting host. This Socket
 * is returned as the value of the accept() method.
 *
 * When McSocket does a connect(), it starts a McSocketMulticaster
 * thread. This thread creates either a ServerSocket (for McSocket<Socket>)
 * or a DatagramSocket  (for McSocket<DatagramSocket>), using any available
 * port number. Then it multicasts McSocketDatagrams containing
 * the request number of the McSocket, the connection type, and the
 * port number of the ServerSocket or DatagramSocket. When a select
 * indicates that data is readable on the created socket, then the multicaster
 * thread quits, passing the accepted socket back to the McSocket via the offer() method.
 */

template <class SocketT>
class McSocket
{
    /**
     * McSocketListener and McSocketMulticaster are friends that setup
     * the socket connection and call the non-public offer() method.
     */
    friend class McSocketListener;

    template<class SocketTT>
    friend class McSocketMulticaster;
public:
    /**
     * Create a McSocket, accepting multicast
     * requests for a socket connection.
     * Typical usage:
     *
*      @code
*      Inet4Address mcastAddr = Inet4Address::getByName("239.0.0.10");
*      int mport = 10000;
*      Inet4SocketAddress mcastSockAddr(mcastAddr,mport);
*      
*      int pport = 99;
*      
*      McSocket<Socket> server;
*      server.setInet4McastSocketAddress(mcastSockAddr);
*      server.setRequestNumber(pport);
*      
*      McSocket<Socket> client;
*      client.setInet4McastSocketAddress(mcastSockAddr);
*      client.setRequestNumber(pport);
*      
*      class McThread: public Thread {
*      public:
*         McThread(McSocket<Socket>& clnt):
*	  	Thread("McThread"),client(clnt) {}
*	int run() throw(Exception)
*	{
*           Socket* socket = client.connect();
*	    char buf[16];
*	    size_t l = socket->recv(buf,sizeof(buf));
*	    buf[l] = '\0';
*	    cerr << "requester read, l=" << l << endl;
*	    if (strcmp(buf,"hello\n"))
*	        throw Exception("McThread socket read not as expected");
*	    try {
*		l = socket->recv(buf,sizeof(buf));
*	    }
*	    catch(const EOFException& e) {
*		cerr << "requester EOF, closing socket" << endl;
*		socket->close();
*		return RUN_OK;
*	    }
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
*
*       cerr << "joining cthread" << endl;
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

    virtual ~McSocket();

    void setInterface(Inet4NetworkInterface iaddr) {
        _iface = iaddr;
    }

    Inet4NetworkInterface getInterface() const { return _iface; }

    std::list<Inet4NetworkInterface> getInterfaces() const throw(IOException);

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
     * Get the request number.
     */
    int getRequestNumber() const { return _requestNum; }

    /**
     * Set the request number.
     * @param val Request number
     */
    void setRequestNumber(int val) { _requestNum = val; }

    /**
     * Register with a McSocketListener which is listening on my multicast
     * address. When a request is received on the address for my pseudoport
     * McSocketListener will call my offer() method when a socket has been
     * connected.  One either uses either form of listen() or accept()
     * to request a connection.
     */
    void listen() throw(IOException);

    /**
     * Like ServerSocket::accept(), this method will return a connected socket.
     * Register with a McSocketListener which is listening on my multicast
     * address. Then wait on a condition variable until a request is received
     * on the address for my pseudoport.  McSocketListener will call the offer()
     * method when a socket has been connected. offer() will signal the
     * condition variable, and then accept() will return with the given socket.
     */
    SocketT* accept() throw(IOException);

    /**
     * Start issuing requests for a connection by mulitcasting datagrams.
     * When a response arrives back, then the connected() method is called.
     */
    void request() throw(IOException);

    /**
     * Method that is called when a socket connection is established.
     * Must be implemented in derived classes if the user wants
     * to use request() and connected() instead of connect().
     */
    virtual void connected(SocketT* sock) {}

    /**
     * Do a request(), and then wait until a connection is established.
     * Returns the pointer to the connected socket.
     */
    SocketT* connect() throw(IOException);

    /**
     * Shutdown the multicasting and listening threads for this McSocket.
     */
    virtual void close() throw(IOException);

private:
    /**
     * How a McSocketListener hands me a connected TCP socket.
     * McSocket will own the pointer to the socket and is responsible
     * for closing and deleting it when done.
     * @param sock A pointer to a Socket. May be null, in which case err
     *       will be a non-zero errno to be reported.
     * @param err If sock is null, an errno.
     */
    void offer(SocketT* sock,int err);

    Inet4SocketAddress _mcastAddr;

    Inet4NetworkInterface _iface;

    int _requestNum;

    Cond _connectCond;

    SocketT* _newsocket;

    bool _socketOffered;

    int _offerErrno;

    /**
     * The thread we start which multicasts for connections.
     */
    Thread* _multicaster;

    Mutex _multicaster_mutex;

};

class McSocketListener: public Thread
{
public:

    /**
     * How a McSocket registers with a McSocketListener. If
     * a McSocketListener is not running on the multicast address
     * of the McSocket, then one is created and stared.
     * When a McSocketDatagram arrives for the McSocket, then
     * a TCP socket connection is made back to the requesting host,
     * and the connected socket passed back to the McSocket via
     * McSocket::offer();
     */
    static void accept(McSocket<Socket>* sock) throw(Exception);

    static void accept(McSocket<DatagramSocket>* sock) throw(Exception);

    static void close(McSocket<Socket>* sock) throw(Exception);

    static void close(McSocket<DatagramSocket>* sock) throw(Exception);

    static int check() throw();

    int run() throw(Exception);

    void interrupt();

private:
    McSocketListener(const Inet4SocketAddress& addr);

    ~McSocketListener();

    void add(McSocket<Socket>* mcsocket);

    void add(McSocket<DatagramSocket>* mcsocket);

    int remove(McSocket<Socket>* mcsocket);

    int remove(McSocket<DatagramSocket>* mcsocket);

private:

    Inet4SocketAddress _mcastAddr;

    Mutex _mcsocket_mutex;

    DatagramSocket* _readsock;

    std::map<int,McSocket<Socket>*> _tcpMcSockets;

    std::map<int,McSocket<DatagramSocket>*> _udpMcSockets;

};

template<class SocketTT>
class McSocketMulticaster: public Thread
{
public:
    McSocketMulticaster(McSocket<SocketTT>* mcsocket);

    virtual ~McSocketMulticaster();

    int run() throw(Exception);

    void interrupt();
private:
    McSocket<SocketTT>* _mcsocket;

    /**
     * If multicasting for a TCP connection, then _serverSocket will
     * point to the TCP socket that listening for a connection.
     */
    ServerSocket* _serverSocket;

    /**
     * If multicasting for a UDP connection, then _datagramSocket will
     * point to the UDP that is waiting for incoming responses.
     */
    SocketTT* _datagramSocket;

    /**
     * The DatagramSocket that requests are sent on.
     */
    DatagramSocket* _requestSocket;

    Mutex _mcsocketMutex;
};

}}	// namespace nidas namespace util

#include <nidas/util/Logger.h>
#include <memory>

namespace nidas { namespace util {

template<class SocketT>
McSocket<SocketT>::McSocket(): _mcastAddr(),_iface(),_requestNum(-1),
	_newsocket(0),_socketOffered(false),_multicaster(0)
{
}

/*
 * Copy constructor.
 */
template<class SocketT>
McSocket<SocketT>::McSocket(const McSocket<SocketT>& x) :
    _mcastAddr(x._mcastAddr),_iface(x._iface),
    _requestNum(x._requestNum),_newsocket(0),_socketOffered(false),_multicaster(0)
{
}

template<class SocketT>
McSocket<SocketT>::~McSocket() {
}

template<class SocketT>
std::list<Inet4NetworkInterface> McSocket<SocketT>::getInterfaces() const throw(IOException)
{
    MulticastSocket tmpsock(_mcastAddr.getPort());
    std::list<Inet4NetworkInterface> ifcs = tmpsock.getInterfaces();
    tmpsock.close();
    return ifcs;
}

template<class SocketT>
void McSocket<SocketT>::listen() throw(IOException)
{
    if (getRequestNumber() < 0)
        throw IOException(_mcastAddr.toString(),"listen",
		"request number has not been set");
    _newsocket = 0;
    _socketOffered = false;
    try {
	McSocketListener::accept(this);
    }
    catch (const Exception& e) {
	throw IOException(_mcastAddr.toString(),"accept",e.what());
    }
}

/*
 * Does a listen and then waits for the connection.
 */
template<class SocketT>
SocketT* McSocket<SocketT>::accept() throw(IOException)
{
    listen();
    _connectCond.lock();
    while(!_socketOffered) _connectCond.wait();
    SocketT* socket = _newsocket;
    _newsocket = 0;
    _connectCond.unlock();
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"accept offerErrno=%d",_offerErrno);
#endif
    if (!socket) throw IOException("McSocket","accept",_offerErrno);
    return socket;
}

template<class SocketT>
void McSocket<SocketT>::request() throw(IOException)
{
    if (getRequestNumber() < 0)
        throw IOException(_mcastAddr.toString(),"listen",
		"request number has not been set");
    _newsocket = 0;
    _socketOffered = false;
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

// #define DEBUG
/*
 * Does a request() and then waits for the connection.
 */
template<class SocketT>
SocketT* McSocket<SocketT>::connect() throw(IOException)
{
    request();
    _connectCond.lock();
    while(!_socketOffered) _connectCond.wait();
    SocketT* socket = _newsocket;
    _newsocket = 0;
    _connectCond.unlock();
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"connect offerErrno=%d",_offerErrno);
#endif
    if (!socket) throw IOException("McSocket","connect",_offerErrno);
    return socket;
}
#undef DEBUG

/*
 * Method that executes in the thread of the McSocketListener
 * or the McsocketMulticaster, and notifies whoever did
 * the listen() or request() that the socket is connected.
 */
// #define DEBUG
template<class SocketT>
void McSocket<SocketT>::offer(SocketT* socket,int err)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocket::offer, err=%d",err);
#endif

    // can't do a multicaster->join here, that would deadlock
    // because we'd be waiting for ourselves,
    // so we spawn a detached thread to join and delete it.

    _multicaster_mutex.lock();

    if (_multicaster) {
#ifdef DEBUG
	Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::offer creating/starting joiner");
#endif
	ThreadJoiner* joiner = new ThreadJoiner(_multicaster);
        _multicaster = 0;
	try {
	    joiner->start();
	}
	// shouldn't happen
	catch(const Exception& e) {
	    Logger::getInstance()->log(LOG_ERR,"%s",e.what());
	}
#ifdef DEBUG
	Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::offer joiner started");
#endif
    }
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::offer doing connectCond.lock");
#endif

    _multicaster_mutex.unlock();

    if (socket) connected(socket);

    _connectCond.lock();
    _socketOffered = true;
    _offerErrno = err;
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"setting offerErrno=%d",_offerErrno);
#endif
    _newsocket = socket;
    _connectCond.signal();
    _connectCond.unlock();

    // note: don't access any class variables or virtual methods
    // after the above unlock.  The calling thread may have
    // deleted this object after it received the connected Socket.
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::offer connectCond.unlock");
#endif

#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::offer done");
#endif
}

#undef DEBUG

// #define DEBUG

template<class SocketT>
void McSocket<SocketT>::close() throw(IOException)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::close");
#endif

    _multicaster_mutex.lock();
    if (_multicaster && _multicaster->isRunning()) _multicaster->interrupt();
    _multicaster_mutex.unlock();

    offer(0,EINTR);

    try {
	McSocketListener::close(this);
    }
    catch (const Exception& e) {
#ifdef DEBUG
        Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::close exception: %s",
    	    e.what());
#endif
	throw IOException(_mcastAddr.toString(),"close",e.what());
    }
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::close done, this=0x%x",
    	this);
#endif
}

template<class SocketT>
McSocketMulticaster<SocketT>::McSocketMulticaster(McSocket<SocketT>* mcsock) :
        Thread("McSocketMulticaster"),
	_mcsocket(mcsock),_requestSocket(0)
{
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
    blockSignal(SIGHUP);
    switch (getMcSocketType(mcsock)) {
    case SOCK_STREAM:
        _serverSocket = new ServerSocket();
        _datagramSocket = 0;
        break;
    default:
        _datagramSocket = new SocketT();
        _serverSocket = 0;
        break;
    }
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
    if (_serverSocket) _serverSocket->close();
    if (_datagramSocket) _datagramSocket->close();
    Thread::interrupt();
    _mcsocketMutex.unlock();
}

template<class SocketT>
int McSocketMulticaster<SocketT>::run() throw(Exception)
{
    MulticastSocket* requestmsock = 0;

    int sockfd = (_serverSocket ? _serverSocket->getFd() :
        _datagramSocket->getFd());
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
    struct timeval waitPeriod,tmpto;
    waitPeriod.tv_sec = 0;
    waitPeriod.tv_usec = USECS_PER_SEC / 4;             // portion of a second

    Inet4SocketAddress mcsockaddr =
    	_mcsocket->getInet4McastSocketAddress();
    Inet4Address mcaddr = mcsockaddr.getInet4Address();

    std::list<Inet4NetworkInterface> ifaces;

    if (mcaddr.isMultiCastAddress()) {
	_requestSocket = requestmsock = new MulticastSocket();
        if (_mcsocket->getInterface().getAddress() == Inet4Address(INADDR_ANY)) {
            std::list<Inet4NetworkInterface> tmpifaces = requestmsock->getInterfaces();
            std::list<Inet4NetworkInterface>::const_iterator ifacei = tmpifaces.begin();
            for ( ; ifacei != tmpifaces.end(); ++ifacei) {
                Inet4NetworkInterface iface = *ifacei;
                int flags = iface.getFlags();
                if (flags & IFF_UP && flags | (IFF_MULTICAST | IFF_LOOPBACK))
                    ifaces.push_back(iface);
            }
        }
    }
    else
	_requestSocket = new DatagramSocket();

    McSocketDatagram dgram;
    dgram.setMagic(dgram.magicVal);
    dgram.setSocketAddress(mcsockaddr);
    dgram.setRequestNumber(_mcsocket->getRequestNumber());
    if (_serverSocket)
        dgram.setRequesterListenPort(_serverSocket->getLocalPort());
    else
        dgram.setRequesterListenPort(_datagramSocket->getLocalPort());

    for (int numCasts=0; ; numCasts++) {
        // If multicast, send on all interfaces
        if (requestmsock && ifaces.size() > 0) {
            std::list<Inet4NetworkInterface>::const_iterator ifacei = ifaces.begin();
            for ( ; ifacei != ifaces.end(); ++ifacei) {
                requestmsock->setInterface(mcaddr,*ifacei);
                _requestSocket->send(dgram);
            }
        }
        else _requestSocket->send(dgram);

	if (!(numCasts % 10))
	    std::cerr << "sent " << numCasts << " dgrams, length=" << dgram.getLength() <<
		", requestNum=" << dgram.getRequestNumber() <<
		", port=" << dgram.getRequesterListenPort() <<
                ", #mcifaces=" << ifaces.size() << std::endl;

	tmpto = waitPeriod;
	int res;
	fd_set tmpset = fdset;
	if ((res = ::select(sockfd+1,&tmpset,0,0,&tmpto)) < 0) {
#ifdef DEBUG
	    Logger::getInstance()->log(LOG_DEBUG,"McSocketMulticaster select error, errno=%d",errno);
#endif
            // close of ServerSocket will cause an EBADF
	    if (errno == EINTR || errno == EBADF) break;
            _mcsocketMutex.lock();
	    if (_mcsocket) _mcsocket->offer((SocketT*)0,errno);
            _mcsocketMutex.unlock();
            if (_serverSocket) _serverSocket->close(); // OK to close twice
            if (_datagramSocket) _datagramSocket->close();
	    _requestSocket->close();
	    throw IOException("McSocket","select",errno);
	}
	if (res > 0 && FD_ISSET(sockfd,&tmpset)) {
            if (_serverSocket) {
                SocketT* socket = _serverSocket->accept();
                _serverSocket->close();
                _mcsocketMutex.lock();
                if (_mcsocket) _mcsocket->offer(socket,0);
                _mcsocketMutex.unlock();
            }
            else {
                // If fishing for UDP responses, send out at least 3 multicasts
                // to see if we get more than one response.
                if (numCasts < 3) continue;
                _mcsocketMutex.lock();
                if (_mcsocket) _mcsocket->offer(_datagramSocket,0);
                _datagramSocket = 0;
                _mcsocketMutex.unlock();
            }
            _requestSocket->close();
	    return RUN_OK;
	}
	if (amInterrupted()) break;
    }
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocketMulticaster break");
#endif
    _mcsocketMutex.lock();
    if (_mcsocket) _mcsocket->offer(0,EINTR);
    _mcsocketMutex.unlock();
    if (_serverSocket) _serverSocket->close();
    if (_datagramSocket) _datagramSocket->close();
    _requestSocket->close();
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocketMulticaster run method exiting");
#endif
    return RUN_OK;
}

}}	// namespace nidas namespace util
#endif
