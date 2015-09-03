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
