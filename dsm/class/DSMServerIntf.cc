/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <DSMServerIntf.h>

#include <Project.h>
#include <Datagrams.h> // defines ADS_XMLRPC_PORT

// #include <atdUtil/Logger.h>

#include <dirent.h>
#include <iostream>
#include <vector>

using namespace dsm;
using namespace std;
using namespace XmlRpc;


void GetDsmList::execute(XmlRpcValue& params, XmlRpcValue& result)
{
    DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
    for ( ; di.hasNext(); ) {
	const DSMConfig* dsm = di.next();
	result[dsm->getName()] = dsm->getLocation();
    }
    cerr << "GetDsmList::execute " << &result << endl;
}


int DSMServerIntf::run() throw(atdUtil::Exception)
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
