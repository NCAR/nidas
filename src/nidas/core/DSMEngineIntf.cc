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

#include "DSMEngineIntf.h"

#include "DSMEngine.h"
#include "DSMSensor.h"
#include "SensorHandler.h"
#include "SocketAddrs.h" // defines DSM_XMLRPC_PORT_TCP
#include "SocketIODevice.h"

#include <nidas/util/Logger.h>

#include <iostream>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

DSMEngineIntf::DSMEngineIntf(): XmlRpcThread("DSMEngineIntf"),
    _xmlrpc_server(new XmlRpc::XmlRpcServer),
    // These constructors register themselves with the XmlRpcServer
    _dsmAction(_xmlrpc_server),
    _sensorAction(_xmlrpc_server)
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

    if      (action == "quit")     DSMEngine::getInstance()->quit();
    else if (action == "start")    DSMEngine::getInstance()->start();
    else if (action == "stop")     DSMEngine::getInstance()->stop();
    else if (action == "restart")  DSMEngine::getInstance()->restart();
    else if (action == "reboot")   DSMEngine::getInstance()->reboot();
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

int DSMEngineIntf::run() throw(n_u::Exception)
{

    // DEBUG - set verbosity of the xmlrpc server
    XmlRpc::setVerbosity(1);

    // Create the server socket on the specified port
    _xmlrpc_server->bindAndListen(DSM_XMLRPC_PORT_TCP);

    // Enable introspection
    _xmlrpc_server->enableIntrospection(true);

    // Wait for requests indefinitely
    // This can be interrupted with a Thread::kill(SIGUSR1);
    _xmlrpc_server->work(-1.0);

    return RUN_OK;
}
