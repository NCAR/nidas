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

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

ServerSocketIODevice::ServerSocketIODevice():
    addrtype(-1),sockPort(-1),serverSocket(0),socket(0)
{
}

ServerSocketIODevice::~ServerSocketIODevice()
{
    closeServerSocket();
    close();
}

void ServerSocketIODevice::close() throw(n_u::IOException)
{
    if (socket && socket->getFd() >= 0) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "closing: %s",getName().c_str());
	socket->close();
    }
    delete socket;
    socket = 0;
}

void ServerSocketIODevice::closeServerSocket() throw(n_u::IOException)
{
    if (serverSocket && serverSocket->getFd() >= 0)
	serverSocket->close();
    delete serverSocket;
    serverSocket = 0;
}

void ServerSocketIODevice::parseAddress(const string& name)
	throw(n_u::ParseException)
{
    string::size_type idx = name.find(':');
    addrtype = -1;
    unixPath = string();
    sockPort = -1;
    if (idx != string::npos) {
	string field = name.substr(0,idx);
	if (!field.compare("inet")) addrtype = AF_INET;
	else if (!field.compare("unix")) addrtype = AF_UNIX;
	idx++;
    }
    if (addrtype < 0)
	throw n_u::ParseException(name,
		"address type prefix should be \"inet:\" or \"unix:\"");

    if (addrtype == AF_UNIX) {
        unixPath = name.substr(idx);
        if (unixPath.length() == 0)
            throw n_u::ParseException(name,
                "cannot parse unix socket name");
    }
    else {
	string::size_type idx2 = name.find(':',idx);
	if (idx2 != string::npos) {
	    string portstr = name.substr(idx2+1);
	    istringstream ist(portstr);
	    ist >> sockPort;
	    if (ist.fail()) sockPort = -1;
	}
        if (sockPort < 0)
            throw n_u::ParseException(name,"cannot parse port");
    }
}

void ServerSocketIODevice::open(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    close();

    if (addrtype < 0) {
	try {
	    parseAddress(getName());
	}
	catch(const n_u::ParseException &e) {
	    throw n_u::InvalidParameterException(e.what());
	}
    }
    if (addrtype == AF_INET) 
        sockAddr.reset(new n_u::Inet4SocketAddress(sockPort));
    else sockAddr.reset(new n_u::UnixSocketAddress(unixPath));

    if (!serverSocket) {
        serverSocket = new n_u::ServerSocket(*sockAddr.get());
        /*
         * Set serverSocket to non-blocking, so that accept does not
         * wait and returns EAGAIN or EWOULDBLOCK if no connections
         * are present to be accepted.
         */
        serverSocket->setNonBlocking(true);
    }
    socket = serverSocket->accept();
    socket->setTcpNoDelay(getTcpNoDelay());
    socket->setNonBlocking(false);
}

