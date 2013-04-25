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

#include <nidas/core/SocketIODevice.h>
#include <nidas/util/BluetoothRFCommSocketAddress.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SocketIODevice::SocketIODevice():
    _addrtype(-1),_desthost(),_destport(-1),_sockAddr()
{
}

SocketIODevice::~SocketIODevice()
{
}

/* static */
void SocketIODevice::parseAddress(const string& name, int& addrtype,
    string& desthost, int& destport) throw(n_u::ParseException)
{
    string::size_type idx = name.find(':');
    addrtype = -1;
    desthost = string();
    destport = -1;
    if (idx != string::npos) {
	string field = name.substr(0,idx);
	if (field == "inet" || field == "sock" || field == "usock")
            addrtype = AF_INET;
	else if (field == "unix") addrtype = AF_UNIX;
        // Docs about the Java API for bluetooth mention "btspp:" in the URL,
        // meaning Bluetooth Serial Port Profile. 
	else if (field == "btspp") addrtype = AF_BLUETOOTH;
	idx++;
    }
    if (addrtype < 0)
	throw n_u::ParseException(name,
		"address type prefix should be \"inet:\", \"sock:\", \"unix:\" or \"usock:\" or \"btspp:\"");

    if (addrtype == AF_UNIX) desthost = name.substr(idx);
    else if (addrtype == AF_INET){
	string::size_type idx2 = name.find(':',idx);
	if (idx2 != string::npos) {
	    desthost = name.substr(idx,idx2-idx);
	    string portstr = name.substr(idx2+1);
	    istringstream ist(portstr);
	    ist >> destport;
	    if (ist.fail()) destport = -1;
	}
    }
#ifdef HAVE_BLUETOOTH_RFCOMM_H
    else if (addrtype == AF_BLUETOOTH){
        int ncolon;
	string::size_type idx2 = idx;
        for (ncolon = 0;
            (idx2 = name.find(":",idx2)) != string::npos; ncolon++,idx2++);

        // xx:xx:xx:xx:xx:xx, or name with default channel=1 
        if (ncolon == 5 || ncolon == 0) {
	    desthost = name.substr(idx);
            destport = 1;
        }
        // xx:xx:xx:xx:xx:xx:channel, or name:channel
        else if (ncolon == 6 || ncolon == 1) {
	    desthost = name.substr(idx,idx2-idx-1);
	    string portstr = name.substr(idx2);
	    istringstream ist(portstr);
	    ist >> destport;
	    if (ist.fail()) destport = -1;
	}
    }
#endif
    // check for empty desthost, except in the case of inet sockets,
    // where desthost can be empty because the ServerSocket is listening
    // on INADDR_ANY.
    if (addrtype != AF_INET && desthost.length() == 0)
	throw n_u::ParseException(name,
	    string("cannot parse host/path in socket address: ") + name);
    if (addrtype == AF_INET && destport < 0)
	throw n_u::ParseException(name,
            string("cannot parse port number in address: ") + name);
#ifdef HAVE_BLUETOOTH_RFCOMM_H
    if (addrtype == AF_BLUETOOTH && destport < 0)
	throw n_u::ParseException(name,
            string("cannot parse channel number in address: ") + name);
#endif
}

void SocketIODevice::open(int /* flags */)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    if (_addrtype < 0) {
	try {
	    parseAddress(getName(),_addrtype,_desthost,_destport);
	}
	catch(const n_u::ParseException &e) {
	    throw n_u::InvalidParameterException(getName(),"name",e.what());
	}
    }
    if (_addrtype == AF_INET) {
	try {
	    _sockAddr.reset(new n_u::Inet4SocketAddress(
		n_u::Inet4Address::getByName(_desthost),_destport));
	}
	catch(const n_u::UnknownHostException &e) {
	    throw n_u::InvalidParameterException(getName(),"name",e.what());
	}
    }
#ifdef HAVE_BLUETOOTH_RFCOMM_H
    else if (_addrtype == AF_BLUETOOTH) {
	try {
	    _sockAddr.reset(new n_u::BluetoothRFCommSocketAddress(
		n_u::BluetoothAddress::getByName(_desthost),_destport));
	}
	catch(const n_u::UnknownHostException &e) {
	    throw n_u::InvalidParameterException(getName(),"name",e.what());
	}
    }
#endif
    else if (_addrtype == AF_UNIX)
        _sockAddr.reset(new n_u::UnixSocketAddress(_desthost));
    else throw n_u::InvalidParameterException(getName(),"name","unsupported address type");
}

