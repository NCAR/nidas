// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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

#include "ServerSocketIODevice.h"
#include "SocketIODevice.h"

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

ServerSocketIODevice::ServerSocketIODevice():
    _addrtype(-1),_unixPath(),
    _sockPort(-1),_sockAddr(),
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

    string bindAddr;

    if (_addrtype < 0) {
	try {
	    SocketIODevice::parseAddress(getName(),_addrtype,_unixPath,_sockPort,bindAddr);
	}
	catch(const n_u::ParseException &e) {
	    throw n_u::InvalidParameterException(e.what());
	}
    }
    if (_addrtype == AF_INET) {
        n_u::Inet4Address addr;
        try {
            if (bindAddr.length() > 0) addr = n_u::Inet4Address::getByName(bindAddr);
        }
        catch (const n_u::UnknownHostException& e) {
	    throw n_u::InvalidParameterException(e.what());
        }
        _sockAddr.reset(new n_u::Inet4SocketAddress(addr,_sockPort));
    }
    else _sockAddr.reset(new n_u::UnixSocketAddress(_unixPath));

    if (!_serverSocket)
        _serverSocket = new n_u::ServerSocket(*_sockAddr.get());
    _socket = _serverSocket->accept();
    _socket->setTcpNoDelay(getTcpNoDelay());
    _socket->setNonBlocking(false);
}

