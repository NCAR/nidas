/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
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
 * Open socket for listening on port 8100.
 */
RemoteSerialListener::RemoteSerialListener() throw(IOException) :
	ServerSocket(8100)
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
    int n = newsock->recv(devname, (sizeof devname) -1, 0);

    devname[n] = 0;

    // cerr << "RemoteSerial accepted connection for devname " << devname << endl;

    newsock->setNonBlocking(true);

    return new RemoteSerialConnection(newsock,devname);
}
