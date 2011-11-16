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

#include <nidas/core/ServerSocketIODevice.h>
#include <nidas/core/SocketIODevice.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

ServerSocketIODevice::ServerSocketIODevice():
    _addrtype(-1),_unixPath(),
    _sockPort(-1),_sockAddr(0),
    _serverSocket(0),_socket(0),_tcpNoDelay(false)
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

void ServerSocketIODevice::open(int)
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

