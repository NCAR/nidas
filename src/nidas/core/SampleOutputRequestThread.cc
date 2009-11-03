/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-06-25 11:42:06 -0600 (Thu, 25 Jun 2009) $

    $LastChangedRevision: 4698 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/SampleOutput.h $
 ********************************************************************
*/

#include <nidas/util/Logger.h>
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
        // if a ThreadJoiner is used, then SampleOutputRequestThread delete's itself
        if (_instance) _instance->interrupt();
        _instance->join();
        delete _instance;
        _instance = 0;
    }
}

SampleOutputRequestThread::SampleOutputRequestThread():
    Thread("SampleOutputRequestThread")
{
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);
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

#ifdef DEBUG
        cerr << "SampleOutputRequestThread, #curreqs=" <<
            curreqs.size() << " tdiffmin=" << tdiffmin << endl;
#endif

        // handle requests whose time has come
        for (ri = curreqs.begin() ; ri != curreqs.end(); ++ri) {
            ConnectRequest request = *ri;
            try {
                request._output->requestConnection(request._requester);
            }
            catch (const n_u::IOException& e) {
                ELOG(("%s: requestConnection: %s",request._output->getName().c_str(),
                    e.what()));
                addConnectRequest(request._output,request._requester,10);
            }
        }
        curreqs.clear();

        _requestCond.lock();

        // If there are postponed requests, do one second sleeps.
        // Between sleeps, check for arrival of new requests (or clear) 
        for ( ; nreq > 0 && _disconnectRequests.size() == 0 &&
            _connectRequests.size() == nreq && tdiffmin > 0; tdiffmin--) {
            cerr << "SampleOutputRequestThread sleeping, tdiffmin=" << tdiffmin << endl;
#ifdef DEBUG
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
