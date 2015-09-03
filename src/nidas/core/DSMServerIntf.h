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

    /**
     * No copying.
     */
    DSMServerIntf(const DSMServerIntf&);

    /**
     * No assignment.
     */
    DSMServerIntf& operator=(const DSMServerIntf&);

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

    /**
     * No copying.
     */
    GetDsmList(const GetDsmList&);

    /**
     * No assignment.
     */
    GetDsmList& operator=(const GetDsmList&);

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

    /**
     * No copying.
     */
    GetAdsFileName(const GetAdsFileName&);

    /**
     * No assignment.
     */
    GetAdsFileName& operator=(const GetAdsFileName&);
};

}}	// namespace nidas namespace core

#endif
