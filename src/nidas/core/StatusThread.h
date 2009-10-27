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

#include <nidas/core/Sample.h>
#include <nidas/core/DSMServer.h>
#include <nidas/util/Thread.h>

#include <iostream> // cerr

namespace nidas { namespace core {

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

protected:
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
};

}}	// namespace nidas namespace core

#endif
