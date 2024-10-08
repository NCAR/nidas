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

#include "DSMServerIntf.h"
#include <nidas/linux/ncar_a2d.h>

#include "FileSet.h"
#include "Project.h"
#include "Site.h"
#include "DSMConfig.h"
#include "DSMSensor.h"
#include "Variable.h"
#include "DSMServer.h"
#include "CalFile.h"
#include "SocketAddrs.h" // defines DSM_SERVER_XMLRPC_PORT_TCP

// #include <nidas/util/Logger.h>

#include <dirent.h>
#include <iostream>

using namespace nidas::core;
using namespace std;
using namespace XmlRpc;

namespace n_u = nidas::util;

void GetDsmList::execute(XmlRpcValue&, XmlRpcValue& result)
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

void GetAdsFileName::execute(XmlRpcValue&, XmlRpcValue& result)
{
    DSMServer* server;
    if (!(server = _serverIntf->getDSMServer())) {
        // result[""] = string("<buzzoff/>");
        result = string("<buzzoff/>");
        return;
    }
    const Project *project = server->getProject();
    assert(project);

    list<nidas::core::FileSet*> fsets = project->findServerSampleOutputStreamFileSets();
    if (fsets.empty()) {
        n_u::Logger::getInstance()->log(LOG_ERR,
        "Cannot find a FileSet for 'acserver'");
        return;
    }
    nidas::core::FileSet *fset = fsets.front();

    // prune the path off
    string filename = fset->getCurrentName();
    size_t fn = filename.rfind("/");
    result = filename.substr(fn+1);
}

int DSMServerIntf::run()
{
    // Create an XMLRPC server
    _xmlrpc_server = new XmlRpcServer;

    // This constructor registers a method with the XMLRPC server
    GetDsmList       getdsmlist       (_xmlrpc_server,this);
    GetAdsFileName   getadsfilename   (_xmlrpc_server,this);

    // DEBUG - set verbosity of the xmlrpc server HIGH...
    XmlRpc::setVerbosity(1);

    // Create the server socket on the specified port
    if (!_xmlrpc_server->bindAndListen(DSM_SERVER_XMLRPC_PORT_TCP))
        throw n_u::IOException("XMLRpcPort", "bind", errno);

    // Enable introspection
    _xmlrpc_server->enableIntrospection(true);

    // Wait for requests indefinitely
    // This can be interrupted with a Thread::kill(SIGUSR1);
    DLOG(("entering XmlRpcServer::work(-1)"));
    _xmlrpc_server->work(-1.0);
    return RUN_OK;
}
