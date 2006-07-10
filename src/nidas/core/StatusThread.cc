/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/StatusThread.h>
#include <nidas/core/DSMEngine.h>
// #include <nidas/core/DSMConfig.h>
#include <nidas/dynld/raf/IRIGSensor.h>
#include <nidas/core/Datagrams.h>

#include <nidas/util/Socket.h>
#include <nidas/util/Exception.h>

#define mSecSleep 1000

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

StatusThread::StatusThread(const std::string& name):Thread(name)
{
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
}

int DSMEngineStat::run() throw(n_u::Exception)
{
    DSMEngine* engine = DSMEngine::getInstance();
    // const DSMConfig* dsm = engine->getDSMConfig();

    string dsm_name(engine->getDSMConfig()->getName());

    const SensorHandler* selector = engine->getSensorHandler();
    const SampleDater* dater = engine->getSampleDater();

    n_u::MulticastSocket msock;
    n_u::Inet4Address maddr =
    	n_u::Inet4Address::getByName(DSM_MULTICAST_ADDR);
    n_u::Inet4SocketAddress msaddr =
	n_u::Inet4SocketAddress(maddr,DSM_MULTICAST_STATUS_PORT);

    std::ostringstream statStream;

    struct tm tm;
    char cstr[24];
    struct timespec nsleep;

    try {
	for (;;) {
	    dsm_time_t tnow = dater->getDataSystemTime();

	    // wakeup (approx) 100 usecs after exact period time
	    long tdiff = USECS_PER_SEC - (tnow % USECS_PER_SEC) + 100;
	    // cerr << "DSMEngineStat , sleep " << tdiff << " usecs" << endl;
	    nsleep.tv_sec = tdiff / USECS_PER_SEC;
	    nsleep.tv_nsec = (tdiff % USECS_PER_SEC) * 1000;

	    if (nanosleep(&nsleep,0) < 0 && errno == EINTR) break;
	    if (isInterrupted()) break;

	    // Must make a copy of list of selector sensors
	    std::list<DSMSensor*> sensors = selector->getOpenedSensors();
	    std::list<DSMSensor*>::const_iterator si;

	    dsm_time_t tt = dater->getDataSystemTime();
            time_t     ut = tt / USECS_PER_SEC;
            gmtime_r(&ut,&tm);
//          int msec = tt % 1000;
            strftime(cstr,sizeof(cstr),"%Y-%m-%d %H:%M:%S",&tm);

            statStream << "<?xml version=\"1.0\"?><group>"
	               << "<name>" << dsm_name << "</name>"
                       << "<clock>" << cstr << "</clock>";

	    // Send status at 00:00, 00:10, etc.
            if ( ((ut % 10) == 0) && sensors.size() > 0) {

              statStream << "<status><![CDATA[";

	      DSMSensor* sensor = 0;
              for (si = sensors.begin(); si != sensors.end(); ++si) {
		sensor = *si;
		if (si == sensors.begin())
			sensor->printStatusHeader(statStream);
		sensor->printStatus(statStream);
              }
              if (sensor) sensor->printStatusTrailer(statStream);
              statStream << "]]></status>";
            }
            statStream << "</group>" << endl;

	    string statstr = statStream.str();
	    statStream.str("");
	    msock.sendto(statstr.c_str(),statstr.length()+1,0,msaddr);
	}
    }
    catch(const n_u::IOException& e) {
	msock.close();
	throw e;
    }
    msock.close();
    cerr << "DSMEngineStat run method returning" << endl;
    return 0;
}

/* static */
DSMServerStat* DSMServerStat::_instance = 0;

/* static */
DSMServerStat* DSMServerStat::getInstance() 
{
    if (!_instance) _instance = new DSMServerStat("DSMServerStat");
    return _instance;
}

DSMServerStat::DSMServerStat(const std::string& name):
	StatusThread(name),_sometime(getSystemTime())
{
}

/* static */
int DSMServerStat::run() throw(n_u::Exception)
{
    const std::string DATA_NETWORK = "192.168.184";
    n_u::MulticastSocket msock;
    n_u::Inet4Address maddr =
    	n_u::Inet4Address::getByName(DSM_MULTICAST_ADDR);
    n_u::Inet4SocketAddress msaddr =
	n_u::Inet4SocketAddress(maddr,DSM_MULTICAST_STATUS_PORT);

    // Set to proper interface if this computer has more than one.
    std::list<n_u::Inet4Address> itf = msock.getInterfaceAddresses();
    std::list<n_u::Inet4Address>::iterator itfi;
    for (itfi = itf.begin(); itfi != itf.end(); ++itfi)
      if ((*itfi).getHostAddress().compare(0, DATA_NETWORK.size(), DATA_NETWORK) == 0)
        msock.setInterface(*itfi);

    std::ostringstream statStream;

    struct tm tm;
    char cstr[24];

    struct timespec sleepTime;

    /* sleep a bit so that we're on an even interval boundary */
    unsigned long mSecVal =
      mSecSleep - (unsigned long)((getSystemTime() / USECS_PER_MSEC) % mSecSleep);

    sleepTime.tv_sec = mSecVal / 1000;
    sleepTime.tv_nsec = (mSecVal % 1000) * 1000000;
    if (nanosleep(&sleepTime,0) < 0) {
      if (errno == EINTR) return RUN_OK;
      throw n_u::Exception(string("nanosleep: ") +
      	n_u::Exception::errnoToString(errno));
    }

    dsm_time_t lasttime = 0;
    char *glyph[] = {"\\","|","/","-"};
    int anim=0;

    try {
        while (!amInterrupted()) {

            if (++anim == 4) anim=0;

	    dsm_time_t tt = _sometime;
            time_t     ut = tt / USECS_PER_SEC;
            gmtime_r(&ut,&tm);

            strftime(cstr,sizeof(cstr),"%Y-%m-%d %H:%M:%S",&tm);

            if (lasttime != _sometime)
            {
              lasttime = _sometime;
              statStream << "<?xml version=\"1.0\"?><group>"
	                 << "<name>dsm_server</name>"
                         << "<clock>" << cstr << "</clock>";
            }
            else
              statStream << "<?xml version=\"1.0\"?><group>"
	                 << "<name>dsm_server</name>"
                         << "<clock>no DSMs active...."+string(glyph[anim])+"</clock>";

//            // Send status at 00:00, 00:10, etc.
//            if ( ((ut % 10) == 0) && _sometime) {
//                statStream << "<status><![CDATA[";
//                statStream << "]]></status>";
//            }
            statStream << "</group>" << endl;
	    string statstr = statStream.str();
	    statStream.str("");
	    msock.sendto(statstr.c_str(),statstr.length()+1,0,msaddr);

            // sleep until the next interval...
            mSecVal =
	      mSecSleep - (unsigned long)((getSystemTime() / USECS_PER_MSEC) % mSecSleep);
            sleepTime.tv_sec = mSecVal / 1000;
            sleepTime.tv_nsec = (mSecVal % 1000) * 1000000;
            if (nanosleep(&sleepTime,0) < 0) {
                if (errno == EINTR) break;
                throw n_u::Exception(string("nanosleep: ") +
			n_u::Exception::errnoToString(errno));
            }
	}
    }
    catch(const n_u::IOException& e) {
	msock.close();
	throw e;
    }
    msock.close();
    cerr << "DSMServerStat run method returning" << endl;
    return 0;
}
