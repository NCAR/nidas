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
	RSERIAL_PORT(8100) {

    {
	ostringstream os;
	os << "RemoteSerialSocket (port " << RSERIAL_PORT << ')';
	name = os.str();
    }

    if ((socketfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	throw IOException(getName(),"open",errno);

    int i = 1;
    if (setsockopt(socketfd,SOL_SOCKET,SO_REUSEADDR,
	      (char *)&i,sizeof i) < 0) {
	close(socketfd);
	socketfd = -1;
	throw IOException(getName(),"setsockopt",errno);
    }

    struct sockaddr_in myAddr;
    memset(&myAddr,0,sizeof myAddr);
    myAddr.sin_family = AF_INET;
    myAddr.sin_port = htons(RSERIAL_PORT);

    if (bind(socketfd, (struct sockaddr *) & myAddr, sizeof(myAddr)) < 0) {
	::close(socketfd);
	socketfd = -1;
	throw IOException(getName(),"bind",errno);
    }

    if (listen(socketfd, 2) < 0) {
	close(socketfd);
	socketfd = -1;
	throw IOException(getName(),"listen",errno);
    }
}

RemoteSerialListener::~RemoteSerialListener() throw (IOException) {
    if (socketfd >= 0 && ::close(socketfd) < 0)
    throw IOException(getName(),"close",errno);
}

RemoteSerialConnection* RemoteSerialListener::acceptConnection() throw(IOException)
{

    struct sockaddr_in clientAddr;
    socklen_t client_len = sizeof(clientAddr);

    int csock = accept(socketfd, (struct sockaddr *) & clientAddr, &client_len);

    if (csock < 0)
	throw IOException(getName(),"accept",errno);

    /*
    * the first message will have the device name in it.
    */
    char devname[128];
    int n = recv(csock, devname, (sizeof devname) -1, 0);
    if (n < 0)
	throw IOException(getName(),"recv",errno);

    devname[n] = 0;

    // cerr << "RemoteSerial accepted connection for devname " << devname << endl;

    int flags;
    /* set io to non-blocking, so network jams don't hang us up */
    if ((flags = fcntl(csock, F_GETFL, 0)) < 0)
	throw IOException(getName(),"fcntl(...,F_GETFL,...)",errno);

    if (fcntl(csock, F_SETFL, flags | O_NONBLOCK) < 0)
	throw IOException(getName(),"fcntl(...,F_SETFL,O_NONBLOCK)",errno);

    return new RemoteSerialConnection(csock,devname);
}
