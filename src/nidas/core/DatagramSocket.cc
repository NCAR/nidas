/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-02-23 10:17:38 -0700 (Mon, 23 Feb 2009) $

    $LastChangedRevision: 4511 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/Socket.cc $
 ********************************************************************

*/

#include <nidas/core/DatagramSocket.h>
#include <nidas/core/McSocketUDP.h>
#include <nidas/util/Process.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

DatagramSocket::DatagramSocket():
        _remotePort(0),
	_nusocket(0),_iochanRequester(0),
        _firstRead(true),_newInput(true),
        _nonBlocking(false)
{
    setName("DatagramSocket (unconnected)");
}

/*
 * Copy constructor.  Should only be called before connection.
 */
DatagramSocket::DatagramSocket(const DatagramSocket& x):
	_remoteSockAddr(x._remoteSockAddr.get() ? x._remoteSockAddr->clone(): 0),
        _remoteHost(x._remoteHost),_remotePort(x._remotePort),
        _unixPath(x._unixPath),
	_nusocket(0),_name(x._name),
        _iochanRequester(x._iochanRequester),
        _firstRead(true),_newInput(true),
        _nonBlocking(x._nonBlocking)
{
    setName(_remoteSockAddr->toString());
    assert(x._nusocket == 0);
}

DatagramSocket::DatagramSocket(nidas::util::DatagramSocket* sock):
        _remotePort(0),
	_nusocket(sock),_iochanRequester(0),
        _firstRead(true),_newInput(true),
        _nonBlocking(false)
{
}


DatagramSocket::~DatagramSocket()
{
    close();
    delete _nusocket;
}

DatagramSocket* DatagramSocket::clone() const 
{
    return new DatagramSocket(*this);
}

size_t DatagramSocket::getBufferSize() const throw()
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

void DatagramSocket::setRemoteSocketAddress(const n_u::SocketAddress& val)
{
    _remoteSockAddr.reset(val.clone());
}

void DatagramSocket::setRemoteHostPort(const string& hostname,
    unsigned short port)
{
    _remoteHost = hostname;
    _remotePort = port;
}

void DatagramSocket::setRemoteUnixPath(const string& unixpath)
{
    _unixPath = unixpath;
}

const n_u::SocketAddress& DatagramSocket::getRemoteSocketAddress()
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

n_u::Inet4Address DatagramSocket::getRemoteInet4Address()
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

IOChannel* DatagramSocket::connect() throw(n_u::IOException)
{
    // getRemoteSocketAddress may throw UnknownHostException
    const n_u::SocketAddress& saddr = getRemoteSocketAddress();
    if (!_nusocket) _nusocket = new n_u::DatagramSocket(saddr.getFamily());
    _nusocket->connect(saddr);
    _nusocket->setNonBlocking(isNonBlocking());
    return this;
}

void DatagramSocket::requestConnection(IOChannelRequester* requester)
	throw(n_u::IOException)
{
    const n_u::SocketAddress& saddr = getRemoteSocketAddress();
    if (!_nusocket) _nusocket = new n_u::DatagramSocket(saddr.getFamily());
    _nusocket->connect(saddr);
    _nusocket->setNonBlocking(isNonBlocking());
    _iochanRequester->connected(this);
}

void DatagramSocket::fromDOMElement(const xercesc::DOMElement* node)
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
	    // Unix socket address
	    else if (aname == "path") unixPath = n_u::Process::expandEnvVars(aval);
	    else if (aname == "port") {
		istringstream ist(n_u::Process::expandEnvVars(aval));
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

