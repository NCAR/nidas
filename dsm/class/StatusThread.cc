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
#include <IRIGSensor.h>
#include <Datagrams.h>

#include <atdUtil/Socket.h>

using namespace dsm;
using namespace std;

StatusThread::StatusThread(const std::string& name):Thread(name)
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

    atdUtil::MulticastSocket msock;
    atdUtil::Inet4Address maddr =
    	atdUtil::Inet4Address::getByName(DSM_MULTICAST_ADDR);
    atdUtil::Inet4SocketAddress msaddr =
	atdUtil::Inet4SocketAddress(maddr,DSM_MULTICAST_STATUS_PORT);

    std::ostringstream statStream;

    struct tm tm;
    char cstr[24];
    struct timespec nsleep;

    try {
        IRIGSensor* irig_sensor = 0;
        const std::list<DSMSensor*>& sensors = dsm->getSensors();
        std::list<DSMSensor*>::const_iterator si;
    
        // locate the IRIG sensor
        for (si = sensors.begin(); si != sensors.end(); ++si) {
            DSMSensor* sensor = *si;
            if (!(sensor->getClassName().compare("IRIGSensor"))) {
                irig_sensor = (IRIGSensor*) sensor;
                break;
            }
        }
        if (!irig_sensor) throw atdUtil::Exception(
          string("Cannot display time.  IRIGSensor not found in XML configuration"));

        int nSec = 0;
	for (;;) {
	    dsm_time_t tnow = dater->getDataSystemTime();

	    // wakeup (approx) 100 usecs after exact period time
	    long tdiff = USECS_PER_SEC - (tnow % USECS_PER_SEC) + 100;
	    // cerr << "StatusThread, sleep " << tdiff << " usecs" << endl;
	    nsleep.tv_sec = tdiff / USECS_PER_SEC;
	    nsleep.tv_nsec = (tdiff % USECS_PER_SEC) * 1000;

	    if (nanosleep(&nsleep,0) < 0 && errno == EINTR) break;
	    if (isInterrupted()) break;

            dsm_time_t tt = irig_sensor->getIRIGTime();
            time_t     ut = tt / USECS_PER_SEC;
            gmtime_r(&ut,&tm);
//          int msec = tt % 1000;
            strftime(cstr,sizeof(cstr),"%Y-%m-%d %H:%M:%S",&tm);
//          cerr << cstr << endl;  // DEBUG show clock...
            statStream << "<?xml version=\"1.0\"?>";

            if (!nSec)
              statStream << "<group>";

            statStream << "<clock>" << cstr << "</clock>";

            if (!nSec) {
              statStream << "<status><![CDATA[" << endl;

              DSMSensor* asen = *(sensors.begin());
              asen->printStatusHeader(statStream);
              for (si = sensors.begin(); si != sensors.end(); ++si) {
		DSMSensor* sensor = *si;
		sensor->printStatus(statStream);
              }
              asen->printStatusTrailer(statStream);
              statStream << "]]></status></group>";
            }
            statStream << endl;

	    string statstr = statStream.str();
	    statStream.str("");
	    msock.sendto(statstr.c_str(),statstr.length()+1,0,msaddr);

            // 10 Second period for generating status messages as well.
            if (++nSec == 10) nSec=0;
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
