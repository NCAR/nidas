/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/BluetoothRFCommSocketIODevice.h>

#ifdef HAVE_BLUETOOTH_RFCOMM_H

#include <nidas/util/Logger.h>

#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <stdlib.h>

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

    // for a bluetooth socket, the bind address will be a digit N,
    // parsed from hciN in the devicename:   btssp:bdaddr::hciN
    if (_bindAddr.length() > 0) {
        int hci_dev_id = atoi(_bindAddr.c_str());
        // cerr << "bluetooth: hci_dev_id=" << hci_dev_id << endl;
        bdaddr_t hciaddr;

        // bluez lib_hci library routine to get the interface's bluetooth address
        // from its index, N in hciN.
        if (::hci_devba(hci_dev_id,&hciaddr) < 0)
            throw n_u::IOException(getName(),"getting hci address",errno);

        n_u::BluetoothRFCommSocketAddress saddr(n_u::BluetoothAddress(&hciaddr),0);
        DLOG(("%s: bluetooth: binding to hci%d, addr: %s",getName().c_str(),hci_dev_id,saddr.toString().c_str()));
        _socket->bind(saddr);
    }
    else _socket->bind(0);
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
