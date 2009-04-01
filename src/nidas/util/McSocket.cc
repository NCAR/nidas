
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

/* static */
void McSocketListener::accept(McSocket<Socket>* mcsocket)
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

/* static */
void McSocketListener::accept(McSocket<DatagramSocket>* mcsocket)
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
	Thread(string("McSocketListener: ")
		+ mcastaddr.toString()), _mcastAddr(mcastaddr),_readsock(0)
{
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
    blockSignal(SIGHUP);
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
    Logger::getInstance()->log(LOG_DEBUG,"McSocketListener add mcsocket=%p,requestNum=%d",
    	mcsocket,mcsocket->getRequestNumber());
#endif
    _mcsocket_mutex.lock();
    _tcpMcSockets[mcsocket->getRequestNumber()] = mcsocket;
    _mcsocket_mutex.unlock();
}

void McSocketListener::add(McSocket<DatagramSocket>* mcsocket)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,"McSocketListener add requestNum=%d",
    	mcsocket->getRequestNumber());
#endif
    _mcsocket_mutex.lock();
    _udpMcSockets[mcsocket->getRequestNumber()] = mcsocket;
    _mcsocket_mutex.unlock();
}

int McSocketListener::remove(McSocket<Socket>* mcsocket)
{
#ifdef DEBUG
    Logger::getInstance()->log(LOG_DEBUG,
    	"McSocketListener remove mcsocket=%p, requestNum=%d, size=%d",
	    mcsocket,mcsocket->getRequestNumber(),_tcpMcSockets.size());
#endif
    Synchronized autolock(_mcsocket_mutex);

    map<int,McSocket<Socket>*>::iterator mapi =
    	_tcpMcSockets.find(mcsocket->getRequestNumber());

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
    	"McSocketListener remove requestNum=%d",
	    mcsocket->getRequestNumber());
#endif
    Synchronized autolock(_mcsocket_mutex);

    map<int,McSocket<DatagramSocket>*>::iterator mapi =
    	_udpMcSockets.find(mcsocket->getRequestNumber());

    if (mapi != _udpMcSockets.end() && mapi->second == mcsocket)
	_udpMcSockets.erase(mapi);

    int nsock = _udpMcSockets.size();
    return nsock;
}

void McSocketListener::interrupt()
{
    if (_readsock) _readsock->close();
    Thread::interrupt();
}

// #define DEBUG
int McSocketListener::run() throw(Exception)
{
    if (_mcastAddr.getInet4Address().isMultiCastAddress()) {
	// can't bind to a specific address, must bind to INADDR_ANY.
	MulticastSocket* msock = new MulticastSocket(_mcastAddr.getPort());
	_readsock = msock;
	msock->joinGroup(_mcastAddr.getInet4Address());
    }
    else
	_readsock = new DatagramSocket(_mcastAddr.getPort());

    _readsock->setTimeout(MSECS_PER_SEC/4);

    while (!amInterrupted()) {

	McSocketDatagram dgram;

	/* receive is a cancelation point.  */
	try {
#ifdef DEBUG
	    cerr << "McSocketListener::receive" << endl;
#endif
	    _readsock->receive(dgram);
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
	    cerr << "McSocketListener::run: " << e.what() << endl;
#ifdef DEBUG
#endif
	    if (e.getError() == EINTR || e.getError() == EBADF) break;
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
        switch (dgram.getSocketType()) {
        case SOCK_STREAM:
            {
                _mcsocket_mutex.lock();
                McSocket<Socket>* mcsocket = 0;
                map<int,McSocket<Socket>*>::iterator mapi =
                    _tcpMcSockets.find(dgram.getRequestNumber());
                if (mapi != _tcpMcSockets.end()) mcsocket = mapi->second;
                _mcsocket_mutex.unlock();

                if (!mcsocket) {
                    Logger::getInstance()->log(LOG_WARNING,"No TCP McSocket for pseudoport:%d from host %s\n",
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
                    if (remote) remote->close();
                    delete remote;
                    mcsocket->offer(0,ioe.getErrno());
                }
            }
            break;
        case SOCK_DGRAM:
            {
                _mcsocket_mutex.lock();
                McSocket<DatagramSocket>* mcsocket = 0;
                map<int,McSocket<DatagramSocket>*>::iterator mapi =
                    _udpMcSockets.find(dgram.getRequestNumber());
                if (mapi != _udpMcSockets.end()) mcsocket = mapi->second;
                _mcsocket_mutex.unlock();

                if (!mcsocket) {
                    Logger::getInstance()->log(LOG_WARNING,"No UDP McSocket for pseudoport:%d from host %s\n",
                        dgram.getRequestNumber(),
                        dgram.getSocketAddress().toString().c_str());
                    continue;
                }
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
                    if (remote) remote->close();
                    delete remote;
                    mcsocket->offer(0,ioe.getErrno());
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
    return 0;
}
// #undef DEBUG
