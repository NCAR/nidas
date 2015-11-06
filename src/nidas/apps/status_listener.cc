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
#include <nidas/core/StatusListener.h>
#include <nidas/core/Datagrams.h>
#include <nidas/util/IOException.h>
#include <nidas/util/Logger.h>

#include <iostream>
#include <string>

#include <unistd.h>
#include <getopt.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

static int usage(const char* argv0)
{
    cerr << argv0 << " [-d] [-h]\n\
\n\
    -d: debug, don't fork to background, send error messages to stderr instead of syslog\n\
    -h: show this help message" << endl;
    return 1;
}

int main(int argc, char** argv)
{
    // extern char *optarg;       /* set by getopt() */
    // extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */
    bool debug = false;

    while ((opt_char = getopt(argc, argv, "dh")) != -1) {
	switch (opt_char) {
	case 'd':
	    debug = true;
	    break;
	case 'h':
	    return usage(argv[0]);
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }

    n_u::LogConfig lc;
    lc.level = n_u::LOGGER_DEBUG;
    if (!debug) {
        // fork to background, send stdout/stderr to /dev/null
        if (daemon(0,0) < 0) {
            n_u::IOException e("status_listener","daemon",errno);
            cerr << "Warning: " << e.toString() << endl;
        }
        n_u::Logger::createInstance("status_listener",LOG_CONS,LOG_LOCAL5);
        lc.level = n_u::LOGGER_INFO;
    }

    n_u::Logger::getInstance()->setScheme(
            n_u::LogScheme().addConfig (lc));

    // Thread that reads html status messages from a multicast socket.
    StatusListener lstn;

    // Create an XMLRPC service
    XmlRpc::XmlRpcServer* xmlrpc_server = new XmlRpc::XmlRpcServer;

    // These constructors register methods with the XMLRPC server
    GetClocks getclocks(xmlrpc_server, &lstn);
    GetStatus getstatus(xmlrpc_server, &lstn);

    // DEBUG - set verbosity of the xmlrpc server HIGH...
    //   XmlRpc::setVerbosity(5);

    // Create the server socket on the specified port
    if (!xmlrpc_server->bindAndListen(NIDAS_XMLRPC_STATUS_PORT_TCP)) {
        n_u::IOException e("XMLRPC status port","bind",errno);
        PLOG(("status_listener XMLRPC server:") << e.what());
        return 1;
    }

    // Enable introspection
    xmlrpc_server->enableIntrospection(true);

    // start up the socket listener thread
    try {
        lstn.start();
        DLOG(("StatusListener thread started"));
    } catch (n_u::Exception& e) {
        PLOG(("StatusListener start: ") << e.toString());
        return 1;
    }

    // Wait for XMLRPC requests indefinitely
    xmlrpc_server->work(-1.0);
    lstn.kill(SIGUSR1);
    lstn.join();

    return 0;
}
