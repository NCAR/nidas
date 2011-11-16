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

#ifndef NIDAS_CORE_STATUSTHREAD_H
#define NIDAS_CORE_STATUSTHREAD_H

#include <nidas/util/SocketAddress.h>
#include <nidas/util/Thread.h>

namespace nidas { namespace core {

class DSMServer;

/**
 * A thread that runs periodically checking and multicasting
 * the status of a DSMEngine.
 */
class StatusThread: public nidas::util::Thread
{
public:
    /**
     * Constructor.
     */
    StatusThread(const std::string& name);

private:

    /** No copying. */
    StatusThread(const StatusThread&);

    /** No assignment. */
    StatusThread& operator=(const StatusThread&);
};

// ------------------

class DSMEngineStat: public StatusThread
{
public:
    DSMEngineStat(const std::string& name,const nidas::util::SocketAddress& saddr):
        StatusThread(name),_sockAddr(saddr.clone()) {};

    ~DSMEngineStat()
    {
        delete _sockAddr;
    }

    int run() throw(nidas::util::Exception);

private:
    nidas::util::SocketAddress* _sockAddr;

    /** No copying. */
    DSMEngineStat(const DSMEngineStat&);

    /** No assignment. */
    DSMEngineStat& operator=(const DSMEngineStat&);
};

class DSMServerStat: public StatusThread
{
public:
    
    DSMServerStat(const std::string& name,DSMServer* svr);

    int run() throw(nidas::util::Exception);

private:

    void setup();

    DSMServer* _server;

    /**
     * Wakeup period.
     */
    int _uSecPeriod;

    /** No copying. */
    DSMServerStat(const DSMServerStat&);

    /** No assignment. */
    DSMServerStat& operator=(const DSMServerStat&);
};

}}	// namespace nidas namespace core

#endif
