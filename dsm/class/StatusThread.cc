/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <StatusThread.h>
#include <DSMEngine.h>
// #include <DSMConfig.h>
// #include <DSMSensor.h>
#include <Datagrams.h>

#include <atdUtil/Socket.h>

using namespace dsm;
using namespace std;

StatusThread::StatusThread(const std::string& name,int runPeriod):
	Thread(name),period(runPeriod)
{
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
}

int StatusThread::run() throw(atdUtil::Exception)
{
    DSMEngine* engine = DSMEngine::getInstance();
    const DSMConfig* dsm = engine->getDSMConfig();
    // const PortSelector* selector = engine->getPortSelector();
    const SampleDater* dater = engine->getSampleDater();

    unsigned long usecPeriod = period * USECS_PER_SEC;

    atdUtil::MulticastSocket msock;
    atdUtil::Inet4Address maddr =
    	atdUtil::Inet4Address::getByName(DSM_MULTICAST_ADDR);
    atdUtil::Inet4SocketAddress msaddr =
	atdUtil::Inet4SocketAddress(maddr,DSM_MULTICAST_STATUS_PORT);

    std::ostringstream statStream;

    struct timespec nsleep;
    try {
	for (;;) {
	    
	    dsm_time_t tnow = dater->getDataSystemTime();

	    // wakeup (approx) 100 usecs after exact period time
	    long tdiff = usecPeriod - (tnow % usecPeriod) + 100;
	    cerr << "StatusThread, sleep " << tdiff << " usecs" << endl;
	    nsleep.tv_sec = tdiff / USECS_PER_SEC;
	    nsleep.tv_nsec = (tdiff % USECS_PER_SEC) * 1000;

	    if (nanosleep(&nsleep,0) < 0 && errno == EINTR) break;
	    if (isInterrupted()) break;

	    DSMSensor::printStatusHeader(statStream);
	    const std::list<DSMSensor*>& sensors = dsm->getSensors();
	    std::list<DSMSensor*>::const_iterator si;
	    for (si = sensors.begin(); si != sensors.end(); ++si) {
		DSMSensor* sensor = *si;
		sensor->printStatus(statStream);
	    }
	    DSMSensor::printStatusTrailer(statStream);

	    string statstr = statStream.str();
	    statStream.str("");
	    msock.sendto(statstr.c_str(),statstr.length()+1,0,msaddr);
	}
    }
    catch(const atdUtil::IOException& e) {
	msock.close();
	throw e;
    }
    msock.close();
    cerr << "StatusThread run method returning" << endl;
    return 0;
}
