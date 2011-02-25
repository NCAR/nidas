/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_CORE_DSMSERVERINTF_H
#define NIDAS_CORE_DSMSERVERINTF_H

#include <iostream>
#include <nidas/core/XmlRpcThread.h>

namespace nidas { namespace core {

using namespace XmlRpc;

class DSMServer;

/**
 * A thread that provides XML-based Remote Procedure Calls
 * to web interfaces from the DSMServer.
 */
class DSMServerIntf : public XmlRpcThread
{
public:
    DSMServerIntf() : XmlRpcThread("DSMServerIntf"),_server(0) {}

    void setDSMServer(DSMServer* val)
    {
      _server = val;
    }

    /**
     * The DSMServer is valid once the Project document has been parsed
     * and an appropriate DSMServer is found. Otherwise this is NULL.
     */
    DSMServer* getDSMServer()
    {
      return _server;
    }

    int run() throw(nidas::util::Exception);

private:
    DSMServer* _server;

};

/// gets a list of DSMs and their locations from the configuration
class GetDsmList : public XmlRpcServerMethod
{
public:
    GetDsmList(XmlRpcServer* s,DSMServerIntf* intf) :
        XmlRpcServerMethod("GetDsmList", s),_serverIntf(intf) {}
    void execute(XmlRpcValue& params, XmlRpcValue& result);
    std::string help() { return std::string("help GetDsmList"); }
private:
    DSMServerIntf* _serverIntf;
};

/// gets the name of the current .ads file
class GetAdsFileName : public XmlRpcServerMethod
{
public:
    GetAdsFileName(XmlRpcServer* s,DSMServerIntf* intf) :
        XmlRpcServerMethod("GetAdsFileName", s),_serverIntf(intf) {}
    void execute(XmlRpcValue& params, XmlRpcValue& result);
    std::string help() { return std::string("help GetAdsFileName"); }
private:
    DSMServerIntf* _serverIntf;
};

}}	// namespace nidas namespace core

#endif
