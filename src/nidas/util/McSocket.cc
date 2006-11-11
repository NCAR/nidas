
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision: 1002 $

    $LastChangedBy: maclean $

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <nidas/util/McSocket.h>
#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::util;
using namespace std;

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
    // invoke subclass assignment.
    (*(DatagramPacketT<McSocketData>*)this) =
	    (DatagramPacketT<McSocketData>) rhs;
    mcdata = rhs.mcdata;    // copy the data
    data = &mcdata; // point to new mcdata
    return *this;
}

McSocket::McSocket(): mcastAddr(),ifaceAddr(),requestNum(-1),
	newsocket(0),socketOffered(false),multicaster(0)
{
}

/*
 * Copy constructor.
 */
McSocket::McSocket(const McSocket& x) :
    mcastAddr(x.mcastAddr),ifaceAddr(x.ifaceAddr),
    requestNum(x.requestNum),newsocket(0),socketOffered(false),multicaster(0)
{
}

list<Inet4Address> McSocket::getInterfaceAddresses() const throw(IOException)
{
    MulticastSocket tmpsock(mcastAddr.getPort());
    list<Inet4Address> addrs = tmpsock.getInterfaceAddresses();
    tmpsock.close();
    return addrs;
}


void McSocket::listen() throw(IOException)
{
    if (getRequestNumber() < 0)
        throw IOException(mcastAddr.toString(),"listen",
		"request number has not been set");
    newsocket = 0;
    socketOffered = false;
    try {
	McSocketListener::accept(this);
    }
    catch (const Exception& e) {
	throw IOException(mcastAddr.toString(),"accept",e.what());
    }
}

/*
 * Does a listen and then waits for the connection.
 */
Socket* McSocket::accept() throw(IOException)
{
    listen();
    connectCond.lock();
    while(!socketOffered) connectCond.wait();
    Socket* socket = newsocket;
    newsocket = 0;
    connectCond.unlock();
    // sleep(1);	// wait for offer method to finish
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"accept offerErrno=%d",offerErrno);
#endif
    if (!socket) throw IOException("McSocket","accept",offerErrno);
    return socket;
}

void McSocket::request() throw(IOException)
{
    if (getRequestNumber() < 0)
        throw IOException(mcastAddr.toString(),"listen",
		"request number has not been set");
    newsocket = 0;
    socketOffered = false;
    // if (!mcastAddr.getInet4Address().isMultiCastAddress())
    // 	throw IOException(mcastAddr.toString(),"accept","is not a multicast address");

    multicaster_mutex.lock();
    if (!multicaster) {
        multicaster = new McSocketMulticaster(this);
	try {
	    multicaster->start();
	}
	catch(const Exception& e) {
	    throw IOException("McSocket","request",e.what());
	}
    }
    multicaster_mutex.unlock();
}

/*
 * Does a request() and then waits for the connection.
 */
Socket* McSocket::connect() throw(IOException)
{
    request();
    connectCond.lock();
    while(!socketOffered) connectCond.wait();
    Socket* socket = newsocket;
    newsocket = 0;
    connectCond.unlock();
    // sleep(1);
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"connect offerErrno=%d",offerErrno);
#endif
    if (!socket) throw IOException("McSocket","connect",offerErrno);
    return socket;
}

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

    multicaster_mutex.lock();
    Thread* thrd = multicaster;
    multicaster = 0;
    multicaster_mutex.unlock();

    if (thrd) {
#ifdef DEBUG
	Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::offer creating/starting joiner");
#endif
	ThreadJoiner* joiner = new ThreadJoiner(thrd);
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

    if (socket) connected(socket);

    connectCond.lock();
    socketOffered = true;
    offerErrno = err;
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"setting offerErrno=%d",offerErrno);
#endif
    newsocket = socket;
    connectCond.signal();
    connectCond.unlock();

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

// #undef DEBUG

// #define DEBUG

void McSocket::close() throw(IOException)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::close");
#endif

    multicaster_mutex.lock();
    Thread* thrd = multicaster;
    multicaster = 0;
    multicaster_mutex.unlock();

    if (thrd) {
        auto_ptr<Thread> athrd(thrd);
#ifdef DEBUG
	Logger::getInstance()->log(LOG_DEBUG,"McSocket::close creating joiner");
#endif
	try {
	    if (athrd->isRunning()) {
#ifdef DEBUG
		Logger::getInstance()->log(LOG_DEBUG,"close, cancelling mcaster");
#endif
		athrd->cancel();
	    }
#ifdef DEBUG
	    Logger::getInstance()->log(LOG_DEBUG,"McSocket::close joining mcaster");
#endif
	    // McSocket::close is not called from the McSocketMulticaster
	    // thread, so we can join the McSocketMulticaster thread here.
	    athrd->join();
#ifdef DEBUG
	    Logger::getInstance()->log(LOG_DEBUG,"McSocket::close joined mcaster");
#endif
	    // auto_ptr deletes it
	}
	catch(const Exception& e) {
	    Logger::getInstance()->log(LOG_ERR,"%s",e.what());
	}
    }

#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McsocketListener::close");
#endif
    try {
	McSocketListener::close(this);
    }
    catch (const Exception& e) {
#ifdef DEBUG
        Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::close exception: %s",
    	    e.what());
#endif
	throw IOException(mcastAddr.toString(),"close",e.what());
    }
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"Mcsocket::close done, this=0x%x",
    	this);
#endif
}

// #undef DEBUG

/* static */
Mutex McSocketListener::listener_mutex;

/* static */
map<Inet4SocketAddress,McSocketListener*> McSocketListener::listener_map;

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
		+ mcastaddr.toString()), mcastAddr(mcastaddr)
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
    mcsocket_mutex.lock();
    mcsockets[mcsocket->getRequestNumber()] = mcsocket;
    mcsocket_mutex.unlock();
}

int McSocketListener::remove(McSocket* mcsocket)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,
    	"McSocketListener remove requestNum=%d",
	    mcsocket->getRequestNumber());
#endif
    Synchronized autolock(mcsocket_mutex);

    map<int,McSocket*>::iterator mapi =
    	mcsockets.find(mcsocket->getRequestNumber());

    if (mapi != mcsockets.end() && mapi->second == mcsocket)
	mcsockets.erase(mapi);

    int nsock = mcsockets.size();
    return nsock;
}

// #define DEBUG
int McSocketListener::run() throw(Exception)
{
    auto_ptr<DatagramSocket> readsock;

    if (mcastAddr.getInet4Address().isMultiCastAddress()) {
	// can't bind to a specific address, must bind to INADDR_ANY.
	MulticastSocket* msock = new MulticastSocket(mcastAddr.getPort());
	readsock.reset(msock);
	msock->joinGroup(mcastAddr.getInet4Address());
    }
    else
	readsock.reset(new DatagramSocket(mcastAddr.getPort()));
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

	mcsocket_mutex.lock();
	map<int,McSocket*>::iterator mapi =
	    mcsockets.find(dgram.getRequestNumber());
	if (mapi != mcsockets.end()) mcsocket = mapi->second;
	mcsocket_mutex.unlock();

	if (!mcsocket) {
	    Logger::getInstance()->log(LOG_WARNING,"No McSocket for pseudoport:%d from host %s\n",
	    	dgram.getRequestNumber(),
		dgram.getSocketAddress().toString().c_str());
	    continue;
	}
	// create and connect socket to remoteAddr
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

#ifdef DEBUG
    cerr << "McSocketListener::run returning" << endl;
#endif
    return 0;
}
// #undef DEBUG

McSocketMulticaster::McSocketMulticaster(McSocket* mcsock) :
        Thread("McSocketMulticaster"),
	mcsocket(mcsock)
{
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
    blockSignal(SIGHUP);
}

McSocketMulticaster::~McSocketMulticaster()
{
}

int McSocketMulticaster::run() throw(Exception)
{

    ServerSocket serverSocket;
    int sockfd = serverSocket.getFd();
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sockfd, &fdset);
    struct timeval waitPeriod,tmpto;
    waitPeriod.tv_sec = 0;
    waitPeriod.tv_usec = 500000;             // 1/2 a second

    Inet4SocketAddress mcsockaddr =
    	mcsocket->getInet4McastSocketAddress();
    Inet4Address mcaddr = mcsockaddr.getInet4Address();

    McSocketDatagram dgram;
    dgram.setMagic(dgram.magicVal);
    dgram.setSocketAddress(mcsockaddr);
    dgram.setRequestNumber(mcsocket->getRequestNumber());
    dgram.setRequesterListenPort(serverSocket.getLocalPort());

    auto_ptr<DatagramSocket> writesock;
    if (mcaddr.isMultiCastAddress()) {
	MulticastSocket* msock = new MulticastSocket();
	writesock.reset(msock);
	msock->setInterface(mcaddr,mcsocket->getInterface());
#ifdef DEBUG
	list<Inet4Address> addrs = msock->getInterfaceAddresses();
	int i = 0;
	for (list<Inet4Address>::const_iterator ii = addrs.begin(); ii != addrs.end(); ++ii) {
	    if (i++ != 2) {
		cerr << "msock setting interface: " << ii->getHostAddress() << ' ' <<
		ii->getHostAddress() << endl;
		msock->setInterface(mcaddr,*ii);
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
	    mcsocket->offer(0,errno);
	    if (errno == EINTR) break;
	    serverSocket.close();
	    writesock->close();
	    throw IOException("McSocket","select",errno);
	}
	if (res > 0 && FD_ISSET(sockfd,&tmpset)) {
	    Socket* socket = serverSocket.accept();
	    serverSocket.close();
	    writesock->close();
	    // std::cerr << "McSocketMulticaster run ending" << std::endl;
	    // std::cerr << "mcsocket=" << hex << mcsocket << endl;
	    mcsocket->offer(socket,0);
	    return RUN_OK;
	}
	if (amInterrupted()) break;
    }
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocketMulticaster break");
#endif
    serverSocket.close();
    writesock->close();
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocketMulticaster run method exiting");
#endif
    return RUN_OK;
}

