/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#include <ConfigRequestor.h>

#include <iostream>

using namespace dsm;
using namespace std;

ConfigRequestor::ConfigRequestor(int listenPort) throw(atdUtil::IOException) :
        Thread("dsm::ConfigRequestor"),lport(listenPort)
{
}

ConfigRequestor::~ConfigRequestor()
{
    msock.close();
}

int ConfigRequestor::run() throw(atdUtil::Exception)
{

    atdUtil::Inet4SocketAddress to(
	    atdUtil::Inet4Address::getByName(DSM_MULTICAST_ADDR),
	    DSM_MULTICAST_PORT);

    XMLConfigRequestDatagram dgram;
    dgram.setSocketAddress(to);
    dgram.setRequestorListenPort(lport);
									
    struct timespec waitPeriod;
    // waitPeriod.tv_sec = 0;
    // waitPeriod.tv_nsec = 500000000;             // 1/2 a second
    waitPeriod.tv_sec = 1;
    waitPeriod.tv_nsec = 0;             // 
    nanosleep(&waitPeriod,0);
									
    for (int numCasts=0; ; numCasts++) {
	dgram.setNumMulticasts(numCasts);

	cerr << "sending dgram, length=" << dgram.getLength() << endl;
	cerr << "sending dgram, port=" << dgram.getRequestorListenPort() << endl;
	cerr << "sending dgram, to=" << dgram.getSocketAddress().toString() << endl;

	msock.send(dgram);
	cerr << "sent dgram" << endl;
	nanosleep(&waitPeriod,0);
	if (amInterrupted()) break;
    }
    cerr << "ConfigRequestor run ending" << endl;
    return RUN_OK;
}
