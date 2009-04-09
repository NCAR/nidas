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
#include <nidas/core/Datagrams.h> // defines DSM_SERVER_XMLRPC_PORT_TCP

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
  XmlRpc::setVerbosity(1);

  // Create the server socket on the specified port
  if (!_xmlrpc_server->bindAndListen(DSM_SERVER_XMLRPC_PORT_TCP))
    throw n_u::IOException("XMLRpcPort","bind",errno);

  // Enable introspection
  _xmlrpc_server->enableIntrospection(true);

    // Wait for requests indefinitely

    // work(-1.0) does a select on the the rpc file descriptors
    // without a timeout, so if there are no rpc requests
    // coming in, then work(-1.0) will never finish, even if
    // you do a XmlRpcServer::exit() or XmlRpcServer::shutdown().
    // So if you do work(-1.0), you must use Thread::cancel()
    // (or send a signal), to break out of work(-1.0), because
    // select is a cancelation point.

    // Or one can do a work(1.0) which gives a 1 second timeout
    // to the select() in work, and you can loop here,
    // checking for isInterrupted(). Do Thread::interrupt()
    // to exit the loop.  The problem is, work() can return
    // instantly if there is nothing to do, and so this
    // could become an infinite loop.

// #define DO_XML_RPC_WORK_LOOP
#ifdef DO_XML_RPC_WORK_LOOP
    for (;;) {
        if (isInterrupted()) break;
        _xmlrpc_server->work(1.0);
    }
#else
    _xmlrpc_server->work(-1.0);
#endif

  return RUN_OK;
}
