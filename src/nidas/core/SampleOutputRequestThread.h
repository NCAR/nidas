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

