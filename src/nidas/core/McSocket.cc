/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/McSocket.h>
#include <nidas/core/Datagrams.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

/*
 * ctor
 */
McSocket::McSocket(): _socket(0),_connectionRequester(0),_amRequester(true),
    _firstRead(true),_newInput(true),_keepAliveIdleSecs(7200),
    _minWriteInterval(USECS_PER_SEC/100),_lastWrite(0),
    _nonBlocking(false)
{
    setName("McSocket");
}

/*
 * Copy constructor. Should only be called before socket connection.
 */
McSocket::McSocket(const McSocket& x):
    n_u::McSocket<n_u::Socket>(x),_socket(0),_name(x._name),
    _connectionRequester(0),_amRequester(x._amRequester),
    _firstRead(true),_newInput(true),_keepAliveIdleSecs(x._keepAliveIdleSecs),
    _minWriteInterval(x._minWriteInterval),_lastWrite(0),
    _nonBlocking(x._nonBlocking)
{
}

/*
 * Copy constructor, but with a new, connected n_u::Socket
 */
McSocket::McSocket(const McSocket& x,n_u::Socket* sock):
    n_u::McSocket<n_u::Socket>(x),_socket(sock),_name(x._name),
    _connectionRequester(0),_amRequester(x._amRequester),
    _firstRead(true),_newInput(true),
    _keepAliveIdleSecs(x._keepAliveIdleSecs),
    _minWriteInterval(x._minWriteInterval),_lastWrite(0),
    _nonBlocking(x._nonBlocking)

{
    try {
        if (_socket->getKeepAliveIdleSecs() != _keepAliveIdleSecs)
	    _socket->setKeepAliveIdleSecs(_keepAliveIdleSecs);
        _socket->setNonBlocking(_nonBlocking);
    }
    catch (const n_u::IOException& e) {
    }
}

McSocket* McSocket::clone() const
{
    return new McSocket(*this);
}

IOChannel* McSocket::connect()
    throw(n_u::IOException)
{
    n_u::Socket* sock;
    if (isRequester()) sock = n_u::McSocket<n_u::Socket>::connect();
    else sock = accept();
    sock->setKeepAliveIdleSecs(_keepAliveIdleSecs);
    sock->setNonBlocking(_nonBlocking);
    setName(sock->getRemoteSocketAddress().toString());
    return new McSocket(*this,sock);
}

void McSocket::requestConnection(ConnectionRequester* requester)
    throw(n_u::IOException)
{
    _connectionRequester = requester;
    if (isRequester()) request();	// starts requester thread
    else listen();			// starts listener thread
}

void McSocket::connected(n_u::Socket* sock)
{
    // cerr << "McSocket::connected, sock=" << sock->getRemoteSocketAddress().toString() << endl;
    McSocket* newsock = new McSocket(*this,sock);
    newsock->setName(sock->getRemoteSocketAddress().toString());
    assert(_connectionRequester);
    _connectionRequester->connected(newsock);
}

n_u::Inet4Address McSocket::getRemoteInet4Address()
{
    if (_socket) {
	const n_u::SocketAddress& addr = _socket->getRemoteSocketAddress();
	const n_u::Inet4SocketAddress* i4addr =
		dynamic_cast<const n_u::Inet4SocketAddress*>(&addr);
	if (i4addr) return i4addr->getInet4Address();
    }
    return n_u::Inet4Address();
}

size_t McSocket::getBufferSize() const throw()
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

/*
 * Do the actual hardware read.
 */
size_t McSocket::read(void* buf, size_t len) throw (n_u::IOException)
{
    if (_firstRead) _firstRead = false;
    else _newInput = false;
    size_t res = _socket->recv(buf,len);
    return res;
}


void McSocket::close() throw (n_u::IOException)
{
    // cerr << "McSocket::close" << endl;
    if (_socket && _socket->getFd() >= 0) _socket->close();
    delete _socket;
    _socket = 0;
    n_u::McSocket<n_u::Socket>::close();
}

int McSocket::getFd() const
{
    if (_socket) return _socket->getFd();
    else return -1;
}

void McSocket::fromDOMElement(const DOMElement* node)
	throw(n_u::InvalidParameterException)
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
	    if (aname == "address") saddr = aval;
	    else if (aname == "port") sport = aval;
	    else if (aname == "requestNumber") {
		int i;
	        istringstream ist(aval);
		ist >> i;
		if (ist.fail())
		    throw n_u::InvalidParameterException(
			    getName(),aname,aval);
		setRequestNumber(i);
	    }
	    else if (aname == "type") {
		if (aval == "mcaccept") {
		    multicast = true;
		    setRequester(false);
		}
		else if (aval == "mcrequest") {
		    multicast = true;
		    setRequester(true);
		}
		else if (aval == "dgaccept") {
		    multicast = false;
		    setRequester(false);
		}
		else if (aval == "dgrequest") {
		    multicast = false;
		    setRequester(true);
		}
		else throw n_u::InvalidParameterException(
			getName(),"type",aval);
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
		ist >> _keepAliveIdleSecs;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),"maxIdle",aval);
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

    // Default address for multicast requesters and accepters
    // is DSM_MULTICAST_ADDR.
    // Default address for unicast accepters is INADDR_ANY.
    // Unicast requesters must know who they are requesting
    // from
    if (saddr.length() == 0) {
        if (multicast) saddr = DSM_MULTICAST_ADDR;
	else if (!isRequester()) saddr = "0.0.0.0";	// any
	else throw n_u::InvalidParameterException(
	    	getName(),"address","unknown address for dgrequest socket");
    }
    n_u::Inet4Address iaddr;
    try {
	iaddr = n_u::Inet4Address::getByName(saddr);
    }
    catch(const n_u::UnknownHostException& e) {
        n_u::Logger::getInstance()->log(LOG_WARNING,"socket: unknown IP address: %s",
            saddr.c_str());
    }

    int port = 0;
    if (sport.length() > 0) port = atoi(sport.c_str());
    else port = DSM_SVC_REQUEST_PORT;

    setInet4McastSocketAddress(n_u::Inet4SocketAddress(iaddr,port));
}

