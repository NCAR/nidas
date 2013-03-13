// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
#include <nidas/util/UTime.h>       // MSECS_PER_SEC
#include <nidas/util/IOTimeoutException.h>

#include <iostream>
#include <memory>  // auto_ptr

using namespace nidas::util;
using namespace std;


namespace 
{
    typedef map<Inet4SocketAddress,McSocketListener*> listener_map_t;
    Mutex listener_mutex;
    listener_map_t listener_map;
}


/* static */
const int McSocketDatagram::magicVal = 0x01234567;

McSocketDatagram::McSocketDatagram(int requestType) :
    DatagramPacketT<McSocketData>(&mcdata,1),mcdata()
{
    setRequestType(requestType);
}

/*
 * Copy constructor.
 */
McSocketDatagram::McSocketDatagram(const McSocketDatagram& x):
    DatagramPacketT<McSocketData>(x),mcdata(x.mcdata)
{
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

/* static */
void McSocketListener::accept(McSocket<Socket>* mcsocket)
	throw(Exception)
{
    const Inet4SocketAddress& mcAddr = mcsocket->getInet4McastSocketAddress();

#ifdef DEBUG
    cerr << "McSocketListener::accept, listener_map.size()=" <<
    	listener_map.size() << " mcAddr=" << mcAddr.toAddressString() <<
	" request=" << mcsocket->getRequestType() << " socketType=" <<
	getMcSocketType(mcsocket) << endl;
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

/* static */
void McSocketListener::accept(McSocket<DatagramSocket>* mcsocket)
	throw(Exception)
{
    const Inet4SocketAddress& mcAddr = mcsocket->getInet4McastSocketAddress();

#ifdef DEBUG
    cerr << "McSocketListener::accept, listener_map.size()=" <<
    	listener_map.size() << " mcAddr=" << mcAddr.toAddressString() <<
	" request=" << mcsocket->getRequestType() << " socketType=" <<
	getMcSocketType(mcsocket) << endl;
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
void McSocketListener::close(McSocket<Socket>* mcsocket)
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
#undef DEBUG

/* static */
void McSocketListener::close(McSocket<DatagramSocket>* mcsocket)
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

/* static */
int McSocketListener::check() throw()
{
    Synchronized sync(listener_mutex);
    return listener_map.size();
}

McSocketListener::McSocketListener(const Inet4SocketAddress&
	mcastaddr) :
	Thread(string("McSocketListener: ") + mcastaddr.toAddressString()),
        _mcastAddr(mcastaddr),_mcsocket_mutex(),_readsock(0),_tcpMcSockets(),
        _udpMcSockets()
{
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
    blockSignal(SIGHUP);
    // install signal handler for SIGUSR1
    unblockSignal(SIGUSR1);
}

McSocketListener::~McSocketListener()
{
    if (_readsock) _readsock->close();
    delete _readsock;
}
// #define DEBUG
void McSocketListener::add(McSocket<Socket>* mcsocket)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocketListener add mcsocket=%p,requestType=%d",
    	mcsocket,mcsocket->getRequestType());
#endif
    _mcsocket_mutex.lock();
    if (_tcpMcSockets[mcsocket->getRequestType()])
        Logger::getInstance()->log(LOG_DEBUG,"McSocketListener TCP requestType=%d already added",
            mcsocket->getRequestType());
    else _tcpMcSockets[mcsocket->getRequestType()] = mcsocket;
    _mcsocket_mutex.unlock();
}

void McSocketListener::add(McSocket<DatagramSocket>* mcsocket)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocketListener add requestType=%d",
    	mcsocket->getRequestType());
#endif
    _mcsocket_mutex.lock();
    if (_udpMcSockets[mcsocket->getRequestType()]) 
        Logger::getInstance()->log(LOG_DEBUG,"McSocketListener UDP requestType=%d already added",
            mcsocket->getRequestType());
    else _udpMcSockets[mcsocket->getRequestType()] = mcsocket;
    _mcsocket_mutex.unlock();
}

int McSocketListener::remove(McSocket<Socket>* mcsocket)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,
    	"McSocketListener remove mcsocket=%p, requestType=%d, size=%d",
	    mcsocket,mcsocket->getRequestType(),_tcpMcSockets.size());
#endif
    Synchronized autolock(_mcsocket_mutex);

    map<int,McSocket<Socket>*>::iterator mapi =
    	_tcpMcSockets.find(mcsocket->getRequestType());

    // When a McSocket has established a socket connection
    // a copy of the original is usually made, and then eventually
    // this McSocketListener::remove will be attempted with the
    // copy.  The copy will not be found in _tcpMcSockets,
    // here, but that is OK.
    if (mapi != _tcpMcSockets.end() && mapi->second == mcsocket)
	_tcpMcSockets.erase(mapi);
#ifdef DEBUG
    else cerr << "McSocketListener::remove mcsocket=" << mcsocket << " not found" << endl;
#endif

    int nsock = _tcpMcSockets.size();
    return nsock;
}
#undef DEBUG

int McSocketListener::remove(McSocket<DatagramSocket>* mcsocket)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,
    	"McSocketListener remove requestType=%d",
	    mcsocket->getRequestType());
#endif
    Synchronized autolock(_mcsocket_mutex);

    map<int,McSocket<DatagramSocket>*>::iterator mapi =
    	_udpMcSockets.find(mcsocket->getRequestType());

    if (mapi != _udpMcSockets.end() && mapi->second == mcsocket)
	_udpMcSockets.erase(mapi);

    int nsock = _udpMcSockets.size();
    return nsock;
}

void McSocketListener::interrupt()
{
    Thread::interrupt();
    try {
        kill(SIGUSR1);
    }
    catch(const Exception& e) {
        PLOG(("%s",e.what()));
    }
}

// #define DEBUG
int McSocketListener::run() throw(Exception)
{
    if (_mcastAddr.getInet4Address().isMultiCastAddress()) {
	// can't bind to a specific address, must bind to INADDR_ANY.
	MulticastSocket* msock = new MulticastSocket(_mcastAddr.getPort());
	_readsock = msock;
	list<Inet4NetworkInterface> interfaces = msock->getInterfaces();
	list<Inet4NetworkInterface>::const_iterator ii = interfaces.begin();
	for ( ; ii != interfaces.end(); ++ii) {
	    Inet4NetworkInterface iface = *ii;
	    int iflags = iface.getFlags();
	    // join interfaces that support MULTICAST or LOOPBACK
	    // ppp interfaces come up with MULTICAST set, but not BROADCAST
	    if (iflags & IFF_UP && iflags & IFF_BROADCAST && iflags & (IFF_MULTICAST | IFF_LOOPBACK)) {
		// cerr << "joining interface " << iface.getName() << endl;
		msock->joinGroup(_mcastAddr.getInet4Address(),iface);
	    }
	}
    }
    else
	_readsock = new DatagramSocket(_mcastAddr.getPort());


    McSocketDatagram dgram;
    Inet4PacketInfoX pktinfo;

    blockSignal(SIGUSR1);
    // get the existing signal mask
    sigset_t sigmask;
    pthread_sigmask(SIG_BLOCK,NULL,&sigmask);

    // remove SIGUSR1 from the mask passed to pselect
    sigdelset(&sigmask,SIGUSR1);

    fd_set readfds;
    FD_ZERO(&readfds);

    while (!amInterrupted()) {

        int fd = _readsock->getFd();
        FD_SET(fd,&readfds);
        int nfd = ::pselect(fd+1,&readfds,NULL,NULL,0,&sigmask);

	/* receive is a cancelation point.  */
	try {
            if (nfd < 0) throw nidas::util::IOException(
                _readsock->getLocalSocketAddress().toString(), "pselect",errno);

#ifdef DEBUG
	    cerr << "McSocketListener::receive" << endl;
#endif
	    _readsock->receive(dgram,pktinfo,0);
#ifdef DEBUG
	    cerr << "McSocketListener::received from " <<
	     	dgram.getSocketAddress().toAddressString() << 
	 	" length=" << dgram.getLength() << endl;
#endif
	}
	catch(const IOTimeoutException& e)
	{
#ifdef DEBUG
	    cerr << "McSocketListener::run: " << e.toAddressString() << endl;
#endif
	    continue;
	}
	catch(const IOException& e)
	{
#ifdef DEBUG
	    cerr << "McSocketListener::run: " << e.what() << endl;
#endif
	    if (e.getErrno() == EINTR || e.getErrno() == EBADF) break;
	    throw e;
	}

	Logger::getInstance()->log(LOG_DEBUG,
	"received dgram, magic=0x%x, requestType=%d, reply to port=%d, socketType=%d, len=%d\n",
		dgram.getMagic(),dgram.getRequestType(),
		dgram.getRequesterListenPort(),dgram.getSocketType(),
		dgram.getLength());

	if (dgram.getMagic() != dgram.magicVal) continue;
	if (dgram.getLength() != sizeof(struct McSocketData)) continue;

	Inet4SocketAddress remoteAddr;
        if (dgram.getSocketAddress().getFamily() == AF_INET) {
            remoteAddr = Inet4SocketAddress((const struct sockaddr_in*)
                dgram.getSocketAddress().getConstSockAddrPtr());
        }

        // socket address to send replies to
        remoteAddr.setPort(dgram.getRequesterListenPort());

        pktinfo.setRemoteSocketAddress(remoteAddr);

	// look for an mcsocket matching this request
        switch (dgram.getSocketType()) {
        case SOCK_STREAM:
            {
                _mcsocket_mutex.lock();
                McSocket<Socket>* mcsocket = 0;
                map<int,McSocket<Socket>*>::iterator mapi =
                    _tcpMcSockets.find(dgram.getRequestType());
                if (mapi != _tcpMcSockets.end()) mcsocket = mapi->second;
                _mcsocket_mutex.unlock();

                if (!mcsocket) {
                    Logger::getInstance()->log(LOG_WARNING,"No TCP McSocket for request type %d from host %s\n",
                        dgram.getRequestType(),
                        dgram.getSocketAddress().toAddressString().c_str());
                    continue;
                }
                // create and connect socket to remoteAddr
                Socket* remote = 0;
                try {
                    remote = new Socket();
                    remote->connect(remoteAddr);
                    mcsocket->offer(remote,pktinfo);
                }
                catch (const IOException& ioe) {
                    Logger::getInstance()->log(LOG_ERR,
                        "Error connecting socket to %s: %s",
                        remoteAddr.toAddressString().c_str(),ioe.what());
                    Logger::getInstance()->log(LOG_ERR,"getErrno=%d",ioe.getErrno());
                    if (remote) remote->close();
                    delete remote;
                    mcsocket->offer(ioe.getErrno());
                }
            }
            break;
        case SOCK_DGRAM:
            {
                _mcsocket_mutex.lock();
                McSocket<DatagramSocket>* mcsocket = 0;
                map<int,McSocket<DatagramSocket>*>::iterator mapi =
                    _udpMcSockets.find(dgram.getRequestType());
                if (mapi != _udpMcSockets.end()) mcsocket = mapi->second;
                _mcsocket_mutex.unlock();

                if (!mcsocket) {
                    Logger::getInstance()->log(LOG_WARNING,"No UDP McSocket for request type %d from host %s\n",
                        dgram.getRequestType(),
                        dgram.getSocketAddress().toAddressString().c_str());
                    continue;
                }
                DatagramSocket* remote = 0;
                try {
                    remote = new DatagramSocket();
                    remote->connect(remoteAddr);
                    mcsocket->offer(remote,pktinfo);
                }
                catch (const IOException& ioe) {
                    Logger::getInstance()->log(LOG_ERR,
                        "Error connecting socket to %s: %s",
                        remoteAddr.toAddressString().c_str(),ioe.what());
                    Logger::getInstance()->log(LOG_ERR,"getErrno=%d",ioe.getErrno());
                    if (remote) remote->close();
                    delete remote;
                    mcsocket->offer(ioe.getErrno());
                }
            }
            break;
        default:
            Logger::getInstance()->log(LOG_ERR,"unknown data socket type");
            break;
        }
    }
#ifdef DEBUG
    cerr << "McSocketListener::run returning" << endl;
#endif
    _readsock->close();
    return 0;
}
// #undef DEBUG
