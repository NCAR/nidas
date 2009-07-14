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

#include <nidas/util/Logger.h>
#include <nidas/linux/ncar_a2d.h>

#include <iostream>

using namespace nidas::core;
using namespace std;
using namespace XmlRpc;

namespace n_u = nidas::util;

static n_u::Logger * logger = n_u::Logger::getInstance();

void GetA2dSetup::execute(XmlRpcValue& params, XmlRpcValue& result)
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
        throw XmlRpcException(faultResp);
    }
    // extract the current channel setup
    ncar_a2d_setup setup;
    try {
        sensor->ioctl(NCAR_A2D_GET_SETUP, &setup, sizeof(setup));
    }
    catch(const nidas::util::IOException& ioe) {
        string faultResp = device + " did not respond to ioctl command: NCAR_A2D_GET_SETUP";
        logger->log(LOG_NOTICE, "%s", faultResp.c_str());
        throw XmlRpcException(faultResp);
    }
    for (int i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
        result["gain"][i]   = setup.gain[i];
        result["offset"][i] = setup.offset[i];
        result["calset"][i] = setup.calset[i];
    }
    result["vcal"]      = setup.vcal;
    logger->log(LOG_NOTICE, "result: %s", result.toXml().c_str());
}

void TestVoltage::execute(XmlRpcValue& params, XmlRpcValue& result)
{
    cerr << "params: " << params.toXml().c_str() << endl << endl;

    string  device =               params[0]["device"];
    int    voltage = atoi( string( params[0]["voltage"] ).c_str() );
    int    channel = atoi( string( params[0]["channel"] ).c_str() );

    if ( (channel < 0) || (NUM_NCAR_A2D_CHANNELS < channel) ) {
        string faultResp = "invalid channel: " + string( params[0]["channel"] );
        logger->log(LOG_NOTICE, "%s", faultResp.c_str());
        throw XmlRpcException(faultResp);
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
        throw XmlRpcException(faultResp);
    }
    // extract the current channel setup
    ncar_a2d_setup setup;
    try {
        sensor->ioctl(NCAR_A2D_GET_SETUP, &setup, sizeof(setup));
    }
    catch(const nidas::util::IOException& ioe) {
        string faultResp = device + " did not respond to ioctl command: NCAR_A2D_GET_SETUP";
        logger->log(LOG_NOTICE, "%s", faultResp.c_str());
        throw XmlRpcException(faultResp);
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
        throw XmlRpcException(faultResp);
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

void Start::execute(XmlRpcValue& params, XmlRpcValue& result)
{
  DSMEngine::getInstance()->start();
  result = "DSM started";
  cerr << &result << endl;
}


void Stop::execute(XmlRpcValue& params, XmlRpcValue& result)
{
  DSMEngine::getInstance()->stop();
  result = "DSM stopped";
  cerr << &result << endl;
}


void Restart::execute(XmlRpcValue& params, XmlRpcValue& result)
{
  DSMEngine::getInstance()->restart();
  result = "DSM restarted";
  cerr << &result << endl;
}


void Quit::execute(XmlRpcValue& params, XmlRpcValue& result)
{
  DSMEngine::getInstance()->quit();
  result = "DSM quit";
  cerr << &result << endl;
}


int DSMEngineIntf::run() throw(n_u::Exception)
{
  // Create an XMLRPC server
  _xmlrpc_server = new XmlRpcServer;

  // These constructors register methods with the XMLRPC server
  GetA2dSetup      getA2dSetup      (_xmlrpc_server);
  TestVoltage      testvoltage      (_xmlrpc_server);
  Start            start            (_xmlrpc_server);
  Stop             stop             (_xmlrpc_server);
  Restart          restart          (_xmlrpc_server);
  Quit             quit             (_xmlrpc_server);

  // DEBUG - set verbosity of the xmlrpc server HIGH...
  XmlRpc::setVerbosity(5);

  // Create the server socket on the specified port
  _xmlrpc_server->bindAndListen(DSM_XMLRPC_PORT_TCP);

  // Enable introspection
  _xmlrpc_server->enableIntrospection(true);

  // Wait for requests indefinitely
  _xmlrpc_server->work(-1.0);

  return RUN_OK;
}
