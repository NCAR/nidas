// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 Copyright 2005 UCAR, NCAR, All Rights Reserved

 $LastChangedDate$

 $LastChangedRevision$

 $LastChangedBy$

 $HeadURL$
 ********************************************************************
 */
#ifndef NIDAS_CORE_STATUSLISTENER_H
#define NIDAS_CORE_STATUSLISTENER_H

#include <xercesc/sax2/XMLReaderFactory.hpp>    // provides SAX2XMLReader
#include <xmlrpcpp/XmlRpc.h>

#include <nidas/util/Thread.h>

#include <iostream>             // cerr
#include <map>

namespace nidas { namespace core {

class StatusHandler;
class GetClocks;
class GetStatus;


/// thread that listens to multicast messages from all of the DSMs.
class StatusListener:public nidas::util::Thread
{
    friend class StatusHandler;
    friend class GetClocks;
    friend class GetStatus;

public:
    StatusListener();
    ~StatusListener();

    int run() throw(nidas::util::Exception);

private:
    /// this map contains the latest clock from each DSM
    std::map < std::string, std::string > _clocks;
    std::map < std::string, std::string > _oldclk;
    std::map < std::string, int >_nstale;

    /// this map contains the latest status message from each DSM
    std::map < std::string, std::string > _status;

    /// this map contains the latest sample pool message from each DSM
    std::map < std::string, std::string > _samplePool;

    /// SAX parser
    xercesc::SAX2XMLReader * _parser;

    /// SAX handler
    StatusHandler *_handler;

    /** No copying. */
    StatusListener(const StatusListener&);

    /** No assignment. */
    StatusListener& operator=(const StatusListener&);
};


/// gets a list of current clock times for each broadcasting DSM.
class GetClocks:public XmlRpc::XmlRpcServerMethod
{
public:
    GetClocks(XmlRpc::XmlRpcServer * s, StatusListener * lstn):
        XmlRpc::XmlRpcServerMethod("GetClocks", s), _listener(lstn)
    {
    }

    void execute(XmlRpc::XmlRpcValue & params,
            XmlRpc::XmlRpcValue & result);

    std::string help()
    {
        return std::string("help GetClocks");
    }

private:
    /// reference to listener thread
    StatusListener * _listener;

    /** No copying. */
    GetClocks(const GetClocks&);

    /** No assignment. */
    GetClocks& operator=(const GetClocks&);
};

/// gets a list of current status reports for each broadcasting DSM.
class GetStatus:public XmlRpc::XmlRpcServerMethod
{
public:
    GetStatus(XmlRpc::XmlRpcServer * s,
            StatusListener *
            lstn):XmlRpc::XmlRpcServerMethod("GetStatus", s),
    _listener(lstn)
    {
    }

    void execute(XmlRpc::XmlRpcValue & params,
            XmlRpc::XmlRpcValue & result);

    std::string help() {
        return std::string("help GetStatus");
    }

private:
    /// reference to listener thread
    StatusListener * _listener;

    /** No copying. */
    GetStatus(const GetStatus&);

    /** No assignment. */
    GetStatus& operator=(const GetStatus&);

};

}}  // namespace nidas namespace core

#endif
