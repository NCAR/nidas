/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_CORE_ENGINEINTF_H
#define NIDAS_CORE_ENGINEINTF_H

#include <nidas/core/XmlRpcThread.h>
#include <nidas/util/IOException.h>
#include <xmlrpcpp/XmlRpcException.h>

#include <iostream>

namespace nidas { namespace core {

class DSMSensor;

/**
 * A thread that provides XML-based Remote Procedure Calls
 * to web interfaces from the DSMEngine.
 */
class DSMEngineIntf : public XmlRpcThread
{
public:
    DSMEngineIntf();

    int run() throw(nidas::util::Exception);

    /**
     * Register a sensor to have its executeXmlRpc() method called
     * when a "SensorAction" XmlRpc request comes in with a "device"
     * string parameter matching devname.
     */
    void registerSensor(const std::string& devname, DSMSensor* sensor)
    {
        _sensorAction.registerSensor(devname,sensor);
    }

private:

    /// Get the current state of the NCAR A2D card channels
    class GetA2dSetup : public XmlRpc::XmlRpcServerMethod
    {
    public:
        GetA2dSetup(XmlRpc::XmlRpcServer* s) : XmlRpc::XmlRpcServerMethod("GetA2dSetup", s) {}
        void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result);
        std::string help() { return std::string("help GetA2dSetup"); }
    };

    /// Set a test voltage on NCAR A2D board channel
    class TestVoltage : public XmlRpc::XmlRpcServerMethod
    {
    public:
        TestVoltage(XmlRpc::XmlRpcServer* s) : XmlRpc::XmlRpcServerMethod("TestVoltage", s) {}
        void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result);
        std::string help() { return std::string("help TestVoltage"); }
    };

    /**
     * Invoke an executeXmlRpc() method on a DSMSensor. The SensorAction::execute()
     * method will look for a DSMSensor which has registered with
     * DSMEngineIntf, with a name (typically a device name) matching the value
     * of params["device"].
     */
    class SensorAction : public XmlRpc::XmlRpcServerMethod
    {
    public:
        SensorAction(XmlRpc::XmlRpcServer* s) : XmlRpc::XmlRpcServerMethod("SensorAction", s) {}
        void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result)
            throw(XmlRpc::XmlRpcException,nidas::util::IOException);

        std::string help() { return std::string("parameter \"device\" should match a device name for a DSMSensor that has registered itself with DSMEngineIntf"); }

        void registerSensor(const std::string& devname, DSMSensor* sensor)
        {
            _nameToSensor[devname] = sensor;
        }

    private:
        std::map<std::string,DSMSensor*> _nameToSensor;
    };

    /// starts the DSMEngine
    class Start : public XmlRpc::XmlRpcServerMethod
    {
    public:
        Start(XmlRpc::XmlRpcServer* s) : XmlRpc::XmlRpcServerMethod("Start", s) {}
        void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result);
        std::string help() { return std::string("help Start"); }
    };

    /// stops the DSMEngine
    class Stop : public XmlRpc::XmlRpcServerMethod
    {
    public:
        Stop(XmlRpc::XmlRpcServer* s) : XmlRpc::XmlRpcServerMethod("Stop", s) {}
        void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result);
        std::string help() { return std::string("help Stop"); }
    };

    /// restarts the DSMEngine
    class Restart : public XmlRpc::XmlRpcServerMethod
    {
    public:
        Restart(XmlRpc::XmlRpcServer* s) : XmlRpc::XmlRpcServerMethod("Restart", s) {}
        void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result);
        std::string help() { return std::string("help Restart"); }
    };

    /// quits the DSMEngine
    class Quit : public XmlRpc::XmlRpcServerMethod
    {
    public:
        Quit(XmlRpc::XmlRpcServer* s) : XmlRpc::XmlRpcServerMethod("Quit", s) {}
        void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result);
        std::string help() { return std::string("help Quit"); }
    };

    XmlRpc::XmlRpcServer* _xmlrpc_server;
    Start _start;
    Stop _stop;
    Restart _restart;
    Quit _quit;
    SensorAction _sensorAction;
    GetA2dSetup _getA2dSetup;
    TestVoltage _testVoltage;
};

}}	// namespace nidas namespace core

#endif
