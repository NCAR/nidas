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

    ConfigDatagram dgram;
    dgram.setSocketAddress(to);
    dgram.setDSMListenPort(lport);
    std::cerr << "sending dgram, length=" << dgram.getLength() << std::endl;
    std::cerr << "sending dgram, port=" << dgram.getDSMListenPort() << std::endl;
									
    struct timespec waitPeriod;
    waitPeriod.tv_sec = 0;
    waitPeriod.tv_nsec = 500000000;             // 1/2 a second
    nanosleep(&waitPeriod,0);
									
    for (int numCasts=0; ; numCasts++) {
	dgram.setNumMulticasts(numCasts);
	msock.send(dgram);
	nanosleep(&waitPeriod,0);
	if (amInterrupted()) break;
    }
    std::cerr << "ConfigRequestor run ending" << std::endl;
    return RUN_OK;
}
