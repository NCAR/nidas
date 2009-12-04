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
    _quit(_xmlrpc_server),
    _getA2dSetup(_xmlrpc_server),
    _testVoltage(_xmlrpc_server)
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

void DSMEngineIntf::GetA2dSetup::execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
    throw()
{
    cerr << "params: " << params.toXml().c_str() << endl << endl;
    string device  = params[0]["device"];
    logger->log(LOG_NOTICE, "looking for %s", device.c_str());

    // find the sensor that matches the params argument
    DSMEngine *engine = DSMEngine::getInstance();
    const SensorHandler *selector = engine->getSensorHandler();
    list < DSMSensor * >sensors = selector->getOpenedSensors();
    DSMSensor *sensor = 0;

    if (sensors.size() > 0) {
        list < DSMSensor * >::const_iterator si;
        for (si = sensors.begin(); si != sensors.end(); ++si) {
            if ( ! (*si)->getDeviceName().compare(device) ) {
                logger->log(LOG_NOTICE, "found %s", device.c_str());
                sensor = *si;
                break;
            }
        }
    }
    if ( !sensor ) {
        string faultResp = "could not find " + device;
        logger->log(LOG_NOTICE, "%s", faultResp.c_str());
        throw XmlRpc::XmlRpcException(faultResp);
    }
    // extract the current channel setup
    ncar_a2d_setup setup;
    try {
        sensor->ioctl(NCAR_A2D_GET_SETUP, &setup, sizeof(setup));
    }
    catch(const nidas::util::IOException& ioe) {
        string faultResp = device + " did not respond to ioctl command: NCAR_A2D_GET_SETUP";
        logger->log(LOG_NOTICE, "%s", faultResp.c_str());
        throw XmlRpc::XmlRpcException(faultResp);
    }
    for (int i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
        result["gain"][i]   = setup.gain[i];
        result["offset"][i] = setup.offset[i];
        result["calset"][i] = setup.calset[i];
    }
    result["vcal"]      = setup.vcal;
    logger->log(LOG_NOTICE, "result: %s", result.toXml().c_str());
}

void DSMEngineIntf::TestVoltage::execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
    throw()
{
    cerr << "params: " << params.toXml().c_str() << endl << endl;

    string  device =               params[0]["device"];
    int    voltage = atoi( string( params[0]["voltage"] ).c_str() );
    int    channel = atoi( string( params[0]["channel"] ).c_str() );

    if ( (channel < 0) || (NUM_NCAR_A2D_CHANNELS < channel) ) {
        string faultResp = "invalid channel: " + string( params[0]["channel"] );
        logger->log(LOG_NOTICE, "%s", faultResp.c_str());
        throw XmlRpc::XmlRpcException(faultResp);
    }

    DSMEngine *engine = DSMEngine::getInstance();
    const SensorHandler *selector = engine->getSensorHandler();
    list < DSMSensor * >sensors = selector->getOpenedSensors();
    DSMSensor *sensor = 0;

    // find the sensor that matches what's passed in...
    if (sensors.size() > 0) {
        list < DSMSensor * >::const_iterator si;
        for (si = sensors.begin(); si != sensors.end(); ++si) {
            if ( ! (*si)->getDeviceName().compare(device) ) {
                sensor = *si;
                break;
            }
        }
    }
    if ( !sensor ) {
        string faultResp = "could not find " + device;
        logger->log(LOG_NOTICE, "%s", faultResp.c_str());
        throw XmlRpc::XmlRpcException(faultResp);
    }
    // extract the current channel setup
    ncar_a2d_setup setup;
    try {
        sensor->ioctl(NCAR_A2D_GET_SETUP, &setup, sizeof(setup));
    }
    catch(const nidas::util::IOException& ioe) {
        string faultResp = device + " did not respond to ioctl command: NCAR_A2D_GET_SETUP";
        logger->log(LOG_NOTICE, "%s", faultResp.c_str());
        throw XmlRpc::XmlRpcException(faultResp);
    }

    struct ncar_a2d_cal_config calConf;
    for (int i = 0; i < NUM_NCAR_A2D_CHANNELS; i++)
        calConf.calset[i] = setup.calset[i];
    if (voltage == 99) {
        calConf.calset[ channel ] = 0;
        calConf.vcal = setup.vcal;
    } else {
        calConf.calset[ channel ] = 1;
        calConf.vcal = voltage;
    }

    // change the calibration configuration
    try {
        sensor->ioctl(NCAR_A2D_SET_CAL, &calConf, sizeof(ncar_a2d_cal_config));
    }
    catch(const nidas::util::IOException& ioe) {
        string faultResp = device + " did not respond to ioctl command: NCAR_A2D_SET_CAL";
        logger->log(LOG_NOTICE, "%s", faultResp.c_str());
        throw XmlRpc::XmlRpcException(faultResp);
    }

    // TODO - generate a javascript response that refreshes the 'List_NCAR_A2Ds' display
    ostringstream ostr;
//  ostr << "<script>window.parent.recvList(";
//  ostr << "xmlrpc.XMLRPCMethod('xmlrpc.php?port=30003&method=List_NCAR_A2Ds', '');";
//  ostr << ");</script>";
    ostr << "<body>";
    ostr << "<br>setting channel: " << channel;
    ostr << " to " << calConf.vcal << " volts";
    ostr << "</body>";

    result = ostr.str();
    logger->log(LOG_NOTICE, "result: %s", result.toXml().c_str());
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
  _xmlrpc_server->work(-1.0);

  return RUN_OK;
}
