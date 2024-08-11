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

#include <nidas/util/Logger.h>
#include "SampleOutputRequestThread.h"

#include <unistd.h> // sleep()

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
SampleOutputRequestThread* SampleOutputRequestThread::_instance = 0;

/* static */
n_u::Mutex SampleOutputRequestThread::_instanceLock;

/* static */
SampleOutputRequestThread* SampleOutputRequestThread::getInstance()
{
    if (!_instance) {
        n_u::Synchronized autosync(_instanceLock);
        if (!_instance) _instance = new SampleOutputRequestThread();
    }
    return _instance;
}

/* static */
void SampleOutputRequestThread::destroyInstance()
{
    if (_instance) {
        n_u::Synchronized autosync(_instanceLock);
        if (_instance) {
            _instance->interrupt();
            DLOG(("calling SampleOutputRequestThread::join"));
            try {
                _instance->join();
            }
            catch (const n_u::Exception& e) {
                WLOG(("SampleOutputRequestThread::join: %s",e.what()));
            }
            DLOG(("called SampleOutputRequestThread::join"));
        }
        delete _instance;
        _instance = 0;
    }
}

SampleOutputRequestThread::SampleOutputRequestThread():
    Thread("SampleOutputRequestThread"),
    _connectRequests(), _disconnectRequests(),_requestCond()
{
}

void SampleOutputRequestThread::addConnectRequest(SampleOutput* output,
    SampleConnectionRequester* requester,
    int delaySecs)
{
    VLOG(("") << "addConnectRequest, output=" << output->getName()
              << " delay=" << delaySecs);
    ConnectRequest req(output,requester,::time(0) + delaySecs);
    _requestCond.lock();
    _connectRequests.push_back(req);
    _requestCond.signal();
    _requestCond.unlock();
}

void SampleOutputRequestThread::addDeleteRequest(SampleOutput* output)
{
    _requestCond.lock();
    _disconnectRequests.push_back(output);
    _requestCond.signal();
    _requestCond.unlock();
}

void SampleOutputRequestThread::clear()
{
    _requestCond.lock();
    _connectRequests.clear();

    // do disconnects
    list<SampleOutput*> curdis = _disconnectRequests;
    _disconnectRequests.clear();
    _requestCond.unlock();

    list<SampleOutput*>::iterator di = curdis.begin();
    for ( ; di != curdis.end(); ++di) {
        SampleOutput* output = *di;
        delete output;
    }
}

void SampleOutputRequestThread::interrupt()
{
    Thread::interrupt();
    clear();
    _requestCond.lock();
    _requestCond.signal();
    _requestCond.unlock();
}

int SampleOutputRequestThread::run()
{
    int tdiffmin = 0;
    _requestCond.lock();
    for (;;) {

        // _requestCond is locked here at the top of this loop
        if (isInterrupted()) break;

        size_t nreq = _connectRequests.size() + _disconnectRequests.size();

        if (nreq == 0) {       // no requests, wait
            _requestCond.wait();
            continue;
        }
        // _requestCond is locked, nreq > 0

        // save a copy of the disconnect requests
        list<SampleOutput*> curdis = _disconnectRequests;
        _disconnectRequests.clear();

        // make list of requests whose time has come,
        // compute time to wait for others
        list<ConnectRequest> curreqs;
        list<ConnectRequest>::iterator ri = _connectRequests.begin();
        tdiffmin = 99999999;
        time_t now = ::time(0);
        for ( ; ri != _connectRequests.end(); ) {
            ConnectRequest request = *ri;
            if (request._when <= now) {
                curreqs.push_back(request);
                ri = _connectRequests.erase(ri);
            }
            else {
                int tdiff = request._when - now;
                if (tdiff < tdiffmin) tdiffmin = tdiff;
                ++ri;
            }
        }
        _requestCond.unlock();

#ifdef DEBUG
        cerr << "SampleOutputRequestThread, #curreqs=" <<
            curreqs.size() << " tdiffmin=" << tdiffmin << endl;
#endif

        // handle connection requests whose time has come
        for (ri = curreqs.begin() ; ri != curreqs.end(); ++ri) {
            ConnectRequest request = *ri;
            nreq--;
            try {
                request._output->requestConnection(request._requester);
            }
            catch (const n_u::IOException& e) {
                PLOG(("%s: requestConnection: %s",request._output->getName().c_str(),
                    e.what()));
                if (request._output->getReconnectDelaySecs() >= 0)
                    addConnectRequest(request._output,request._requester,
                            request._output->getReconnectDelaySecs());
            }
        }

        // do the disconnect requests
        list<SampleOutput*>::iterator di = curdis.begin();
        for ( ; di != curdis.end(); ++di) {
            SampleOutput* output = *di;
            delete output;
            nreq--;
        }

        _requestCond.lock();

        // If there are postponed requests, do one second sleeps.
        // Between sleeps, check for arrival of new requests (or clear) 
        for ( ; nreq > 0 && _disconnectRequests.size() == 0 &&
            _connectRequests.size() == nreq && tdiffmin > 0; tdiffmin--) {
#ifdef DEBUG
            cerr << "SampleOutputRequestThread sleeping, tdiffmin=" << tdiffmin << endl;
#endif
            if (amInterrupted()) break;
            _requestCond.unlock();
            ::sleep(1);
            _requestCond.lock();
        }
    }
    _requestCond.unlock();
    return RUN_OK;
}
