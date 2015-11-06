// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_SAMPLEOUTPUTREQUESTTHREAD_H
#define NIDAS_CORE_SAMPLEOUTPUTREQUESTTHREAD_H

#include <list>

#include <nidas/core/SampleOutput.h>
#include <nidas/util/Thread.h>

namespace nidas { namespace core {

/**
 * Interface of an output stream of samples.
 */
class SampleOutputRequestThread: public nidas::util::Thread
{
public:

    /**
     * Return pointer to instance of singleton, creating instance if
     * necessary.
     */
    static SampleOutputRequestThread* getInstance();

    /**
     * Singleton destructor
     */
    static void destroyInstance();

    int run() throw(nidas::util::Exception);

    /**
     * Interrupt the thread. It will delete itself.
     */
    void interrupt();

    /**
     * Clear all current requests.
     */
    void clear();

    /**
     * Add an connect request of a SampleOutput.
     */
    void addConnectRequest(SampleOutput*,SampleConnectionRequester*,int delaySecs);

    /**
     * Request that SampleOutputRequestThread delete the output when
     * it can. SampleOutputRequestThread owns the pointer after this call.
     */
    void addDeleteRequest(SampleOutput*);

    class ConnectRequest {
    public:
        ConnectRequest(SampleOutput* o, SampleConnectionRequester* r, time_t when):
            _output(o),_requester(r),_when(when) 
        {
        }
        SampleOutput* _output;
        SampleConnectionRequester* _requester;
        time_t _when;
    };

private:

    SampleOutputRequestThread();

    ~SampleOutputRequestThread() {}

    static SampleOutputRequestThread * _instance;

    static nidas::util::Mutex _instanceLock;

    std::list<ConnectRequest> _connectRequests;

    std::list<SampleOutput*> _disconnectRequests;

    nidas::util::Cond _requestCond;

};

}}	// namespace nidas namespace core

#endif

