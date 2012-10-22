// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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
    _sockAddr(),_host(), _port(0),_unixPath(),
    _nusocket(0),_name(),_iochanRequester(0),
    _nonBlocking(false)
{
    setName("DatagramSocket (unconnected)");
}

/*
 * Copy constructor.  Should only be called before connection.
 */
DatagramSocket::DatagramSocket(const DatagramSocket& x): IOChannel(x),
        _sockAddr(x._sockAddr.get() ? x._sockAddr->clone(): 0),
        _host(x._host),_port(x._port),
        _unixPath(x._unixPath),
	_nusocket(0),_name(x._name),
        _iochanRequester(x._iochanRequester),
        _nonBlocking(x._nonBlocking)
{
    if (_sockAddr.get())
        setName(_sockAddr->toString());
}

/*
 * Assignment operator.  Should only be called before connection.
 */
DatagramSocket& DatagramSocket::operator=(const DatagramSocket& rhs)
{
    if (&rhs != this) {
        *(IOChannel*)this = rhs;
        _sockAddr.reset(rhs._sockAddr.get() ? rhs._sockAddr->clone(): 0);
        _host = rhs._host;
        _port = rhs._port;
        _unixPath = rhs._unixPath;
	_nusocket = 0;
        _name = rhs._name;
        _iochanRequester = rhs._iochanRequester;
        _nonBlocking = rhs._nonBlocking;
        if (_sockAddr.get())
            setName(_sockAddr->toString());
    }
    return *this;
}

DatagramSocket::DatagramSocket(nidas::util::DatagramSocket* sock):
    _sockAddr(),_host(), _port(0),_unixPath(),
    _nusocket(sock),_name(),_iochanRequester(0),
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

void DatagramSocket::setSocketAddress(const n_u::SocketAddress& val)
{
    // deletes the old
    _sockAddr.reset(val.clone());
}

void DatagramSocket::setHostPort(const string& hostname,
    unsigned short port)
{
    _host = hostname;
    _port = port;
}

void DatagramSocket::setUnixPath(const string& unixpath)
{
    _unixPath = unixpath;
}

const n_u::SocketAddress& DatagramSocket::getSocketAddress()
    throw(n_u::UnknownHostException)
{
    // lookup up address on every call.
    if (_host.length() > 0) {
        // throws UnknownHostException
        n_u::Inet4Address haddr =
            n_u::Inet4Address::getByName(_host);
        n_u::Inet4SocketAddress saddr(haddr,_port);
        setSocketAddress(saddr);
    }
    else if (_unixPath.length() > 0) {
        n_u::UnixSocketAddress saddr(getUnixPath());
        setSocketAddress(saddr);
    }
    else {
        // INADDR_ANY
        n_u::Inet4SocketAddress saddr(_port);
        setSocketAddress(saddr);
    }
    setName(_sockAddr->toString());
    return *_sockAddr.get();
}

IOChannel* DatagramSocket::connect() throw(n_u::IOException)
{
    try {
        const n_u::SocketAddress& saddr = getSocketAddress();

        if (saddr.getFamily() == AF_INET) {
            n_u::Inet4SocketAddress i4saddr =
            n_u::Inet4SocketAddress((const struct sockaddr_in*)
                    saddr.getConstSockAddrPtr());
            // check if INADDR_ANY, if so bind
            if (i4saddr.getInet4Address() == n_u::Inet4Address()) {
                if (!_nusocket) _nusocket = new n_u::DatagramSocket(saddr);
                else _nusocket->bind(saddr);
            }
            else {
                if (!_nusocket) _nusocket = new n_u::DatagramSocket();
                _nusocket->connect(saddr);
            }
        }
        else if (saddr.getFamily() == AF_UNIX) {
            if (!_nusocket) _nusocket = new n_u::DatagramSocket(saddr);
            else _nusocket->bind(saddr);
        }
        _nusocket->setNonBlocking(isNonBlocking());
    }
    catch(const n_u::UnknownHostException& e) {
        // getSocketAddress may throw UnknownHostException, which
        // is not derived from IOException
        throw n_u::IOException(getName(),"connect",e.what());
    }
    return this;
}

void DatagramSocket::requestConnection(IOChannelRequester* requester)
	throw(n_u::IOException)
{
    _iochanRequester = requester;
    connect();
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
		if (aval != "udp")
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
    if (port > 0 && unixPath.length() > 0)
        throw n_u::InvalidParameterException("socket","address",
            "cannot specify both a port number and a unix socket path");
    if (unixPath.length() > 0) setUnixPath(unixPath);
    else {
        if (port <= 0)
            throw n_u::InvalidParameterException("socket","port",
                "unknown port number");
        setHostPort(remoteHost,port);
        // Warn, but don't throw exception if address
        // for host cannot be found.
        if (remoteHost.length() > 0) {
            try {
                n_u::Inet4Address::getByName(remoteHost);
            }
            catch(const n_u::UnknownHostException& e) {
                WLOG(("") << getName() << ": " << e.what());
            }
        }
    }
}

