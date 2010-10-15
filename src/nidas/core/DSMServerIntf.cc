/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/DSMServerIntf.h>
#include <nidas/linux/ncar_a2d.h>

#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/Variable.h>
#include <nidas/core/DSMServer.h>
#include <nidas/core/CalFile.h>
#include <nidas/core/SocketAddrs.h> // defines DSM_SERVER_XMLRPC_PORT_TCP

// #include <nidas/util/Logger.h>

#include <dirent.h>
#include <iostream>

using namespace nidas::core;
using namespace std;
using namespace XmlRpc;

namespace n_u = nidas::util;

void GetDsmList::execute(XmlRpcValue& params, XmlRpcValue& result)
{
    DSMServer* server;
    if (!(server = _serverIntf->getDSMServer())) {
        // result[""] = string("<buzzoff/>");
        result = string("<buzzoff/>");
        return;
    }
    const Project *project = server->getProject();
    assert(project);

    DSMConfigIterator di = project->getDSMConfigIterator();
    for ( ; di.hasNext(); ) {
        const DSMConfig *dsm = di.next();
        result[dsm->getName()] = dsm->getLocation();
    }
}

int DSMServerIntf::run() throw(n_u::Exception)
{
    // Create an XMLRPC server
    _xmlrpc_server = new XmlRpcServer;

    // This constructor registers a method with the XMLRPC server
    GetDsmList     getdsmlist   (_xmlrpc_server,this);

    // DEBUG - set verbosity of the xmlrpc server HIGH...
    XmlRpc::setVerbosity(5);

    // Create the server socket on the specified port
    if (!_xmlrpc_server->bindAndListen(DSM_SERVER_XMLRPC_PORT_TCP))
        throw n_u::IOException("XMLRpcPort", "bind", errno);

    // Enable introspection
    _xmlrpc_server->enableIntrospection(true);

    // Wait for requests indefinitely
    // This can be interrupted with a Thread::kill(SIGUSR1);
    _xmlrpc_server->work(-1.0);
    return RUN_OK;
}
