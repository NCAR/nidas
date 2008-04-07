/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-09-18 09:20:54 -0600 (Tue, 18 Sep 2007) $

    $LastChangedRevision: 3980 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/core/TCPSocketIODevice.cc $
 ********************************************************************

*/

#include <nidas/core/TCPSocketIODevice.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

TCPSocketIODevice::TCPSocketIODevice():
    socket(0), tcpNoDelay(false)
{
}

TCPSocketIODevice::~TCPSocketIODevice()
{
    close();
}

void TCPSocketIODevice::close() throw(n_u::IOException)
{
    if (socket && socket->getFd() >= 0) {
	n_u::Logger::getInstance()->log(LOG_INFO,
	    "closing: %s",getName().c_str());
	socket->close();
    }
    delete socket;
    socket = 0;
}

void TCPSocketIODevice::open(int flags)
	throw(n_u::IOException,n_u::InvalidParameterException)
{
    SocketIODevice::open(flags);

    if (!socket) socket = new n_u::Socket();

    socket->connect(*sockAddr.get());
    socket->setTcpNoDelay(getTcpNoDelay());
}

