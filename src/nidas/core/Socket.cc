// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#include "Socket.h"
#include "McSocket.h"
#include "McSocketUDP.h"
#include "DatagramSocket.h"
#include "MultipleUDPSockets.h"
#include <nidas/util/Process.h>
#include <nidas/util/Logger.h>

#include <sys/stat.h>
#include <unistd.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

Socket::Socket(): IOChannel(),
    _remoteSockAddr(),_remoteHost(),_remotePort(0),_unixPath(),
    _nusocket(0),_name(),_iochanRequester(0),_connectionThread(0),
    _firstRead(true),_newInput(true),_keepAliveIdleSecs(7200),
    _nonBlocking(false),_connectionMutex(),_requestType(UNKNOWN_REQUEST)
{
    setName("Socket (unconnected)");
}

/*
 * Copy constructor.  Should only be called before connection.
 */
Socket::Socket(const Socket& x): IOChannel(x),
    _remoteSockAddr(x._remoteSockAddr.get() ? x._remoteSockAddr->clone(): 0),
    _remoteHost(x._remoteHost),_remotePort(x._remotePort),
    _unixPath(x._unixPath),
    _nusocket(0),_name(x._name),
    _iochanRequester(x._iochanRequester),
    _connectionThread(0),
    _firstRead(true),_newInput(true),
    _keepAliveIdleSecs(x._keepAliveIdleSecs),
    _nonBlocking(x._nonBlocking),_connectionMutex(),
    _requestType(x._requestType)
{
    assert(x._nusocket == 0);
}

/*
 * Constructor with a connected n_u::Socket.
 */
Socket::Socket(n_u::Socket* sock): IOChannel(),
    _remoteSockAddr(sock->getRemoteSocketAddress().clone()),
    _remoteHost(),_remotePort(0),_unixPath(),
    _nusocket(sock),_name(_remoteSockAddr->toAddressString()),
    _iochanRequester(0),_connectionThread(0),
    _firstRead(true),_newInput(true),_keepAliveIdleSecs(7200),
    _nonBlocking(false),_connectionMutex(),_requestType(UNKNOWN_REQUEST)
{
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
        // _nusocket->setTcpNoDelay(true);
	_keepAliveIdleSecs = _nusocket->getKeepAliveIdleSecs();
        _nonBlocking = _nusocket->isNonBlocking();
    }
    catch (const n_u::IOException& e) {
    }
}

Socket::~Socket()
{
    n_u::Autolock alock(_connectionMutex);
    // interrupt closes and deletes the _nusocket, and the thread joins itself
    if (_connectionThread) _connectionThread->interrupt();
    else {
	try {
	    close();
	}
	catch (const n_u::IOException& e) {
	    WLOG(("%s",e.what()));
	}
        delete _nusocket;
    }
}

Socket* Socket::clone() const 
{
    return new Socket(*this);
}

void Socket::close()
{
    if (_nusocket) _nusocket->close();
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
        // set name using toAddressString() which does not do
        // DNS lookups.
        setName(_remoteSockAddr->toAddressString());
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

IOChannel* Socket::connect()
{
    const n_u::SocketAddress& saddr = getRemoteSocketAddress();

    if (!_nusocket)
        _nusocket = new n_u::Socket(saddr.getFamily());
    _nusocket->connect(saddr);
    _nusocket->setKeepAliveIdleSecs(_keepAliveIdleSecs);
    _nusocket->setNonBlocking(_nonBlocking);

    _newInput = true;
    _firstRead = true;

    setName(_nusocket->getRemoteSocketAddress().toAddressString());
    
    std::list<n_u::Inet4NetworkInterface> ifaces = _nusocket->getInterfaces();
    n_u::Inet4NetworkInterface iface;
    if (!ifaces.empty()) iface = ifaces.front();

    n_u::Inet4SocketAddress i4saddrRemote;
    if (saddr.getFamily() == AF_INET)
        i4saddrRemote = n_u::Inet4SocketAddress((const struct sockaddr_in*)
                    saddr.getConstSockAddrPtr());

    const n_u::SocketAddress& localSaddr = _nusocket->getLocalSocketAddress();
    n_u::Inet4Address localAddr;
    if (localSaddr.getFamily() == AF_INET)
        localAddr = n_u::Inet4SocketAddress((const struct sockaddr_in*)
                    saddr.getConstSockAddrPtr()).getInet4Address();

    ConnectionInfo info(i4saddrRemote,localAddr,iface);
    setConnectionInfo(info);
    return this;
}

void Socket::requestConnection(IOChannelRequester* requester)
{
    _iochanRequester = requester;
    n_u::Autolock alock(_connectionMutex);
    if (!_connectionThread) {
        _connectionThread = new ConnectionThread(this);
        try {
            _connectionThread->start();
        }
        catch(const n_u::Exception& e) {
            throw n_u::IOException(getName(),"requestConnection",e.what());
        }
    }
}

ServerSocket::ServerSocket():
    IOChannel(),
    _localSockAddr(new n_u::Inet4SocketAddress(0)),
    _name(),
    _servSock(0),_iochanRequester(0),
    _connectionThread(0),_keepAliveIdleSecs(7200),
    _nonBlocking(false)
{
    setName("ServerSocket " + _localSockAddr->toAddressString());
}

ServerSocket::ServerSocket(const n_u::SocketAddress& addr):
    IOChannel(),
    _localSockAddr(addr.clone()),
    _name(),
    _servSock(0),_iochanRequester(0),
    _connectionThread(0),_keepAliveIdleSecs(7200),
    _nonBlocking(false)
{
    setName("ServerSocket " + _localSockAddr->toAddressString());
}

ServerSocket::ServerSocket(const ServerSocket& x):IOChannel(x),
    _localSockAddr(x._localSockAddr->clone()),_name(x._name),
    _servSock(0),_iochanRequester(0),_connectionThread(0),
    _keepAliveIdleSecs(x._keepAliveIdleSecs),
    _nonBlocking(x._nonBlocking)
{
}

ServerSocket::~ServerSocket()
{
    try {
        close();
    }
    catch(const n_u::IOException& e) {
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s",e.what());
    }
    if (_connectionThread) {
	try {
            DLOG(("joining connectionThread"));
	    _connectionThread->join();
            DLOG(("joined connectionThread"));
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

void ServerSocket::close()
{
    // The _connectionThread is likely doing a n_u::ServerSocket::accept(),
    // which is doing a ppoll on the file descriptor and catching SIGUSR1.
    //
    // Even if the signal is sent first here by interrupt() and the file descriptor
    // is then closed, ppoll errors out on the invalid file descriptor before
    // seeing the signal.  Not really a problem, just a curiosity.

    if (_connectionThread)
        _connectionThread->interrupt();
    if (_servSock) _servSock->close();
}

IOChannel* ServerSocket::connect()
{
    if (!_servSock) {
        // delete AF_UNIX sockets if they exist
        if (_localSockAddr.get()->getFamily() == AF_UNIX) {
            string sockpath = _localSockAddr.get()->toString();
            if (sockpath.substr(0,6) == "unix:/") {
                sockpath = sockpath.substr(5);
                struct stat statbuf;
                if (::stat(sockpath.c_str(),&statbuf) == 0 &&
                    S_ISSOCK(statbuf.st_mode)) {
                    ILOG(("unlinking: ") << sockpath);
                    ::unlink(sockpath.c_str());
                }
            }
        }
        _servSock= new n_u::ServerSocket(*_localSockAddr.get());
    }
    n_u::Socket* newsock = _servSock->accept();

    newsock->setKeepAliveIdleSecs(_keepAliveIdleSecs);

    nidas::core::Socket* newCSocket = new nidas::core::Socket(newsock);
    newCSocket->setNonBlocking(_nonBlocking);

    return newCSocket;
}

void ServerSocket::requestConnection(IOChannelRequester* requester)
{
    _iochanRequester = requester;
    if (!_servSock) {
        // delete AF_UNIX sockets if they exist
        if (_localSockAddr.get()->getFamily() == AF_UNIX) {
            string sockpath = _localSockAddr.get()->toString();
            if (sockpath.substr(0,6) == "unix:/") {
                sockpath = sockpath.substr(5);
                struct stat statbuf;
                if (::stat(sockpath.c_str(),&statbuf) == 0 &&
                    S_ISSOCK(statbuf.st_mode)) {
                    ILOG(("unlinking: ") << sockpath);
                    ::unlink(sockpath.c_str());
                }
            }
        }
        _servSock= new n_u::ServerSocket(*_localSockAddr.get());
    }
    if (!_connectionThread) _connectionThread = new ConnectionThread(this);
    try {
	if (!_connectionThread->isRunning()) _connectionThread->start();
    }
    catch(const n_u::Exception& e) {
        throw n_u::IOException(getName(),"requestConnection",e.what());
    }
}

ServerSocket::ConnectionThread::ConnectionThread(ServerSocket* sock):
    Thread("ServerSocketConnectionThread"),_socket(sock)
{
    // atomically unblocked and caught by ServerSocket::accept()
    blockSignal(SIGUSR1);
}

void ServerSocket::ConnectionThread::interrupt()
{
    Thread::interrupt();
    kill(SIGUSR1);
}

int ServerSocket::ConnectionThread::run()
{
    for (;!isInterrupted();) {

        n_u::Socket* lowsock;
        try {
            lowsock = _socket->_servSock->accept();
        }
        catch(const n_u::IOException& e) {
            if (isInterrupted() || errno == EINTR) continue;   // interrupted, probably SIGUSR1
            throw e;
        }

	lowsock->setKeepAliveIdleSecs(_socket->getKeepAliveIdleSecs());
        lowsock->setNonBlocking(_socket->isNonBlocking());

	// create nidas::core::Socket from n_u::Socket
	nidas::core::Socket* newsock = new nidas::core::Socket(lowsock);

	DLOG(("Accepted connection: remote=%s",
		newsock->getRemoteSocketAddress().toAddressString().c_str()));
	_socket->_iochanRequester->connected(newsock);
    }
    return RUN_OK;
}

Socket::ConnectionThread::ConnectionThread(Socket* sock):
    Thread("SocketConnectionThread"),_socket(sock)
{
}

Socket::ConnectionThread::~ConnectionThread()
{
    nidas::util::Autolock alock(_socket->_connectionMutex);
    _socket->_connectionThread = 0;
}

void Socket::ConnectionThread::interrupt()
{
    Thread::interrupt();
    n_u::Autolock alock(_socket->_connectionMutex);
    if (_socket->_nusocket) _socket->_nusocket->close();
}

int Socket::ConnectionThread::run()
{
    for (; !isInterrupted(); ) {

        try {
            _socket->connect();

            _socket->_connectionMutex.lock();
            _socket->_connectionThread = 0;
            _socket->_connectionMutex.unlock();

            // cerr << "Socket::connected " << getName();
            DLOG(("Socket connected to %s",
                    _socket->getRemoteInet4Address().getHostAddress().c_str()));

            _socket->_iochanRequester->connected(_socket);

            n_u::ThreadJoiner* joiner = new n_u::ThreadJoiner(this);
            joiner->start();
            return RUN_OK;
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

    {
        n_u::Autolock alock(_socket->_connectionMutex);
        _socket->_connectionThread = 0;
        _socket->_nusocket->close();
        delete _socket->_nusocket;
        _socket->_nusocket = 0;
    }
    n_u::ThreadJoiner* joiner = new n_u::ThreadJoiner(this);
    joiner->start();
    return RUN_EXCEPTION;
}

/* static */
IOChannel* Socket::createSocket(const xercesc::DOMElement* node)
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
#ifdef NOT_USED_YET
    else if (type == "mcacceptUDP" || type == "mcrequestUDP" ||
	type == "dgacceptUDP" || type == "dgrequestUDP")
    	channel = new McSocketUDP();
#endif
    else if (type == "calUDPData" || type == "dataUDP")
        // new name: calUDPData, perhaps a bit more descriptive
        // allow old one for now.
    	channel = new MultipleUDPSockets();
    else if (type == "udp")
    	channel = new DatagramSocket();
    else throw n_u::InvalidParameterException(
	    "Socket::createSocket","unknown socket type",type);
    return channel;
}

void Socket::fromDOMElement(const xercesc::DOMElement* node)
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
            // Unix socket address
            else if (aname == "path") unixPath = n_u::Process::expandEnvVars(aval);
            else if (aname == "port") {
                istringstream ist(n_u::Process::expandEnvVars(aval));
                ist >> port;
                if (ist.fail())
                    throw n_u::InvalidParameterException
                        ("socket", "invalid port number", aval);
            }
            else if (aname == "type") {
                if (aval != "client")
                    throw n_u::InvalidParameterException
                        ("socket", "invalid socket type", aval);
            }
            else if (aname == "block") {
                std::istringstream ist(aval);
                ist >> boolalpha;
                bool val;
                ist >> val;
                if (ist.fail())
                    throw n_u::InvalidParameterException("socket", "block", aval);
                setNonBlocking(!val);
            }
            else if (aname == "maxIdle") {
                istringstream ist(aval);
                int secs;
                ist >> secs;
                if (ist.fail())
                    throw n_u::InvalidParameterException
                        (getName(), "maxIdle", aval);
                try {
                    setKeepAliveIdleSecs(secs);
                }
                catch (const n_u::IOException& e) {     // won't happen
                }
            }
            else throw n_u::InvalidParameterException
                     (string("unrecognized socket attribute: ") + aname);
        }
    }
    if (remoteHost.length() > 0 && unixPath.length() > 0)
        throw n_u::InvalidParameterException
            ("socket", "address",
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
            n_u::Inet4Address::getByName(remoteHost);
        }
        catch(const n_u::UnknownHostException& e) {
            WLOG(("") << getName() << ": " << e.what());
        }
    }
}

void ServerSocket::fromDOMElement(const xercesc::DOMElement* node)
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
                istringstream ist(n_u::Process::expandEnvVars(aval));
                ist >> port;
                if (ist.fail())
                    throw n_u::InvalidParameterException
                        ("socket", "invalid port number", aval);
            }
            // Unix socket address
            else if (aname == "path") {
                path = n_u::Process::expandEnvVars(aval);
            }
            else if (aname == "type") {
                if (aval != "server")
                    throw n_u::InvalidParameterException
                        ("socket", "invalid socket type", aval);
            }
            else if (aname == "block") {
                std::istringstream ist(aval);
                ist >> boolalpha;
                bool val;
                ist >> val;
                if (ist.fail())
                    throw n_u::InvalidParameterException
                        ("socket", "block", aval);
                setNonBlocking(!val);
            }
            else if (aname == "maxIdle") {
                istringstream ist(aval);
                int secs;
                ist >> secs;
                if (ist.fail())
                    throw n_u::InvalidParameterException
                        (getName(), "maxIdle", aval);
                try {
                    setKeepAliveIdleSecs(secs);
                }
                catch (const n_u::IOException& e) {     // won't happen
                }
            }
            else throw n_u::InvalidParameterException
                     (string("unrecognized socket attribute: ") + aname);
        }
    }
    if (port >= 0 && path.length() > 0)
        throw n_u::InvalidParameterException
            ("socket", "address",
             "cannot specify both an IP socket port and a unix socket path");

    if (path.length() > 0)
        _localSockAddr.reset(new n_u::UnixSocketAddress(path));
    else
        _localSockAddr.reset(new n_u::Inet4SocketAddress(port));
    setName("ServerSocket " + _localSockAddr->toAddressString());
}

xercesc::DOMElement*
ServerSocket::
toDOMParent(xercesc::DOMElement* parent, bool complete) const
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS
        ((const XMLCh*)XMLStringConverter("dsmconfig"),
         DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem, complete);
}

xercesc::DOMElement*
ServerSocket::
toDOMElement(xercesc::DOMElement* node, bool) const
{
    return node;
}


