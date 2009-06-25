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
#include <nidas/core/McSocketUDP.h>
#include <nidas/core/MultipleUDPSockets.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

Socket::Socket():
        _remotePort(0),
	_nusocket(0),_iochanRequester(0),_connectionThread(0),
        _firstRead(true),_newInput(true),_keepAliveIdleSecs(7200),
        _minWriteInterval(USECS_PER_SEC/2000),_lastWrite(0),
        _nonBlocking(false)
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
	_nusocket(0),_name(x._name),
        _iochanRequester(x._iochanRequester),
        _connectionThread(0),
        _firstRead(true),_newInput(true),
        _keepAliveIdleSecs(x._keepAliveIdleSecs),
        _minWriteInterval(x._minWriteInterval),_lastWrite(0),
        _nonBlocking(x._nonBlocking)
{
    assert(x._nusocket == 0);
}

/*
 * Constructor with a connected n_u::Socket.
 */
Socket::Socket(n_u::Socket* sock):
	_remoteSockAddr(sock->getRemoteSocketAddress().clone()),
	_nusocket(sock),_iochanRequester(0),_connectionThread(0),
        _firstRead(true),_newInput(true),_keepAliveIdleSecs(7200),
        _minWriteInterval(USECS_PER_SEC/2000),_lastWrite(0)
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
	_keepAliveIdleSecs = _nusocket->getKeepAliveIdleSecs();
        _nonBlocking = _nusocket->isNonBlocking();
    }
    catch (const n_u::IOException& e) {
    }
}

Socket::~Socket()
{
    if (_connectionThread) _connectionThread->interrupt();
    close();
    if (_connectionThread) {
        try {
	    if (_connectionThread->isRunning()) _connectionThread->kill(SIGUSR1);
            _connectionThread->join();
        }
        catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_WARNING,"%s",e.what());
        }
        delete _connectionThread;
    }
    delete _nusocket;
}

Socket* Socket::clone() const 
{
    return new Socket(*this);
}

size_t Socket::getBufferSize() const throw()
{
    size_t blen = 16384;
    try {
	if (_nusocket) blen = _nusocket->getReceiveBufferSize();
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
        setName(_remoteSockAddr->toString());
    }
    return *_remoteSockAddr.get();
}

n_u::Inet4Address Socket::getRemoteInet4Address()
{
    try {
        const n_u::SocketAddress& saddr = getRemoteSocketAddress();
        if (saddr.getFamily() == AF_INET) {
            n_u::Inet4SocketAddress i4saddr =
                n_u::Inet4SocketAddress((const struct sockaddr_in*)
                    saddr.getConstSockAddrPtr());
            return i4saddr.getInet4Address();
        }
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
    waitsock->setKeepAliveIdleSecs(_keepAliveIdleSecs);
    waitsock->setNonBlocking(_nonBlocking);
    return new nidas::core::Socket(waitsock);
}

void Socket::requestConnection(IOChannelRequester* requester)
	throw(n_u::IOException)
{
    _iochanRequester = requester;
    if (!_connectionThread) _connectionThread = new ConnectionThread(this);
    try {
	if (!_connectionThread->isRunning()) _connectionThread->start();
    }
    catch(const n_u::Exception& e) {
        throw n_u::IOException(getName(),"requestConnection",e.what());
    }
}

ServerSocket::ServerSocket():
	_localSockAddr(new n_u::Inet4SocketAddress(0)),
        _servSock(0),_iochanRequester(0),
        _connectionThread(0),_keepAliveIdleSecs(7200),
        _minWriteInterval(USECS_PER_SEC/2000),
        _nonBlocking(false)
{
    setName("ServerSocket " + _localSockAddr->toString());
}

ServerSocket::ServerSocket(const n_u::SocketAddress& addr):
	_localSockAddr(addr.clone()),
        _servSock(0),_iochanRequester(0),
        _connectionThread(0),_keepAliveIdleSecs(7200),
        _minWriteInterval(USECS_PER_SEC/2000),
        _nonBlocking(false)
{
    setName("ServerSocket " + _localSockAddr->toString());
}

ServerSocket::ServerSocket(const ServerSocket& x):
	_localSockAddr(x._localSockAddr->clone()),_name(x._name),
	_servSock(0),_iochanRequester(0),_connectionThread(0),
	_keepAliveIdleSecs(x._keepAliveIdleSecs),
        _minWriteInterval(x._minWriteInterval),
        _nonBlocking(x._nonBlocking)
{
}

ServerSocket::~ServerSocket()
{
    if (_connectionThread) _connectionThread->interrupt();
    close();
    if (_connectionThread) {
	try {
	    if (_connectionThread->isRunning()) _connectionThread->kill(SIGUSR1);
#ifdef DEBUG
            cerr << "~ServerSocket joining connectionThread" << endl;
#endif
	    _connectionThread->join();
#ifdef DEBUG
            cerr << "~ServerSocket joined connectionThread" << endl;
#endif
	}
	catch(const n_u::Exception& e) {
            n_u::Logger::getInstance()->log(LOG_WARNING,"%s",e.what());
	}
	delete _connectionThread;
    }
    delete _servSock;
}

ServerSocket* ServerSocket::clone() const 
{
    return new ServerSocket(*this);
}

void ServerSocket::close() throw (nidas::util::IOException)
{
    if (_servSock) _servSock->close();
}


IOChannel* ServerSocket::connect() throw(n_u::IOException)
{
    if (!_servSock) _servSock= new n_u::ServerSocket(*_localSockAddr.get());
    n_u::Socket* newsock = _servSock->accept();

    newsock->setKeepAliveIdleSecs(_keepAliveIdleSecs);

    nidas::core::Socket* newCSocket = new nidas::core::Socket(newsock);
    newCSocket->setMinWriteInterval(getMinWriteInterval());
    newCSocket->setNonBlocking(_nonBlocking);

    return newCSocket;
}

void ServerSocket::requestConnection(IOChannelRequester* requester)
	throw(n_u::IOException)
{
    _iochanRequester = requester;
    if (!_servSock) _servSock= new n_u::ServerSocket(*_localSockAddr.get());
    if (!_connectionThread) _connectionThread = new ConnectionThread(this);
    try {
	if (!_connectionThread->isRunning()) _connectionThread->start();
    }
    catch(const n_u::Exception& e) {
        throw n_u::IOException(getName(),"requestConnection",e.what());
    }
}

int ServerSocket::ConnectionThread::run() throw(n_u::IOException)
{
    for (;;) {
	// create nidas::core::Socket from n_u::Socket
	n_u::Socket* lowsock = _socket->_servSock->accept();
	lowsock->setKeepAliveIdleSecs(_socket->getKeepAliveIdleSecs());
        lowsock->setNonBlocking(_socket->isNonBlocking());

	nidas::core::Socket* newsock = new nidas::core::Socket(lowsock);
        newsock->setMinWriteInterval(_socket->getMinWriteInterval());

	n_u::Logger::getInstance()->log(LOG_DEBUG,
		"Accepted connection: remote=%s",
		newsock->getRemoteInet4Address().getHostAddress().c_str());
	_socket->_iochanRequester->connected(newsock);
    }
    return RUN_OK;
}

int Socket::ConnectionThread::run() throw(n_u::IOException)
{
    for (; !isInterrupted(); ) {

        try {
            // getRemoteSocketAddress may throw UnknownHostException
            const n_u::SocketAddress& saddr = _socket->getRemoteSocketAddress();
            if (!_socket->_nusocket) _socket->_nusocket = new n_u::Socket(saddr.getFamily());
            _socket->_nusocket->connect(saddr);
            _socket->_nusocket->setKeepAliveIdleSecs(_socket->getKeepAliveIdleSecs());
            _socket->_nusocket->setNonBlocking(_socket->isNonBlocking());

            // cerr << "Socket::connected " << getName();
            n_u::Logger::getInstance()->log(LOG_DEBUG,
                    "connected to %s",
                    _socket->getRemoteInet4Address().getHostAddress().c_str());
            _socket->_iochanRequester->connected(_socket);
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
    return RUN_OK;
}

/* static */
IOChannel* Socket::createSocket(const xercesc::DOMElement* node)
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
    else if (type == "mcacceptUDP" || type == "mcrequestUDP" ||
	type == "dgacceptUDP" || type == "dgrequestUDP")
    	channel = new McSocketUDP();
    else if (type == "dataUDP")
    	channel = new MultipleUDPSockets();
    else throw n_u::InvalidParameterException(
	    "Socket::createSocket","unknown socket type",type);
    return channel;
}

void Socket::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    int port = 0;
    string unixPath;
    string remoteHost;

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
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

void ServerSocket::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    int port = -1;
    string path;

    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
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
        _localSockAddr.reset(new n_u::UnixSocketAddress(path));
    else
        _localSockAddr.reset(new n_u::Inet4SocketAddress(port));
}

xercesc::DOMElement* ServerSocket::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

xercesc::DOMElement* ServerSocket::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}


