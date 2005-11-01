/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/
#include <StatusListener.h>
#include <Datagrams.h>

#include <iostream>
#include <string>

using namespace dsm;
using namespace std;
using namespace atdUtil;

int main(int argc, char** argv)
{
  // start up the socket listener thread
  cerr << "StatusListener lstn();" << endl;
  StatusListener lstn;
  try {
    lstn.start();
  } catch (Exception& e) {
    cerr << "Exception: " << e.toString() << endl;

    // stop the socket listener thread
    lstn.cancel();
    lstn.join();
    return 0;
  }
  // Create an XMLRPC web service
  XmlRpc::XmlRpcServer* xmlrpc_server = new XmlRpc::XmlRpcServer;

  // These constructors register methods with the XMLRPC server
  GetClocks getclocks(xmlrpc_server, &lstn);
  GetStatus getstatus(xmlrpc_server, &lstn);

  // DEBUG - set verbosity of the xmlrpc server HIGH...
//   XmlRpc::setVerbosity(5);

  // Create the server socket on the specified port
  xmlrpc_server->bindAndListen(ADS_XMLRPC_STATUS_PORT);

  // Enable introspection
  xmlrpc_server->enableIntrospection(true);

  // Wait for requests indefinitely
  xmlrpc_server->work(-1.0);

  return 1;
}
