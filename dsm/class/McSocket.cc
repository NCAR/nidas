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
    firstRead(true),newFile(true),keepAliveIdleSecs(7200)
{
    setName("McSocket");
}

/*
 * Copy constructor. Should only be called before socket connection.
 */
McSocket::McSocket(const McSocket& x):
    atdUtil::McSocket(x),socket(0),name(x.name),
    connectionRequester(0),amRequester(x.amRequester),
    firstRead(true),newFile(true),keepAliveIdleSecs(x.keepAliveIdleSecs)
{
}

/*
 * constructor, but with a new, connected atdUtil::Socket
 */
McSocket::McSocket(const McSocket& x,atdUtil::Socket* sock):
    atdUtil::McSocket(x),socket(sock),name(x.name),
    connectionRequester(0),amRequester(x.amRequester),
    firstRead(true),newFile(true),
    keepAliveIdleSecs(x.keepAliveIdleSecs)

{
    if (socket->getKeepAliveIdleSecs() != keepAliveIdleSecs) {
	try {
	    socket->setKeepAliveIdleSecs(keepAliveIdleSecs);
	}
	catch (const atdUtil::IOException& e) {
	}
    }
}

McSocket* McSocket::clone() const
{
    return new McSocket(*this);
}

IOChannel* McSocket::connect()
    throw(atdUtil::IOException)
{
    atdUtil::Socket* sock;
    if (isRequester()) sock = atdUtil::McSocket::connect();
    else sock = accept();
    sock->setKeepAliveIdleSecs(keepAliveIdleSecs);
    setName(sock->getRemoteSocketAddress().toString());
    return new McSocket(*this,sock);
}

void McSocket::requestConnection(ConnectionRequester* requester)
    throw(atdUtil::IOException)
{
    connectionRequester = requester;
    if (isRequester()) request();	// starts requester thread
    else listen();			// starts listener thread
}

void McSocket::connected(atdUtil::Socket* sock)
{
    // cerr << "McSocket::connected, sock=" << sock->getRemoteSocketAddress().toString() << endl;
    sock->setKeepAliveIdleSecs(keepAliveIdleSecs);
    McSocket* newsock = new McSocket(*this,sock);
    newsock->setName(sock->getRemoteSocketAddress().toString());
    assert(connectionRequester);
    connectionRequester->connected(newsock);
}

atdUtil::Inet4Address McSocket::getRemoteInet4Address() const throw()
{
    if (socket) {
	const atdUtil::SocketAddress& addr = socket->getRemoteSocketAddress();
	const atdUtil::Inet4SocketAddress* i4addr =
		dynamic_cast<const atdUtil::Inet4SocketAddress*>(&addr);
	if (i4addr) return i4addr->getInet4Address();
    }
    return atdUtil::Inet4Address();
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
    string saddr;
    string sport;
    bool multicast = true;

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
	    else if (!aname.compare("requestNumber")) {
		int i;
	        istringstream ist(aval);
		ist >> i;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(
			    getName(),aname,aval);
		setRequestNumber(i);
	    }
	    else if (!aname.compare("type")) {
		if (!aval.compare("mcaccept")) {
		    multicast = true;
		    setRequester(false);
		}
		else if (!aval.compare("mcrequest")) {
		    multicast = true;
		    setRequester(true);
		}
		else if (!aval.compare("dgaccept")) {
		    multicast = false;
		    setRequester(false);
		}
		else if (!aval.compare("dgrequest")) {
		    multicast = false;
		    setRequester(true);
		}
		else throw atdUtil::InvalidParameterException(
			getName(),"type",aval);
	    }
	    else if (!aname.compare("maxIdle")) {
		istringstream ist(aval);
		ist >> keepAliveIdleSecs;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(getName(),"maxIdle",aval);
	    }
	    else throw atdUtil::InvalidParameterException(
	    	string("unrecognized socket attribute: ") + aname);
	}
    }

    // Default address for multicast requesters and accepters
    // is DSM_MULTICAST_ADDR.
    // Default address for unicast accepters is INADDR_ANY.
    // Unicast requesters must know who they are requesting
    // from
    if (saddr.length() == 0) {
        if (multicast) saddr = DSM_MULTICAST_ADDR;
	else if (!isRequester()) saddr = "0.0.0.0";	// any
	else throw atdUtil::InvalidParameterException(
	    	getName(),"address","unknown address for dgrequest socket");
    }
    atdUtil::Inet4Address iaddr;
    try {
	iaddr = atdUtil::Inet4Address::getByName(saddr);
    }
    catch(const atdUtil::UnknownHostException& e) {
	throw atdUtil::InvalidParameterException(
	    "mcsocket: parsing XML","unknown IP address",saddr);
    }

    int port = 0;
    if (sport.length() > 0) port = atoi(sport.c_str());
    else port = DSM_SVC_REQUEST_PORT;

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
