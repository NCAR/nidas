/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/Socket.h>
#include <nidas/core/McSocket.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

Socket::Socket():
        _remotePort(0),
	_socket(0),connectionRequester(0),connectionThread(0),
        firstRead(true),newFile(true),keepAliveIdleSecs(7200),
        minWriteInterval(USECS_PER_SEC/2000),lastWrite(0),
        nonBlocking(false)
{
    setName("Socket (unconnected)");
}

/*
 * Copy constructor.  Should only be called before connection.
 */
Socket::Socket(const Socket& x):
	_remoteSockAddr(x._remoteSockAddr.get() ? x._remoteSockAddr->clone(): 0),
        _remoteHost(x._remoteHost),_remotePort(x._remotePort),
        _unixPath(x._unixPath),
	_socket(0),name(x.name),
        connectionRequester(x.connectionRequester),
        connectionThread(0),
        firstRead(true),newFile(true),
        keepAliveIdleSecs(x.keepAliveIdleSecs),
        minWriteInterval(x.minWriteInterval),lastWrite(0),
        nonBlocking(x.nonBlocking)
{
    assert(x._socket == 0);
}

/*
 * Constructor with a connected n_u::Socket.
 */
Socket::Socket(n_u::Socket* sock):
	_remoteSockAddr(sock->getRemoteSocketAddress().clone()),
	_socket(sock),connectionRequester(0),connectionThread(0),
        firstRead(true),newFile(true),keepAliveIdleSecs(7200),
        minWriteInterval(USECS_PER_SEC/2000),lastWrite(0)
{
    setName(_remoteSockAddr->toString());
    const n_u::Inet4SocketAddress* i4saddr =
        dynamic_cast<const n_u::Inet4SocketAddress*>(_remoteSockAddr.get());
    if (_remoteHost.length() == 0 && i4saddr) {
        // getHostAddress returns the address as a string in dot notation,
        // w.x.y.z, and does not do a reverse DNS lookup.
        setRemoteHostPort(i4saddr->getInet4Address().getHostAddress(),
            _remoteSockAddr->getPort());
    }
    else if (_unixPath.length() == 0) {
        const n_u::UnixSocketAddress* usaddr =
            dynamic_cast<const n_u::UnixSocketAddress*>(_remoteSockAddr.get());
        if (usaddr) setRemoteUnixPath(usaddr->toString());
    }

    try {
	keepAliveIdleSecs = _socket->getKeepAliveIdleSecs();
        nonBlocking = _socket->isNonBlocking();
    }
    catch (const n_u::IOException& e) {
    }
}

Socket::~Socket()
{
    connectionMutex.lock();
    if (connectionThread) {
        connectionThread->interrupt();
        connectionThread->kill(SIGUSR1);
    }
    connectionMutex.unlock();
    close();
    delete _socket;
}

Socket* Socket::clone() const 
{
    return new Socket(*this);
}

void Socket::connectionThreadFinished()
{
    connectionMutex.lock();
    connectionThread = 0;
    connectionMutex.unlock();
}

size_t Socket::getBufferSize() const throw()
{
    size_t blen = 16384;
    try {
	if (_socket) blen = _socket->getReceiveBufferSize();
    }
    catch (const n_u::IOException& e) {}

    // linux sockets (x86 laptop, FC5) return a receive buffer
    // sizeof 87632.  We don't need that much.
    // 
    if (blen > 16384) blen = 16384;
    return blen;
}

void Socket::setRemoteSocketAddress(const n_u::SocketAddress& val)
{
    _remoteSockAddr.reset(val.clone());
}

void Socket::setRemoteHostPort(const string& hostname,
    unsigned short port)
{
    _remoteHost = hostname;
    _remotePort = port;
}

void Socket::setRemoteUnixPath(const string& unixpath)
{
    _unixPath = unixpath;
}

const n_u::SocketAddress& Socket::getRemoteSocketAddress()
    throw(n_u::UnknownHostException)
{
    if (!_remoteSockAddr.get()) {
        if (_remoteHost.length() > 0) {
            // throws UnknownHostException
            n_u::Inet4Address haddr =
                n_u::Inet4Address::getByName(_remoteHost);
            n_u::Inet4SocketAddress saddr(haddr,_remotePort);
            setRemoteSocketAddress(saddr);
        }
        else {
            n_u::UnixSocketAddress saddr(getRemoteUnixPath());
            setRemoteSocketAddress(saddr);
        }
    }
    return *_remoteSockAddr.get();
}

n_u::Inet4Address Socket::getRemoteInet4Address()
{
    try {
        const n_u::SocketAddress& saddr = getRemoteSocketAddress();
        const n_u::Inet4SocketAddress* i4saddr =
            dynamic_cast<const n_u::Inet4SocketAddress*>(&saddr);
        if (i4saddr) return i4saddr->getInet4Address();
    }
    catch(const n_u::UnknownHostException& e) {
	n_u::Logger::getInstance()->log(LOG_WARNING,"%s",e.what());
    }
    return n_u::Inet4Address();
}

IOChannel* Socket::connect() throw(n_u::IOException)
{
    // getRemoteSocketAddress may throw UnknownHostException
    const n_u::SocketAddress& saddr = getRemoteSocketAddress();

    n_u::Socket* waitsock = new n_u::Socket(saddr.getFamily());
    waitsock->connect(saddr);
    waitsock->setKeepAliveIdleSecs(keepAliveIdleSecs);
    waitsock->setNonBlocking(nonBlocking);
    return new nidas::core::Socket(waitsock);
}

void Socket::requestConnection(ConnectionRequester* requester)
	throw(n_u::IOException)
{
    connectionRequester = requester;
    connectionThread = new ClientSocketConnectionThread(*this);
    try {
	connectionThread->start();
    }
    catch(const n_u::Exception& e) {
        throw n_u::IOException(getName(),"requestConnection",e.what());
    }
}

/*
 * Do the actual hardware read.
 */
size_t Socket::read(void* buf, size_t len) throw (n_u::IOException)
{
    if (firstRead) firstRead = false;
    else newFile = false;
    return _socket->recv(buf,len);
}

ServerSocket::ServerSocket():
	localSockAddr(new n_u::Inet4SocketAddress(0)),
        servSock(0),connectionRequester(0),
        connectionThread(0),keepAliveIdleSecs(7200),
        minWriteInterval(USECS_PER_SEC/2000),
        nonBlocking(false)
{
    setName("ServerSocket " + localSockAddr->toString());
}

ServerSocket::ServerSocket(const n_u::SocketAddress& addr):
	localSockAddr(addr.clone()),
        servSock(0),connectionRequester(0),
        connectionThread(0),keepAliveIdleSecs(7200),
        minWriteInterval(USECS_PER_SEC/2000),
        nonBlocking(false)
{
    setName("ServerSocket " + localSockAddr->toString());
}

ServerSocket::ServerSocket(const ServerSocket& x):
	localSockAddr(x.localSockAddr->clone()),name(x.name),
	servSock(0),connectionRequester(0),connectionThread(0),
	keepAliveIdleSecs(x.keepAliveIdleSecs),
        minWriteInterval(x.minWriteInterval),
        nonBlocking(x.nonBlocking)
{
}

ServerSocket::~ServerSocket()
{
    if (connectionThread) {
	try {
	    if (connectionThread->isRunning()) connectionThread->kill(SIGUSR1);
#ifdef DEBUG
            cerr << "~ServerSocket joining connectionThread" << endl;
#endif
	    connectionThread->join();
#ifdef DEBUG
            cerr << "~ServerSocket joined connectionThread" << endl;
#endif
	}
	catch(const n_u::Exception& e) {
	}
	delete connectionThread;
    }
    close();
    delete servSock;
}

ServerSocket* ServerSocket::clone() const 
{
    return new ServerSocket(*this);
}

void ServerSocket::close() throw (nidas::util::IOException)
{
    if (servSock) servSock->close();
}


IOChannel* ServerSocket::connect() throw(n_u::IOException)
{
    if (!servSock) servSock= new n_u::ServerSocket(*localSockAddr.get());
    n_u::Socket* newsock = servSock->accept();

    newsock->setKeepAliveIdleSecs(keepAliveIdleSecs);

    nidas::core::Socket* newCSocket = new nidas::core::Socket(newsock);
    newCSocket->setMinWriteInterval(getMinWriteInterval());
    newCSocket->setNonBlocking(nonBlocking);

    return newCSocket;
}

void ServerSocket::requestConnection(ConnectionRequester* requester)
	throw(n_u::IOException)
{
    connectionRequester = requester;
    if (!servSock) servSock= new n_u::ServerSocket(*localSockAddr.get());
    if (!connectionThread) connectionThread = new ServerSocketConnectionThread(*this);
    try {
	if (!connectionThread->isRunning()) connectionThread->start();
    }
    catch(const n_u::Exception& e) {
        throw n_u::IOException(getName(),"requestConnection",e.what());
    }
}

int ServerSocketConnectionThread::run() throw(n_u::IOException)
{
    for (;;) {
	// create nidas::core::Socket from n_u::Socket
	n_u::Socket* lowsock = _socket.servSock->accept();
	lowsock->setKeepAliveIdleSecs(_socket.getKeepAliveIdleSecs());
        lowsock->setNonBlocking(_socket.isNonBlocking());

	nidas::core::Socket* newsock = new nidas::core::Socket(lowsock);
        newsock->setMinWriteInterval(_socket.getMinWriteInterval());

	n_u::Logger::getInstance()->log(LOG_DEBUG,
		"Accepted connection: remote=%s",
		newsock->getRemoteInet4Address().getHostAddress().c_str());
	_socket.connectionRequester->connected(newsock);
    }
    return RUN_OK;
}

int ClientSocketConnectionThread::run() throw(n_u::IOException)
{
    n_u::Socket* lowsock = 0;

    for (; !isInterrupted(); ) {

        try {
            // getRemoteSocketAddress may throw UnknownHostException
            const n_u::SocketAddress& saddr = _socket.getRemoteSocketAddress();
            if (!lowsock) lowsock = new n_u::Socket(saddr.getFamily());
            lowsock->connect(saddr);
            lowsock->setKeepAliveIdleSecs(_socket.getKeepAliveIdleSecs());
            lowsock->setNonBlocking(_socket.isNonBlocking());

            // cerr << "Socket::connected " << getName();
            nidas::core::Socket* newsock = new nidas::core::Socket(lowsock);
            lowsock = 0;
            newsock->setMinWriteInterval(_socket.getMinWriteInterval());
            n_u::Logger::getInstance()->log(LOG_DEBUG,
                    "connected to %s",
                    newsock->getRemoteInet4Address().getHostAddress().c_str());
            _socket.connectionRequester->connected(newsock);
            break;
        }
        // Wait for dynamic dns to get new address
        catch(const n_u::UnknownHostException& e) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                    "%s: %s",getName().c_str(),e.what());
            if (!isInterrupted()) sleep(30);
        }
        catch(const n_u::IOException& e) {
            n_u::Logger::getInstance()->log(LOG_ERR,
                    "%s: %s",getName().c_str(),e.what());
#ifdef BREAK_OUT
            // keep trying in these case
            if (e.getErrno() != ECONNREFUSED &&
                e.getErrno() != ENETUNREACH &&
                e.getErrno() != ETIMEDOUT) break;
#endif
            if (!isInterrupted()) sleep(10);
        }
    }
    if (lowsock) {
        lowsock->close();
        delete lowsock;
    }
    _socket.connectionThreadFinished();
    n_u::ThreadJoiner* joiner = new n_u::ThreadJoiner(this);
    joiner->start();
    return RUN_OK;
}

/* static */
IOChannel* Socket::createSocket(const DOMElement* node)
            throw(n_u::InvalidParameterException)
{
    IOChannel* channel = 0;
    XDOMElement xnode(node);
    const string& type = xnode.getAttributeValue("type");

    if (type == "mcaccept" || type == "mcrequest" ||
	type == "dgaccept" || type == "dgrequest")
    	channel = new McSocket();
    else if (type == "server")
    	channel = new ServerSocket();
    else if (type == "client" || type.length() == 0)
    	channel = new Socket();
    else throw n_u::InvalidParameterException(
	    "Socket::createSocket","unknown socket type",type);
    return channel;
}

void Socket::fromDOMElement(const DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    int port = 0;
    string unixPath;
    string remoteHost;

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (aname == "address") remoteHost = aval;
	    else if (aname == "path") unixPath = aval; // Unix socket address
	    else if (aname == "port") {
		istringstream ist(aval);
		ist >> port;
		if (ist.fail())
			throw n_u::InvalidParameterException(
			    "socket","invalid port number",aval);
	    }
	    else if (aname == "type") {
		if (aval != "client")
			throw n_u::InvalidParameterException(
			    "socket","invalid socket type",aval);
	    }
	    else if (aname == "block") {
		std::istringstream ist(aval);
		ist >> boolalpha;
		bool val;
		ist >> val;
		if (ist.fail())
			throw n_u::InvalidParameterException(
			    "socket","block",aval);
		setNonBlocking(!val);
	    }
	    else if (aname == "maxIdle") {
		istringstream ist(aval);
		int secs;
		ist >> secs;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),"maxIdle",aval);
		try {
		    setKeepAliveIdleSecs(secs);
		}
		catch (const n_u::IOException& e) {		// won't happen
		}
	    }
	    else if (aname == "minWrite") {
		istringstream ist(aval);
		int usecs;
		ist >> usecs;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),"minWrite",aval);
                setMinWriteInterval(usecs);
	    }
	    else throw n_u::InvalidParameterException(
	    	string("unrecognized socket attribute: ") + aname);
	}
    }
    if (remoteHost.length() > 0 && unixPath.length() > 0)
        throw n_u::InvalidParameterException("socket","address",
            "cannot specify both an IP socket address and a unix socket path");
    if (remoteHost.length() > 0 && port <= 0)
        throw n_u::InvalidParameterException("socket","port",
            "unknown port number");

    if (unixPath.length() > 0) setRemoteUnixPath(unixPath);
    else {
        setRemoteHostPort(remoteHost,port);
        // Warn, but don't throw exception if address
        // for host cannot be found.
        try {
            n_u::Inet4Address haddr =
                    n_u::Inet4Address::getByName(remoteHost);
        }
        catch(const n_u::UnknownHostException& e) {
            WLOG(("") << getName() << ": " << e.what());
        }
    }
}

void ServerSocket::fromDOMElement(const DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    int port = -1;
    string path;

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (aname == "port") {
		istringstream ist(aval);
		ist >> port;
		if (ist.fail())
			throw n_u::InvalidParameterException(
			    "socket","invalid port number",aval);
	    }
            // Unix socket address
	    else if (aname == "path") {
                path = aval;
	    }
	    else if (aname == "type") {
		if (aval != "server")
			throw n_u::InvalidParameterException(
			    "socket","invalid socket type",aval);
	    }
	    else if (aname == "block") {
		std::istringstream ist(aval);
		ist >> boolalpha;
		bool val;
		ist >> val;
		if (ist.fail())
			throw n_u::InvalidParameterException(
			    "socket","block",aval);
		setNonBlocking(!val);
	    }
	    else if (aname == "maxIdle") {
		istringstream ist(aval);
		int secs;
		ist >> secs;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),"maxIdle",aval);
		try {
		    setKeepAliveIdleSecs(secs);
		}
		catch (const n_u::IOException& e) {		// won't happen
		}
	    }
	    else if (aname == "minWrite") {
		istringstream ist(aval);
		int usecs;
		ist >> usecs;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),"minWrite",aval);
                setMinWriteInterval(usecs);
	    }
	    else throw n_u::InvalidParameterException(
	    	string("unrecognized socket attribute: ") + aname);
	}
    }
    if (port >= 0 && path.length() > 0)
        throw n_u::InvalidParameterException("socket","address",
            "cannot specify both an IP socket port and a unix socket path");

    if (path.length() > 0) 
        localSockAddr.reset(new n_u::UnixSocketAddress(path));
    else
        localSockAddr.reset(new n_u::Inet4SocketAddress(port));
}

DOMElement* ServerSocket::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* ServerSocket::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


