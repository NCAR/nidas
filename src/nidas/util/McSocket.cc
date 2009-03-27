
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/util/McSocket.h>
#include <nidas/util/Logger.h>
#include <nidas/util/IOTimeoutException.h>

#include <iostream>
#include <memory>  // auto_ptr

using namespace nidas::util;
using namespace std;

typedef map<Inet4SocketAddress,McSocketListener*> listener_map_t;

struct ListenerMap : public listener_map_t
{
  ListenerMap()
  {
#ifdef DEBUG
    std::cerr << "construct ListenerMap\n";
#endif
  }

  ~ListenerMap()
  {
#ifdef DEBUG
    std::cerr << "destroy ListenerMap\n";
#endif
  }
};


namespace 
{
  Mutex listener_mutex;

  ListenerMap listener_map;
}


/* static */
const int McSocketDatagram::magicVal = 0x01234567;

McSocketDatagram::McSocketDatagram(int requestNum) :
    DatagramPacketT<McSocketData>(&mcdata,1)
{
    setRequestNumber(requestNum);
}

/*
 * Copy constructor.
 */
McSocketDatagram::McSocketDatagram(const McSocketDatagram& x):
    DatagramPacketT<McSocketData>(x)
{
    mcdata = x.mcdata;
    data = &mcdata; // point to new mcdata
}

/*
 * Assignment operator.
 */
McSocketDatagram& McSocketDatagram::operator=(const McSocketDatagram& rhs)
{
    if (this != &rhs) {
        // invoke subclass assignment.
        (*(DatagramPacketT<McSocketData>*)this) =
                (DatagramPacketT<McSocketData>) rhs;
        mcdata = rhs.mcdata;    // copy the data
        data = &mcdata; // point to new mcdata
    }
    return *this;
}

McSocket::McSocket(): _mcastAddr(),_iface(),_requestNum(-1),
	_newsocket(0),_socketOffered(false),_multicaster(0)
{
}

/*
 * Copy constructor.
 */
McSocket::McSocket(const McSocket& x) :
    _mcastAddr(x._mcastAddr),_iface(x._iface),
    _requestNum(x._requestNum),_newsocket(0),_socketOffered(false),_multicaster(0)
{
}

McSocket::~McSocket() {
}

list<Inet4NetworkInterface> McSocket::getInterfaces() const throw(IOException)
{
    MulticastSocket tmpsock(_mcastAddr.getPort());
    list<Inet4NetworkInterface> ifcs = tmpsock.getInterfaces();
    tmpsock.close();
    return ifcs;
}


void McSocket::listen() throw(IOException)
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
Socket* McSocket::accept() throw(IOException)
{
    listen();
    _connectCond.lock();
    while(!_socketOffered) _connectCond.wait();
    Socket* socket = _newsocket;
    _newsocket = 0;
    _connectCond.unlock();
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"accept offerErrno=%d",_offerErrno);
#endif
    if (!socket) throw IOException("McSocket","accept",_offerErrno);
    return socket;
}

void McSocket::request() throw(IOException)
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
        _multicaster = new McSocketMulticaster(this);
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
Socket* McSocket::connect() throw(IOException)
{
    request();
    _connectCond.lock();
    while(!_socketOffered) _connectCond.wait();
    Socket* socket = _newsocket;
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
void McSocket::offer(Socket* socket,int err)
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

void McSocket::close() throw(IOException)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::close");
#endif

    _multicaster_mutex.lock();

    if (_multicaster && _multicaster->isRunning()) {
        _multicaster->interrupt();
    }
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

#undef DEBUG

/* static */
void McSocketListener::accept(McSocket* mcsocket)
	throw(Exception)
{
    const Inet4SocketAddress& mcAddr = mcsocket->getInet4McastSocketAddress();

#ifdef DEBUG
    cerr << "McSocketListener::accept, listener_map.size()=" <<
    	listener_map.size() << endl;
#endif

    listener_mutex.lock();
    McSocketListener* lnr = listener_map[mcAddr];

    if (!lnr) {
	lnr = new McSocketListener(mcAddr);
	listener_map[mcAddr] = lnr;
	listener_mutex.unlock();

	lnr->add(mcsocket);
	lnr->start();
    }
    else {
	listener_mutex.unlock();
        lnr->add(mcsocket);
    }
}

// #define DEBUG
/* static */
void McSocketListener::close(McSocket* mcsocket)
	throw(Exception)
{
    const Inet4SocketAddress& mcAddr = mcsocket->getInet4McastSocketAddress();

    Synchronized sync(listener_mutex);

#ifdef DEBUG
    cerr << "McSocketListener::close, map size=" << listener_map.size() << endl;
#endif
    map<Inet4SocketAddress,McSocketListener*>::iterator mapi =
	listener_map.find(mcAddr);

    if (mapi == listener_map.end()) return;

    McSocketListener* lnr = mapi->second;
    if (!lnr) return;

    // how many McSockets are still being serviced
    int nsock = lnr->remove(mcsocket);
#ifdef DEBUG
    cerr << "nsock=" << nsock << endl;
#endif
    if (nsock == 0) {
	lnr->interrupt();
	// lnr->cancel();
#ifdef DEBUG
        cerr << "McSocketListener::close, lnr->join()" << endl;
#endif
	lnr->join();
#ifdef DEBUG
        cerr << "McSocketListener::close, lnr->joined" << endl;
#endif
	delete lnr;
	listener_map.erase(mapi);
    }
}
// #undef DEBUG

/* static */
int McSocketListener::check() throw()
{
    Synchronized sync(listener_mutex);
    return listener_map.size();
}

McSocketListener::McSocketListener(const Inet4SocketAddress&
	mcastaddr) :
	Thread(string("McSocketListener: ")
		+ mcastaddr.toString()), _mcastAddr(mcastaddr)
{
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
    blockSignal(SIGHUP);
}

void McSocketListener::add(McSocket* mcsocket)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocketListener add requestNum=%d",
    	mcsocket->getRequestNumber());
#endif
    _mcsocket_mutex.lock();
    _mcsockets[mcsocket->getRequestNumber()] = mcsocket;
    _mcsocket_mutex.unlock();
}

int McSocketListener::remove(McSocket* mcsocket)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,
    	"McSocketListener remove requestNum=%d",
	    mcsocket->getRequestNumber());
#endif
    Synchronized autolock(_mcsocket_mutex);

    map<int,McSocket*>::iterator mapi =
    	_mcsockets.find(mcsocket->getRequestNumber());

    if (mapi != _mcsockets.end() && mapi->second == mcsocket)
	_mcsockets.erase(mapi);

    int nsock = _mcsockets.size();
    return nsock;
}

// #define DEBUG
int McSocketListener::run() throw(Exception)
{
    auto_ptr<DatagramSocket> readsock;

    if (_mcastAddr.getInet4Address().isMultiCastAddress()) {
	// can't bind to a specific address, must bind to INADDR_ANY.
	MulticastSocket* msock = new MulticastSocket(_mcastAddr.getPort());
	readsock.reset(msock);
	msock->joinGroup(_mcastAddr.getInet4Address());
    }
    else
	readsock.reset(new DatagramSocket(_mcastAddr.getPort()));
    readsock->setTimeout(1000);		// 1000 millisecs

    while (!amInterrupted()) {

	McSocketDatagram dgram;

	/* receive is a cancelation point. If this thread is
	 * canceled it will throw an IOException here.
	 * Our cancel method first does a cancel on
	 * the startedClients.
	 */
	try {
#ifdef DEBUG
	    cerr << "McSocketListener::receive" << endl;
#endif
	    readsock->receive(dgram);
#ifdef DEBUG
	    cerr << "McSocketListener::received from " <<
	     	dgram.getSocketAddress().toString() << 
	 	" length=" << dgram.getLength() << endl;
#endif
	}
	catch(const IOTimeoutException& e)
	{
#ifdef DEBUG
	    cerr << "McSocketListener::run: " << e.toString() << endl;
#endif
	    continue;
	}
	catch(const IOException& e)
	{
#ifdef DEBUG
	    cerr << "McSocketListener::run: " << e.what() << endl;
#endif
	    if (e.getError() == EINTR) break;
	    throw e;
	}

	Logger::getInstance()->log(LOG_DEBUG,
	"received dgram, magic=0x%x, requestNum=%d, len=%d, port=%d, sizeof=%d\n",
		dgram.getMagic(),dgram.getRequestNumber(),
		dgram.getLength(),dgram.getRequesterListenPort(),
	    	sizeof(McSocketDatagram));

	if (dgram.getMagic() != dgram.magicVal) continue;

	unsigned short port = dgram.getRequesterListenPort();
	Inet4SocketAddress& daddr =
		dynamic_cast<Inet4SocketAddress&>(dgram.getSocketAddress());
	Inet4SocketAddress remoteAddr(daddr.getInet4Address(),port);

	// look for an mcsocket matching this request

	McSocket* mcsocket = 0;

	_mcsocket_mutex.lock();
	map<int,McSocket*>::iterator mapi =
	    _mcsockets.find(dgram.getRequestNumber());
	if (mapi != _mcsockets.end()) mcsocket = mapi->second;
	_mcsocket_mutex.unlock();

	if (!mcsocket) {
	    Logger::getInstance()->log(LOG_WARNING,"No McSocket for pseudoport:%d from host %s\n",
	    	dgram.getRequestNumber(),
		dgram.getSocketAddress().toString().c_str());
	    continue;
	}
	// create and connect socket to remoteAddr
        switch (dgram.getSocketType()) {
        case SOCK_STREAM:
            {
                Socket* remote = 0;
                try {
                    remote = new Socket();
                    remote->connect(remoteAddr);
                    // cerr << "McSocketListener offering, mcsocket=" << hex << mcsocket << endl;
                    mcsocket->offer(remote,0);
                }
                catch (const IOException& ioe) {
                    Logger::getInstance()->log(LOG_ERR,
                        "Error connecting socket to %s: %s",
                        remoteAddr.toString().c_str(),ioe.what());
                    Logger::getInstance()->log(LOG_ERR,"getErrno=%d",ioe.getErrno());
                    remote->close();
                    delete remote;
                    mcsocket->offer(0,ioe.getErrno());
                }
            }
            break;
#ifdef NOT_SUPPORTED_YET
        case SOCK_DGRAM:
            {
                DatagramSocket* remote = 0;
                try {
                    remote = new DatagramSocket();
                    remote->connect(remoteAddr);
                    // cerr << "McSocketListener offering, mcsocket=" << hex << mcsocket << endl;
                    mcsocket->offer(remote,0);
                }
                catch (const IOException& ioe) {
                    Logger::getInstance()->log(LOG_ERR,
                        "Error connecting socket to %s: %s",
                        remoteAddr.toString().c_str(),ioe.what());
                    Logger::getInstance()->log(LOG_ERR,"getErrno=%d",ioe.getErrno());
                    remote->close();
                    delete remote;
                    mcsocket->offer((DatagramSocket*)0,ioe.getErrno());
                }
            }
            break;
#endif
        default:
            Logger::getInstance()->log(LOG_ERR,"unknown data socket type");
            break;
        }
    }
#ifdef DEBUG
    cerr << "McSocketListener::run returning" << endl;
#endif
    return 0;
}
// #undef DEBUG

McSocketMulticaster::McSocketMulticaster(McSocket* mcsock) :
        Thread("McSocketMulticaster"),
	_mcsocket(mcsock)
{
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
    blockSignal(SIGHUP);
}

McSocketMulticaster::~McSocketMulticaster()
{
    _serverSocket.close();
}

void McSocketMulticaster::interrupt()
{
    _mcsocketMutex.lock();
    _mcsocket = 0;
    _serverSocket.close();
    Thread::interrupt();
    _mcsocketMutex.unlock();
}

// #define DEBUG
int McSocketMulticaster::run() throw(Exception)
{
    int sockfd = _serverSocket.getFd();
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
    struct timeval waitPeriod,tmpto;
    waitPeriod.tv_sec = 0;
    waitPeriod.tv_usec = 500000;             // 1/2 a second

    Inet4SocketAddress mcsockaddr =
    	_mcsocket->getInet4McastSocketAddress();
    Inet4Address mcaddr = mcsockaddr.getInet4Address();

    McSocketDatagram dgram;
    dgram.setMagic(dgram.magicVal);
    dgram.setSocketAddress(mcsockaddr);
    dgram.setRequestNumber(_mcsocket->getRequestNumber());
    dgram.setRequesterListenPort(_serverSocket.getLocalPort());

    auto_ptr<DatagramSocket> writesock;
    if (mcaddr.isMultiCastAddress()) {
	MulticastSocket* msock = new MulticastSocket();
	writesock.reset(msock);
#ifdef SET_INTERFACE
	msock->setInterface(mcaddr,_mcsocket->getInterface());
	list<Inet4NetworkInterface> addrs = msock->getInterfaces();
	int i = 0;
	for (list<Inet4NetworkInterface>::const_iterator ii = addrs.begin(); ii != addrs.end(); ++ii) {
	    if (i++ != 2) {
                Inet4NetworkInterface iface = *ii;
		cerr << "msock setting interface: " << iface.getAddress() << ' ' <<
                    ii->getHostAddress() << endl;
		msock->setInterface(mcaddr,iface);
	    }
	}
#endif
    }
    else
	writesock.reset(new DatagramSocket());

    for (int numCasts=0; ; numCasts++) {
	dgram.setNumMulticasts(numCasts);
	if (!(numCasts % 10))
	    std::cerr << "sent " << numCasts << " dgrams, length=" << dgram.getLength() <<
		", requestNum=" << dgram.getRequestNumber() <<
		", port=" << dgram.getRequesterListenPort() << std::endl;
	writesock->send(dgram);

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
	    if (_mcsocket) _mcsocket->offer((Socket*)0,errno);
            _mcsocketMutex.unlock();
	    writesock->close();
            _serverSocket.close();
	    throw IOException("McSocket","select",errno);
	}
	if (res > 0 && FD_ISSET(sockfd,&tmpset)) {
	    Socket* socket = _serverSocket.accept();
	    writesock->close();
	    // std::cerr << "McSocketMulticaster run ending" << std::endl;
	    // std::cerr << "mcsocket=" << hex << mcsocket << endl;
            _serverSocket.close();
            _mcsocketMutex.lock();
	    if (_mcsocket) _mcsocket->offer(socket,0);
            _mcsocketMutex.unlock();
	    return RUN_OK;
	}
	if (amInterrupted()) break;
    }
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocketMulticaster break");
#endif
    _mcsocketMutex.lock();
    if (_mcsocket) _mcsocket->offer((Socket*)0,errno);
    _mcsocketMutex.unlock();
    _serverSocket.close();
    writesock->close();
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocketMulticaster run method exiting");
#endif
    return RUN_OK;
}
#undef DEBUG

