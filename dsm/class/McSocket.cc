/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <McSocket.h>
#include <Datagrams.h>

using namespace dsm;
using namespace std;
using namespace xercesc;


CREATOR_FUNCTION(McSocket)

/*
 * ctor
 */
McSocket::McSocket(): socket(0),connectionRequester(0),amRequester(true),
    firstRead(true),newFile(true)
{
}

/*
 * Copy constructor. Should only be called before socket connection.
 */
McSocket::McSocket(const McSocket& x):
    atdUtil::McSocket(x),socket(0),
    connectionRequester(0),amRequester(x.amRequester),
    firstRead(true),newFile(true)
{
}

/*
 * constructor, but with a new, connected atdUtil::Socket
 */
McSocket::McSocket(const McSocket& x,atdUtil::Socket* sock):
    atdUtil::McSocket(x),socket(sock),
    connectionRequester(0),amRequester(x.amRequester),
    firstRead(true),newFile(true)
{
}

McSocket* McSocket::clone() const
{
    return new McSocket(*this);
}

IOChannel* McSocket::connect(int pseudoPort)
    throw(atdUtil::IOException)
{
    setPseudoPort(pseudoPort);
    atdUtil::Socket* sock;
    if (isRequester()) sock = atdUtil::McSocket::connect();
    else sock = accept();
    return new McSocket(*this,sock);
}

void McSocket::requestConnection(ConnectionRequester* requester,
	int pseudoPort)
    throw(atdUtil::IOException)
{
    connectionRequester = requester;
    setPseudoPort(pseudoPort);
    if (isRequester()) request();	// starts requester thread
    else listen();			// starts listener thread
}

void McSocket::connected(atdUtil::Socket* sock)
{
    cerr << "McSocket::connected, sock=" << sock->getInet4SocketAddress().toString() << endl;
    McSocket* newsock = new McSocket(*this,sock);
    assert(connectionRequester);
    connectionRequester->connected(newsock);
}

atdUtil::Inet4Address McSocket::getRemoteInet4Address() const throw()
{
    if (socket) return socket->getInet4Address();
    else return atdUtil::Inet4Address();
}

size_t McSocket::getBufferSize() const throw()
{
    try {
	if (socket) return socket->getReceiveBufferSize();
    }
    catch (const atdUtil::IOException& e) {}
    return 16384;
}

/*
 * Do the actual hardware read.
 */
size_t McSocket::read(void* buf, size_t len) throw (atdUtil::IOException)
{
    if (firstRead) firstRead = false;
    else newFile = false;
    size_t res = socket->recv(buf,len);
    return res;
}


void McSocket::close() throw (atdUtil::IOException)
{
    // cerr << "McSocket::close" << endl;
    if (socket && socket->getFd() >= 0) socket->close();
    delete socket;
    socket = 0;
    atdUtil::McSocket::close();
}

int McSocket::getFd() const
{
    if (socket) return socket->getFd();
    else return -1;
}

void McSocket::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    string stype;
    string saddr;
    string sport;

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
	    if (!aname.compare("address")) saddr = aval;
	    else if (!aname.compare("port")) sport = aval;
	    else if (!aname.compare("type")) {
		if (!aval.compare("mcaccept")) setRequester(false);
		else setRequester(true);
	    }
	    else throw atdUtil::InvalidParameterException(
	    	string("unrecognized socket attribute: ") + aname);
	}
    }


    int port = 0;
    if (sport.length() > 0) port = atoi(sport.c_str());
    else port = DSM_MULTICAST_PORT;

    if (saddr.length() == 0) saddr = DSM_MULTICAST_ADDR;
    atdUtil::Inet4Address iaddr;
    try {
	iaddr = atdUtil::Inet4Address::getByName(saddr);
    }
    catch(const atdUtil::UnknownHostException& e) {
	throw atdUtil::InvalidParameterException(
	    "parsing XML","unknown IP address",saddr);
    }

    setInet4McastSocketAddress(atdUtil::Inet4SocketAddress(iaddr,port));
}

DOMElement* McSocket::toDOMParent(
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

DOMElement* McSocket::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}


