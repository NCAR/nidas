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

#include <nidas/core/Project.h>
#include <nidas/core/Datagrams.h> // defines ADS_XMLRPC_PORT

// #include <nidas/util/Logger.h>

#include <dirent.h>
#include <iostream>
#include <vector>

using namespace nidas::core;
using namespace std;
using namespace XmlRpc;

namespace n_u = nidas::util;

void GetDsmList::execute(XmlRpcValue& params, XmlRpcValue& result)
{
    DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
    for ( ; di.hasNext(); ) {
	const DSMConfig* dsm = di.next();
	result[dsm->getName()] = dsm->getLocation();
    }
    cerr << "GetDsmList::execute " << &result << endl;
}


int DSMServerIntf::run() throw(n_u::Exception)
{
  // Create an XMLRPC server
  _xmlrpc_server = new XmlRpcServer;

  // These constructors register methods with the XMLRPC server
  GetDsmList     getdsmlist     (_xmlrpc_server);

  // DEBUG - set verbosity of the xmlrpc server HIGH...
  XmlRpc::setVerbosity(5);

  // Create the server socket on the specified port
  _xmlrpc_server->bindAndListen(ADS_XMLRPC_PORT);

  // Enable introspection
  _xmlrpc_server->enableIntrospection(true);

  // Wait for requests indefinitely
  _xmlrpc_server->work(-1.0);

  return RUN_OK;
}
