/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-06-25 11:42:06 -0600 (Thu, 25 Jun 2009) $

    $LastChangedRevision: 4698 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/SampleOutput.h $
 ********************************************************************
*/

#include <nidas/core/SampleOutputRequestThread.h>

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
        if (_instance) _instance->interrupt(); // delete's itself
        _instance = 0;
    }
}

SampleOutputRequestThread::SampleOutputRequestThread():
    Thread("SampleOutputRequestThread")
{
}

void SampleOutputRequestThread::addConnectRequest(SampleOutput* output,
    SampleConnectionRequester* requester,
    int delaySecs)
{
#ifdef DEBUG
    cerr << "addConnectRequest, output=" << output->getName() <<
        " delay=" << delaySecs << endl;
#endif
    ConnectRequest req(output,requester,::time(0) + delaySecs);
    _requestCond.lock();
    _connectRequests.push_back(req);
    _requestCond.unlock();
    _requestCond.signal();
}

void SampleOutputRequestThread::addDeleteRequest(SampleOutput* output)
{
    _requestCond.lock();
    _disconnectRequests.push_back(output);
    _requestCond.unlock();
    _requestCond.signal();
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
    _requestCond.signal();
}

int SampleOutputRequestThread::run() throw(nidas::util::Exception)
{
    int tdiffmin = 0;
    _requestCond.lock();
    for (;;) {

        // _requestCond is locked here at the top of this loop
        if (amInterrupted()) break;

        size_t nreq = _connectRequests.size() + _disconnectRequests.size();

        if (nreq == 0) {       // no requests, wait
            _requestCond.wait();
            continue;
        }
        // _requestCond is locked, nreq > 0

        // do disconnects
        list<SampleOutput*> curdis = _disconnectRequests;
        _disconnectRequests.clear();
        _requestCond.unlock();

        list<SampleOutput*>::iterator di = curdis.begin();
        for ( ; di != curdis.end(); ++di) {
            SampleOutput* output = *di;
            delete output;
        }

        _requestCond.lock();
        // make list of requests whose time has come, compute time to wait for others
        list<ConnectRequest> curreqs;
        list<ConnectRequest>::iterator ri = _connectRequests.begin();
        tdiffmin = 99999999;
        time_t now = ::time(0);
        for ( ; ri != _connectRequests.end(); ) {
            ConnectRequest request = *ri;
            if (request._when <= now) {
                curreqs.push_back(request);
                ri = _connectRequests.erase(ri);
                nreq--;
            }
            else {
                int tdiff = request._when - now;
                if (tdiff < tdiffmin) tdiffmin = tdiff;
                ++ri;
            }
        }
        _requestCond.unlock();

        cerr << "SampleOutputRequestThread, #curreqs=" <<
            curreqs.size() << " tdiffmin=" << tdiffmin << endl;
#ifdef DEBUG
#endif

        // handle requests whose time has come
        for (ri = curreqs.begin() ; ri != curreqs.end(); ++ri) {
            ConnectRequest request = *ri;
            request._output->requestConnection(request._requester);
        }
        curreqs.clear();

        _requestCond.lock();

        // If there are postponed requests, do one second sleeps.
        // Between sleeps, check for arrival of new requests (or clear) 
        for ( ; nreq > 0 && _disconnectRequests.size() == 0 &&
            _connectRequests.size() == nreq && tdiffmin > 0; tdiffmin--) {
            cerr << "SampleOutputRequestThread, tdiffmin=" << tdiffmin << endl;
#ifdef DEBUG
#endif
            if (amInterrupted()) break;
            _requestCond.unlock();
            ::sleep(1);
            _requestCond.lock();
        }
    }
    _requestCond.unlock();
    // clean up myself
    _instanceLock.lock();
    _instance = 0;
    _instanceLock.unlock();
    n_u::ThreadJoiner* joiner = new n_u::ThreadJoiner(this);
    joiner->start();
    return RUN_OK;
}
