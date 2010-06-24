/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/core/SocketIODevice.cc $
 ********************************************************************

*/

#include <nidas/core/ServerSocketIODevice.h>
#include <nidas/core/SocketIODevice.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

ServerSocketIODevice::ServerSocketIODevice():
    _addrtype(-1),_sockPort(-1),_serverSocket(0),_socket(0)
{
}

ServerSocketIODevice::~ServerSocketIODevice()
{
    closeServerSocket();
    close();
}

void ServerSocketIODevice::close() throw(n_u::IOException)
{
    if (_socket && _socket->getFd() >= 0) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "closing: %s",getName().c_str());
	_socket->close();
    }
    delete _socket;
    _socket = 0;
}

void ServerSocketIODevice::closeServerSocket() throw(n_u::IOException)
{
    if (_serverSocket && _serverSocket->getFd() >= 0)
	_serverSocket->close();
    delete _serverSocket;
    _serverSocket = 0;
}

void ServerSocketIODevice::open(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    close();

    if (_addrtype < 0) {
	try {
	    SocketIODevice::parseAddress(getName(),_addrtype,_unixPath,_sockPort);
	}
	catch(const n_u::ParseException &e) {
	    throw n_u::InvalidParameterException(e.what());
	}
    }
    if (_addrtype == AF_INET) 
        _sockAddr.reset(new n_u::Inet4SocketAddress(_sockPort));
    else _sockAddr.reset(new n_u::UnixSocketAddress(_unixPath));

    if (!_serverSocket)
        _serverSocket = new n_u::ServerSocket(*_sockAddr.get());
    _socket = _serverSocket->accept();
    _socket->setTcpNoDelay(getTcpNoDelay());
    _socket->setNonBlocking(false);
}

