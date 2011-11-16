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

#include <nidas/core/RemoteSerialListener.h>

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstdio>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

/**
 * Open a ServerSocket for listening on a given port.
 */
RemoteSerialListener::RemoteSerialListener(unsigned short port) throw(n_u::IOException) :
	_socket(port)
{
}

RemoteSerialListener::~RemoteSerialListener() throw (n_u::IOException)
{
    _socket.close();
}

RemoteSerialConnection* RemoteSerialListener::acceptConnection() throw(n_u::IOException)
{
    n_u::Socket* newsock = _socket.accept();
    return new RemoteSerialConnection(newsock);
}
