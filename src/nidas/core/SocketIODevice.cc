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

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SocketIODevice::SocketIODevice():
    addrtype(-1),destport(-1)
{
}

SocketIODevice::~SocketIODevice()
{
}


void SocketIODevice::parseAddress(const string& name)
	throw(n_u::ParseException)
{
    string::size_type idx = name.find(':');
    addrtype = -1;
    desthost = string();
    destport = -1;
    if (idx != string::npos) {
	string field = name.substr(0,idx);
	if (field == "inet" || field == "sock") addrtype = AF_INET;
	else if (field == "unix") addrtype = AF_UNIX;
	idx++;
    }
    if (addrtype < 0)
	throw n_u::ParseException(name,
		"address type prefix should be \"inet:\", \"sock:\", or \"unix:\"");

    if (addrtype == AF_UNIX) desthost = name.substr(idx);
    else {
	string::size_type idx2 = name.find(':',idx);
	if (idx2 != string::npos) {
	    desthost = name.substr(idx,idx2-idx);
	    string portstr = name.substr(idx2+1);
	    istringstream ist(portstr);
	    ist >> destport;
	    if (ist.fail()) destport = -1;
	}
    }
    if (addrtype == AF_UNIX && desthost.length() == 0)
	throw n_u::ParseException(getName() + ":" + name,
	    "cannot parse path part of UNIX socket address");
    if (addrtype == AF_INET && destport < 0)
	throw n_u::ParseException(getName() + ":" + name,"cannot parse port");

}

void SocketIODevice::open(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    n_u::Logger::getInstance()->log(LOG_NOTICE,
    	"opening: %s",getName().c_str());

    if (addrtype < 0) {
	try {
	    parseAddress(getName());
	}
	catch(const n_u::ParseException &e) {
	    throw n_u::InvalidParameterException(e.what());
	}
    }
    if (addrtype == AF_INET) {
	try {
	    sockAddr.reset(new n_u::Inet4SocketAddress(
		n_u::Inet4Address::getByName(desthost),destport));
	}
	catch(const n_u::UnknownHostException &e) {
	    throw n_u::InvalidParameterException(e.what());
	}
    }
    else sockAddr.reset(new n_u::UnixSocketAddress(desthost));

}

