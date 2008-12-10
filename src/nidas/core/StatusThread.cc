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
    const SampleClock* clock = engine->getSampleClock();

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
	    dsm_time_t tnow = clock->getTime();

	    // wakeup (approx) 100 usecs after exact period time
	    int tdiff = USECS_PER_SEC - (tnow % USECS_PER_SEC) + 100;
	    // cerr << "DSMEngineStat , sleep " << tdiff << " usecs" << endl;
	    nsleep.tv_sec = tdiff / USECS_PER_SEC;
	    nsleep.tv_nsec = (tdiff % USECS_PER_SEC) * NSECS_PER_USEC;

	    if (nanosleep(&nsleep,0) < 0 && errno == EINTR) break;
	    if (isInterrupted()) break;

	    // Must make a copy of list of selector sensors
	    std::list<DSMSensor*> sensors = selector->getAllSensors();
	    std::list<DSMSensor*>::const_iterator si;

	    dsm_time_t tt = clock->getTime();
            time_t     ut = tt / USECS_PER_SEC;
            gmtime_r(&ut,&tm);
//          int msec = tt % MSECS_PER_SEC;
            strftime(cstr,sizeof(cstr),"%Y-%m-%d %H:%M:%S",&tm);

            statStream << "<?xml version=\"1.0\"?><group>"
	               << "<name>" << dsm_name << "</name>"
                       << "<clock>" << cstr << "</clock>";

	    // Send status at 00:00, 00:03, etc.
            if ( ((ut % 3) == 0) && sensors.size() > 0) {

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
	StatusThread(name),_sometime(getSystemTime()),
        uSecPeriod(USECS_PER_SEC)
{
}

/* static */
int DSMServerStat::run() throw(n_u::Exception)
{
    // TODO: multicast network should be configured in the XML
    // const std::string DATA_NETWORK = "192.168.184.0";
    const std::string DATA_NETWORK = "127.0.0.0";
    n_u::Inet4Address dataAddr;
    n_u::Inet4Address maddr;
    try {
        dataAddr = n_u::Inet4Address::getByName(DATA_NETWORK);
        maddr = n_u::Inet4Address::getByName(DSM_MULTICAST_ADDR);
    }
    catch(const n_u::UnknownHostException& e) {
    }

    n_u::Inet4SocketAddress msaddr =
	n_u::Inet4SocketAddress(maddr,DSM_MULTICAST_STATUS_PORT);

    n_u::MulticastSocket msock;

    // Set to interface with closest address if this computer has more
    // than one.
    int matchbits = -1;
    n_u::Inet4Address matchIface;

    std::list<n_u::Inet4Address> itf = msock.getInterfaceAddresses();
    std::list<n_u::Inet4Address>::iterator itfi;
    for (itfi = itf.begin(); itfi != itf.end(); ++itfi) {
        int i = itfi->bitsMatch(dataAddr);
        if (i > matchbits) {
            matchIface = *itfi;
            matchbits = i;
        }
    }
    if (matchbits >= 0) {
#ifdef DEBUG
        cerr << "setting interface for multicast socket to " <<
            matchIface.getHostAddress() << ", bits=" << matchbits << endl;
#endif
        msock.setInterface(matchIface);
    }

    std::ostringstream statStream;

    struct tm tm;
    char cstr[24];

    // For some reason Thread::cancel() to a thread
    // waiting in nanosleep causes a seg fault.
    // Using select() works.  g++ 4.1.1, FC5
    // When cancelled, select does not return EINVAL,
    // it just never returns and the thread is gone.

// #define USE_NANOSLEEP
#ifdef USE_NANOSLEEP
    struct timespec sleepTime;
    struct timespec leftTime;   // try to figure out why nanosleep
                                // seg faults on thread cancel.
#else
    struct timeval sleepTime;
#endif

    /* sleep a bit so that we're on an even interval boundary */
    unsigned int uSecVal =
      uSecPeriod - (unsigned int)(getSystemTime() % uSecPeriod);

#ifdef USE_NANOSLEEP
    sleepTime.tv_sec = uSecVal / USECS_PER_SEC;
    sleepTime.tv_nsec = (uSecVal % USECS_PER_SEC) * NSECS_PER_USEC;
    if (nanosleep(&sleepTime,&leftTime) < 0) {
        if (errno == EINTR) return RUN_OK;
        throw n_u::Exception(string("nanosleep: ") +
            n_u::Exception::errnoToString(errno));
    }
#else
    sleepTime.tv_sec = uSecVal / USECS_PER_SEC;
    sleepTime.tv_usec = uSecVal % USECS_PER_SEC;
    if (::select(0,0,0,0,&sleepTime) < 0) {
        if (errno == EINTR) return RUN_OK;
        throw n_u::Exception(string("select: ") +
            n_u::Exception::errnoToString(errno));
    }
#endif

    dsm_time_t lasttime = 0;
    int sametime = 1;
    const char *glyph[] = {"\\","|","/","-"};
    int anim=0;

    try {
        while (!amInterrupted()) {

            if (++anim == 4) anim=0;

	    dsm_time_t tt = _sometime;
            time_t     ut = tt / USECS_PER_SEC;
            gmtime_r(&ut,&tm);

            strftime(cstr,sizeof(cstr),"%Y-%m-%d %H:%M:%S",&tm);

            if ((lasttime != _sometime) || sametime)
            {
              if (lasttime != _sometime) sametime = 1;
              else sametime = 0;
              lasttime = _sometime;
              statStream << "<?xml version=\"1.0\"?><group>"
	                 << "<name>dsm_server</name>"
                         << "<clock>" << cstr << "</clock>";
            }
            else
              statStream << "<?xml version=\"1.0\"?><group>"
	                 << "<name>dsm_server</name>"
                         << "<clock>no DSMs active...."+string(glyph[anim])+"</clock>";

//            // Send status at 00:00, 00:03, etc.
//            if ( ((ut % 3) == 0) && _sometime) {
//                statStream << "<status><![CDATA[";
//                statStream << "]]></status>";
//            }
            statStream << "</group>" << endl;
	    string statstr = statStream.str();
	    statStream.str("");
	    msock.sendto(statstr.c_str(),statstr.length()+1,0,msaddr);

            // sleep until the next interval...
            uSecVal =
	      uSecPeriod - (unsigned int)(getSystemTime() % uSecPeriod);
#ifdef USE_NANOSLEEP
            sleepTime.tv_sec = uSecVal / USECS_PER_SEC;
            sleepTime.tv_nsec = (uSecVal % USECS_PER_SEC) * NSECS_PER_USEC;
            if (nanosleep(&sleepTime,&leftTime) < 0) {
                cerr << "nanosleep error return" << endl;
                if (errno == EINTR) break;
                throw n_u::Exception(string("nanosleep: ") +
			n_u::Exception::errnoToString(errno));
            }
#else
            sleepTime.tv_sec = uSecVal / USECS_PER_SEC;
            sleepTime.tv_usec = uSecVal % USECS_PER_SEC;
            if (::select(0,0,0,0,&sleepTime) < 0) {
                if (errno == EINTR) return RUN_OK;
                cerr << "select error" << endl;
                throw n_u::Exception(string("select: ") +
                    n_u::Exception::errnoToString(errno));
            }
#endif
	}
    }
    catch(const n_u::IOException& e) {
	msock.close();
	throw e;
    }
    msock.close();
    return RUN_OK;
}
