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

    /*
    * the first message will have the device name in it.
    */
    char devname[128];
    int n = newsock->recv(devname, (sizeof devname) - 1, 0);

    devname[n] = 0;
    char* nl = strchr(devname,'\n');
    if (nl) *nl = 0;

    cerr << "RemoteSerial accepted connection for devname \"" <<
    	devname << "\"" << endl;

    newsock->setNonBlocking(true);

    return new RemoteSerialConnection(newsock,devname);
}
