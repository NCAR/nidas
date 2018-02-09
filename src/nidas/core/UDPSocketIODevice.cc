/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
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

#include "UDPSocketIODevice.h"

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

UDPSocketIODevice::UDPSocketIODevice():
    _socket(0)
{
}

UDPSocketIODevice::~UDPSocketIODevice()
{
    close();
}

void UDPSocketIODevice::close() throw(n_u::IOException)
{
    if (_socket && _socket->getFd() >= 0) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "closing: %s",getName().c_str());
	_socket->close();
    }
    delete _socket;
    _socket = 0;
}

void UDPSocketIODevice::open(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    SocketIODevice::open(flags);

    bool multicast = false;
    const n_u::Inet4SocketAddress *i4saddr = 0;

    if (_sockAddr->getFamily() == AF_INET) {
        i4saddr = dynamic_cast<const n_u::Inet4SocketAddress*>(_sockAddr.get());
        assert(i4saddr);
        multicast = i4saddr->getInet4Address().isMultiCastAddress();
    }
    
    n_u::MulticastSocket* msock = 0;
    if (!_socket) {
        if (multicast) _socket = msock = new n_u::MulticastSocket();
        else _socket = new n_u::DatagramSocket();
    }
    // cerr << "binding to address: " << _sockAddr.get()->toString() << endl;
    _socket->setBroadcastEnable(true);
    _socket->bind(*_sockAddr);

    if (msock) msock->joinGroup(i4saddr->getInet4Address());
}

size_t UDPSocketIODevice::read(void *buf, size_t len, int msecTimeout)
    throw(nidas::util::IOException)
{
    size_t l = 0;
    try {
        _socket->setTimeout(msecTimeout);
        l = _socket->recv(buf,len);
        _socket->setTimeout(0);
    }
    catch(const nidas::util::IOException& e) {
        _socket->setTimeout(0);
        throw e;
    }
    return l;
}
