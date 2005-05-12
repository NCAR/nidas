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

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(Socket)
CREATOR_ENTRY_POINT(ServerSocket)

Socket::Socket():socket(0)
{
}

/*
 * copy constructor.
 */
Socket::Socket(const Socket& x):saddr(x.saddr),socket(0)
{
    if (x.socket) {
        socket = new atdUtil::Socket(*x.socket);
	setName(socket->getInet4SocketAddress().toString());
    }
}

Socket::Socket(const atdUtil::Socket* sock):socket(new atdUtil::Socket(*sock))
{
    setName(socket->getInet4SocketAddress().toString());
}

Socket::~Socket()
{
    delete socket;
}

ServerSocket::ServerSocket():socket(0),thread(0)
{
}

ServerSocket::ServerSocket(const ServerSocket& x):port(x.port),
	socket(0),thread(0)
{
    if (x.socket) {
        socket = new atdUtil::ServerSocket(*x.socket);
	setName(socket->getInet4SocketAddress().toString());
    }
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
    delete socket;
}

void Socket::requestConnection(ConnectionRequester* requester,
	int pseudoPort) throw(atdUtil::IOException)
{
    if (!socket) socket = new atdUtil::Socket();
    socket->connect(saddr);
    setName(socket->getInet4SocketAddress().toString());
    cerr << "Socket::connected " << getName();
    requester->connected(this);
}

IOChannel* Socket::clone() const
{
    return new Socket(*this);
}

IOChannel* ServerSocket::clone() const
{
    return new ServerSocket(*this);
}

void ServerSocket::requestConnection(ConnectionRequester* requester,
	int pseudoPort) throw(atdUtil::IOException)
{
    connectionRequester = requester;
    if (!socket) socket = new atdUtil::ServerSocket(port);
    if (!thread) thread = new ServerSocketConnectionThread(*this);
    if (!thread->isRunning()) thread->start();
}

int ServerSocketConnectionThread::run() throw(atdUtil::IOException)
{
    for (;;) {
	atdUtil::Socket* newsock = ssock.socket->accept();
	Socket* sock = new Socket(newsock);
	delete newsock;
	ssock.connectionRequester->connected(sock);
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
	    else throw atdUtil::InvalidParameterException(
	    	string("unrecognized socket attribute:") + aname);
	}
    }
    saddr = atdUtil::Inet4SocketAddress(addr,port);
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
	    else throw atdUtil::InvalidParameterException(
	    	string("unrecognized socket attribute:") + aname);
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


