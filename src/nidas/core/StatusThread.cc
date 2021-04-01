/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "StatusThread.h"
#include "SensorHandler.h"
#include "DSMEngine.h"
#include "DSMServer.h"
#include "DSMService.h"
#include "DSMConfig.h"
#include "Datagrams.h"
#include "ChronyStatus.h"

#include <nidas/util/Socket.h>
#include <nidas/util/Logger.h>
#include <nidas/util/Exception.h>
#include <nidas/util/UTime.h>
#include <nidas/util/auto_ptr.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

namespace {
	const int COMPLETE_STATUS_CNT = 3;
	const int CHRONY_STATUS_CNT = 4;
}

StatusThread::StatusThread(const std::string& name):Thread(name)
{
    unblockSignal(SIGUSR1);
}

void StatusThread::sendStatus(n_u::DatagramSocket* dsock,
    n_u::SocketAddress* saddr,
    const string& statstr)
{
    dsock->sendto(statstr.c_str(), statstr.length()+1, 0, *saddr);
}

#ifdef SEND_ALL_INTERFACES
void StatusThread::sendStatus(n_u::MulticastSocket* msock,
    n_u::SocketAddress* saddr,
    n_u::Inet4Address& mcaddr,
    std::vector<n_u::Inet4NetworkInterface>& ifaces,
    const string& statstr)
{
    // If multicast, loop over interfaces
    if (!ifaces.empty()) {
        for (unsigned int i=0; i < ifaces.size(); i++) {
            n_u::Inet4NetworkInterface iface = ifaces[i];
            msock->setInterface(mcaddr,iface);
            msock->sendto(statstr.c_str(), statstr.length()+1, 0, *saddr);
        }
    }
    else
        msock->sendto(statstr.c_str(), statstr.length()+1, 0, *saddr);
}
#endif

int DSMEngineStat::run() throw(n_u::Exception)
{
    DSMEngine* engine = DSMEngine::getInstance();

    // const DSMConfig* dsm = engine->getDSMConfig();
    string dsm_name(engine->getDSMConfig()->getName());

    const SensorHandler* selector = engine->getSensorHandler();

    std::ostringstream statStream;

    n_u::DatagramSocket dsock;

    struct timespec nsleep;

    SamplePoolInterface* charPool = SamplePool<SampleT<char> >::getInstance();

    // arbitrary limit on number of errors, like "no route to host"
    // if link drops out for a bit
    const int MAX_SOCK_ERR = 20;

    for (int nerr = 0;;) {
        dsm_time_t tnow = n_u::getSystemTime();

        // wakeup (approx) 100 usecs after exact period time
        int tdiff = USECS_PER_SEC - (tnow % USECS_PER_SEC) + 100;

        nsleep.tv_sec = tdiff / USECS_PER_SEC;
        nsleep.tv_nsec = (tdiff % USECS_PER_SEC) * NSECS_PER_USEC;

        // previously this did a break if errno==EINTR
        // Apparently when a system is suspended, processes
        // receive a SIGSTOP and then SIGCONT on wakeup. On
        // receipt of these (likely the latter) nanosleep
        // would fail with errno=EINTR. We'll ignore it.
        nanosleep(&nsleep,0);
        if (isInterrupted()) break;

        dsm_time_t tt = n_u::getSystemTime();

        statStream << "<?xml version=\"1.0\"?><group>"
                   << "<name>" << dsm_name << "</name>"
                   << "<clock>" << n_u::UTime(tt).format(true,"%Y-%m-%d %H:%M:%S.%1f") << "</clock>";

        bool completeStatus = ((tt + USECS_PER_SEC / 2) / USECS_PER_SEC % COMPLETE_STATUS_CNT) == 0;
        // Send status at 00:00, 00:03, etc.
        if ( completeStatus ) {

            statStream << "<samplepool>" <<
                "#s=" << charPool->getNSmallSamplesIn() << ',' <<
                "#m=" << charPool->getNMediumSamplesIn() << ',' <<
                "#l=" << charPool->getNLargeSamplesIn() << ',' <<
                "#o=" << charPool->getNSamplesOut() <<
                "</samplepool>";

            // Make a copy of list of selector sensors
            std::list<DSMSensor*> sensors = selector->getAllSensors();
            std::list<DSMSensor*>::const_iterator si;
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

        try {
            sendStatus(&dsock, _sockAddr, statstr);
	}
        catch(const n_u::IOException& e) {
            nerr++;
            WLOG(("%s: %s, nerr=%d",dsock.getLocalSocketAddress().toAddressString().c_str(),
                    e.what(),nerr));
            if (nerr > MAX_SOCK_ERR) {
                dsock.close();
                throw e;
            }
        }
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
    n_u::auto_ptr<n_u::SocketAddress> saddr(_server->getStatusSocketAddr().clone());
    n_u::Inet4Address mcaddr;

    if (saddr->getFamily() == AF_INET) {
        n_u::Inet4SocketAddress i4saddr =
            n_u::Inet4SocketAddress((const struct sockaddr_in*)
		saddr->getConstSockAddrPtr());
	mcaddr= i4saddr.getInet4Address();
    }

    n_u::auto_ptr<n_u::DatagramSocket> dsock;
    n_u::MulticastSocket* msock = 0;

    std::vector<n_u::Inet4NetworkInterface> ifaces;

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
	    if (flags & IFF_UP && flags & IFF_BROADCAST && flags & (IFF_MULTICAST | IFF_LOOPBACK))
		ifaces.push_back(iface);
#endif
	}
    }
    else
	dsock.reset(new n_u::DatagramSocket());

    struct timespec sleepTime;

    /* sleep a bit so that we're on an even interval boundary */
    unsigned int uSecVal =
      _uSecPeriod - (unsigned int)(n_u::getSystemTime() % _uSecPeriod);

    sleepTime.tv_sec = uSecVal / USECS_PER_SEC;
    sleepTime.tv_nsec = (uSecVal % USECS_PER_SEC) * NSECS_PER_USEC;

    nanosleep(&sleepTime,0);

    dsm_time_t lasttime = n_u::getSystemTime();
    // const char *glyph[] = {"\\","|","/","-"};
    // int anim=0;
    //

    float deltat = _uSecPeriod / USECS_PER_SEC;
    const list<DSMService*>& svcs = _server->getServices();

    list<ChronyStatusNode*> ssnodes;

    for (SensorIterator si = _server->getSensorIterator(); si.hasNext(); ) {
        DSMSensor* sensor = si.next();
        ChronyStatusNode* node = dynamic_cast<ChronyStatusNode*>(sensor);
        if (node) ssnodes.push_back(node);
    }

    try {
        while (!amInterrupted()) {
            // sleep until the next interval...
            uSecVal =
	      _uSecPeriod - (unsigned int)(n_u::getSystemTime() % _uSecPeriod);
            sleepTime.tv_sec = uSecVal / USECS_PER_SEC;
            sleepTime.tv_nsec = (uSecVal % USECS_PER_SEC) * NSECS_PER_USEC;
            nanosleep(&sleepTime,0);

            dsm_time_t tt = n_u::getSystemTime();
            bool completeStatus = ((tt + USECS_PER_SEC / 2) / USECS_PER_SEC % COMPLETE_STATUS_CNT) == 0;
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
                    try {
#ifdef SEND_ALL_INTERFACES
                        if (msock)
                            sendStatus(msock, saddr.get(), mcaddr, ifaces, statStream.str());
                        else
#endif
                            sendStatus(dsock.get(), saddr.get(), statStream.str());
                        ni++;
                    }
                    catch(const n_u::IOException& e) {
                        WLOG(("%s: %s",dsock->getLocalSocketAddress().toAddressString().c_str(),
                                e.what()));
                    }
                }
            }

            bool chronyStatus = ((tt + USECS_PER_SEC / 2) / USECS_PER_SEC % CHRONY_STATUS_CNT) == 0;
            if (chronyStatus) {

                std::ostringstream statStream;
                statStream << "<?xml version=\"1.0\"?><group>"
                       << "<name>chrony</name>";
                statStream << "<status><![CDATA[";

                printChronyHeader(statStream);

                list<ChronyStatusNode*>::const_iterator ssi = ssnodes.begin();
                for ( ; ssi != ssnodes.end(); ++ssi) {
                    ChronyStatusNode* ssnode = *ssi;
                    std::ostringstream statStream;
                    ssnode->printChronyStatus(statStream);
                }

                printChronyTrailer(statStream);
                statStream << "]]></status>";
                statStream << "</group>" << endl;

                string statstr = statStream.str();
                try {
#ifdef SEND_ALL_INTERFACES
                    if (msock)
                        sendStatus(msock, saddr.get(), mcaddr, ifaces, statStream.str());
                    else
#endif
                        sendStatus(dsock.get(), saddr.get(), statStream.str());
                }
                catch(const n_u::IOException& e) {
                    WLOG(("%s: %s",dsock->getLocalSocketAddress().toAddressString().c_str(),
                            e.what()));
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

