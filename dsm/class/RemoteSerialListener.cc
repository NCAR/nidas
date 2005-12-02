/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <RemoteSerialListener.h>

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>

using namespace std;
using namespace dsm;
using namespace atdUtil;

/**
 * Open a ServerSocket for listening on a given port.
 */
RemoteSerialListener::RemoteSerialListener(unsigned short port) throw(IOException) :
	ServerSocket(port)
{
}

RemoteSerialListener::~RemoteSerialListener() throw (IOException)
{
    close();
}

RemoteSerialConnection* RemoteSerialListener::acceptConnection() throw(IOException)
{
    Socket* newsock = accept();
    return new RemoteSerialConnection(newsock);
}
