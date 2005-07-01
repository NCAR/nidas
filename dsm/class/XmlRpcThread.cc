/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <XmlRpcThread.h>
#include <iostream>
#include <DSMEngine.h>
#include <Datagrams.h>

using namespace dsm;
using namespace std;
using namespace XmlRpc;

// XMLRPC command to start a DSM
class Start : public XmlRpcServerMethod
{
public:
  Start(XmlRpcServer* s) : XmlRpcServerMethod("start", s) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result)
  {
    DSMEngine::getInstance()->mainStart();
    result = "DSM says start";
    cerr << result << endl;
  }

  std::string help() { return std::string("Say help start"); }
};

// XMLRPC command to stop a DSM
class Stop : public XmlRpcServerMethod
{
public:
  Stop(XmlRpcServer* s) : XmlRpcServerMethod("stop", s) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result)
  {
    DSMEngine::getInstance()->mainStop();
    result = "DSM says stop";
    cerr << result << endl;
  }

  std::string help() { return std::string("Say help stop"); }
};

// XMLRPC command to restart a DSM
class Restart : public XmlRpcServerMethod
{
public:
  Restart(XmlRpcServer* s) : XmlRpcServerMethod("restart", s) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result)
  {
    DSMEngine::getInstance()->mainRestart();
    result = "DSM says restart";
    cerr << result << endl;
  }

  std::string help() { return std::string("Say help restart"); }
};

// XMLRPC command to quit a DSM
class Quit : public XmlRpcServerMethod
{
public:
  Quit(XmlRpcServer* s) : XmlRpcServerMethod("quit", s) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result)
  {
    DSMEngine::getInstance()->mainQuit();
    result = "DSM says quit";
    cerr << result << endl;
  }

  std::string help() { return std::string("Say help quit"); }
};

// constructor
XmlRpcThread::XmlRpcThread(const std::string& name):
  Thread(name), xmlrpc_server(0)
{
  blockSignal(SIGINT);
  blockSignal(SIGHUP);
  blockSignal(SIGTERM);
}

// destructor
XmlRpcThread::~XmlRpcThread()
{
  if (isRunning()) {
    cancel();
    join();
  }
  delete xmlrpc_server;
}

// thread run method
int XmlRpcThread::run() throw(atdUtil::Exception)
{
  // Create an XMLRPC server
  xmlrpc_server = new XmlRpcServer;

  // These constructors register methods with the XMLRPC server
  Start     start    (xmlrpc_server);
  Stop      stop     (xmlrpc_server);
  Restart   restart  (xmlrpc_server);
  Quit      quit     (xmlrpc_server);

  // DEBUG - set verbosity of the xmlrpc server HIGH...
  XmlRpc::setVerbosity(5);

  // Create the server socket on the specified port
  xmlrpc_server->bindAndListen(DSM_XMLRPC_PORT);

  // Enable introspection
  xmlrpc_server->enableIntrospection(true);

  // Wait for requests indefinitely
  xmlrpc_server->work(-1.0);

  return 0;
}
