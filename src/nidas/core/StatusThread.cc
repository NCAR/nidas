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
#include <nidas/core/DSMServer.h>
#include <nidas/core/Datagrams.h>

#include <nidas/util/Socket.h>
#include <nidas/util/Exception.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

namespace {
	const int COMPLETE_STATUS_CNT = 3;
}

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

    std::ostringstream statStream;

    n_u::DatagramSocket dsock;

    struct timespec nsleep;

    try {
	for (;;) {
	    dsm_time_t tnow = getSystemTime();

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

	    dsm_time_t tt = getSystemTime();

            bool completeStatus = ((tt + USECS_PER_SEC/2)/USECS_PER_SEC % COMPLETE_STATUS_CNT) == 0;

            statStream << "<?xml version=\"1.0\"?><group>"
	               << "<name>" << dsm_name << "</name>"
                       << "<clock>" << n_u::UTime(tt).format(true,"%Y-%m-%d %H:%M:%S.%1f") << "</clock>";

	    // Send status at 00:00, 00:03, etc.
            if ( completeStatus && sensors.size() > 0) {

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
	    dsock.sendto(statstr.c_str(),statstr.length()+1,0,*_sockAddr);
	}
    }
    catch(const n_u::IOException& e) {
	dsock.close();
	throw e;
    }
    dsock.close();
    return 0;
}

DSMServerStat::DSMServerStat(const std::string& name,DSMServer* server):
	StatusThread(name),_server(server),_uSecPeriod(USECS_PER_SEC)
{
}

int DSMServerStat::run() throw(n_u::Exception)
{
    auto_ptr<n_u::SocketAddress> saddr(_server->getStatusSocketAddr().clone());
    n_u::Inet4Address mcaddr;

    if (saddr->getFamily() == AF_INET) {
    n_u::Inet4SocketAddress i4saddr =
	n_u::Inet4SocketAddress((const struct sockaddr_in*)
		saddr->getConstSockAddrPtr());
	mcaddr= i4saddr.getInet4Address();
    }

    auto_ptr<n_u::DatagramSocket> dsock;
    n_u::MulticastSocket* msock = 0;

// #define SEND_ALL_INTERFACES
#ifdef SEND_ALL_INTERFACES
    std::vector<n_u::Inet4NetworkInterface> ifaces;
#endif

    if (mcaddr.isMultiCastAddress()) {
	msock = new n_u::MulticastSocket();
        dsock.reset(msock);
	std::list<n_u::Inet4NetworkInterface> tmpifaces = msock->getInterfaces();
	std::list<n_u::Inet4NetworkInterface>::const_iterator ifacei = tmpifaces.begin();
	for ( ; ifacei != tmpifaces.end(); ++ifacei) {
	    n_u::Inet4NetworkInterface iface = *ifacei;
	    int flags = iface.getFlags();
	    if (flags & IFF_UP && flags & IFF_LOOPBACK) {
		ILOG(("DSMServerStat, setting interface on %s",
			iface.getAddress().getHostAddress().c_str()));
		msock->setInterface(mcaddr,iface);
	    }
#ifdef SEND_ALL_INTERFACES
            // also can check IFF_POINTOPOINT
	    if (flags & IFF_UP && flags & IFF_BROADCAST && flags & (IFF_MULTICASE | IFF_LOOPBACK))
		ifaces.push_back(iface);
#endif
	}
    }
    else
	dsock.reset(new n_u::DatagramSocket());

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
      _uSecPeriod - (unsigned int)(getSystemTime() % _uSecPeriod);

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

    dsm_time_t lasttime = getSystemTime();
    // const char *glyph[] = {"\\","|","/","-"};
    // int anim=0;
    //

    float deltat = _uSecPeriod / USECS_PER_SEC;
    const list<DSMService*>& svcs = _server->getServices();

    try {
        while (!amInterrupted()) {
            // sleep until the next interval...
            uSecVal =
	      _uSecPeriod - (unsigned int)(getSystemTime() % _uSecPeriod);
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
            dsm_time_t tt = getSystemTime();
            bool completeStatus = ((tt + USECS_PER_SEC/2)/USECS_PER_SEC % COMPLETE_STATUS_CNT) == 0;
            if (completeStatus) {
                deltat = (float)(tt - lasttime) / USECS_PER_SEC;
                lasttime = tt;
            }

            list<DSMService*>::const_iterator si = svcs.begin();
            for (int ni = 0; si != svcs.end(); ++si) {
                DSMService* svc = *si;
                std::ostringstream statStream;
		if (ni == 0)
		    statStream << "<?xml version=\"1.0\"?><group>"
			   << "<name>dsm_server" << "</name>";
		else
		    statStream << "<?xml version=\"1.0\"?><group>"
			   << "<name>dsm_server_" << ni << "</name>";

                ostream::pos_type pos1 = statStream.tellp();
                if (completeStatus) svc->printStatus(statStream,deltat);
                else svc->printClock(statStream);
                ostream::pos_type pos2 = statStream.tellp();

                statStream << "</group>" << endl;

                // cerr << "pos1=" << pos1 << " pos2=" << pos2 << endl;
                if (pos2 != pos1) {
                    string statstr = statStream.str();
#ifdef DEBUG
                    cerr << "####################################" << endl;
                    cerr << statstr;
                    cerr << "####################################" << endl;
#endif
		    try {
#ifdef SEND_ALL_INTERFACES
			// If multicast, loop over interfaces
			if (msock && ifaces.size() > 0) {
			    for (int i=0; i < ifaces.size(); i++) {
				Inet4NetworkInterface iface = ifaces[i];
				msock->setInterface(mcaddr,iface);
				dsock->sendto(statstr.c_str(),statstr.length()+1,0,*saddr);
			    }
			}
			else
#endif
			    dsock->sendto(statstr.c_str(),statstr.length()+1,0,*saddr);
			ni++;
		    }
		    catch(const n_u::IOException& e) {
			WLOG(("%s: %s",dsock->getLocalSocketAddress().toString().c_str(),
				e.what()));
		    }
                }
            }
	}
    }
    catch(const n_u::IOException& e) {
	dsock->close();
	throw e;
    }
    dsock->close();
    return RUN_OK;
}
