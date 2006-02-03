/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-01-20 09:58:25 -0700 (Fri, 20 Jan 2006) $

    $LastChangedRevision: 3245 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/SocketIODevice.cc $
 ********************************************************************

*/

#include <SocketIODevice.h>

#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;

SocketIODevice::SocketIODevice():
    addrtype(-1),destport(-1),socket(0)
{
}

SocketIODevice::~SocketIODevice()
{
    close();
}

void SocketIODevice::close() throw(atdUtil::IOException)
{
    if (socket && socket->getFd() >= 0) {
	atdUtil::Logger::getInstance()->log(LOG_INFO,
	    "closing: %s",getName().c_str());
	socket->close();
    }
    delete socket;
    socket = 0;
}

void SocketIODevice::parseAddress(const string& name)
	throw(atdUtil::ParseException)
{
    size_t idx = name.find(':');
    addrtype = -1;
    desthost = string();
    destport = -1;
    if (idx != string::npos) {
	string field = name.substr(0,idx);
	if (!field.compare("inet")) addrtype = AF_INET;
	else if (!field.compare("unix")) addrtype = AF_UNIX;
	idx++;
    }
    if (addrtype < 0)
	throw atdUtil::ParseException(name,
		"address type prefix should be \"inet:\" or \"unix:\"");

    if (addrtype == AF_UNIX) desthost = name.substr(idx);
    else {
	size_t idx2 = name.find(':',idx);
	if (idx2 != string::npos) {
	    desthost = name.substr(idx,idx2-idx);
	    string portstr = name.substr(idx2+1);
	    istringstream ist(portstr);
	    ist >> destport;
	    if (ist.fail()) destport = -1;
	}
    }
    if (desthost.length() == 0)
	throw atdUtil::ParseException(name,
	    "cannot parse host name");
    if (addrtype == AF_INET && destport < 0)
	throw atdUtil::ParseException(name,"cannot parse port");

}

void SocketIODevice::open(int flags)
	throw(atdUtil::IOException,atdUtil::InvalidParameterException)
{
    atdUtil::Logger::getInstance()->log(LOG_NOTICE,
    	"opening: %s",getName().c_str());

    if (addrtype < 0) {
	try {
	    parseAddress(getName());
	}
	catch(const atdUtil::ParseException &e) {
	    throw atdUtil::InvalidParameterException(e.what());
	}
    }
    if (addrtype == AF_INET) {
	try {
	    sockAddr.reset(new atdUtil::Inet4SocketAddress(
		atdUtil::Inet4Address::getByName(desthost),destport));
	}
	catch(const atdUtil::UnknownHostException &e) {
	    throw atdUtil::InvalidParameterException(e.what());
	}
    }
    else sockAddr.reset(new atdUtil::UnixSocketAddress(desthost));

    if (!socket) socket = new atdUtil::Socket();

    socket->connect(*sockAddr.get());
    socket->setTcpNoDelay(getTcpNoDelay());
}

