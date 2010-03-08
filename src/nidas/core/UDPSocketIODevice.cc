/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-09-18 09:20:54 -0600 (Tue, 18 Sep 2007) $

    $LastChangedRevision: 3980 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/core/UDPSocketIODevice.cc $
 ********************************************************************

*/

#include <nidas/core/UDPSocketIODevice.h>

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
    if (!_socket) _socket = new n_u::DatagramSocket();
    _socket->setBroadcastEnable(true);
    // cerr << "binding to address: " << _sockAddr.get()->toString() << endl;
    _socket->bind(*_sockAddr.get());
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
