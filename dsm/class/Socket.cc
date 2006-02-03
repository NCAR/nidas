/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <Socket.h>
#include <McSocket.h>
#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(Socket)
CREATOR_FUNCTION(ServerSocket)

Socket::Socket():
	remoteSockAddr(
		auto_ptr<atdUtil::SocketAddress>(new atdUtil::Inet4SocketAddress())),
	socket(0),firstRead(true),newFile(true),keepAliveIdleSecs(7200)
{
    setName("Socket " + remoteSockAddr->toString());
}

/*
 * Copy constructor.  Should only be called before connection.
 */
Socket::Socket(const Socket& x):
	remoteSockAddr(
		auto_ptr<atdUtil::SocketAddress>(x.remoteSockAddr->clone())),
	socket(0),name(x.name),
	firstRead(true),newFile(true),keepAliveIdleSecs(x.keepAliveIdleSecs)
{
}

/*
 * Constructor with a connected atdUtil::Socket.
 */
Socket::Socket(atdUtil::Socket* sock):
	remoteSockAddr(
	    auto_ptr<atdUtil::SocketAddress>(sock->getRemoteSocketAddress().clone())),
	socket(sock),firstRead(true),newFile(true),
	keepAliveIdleSecs(7200)
{
    setName(remoteSockAddr->toString());
    try {
	keepAliveIdleSecs = socket->getKeepAliveIdleSecs();
    }
    catch (const atdUtil::IOException& e) {
    }
}

Socket::~Socket()
{
    close();
    delete socket;
}

Socket* Socket::clone() const 
{
    return new Socket(*this);
}

atdUtil::Inet4Address Socket::getRemoteInet4Address() const throw()
{
    if (socket) {
	const atdUtil::SocketAddress& addr = socket->getRemoteSocketAddress();
	const atdUtil::Inet4SocketAddress* i4addr =
		dynamic_cast<const atdUtil::Inet4SocketAddress*>(&addr);
	if (i4addr) return i4addr->getInet4Address();
    }
    return atdUtil::Inet4Address();
}


IOChannel* Socket::connect(int pseudoPort) throw(atdUtil::IOException)
{
    atdUtil::Socket* waitsock = new atdUtil::Socket();
    waitsock->connect(*remoteSockAddr.get());
    waitsock->setKeepAliveIdleSecs(keepAliveIdleSecs);
    return new dsm::Socket(waitsock);
}

void Socket::requestConnection(ConnectionRequester* requester,
	int pseudoPort) throw(atdUtil::IOException)
{
    atdUtil::Socket* waitsock = new atdUtil::Socket();
    waitsock->connect(*remoteSockAddr.get());
    waitsock->setKeepAliveIdleSecs(keepAliveIdleSecs);
    // cerr << "Socket::connected " << getName();
    requester->connected(new dsm::Socket(waitsock));
}

/*
 * Do the actual hardware read.
 */
size_t Socket::read(void* buf, size_t len) throw (atdUtil::IOException)
{
    if (firstRead) firstRead = false;
    else newFile = false;
    return socket->recv(buf,len);
}

ServerSocket::ServerSocket(int p):port(p),servSock(0),
	connectionRequester(0),thread(0),keepAliveIdleSecs(7200)
{
    atdUtil::Inet4SocketAddress addr(INADDR_ANY,port);
    setName("ServerSocket " + addr.toString());
}

ServerSocket::ServerSocket(const ServerSocket& x):
	port(x.port),name(x.name),
	servSock(0),connectionRequester(0),thread(0),
	keepAliveIdleSecs(x.keepAliveIdleSecs)
{
}

ServerSocket::~ServerSocket()
{
    if (thread) {
	try {
	    if (thread->isRunning()) thread->cancel();
	    thread->join();
	}
	catch(const atdUtil::Exception& e) {
	}
	delete thread;
    }
    close();
    delete servSock;
}

ServerSocket* ServerSocket::clone() const 
{
    return new ServerSocket(*this);
}

IOChannel* ServerSocket::connect(int pseudoPort) throw(atdUtil::IOException)
{
    if (!servSock) servSock= new atdUtil::ServerSocket(port);
    atdUtil::Socket* newsock = servSock->accept();
    newsock->setKeepAliveIdleSecs(keepAliveIdleSecs);
    return new dsm::Socket(newsock);
}

void ServerSocket::requestConnection(ConnectionRequester* requester,
	int pseudoPort) throw(atdUtil::IOException)
{
    connectionRequester = requester;
    if (!servSock) servSock= new atdUtil::ServerSocket(port);
    if (!thread) thread = new ServerSocketConnectionThread(*this);
    try {
	if (!thread->isRunning()) thread->start();
    }
    catch(const atdUtil::Exception& e) {
        throw atdUtil::IOException(getName(),"requestConnection",e.what());
    }
}

int ServerSocketConnectionThread::run() throw(atdUtil::IOException)
{
    for (;;) {
	// create dsm::Socket from atdUtil::Socket
	atdUtil::Socket* lowsock = socket.servSock->accept();
	lowsock->setKeepAliveIdleSecs(socket.getKeepAliveIdleSecs());

	dsm::Socket* newsock = new dsm::Socket(lowsock);

	atdUtil::Logger::getInstance()->log(LOG_DEBUG,
		"Accepted connection: remote=%s",
		newsock->getRemoteInet4Address().getHostAddress().c_str());
	socket.connectionRequester->connected(newsock);
    }
    return RUN_OK;
}

/* static */
IOChannel* Socket::createSocket(const DOMElement* node)
            throw(atdUtil::InvalidParameterException)
{
    IOChannel* channel = 0;
    XDOMElement xnode(node);
    const string& type = xnode.getAttributeValue("type");

    if (!type.compare("mcaccept") || !type.compare("mcrequest"))
    	channel = new McSocket();
    else if (!type.compare("server"))
    	channel = new ServerSocket();
    else if (!type.compare("client") || type.length() == 0)
    	channel = new Socket();
    else throw atdUtil::InvalidParameterException(
	    "Socket::createSocket","unknown socket type",type);
    return channel;
}

void Socket::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    atdUtil::Inet4Address addr;
    int port;

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
	    if (!aname.compare("address")) {
		try {
		    addr = atdUtil::Inet4Address::getByName(aval);
		}
		catch(const atdUtil::UnknownHostException& e) {
		    throw atdUtil::InvalidParameterException(
			"socket","unknown host",aval);
		}
	    }
	    else if (!aname.compare("port")) {
		istringstream ist(aval);
		ist >> port;
		if (ist.fail())
			throw atdUtil::InvalidParameterException(
			    "socket","invalid port number",aval);
	    }
	    else if (!aname.compare("type")) {
		if (aval.compare("client"))
			throw atdUtil::InvalidParameterException(
			    "socket","invalid socket type",aval);
	    }
	    else if (!aname.compare("maxIdle")) {
		istringstream ist(aval);
		int secs;
		ist >> secs;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(getName(),"maxIdle",aval);
		try {
		    setKeepAliveIdleSecs(secs);
		}
		catch (const atdUtil::IOException& e) {		// won't happen
		}
	    }
	    else throw atdUtil::InvalidParameterException(
	    	string("unrecognized socket attribute: ") + aname);
	}
    }
    remoteSockAddr =
    	auto_ptr<atdUtil::SocketAddress>(new atdUtil::Inet4SocketAddress(addr,port));
}

DOMElement* Socket::toDOMParent(
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

DOMElement* Socket::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

void ServerSocket::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
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
	    if (!aname.compare("port")) {
		istringstream ist(aval);
		ist >> port;
		if (ist.fail())
			throw atdUtil::InvalidParameterException(
			    "socket","invalid port number",aval);
	    }
	    else if (!aname.compare("type")) {
		if (aval.compare("server"))
			throw atdUtil::InvalidParameterException(
			    "socket","invalid socket type",aval);
	    }
	    else if (!aname.compare("maxIdle")) {
		istringstream ist(aval);
		int secs;
		ist >> secs;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(getName(),"maxIdle",aval);
		try {
		    setKeepAliveIdleSecs(secs);
		}
		catch (const atdUtil::IOException& e) {		// won't happen
		}
	    }
	    else throw atdUtil::InvalidParameterException(
	    	string("unrecognized socket attribute: ") + aname);
	}
    }
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


