/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/DSMEngineIntf.h>

#include <nidas/core/DSMEngine.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/SensorHandler.h>
#include <nidas/core/SocketAddrs.h> // defines DSM_XMLRPC_PORT_TCP
#include <nidas/core/SocketIODevice.h>

#include <nidas/util/Logger.h>
#include <nidas/linux/ncar_a2d.h>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

static n_u::Logger * logger = n_u::Logger::getInstance();

DSMEngineIntf::DSMEngineIntf(): XmlRpcThread("DSMEngineIntf"),
    _xmlrpc_server(new XmlRpc::XmlRpcServer),
    // These constructors register themselves with the XmlRpcServer
    _dsmAction(_xmlrpc_server),
    _sensorAction(_xmlrpc_server),
    _start(_xmlrpc_server),
    _stop(_xmlrpc_server),
    _restart(_xmlrpc_server),
    _quit(_xmlrpc_server)
{
}

void DSMEngineIntf::DSMAction::execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
    throw()
{
    string action = "unknown";
    if (params.getType() == XmlRpc::XmlRpcValue::TypeStruct)
        action = string(params["action"]);
    else if (params.getType() == XmlRpc::XmlRpcValue::TypeArray)
        action = string(params[0]["action"]);

    if (action == "quit") DSMEngine::getInstance()->quit();
    else if (action == "start") DSMEngine::getInstance()->start();
    else if (action == "stop") DSMEngine::getInstance()->stop();
    else if (action == "restart") DSMEngine::getInstance()->restart();
    else if (action == "reboot") DSMEngine::getInstance()->reboot();
    else if (action == "shutdown") DSMEngine::getInstance()->shutdown();
    else {
        string errmsg = string("XmlRpc error: DSMAction ") + action + " not supported";
        PLOG(("XmlRpc error: ") << errmsg);
        result = errmsg;
        return;
    }
    result = action + " requested";
}

void DSMEngineIntf::SensorAction::execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
    throw()
{
    // cerr << "params: " << params.toXml().c_str() << endl << endl;

    string devname = "unknown";
    if (params.getType() == XmlRpc::XmlRpcValue::TypeStruct)
        devname = string(params["device"]);
    else if (params.getType() == XmlRpc::XmlRpcValue::TypeArray)
        devname = string(params[0]["device"]);

    DSMSensor* sensor = _nameToSensor[devname];

    if (!sensor) {
        string errmsg = "sensor " + devname + " not found";
        PLOG(("XmlRpc error: ") << errmsg);
        result = errmsg;
        return;
    }
    sensor->executeXmlRpc(params,result);
}

void DSMEngineIntf::Start::execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
    throw()
{
  DSMEngine::getInstance()->start();
  result = "DSM started";
  cerr << &result << endl;
}

void DSMEngineIntf::Stop::execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
    throw()
{
  DSMEngine::getInstance()->stop();
  result = "DSM stopped";
  cerr << &result << endl;
}

void DSMEngineIntf::Restart::execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
    throw()
{
  DSMEngine::getInstance()->restart();
  result = "DSM restarted";
  cerr << &result << endl;
}

void DSMEngineIntf::Quit::execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
    throw()
{
  DSMEngine::getInstance()->quit();
  result = "DSM quit";
  cerr << &result << endl;
}

int DSMEngineIntf::run() throw(n_u::Exception)
{

  // DEBUG - set verbosity of the xmlrpc server
  XmlRpc::setVerbosity(3);

  // Create the server socket on the specified port
  _xmlrpc_server->bindAndListen(DSM_XMLRPC_PORT_TCP);

  // Enable introspection
  _xmlrpc_server->enableIntrospection(true);

  // Wait for requests indefinitely
  // This can be interrupted with a Thread::kill(SIGUSR1);
  _xmlrpc_server->work(-1.0);

  return RUN_OK;
}
