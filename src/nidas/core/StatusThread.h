// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

#ifndef NIDAS_CORE_STATUSTHREAD_H
#define NIDAS_CORE_STATUSTHREAD_H

#include <nidas/util/SocketAddress.h>
#include <nidas/util/Socket.h>
#include <nidas/util/Thread.h>

namespace nidas { namespace core {

class DSMServer;

/**
 * A thread that runs periodically checking and multicasting
 * the status of a DSMEngine.
 */
class StatusThread: public nidas::util::Thread
{
public:
    /**
     * Constructor.
     */
    StatusThread(const std::string& name);

// Define this if you want to multicast over all available multicast interfaces.
// #define SEND_ALL_INTERFACES

#ifdef SEND_ALL_INTERFACES
    /**
     *  Send string over MulticastSocket, with an option to send over
     *  multiple interfaces, which may be useful in some circumstances.
     *  See https://tldp.org/HOWTO/Multicast-HOWTO-2.html
     *      Interface selection.
     *      Hosts attached to more than one network should provide a way for
     *      applications to decide which network interface will be used to output
     *      the transmissions. If not specified, the kernel chooses a default one
     *      based on system administrator's configuration.
     */
    void sendStatus(nidas::util::MulticastSocket* msock,
        nidas::util::SocketAddress* saddr,
        nidas::util::Inet4Address& mcaddr,
        std::vector<nidas::util::Inet4NetworkInterface>& ifaces,
        const std::string& statstr);
#endif

    /**
     *  Send string over DatagramSocket.
     */
    void sendStatus(nidas::util::DatagramSocket* dsock,
        nidas::util::SocketAddress* saddr,
        const std::string& statstr);

private:

    /** No copying. */
    StatusThread(const StatusThread&);

    /** No assignment. */
    StatusThread& operator=(const StatusThread&);
};

// ------------------


/**
 * Thread which provides status in XML form from a dsm on a
 * datagram socket, to be read by the status_listener.
 *
 * The XML packet contains a top-level <group> element, which
 * encloses a <name> element with the dsm name, and <status>, <clock>
 * or <samplepool> elements.  For example:
 *
 *      <?xml version=\"1.0\"?><group>"
 *          <name>dsm303</name>
 *          <clock>
                current time formatted with "%Y-%m-%d %H:%M:%S.%1f"
 *          </clock>
 *          <samplepool>
 *             samplepool statistics
 *          </samplepool>
 *          <status><![CDATA[
 *             html table of status from DSM sensors
 *          ]]></status>
 *      </group>
 */

class DSMEngineStat: public StatusThread
{
public:
    DSMEngineStat(const std::string& name,const nidas::util::SocketAddress& saddr):
        StatusThread(name),_sockAddr(saddr.clone()) {};

    ~DSMEngineStat()
    {
        delete _sockAddr;
    }

    int run() throw(nidas::util::Exception);

private:
    nidas::util::SocketAddress* _sockAddr;

    /** No copying. */
    DSMEngineStat(const DSMEngineStat&);

    /** No assignment. */
    DSMEngineStat& operator=(const DSMEngineStat&);
};

/**
 * Thread which provides status from a dsm_server on a datagram socket.
 * in a similar way to DSMEngineStat.
 *
 * <clock> and <status> elements are sent.
 *
 * As with the DSMEngine, the <status> elements will be enclosed
 * in a <group>, with an associated <name>, for example:
 *
 *      <?xml version=\"1.0\"?><group>"
 *          <name>dsm_server</name>
 *          <clock>
                current time formatted with "%Y-%m-%d %H:%M:%S.%1f"
 *          </clock>
 *          <status><![CDATA[
 *              html table of dsm_server info
 *          ]]></status>
 *
 *          <name>chrony</name>
 *          <status><![CDATA[
 *              html table of chrony info
 *          ]]></status>
 *      </group>
 *
 * To generate the dsm_server status, printStatus() is called on all
 * DSMServices. Right now only RawSampleService generates any
 * output on its printStatus() method.
 *
 * For the chrony status, the run method creates a list
 * of those sensors that can be dynamic_cast'd to a ChronyStatusNode,
 * and periodically calls the printChronyStatus() method on those nodes.
 *
 * XMLRPC clients of status_listener then call the GetStatus method with
 * an argument of "dsm_server" for the dsm_server status, "chrony" to
 * receive the chrony status.
 */

class DSMServerStat: public StatusThread
{
public:
    
    DSMServerStat(const std::string& name,DSMServer* svr);

    int run() throw(nidas::util::Exception);

private:

    void setup();

    DSMServer* _server;

    /**
     * Wakeup period.
     */
    int _uSecPeriod;

    /** No copying. */
    DSMServerStat(const DSMServerStat&);

    /** No assignment. */
    DSMServerStat& operator=(const DSMServerStat&);
};

}}	// namespace nidas namespace core

#endif
