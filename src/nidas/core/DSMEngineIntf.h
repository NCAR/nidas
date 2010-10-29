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

    /**
     * Send a command to a DSMEngine. Expects one string parameter named "action",
     * containing a value of "start", "stop", "restart", "quit", "reboot", or "shutdown".
     */
    class DSMAction : public XmlRpc::XmlRpcServerMethod
    {
    public:
        DSMAction(XmlRpc::XmlRpcServer* s) : XmlRpc::XmlRpcServerMethod("DSMAction", s) {}
        void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result) throw();
        std::string help() { return std::string("parameter \"action\" should be \"start\", \"stop\", \"restart\", \"quit\", \"reboot\" or \"shutdown\""); }

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
        void execute(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result) throw();

        std::string help() { return std::string("parameter \"device\" should match a device name for a DSMSensor that has registered itself with DSMEngineIntf"); }

        void registerSensor(const std::string& devname, DSMSensor* sensor)
        {
            _nameToSensor[devname] = sensor;
        }

    private:
        std::map<std::string,DSMSensor*> _nameToSensor;
    };

    XmlRpc::XmlRpcServer* _xmlrpc_server;
    DSMAction _dsmAction;
    SensorAction _sensorAction;
};

}}	// namespace nidas namespace core

#endif
