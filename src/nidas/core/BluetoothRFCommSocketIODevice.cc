/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-09-18 09:20:54 -0600 (Tue, 18 Sep 2007) $

    $LastChangedRevision: 3980 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/core/BluetoothRFCommSocketIODevice.cc $
 ********************************************************************

*/

#ifdef HAS_BLUETOOTHRFCOMM_H

#include <nidas/core/BluetoothRFCommSocketIODevice.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

BluetoothRFCommSocketIODevice::BluetoothRFCommSocketIODevice():
    _socket(0)
{
}

BluetoothRFCommSocketIODevice::~BluetoothRFCommSocketIODevice()
{
    close();
}

void BluetoothRFCommSocketIODevice::close() throw(n_u::IOException)
{
    if (_socket && _socket->getFd() >= 0) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "closing: %s",getName().c_str());
	_socket->close();
    }
    delete _socket;
    _socket = 0;
}

void BluetoothRFCommSocketIODevice::open(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    // parses the device name into the _sockAddr
    SocketIODevice::open(flags);

    // cerr << "sockaddr=" << _sockAddr->toString() << endl;
    if (!_socket) _socket = new n_u::BluetoothRFCommSocket();
    _socket->bind(0);
    _socket->connect(*_sockAddr.get());
}

size_t BluetoothRFCommSocketIODevice::read(void *buf, size_t len, int msecTimeout)
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
#endif
