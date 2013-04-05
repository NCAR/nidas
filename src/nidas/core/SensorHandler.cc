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

#include <nidas/core/SensorHandler.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <cerrno>
#include <unistd.h>
#include <csignal>

#include <sys/epoll.h>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SensorHandler::
SensorHandler(unsigned short rserialPort):Thread("SensorHandler"),
    _pollingMutex(),_allSensors(),_openedSensors(),_polledSensors(),
    _epollfd(-1), _events(0), _nevents(0),
    _newOpenedSensors(), _pendingSensorClosures(), _pollingChanged(false),
    _remoteSerialSocketPort(rserialPort), _rserial(0),
    _pendingRserialClosures(), _activeRserialConns(),
    _sensorCheckTime(0),_sensorStatsTime(0),
    _sensorCheckIntervalMsecs(-1),
    _sensorCheckIntervalUsecs(0),
    _sensorStatsInterval(0),
    _opener(this)
#ifndef HAS_EPOLL_PWAIT
    ,_notifyPipe(0)
#endif
{
    // block SIGUSR1. It will be atomically unblocked in epoll_pwait.
    blockSignal(SIGUSR1);

    setSensorStatsInterval(5 * MSECS_PER_SEC);
    _sensorStatsTime = n_u::timeCeiling(n_u::getSystemTime(), _sensorStatsInterval);
}

/**
 * Close any remaining sensors. Before this is called
 * the run method should be finished.
 */
SensorHandler::~SensorHandler()
{
    delete _rserial;
#ifndef HAS_EPOLL_PWAIT
    delete _notifyPipe;
#endif
    list<EpolledDSMSensor*>::const_iterator pi = _polledSensors.begin();
    for ( ; pi  != _polledSensors.end(); ++pi) {
        EpolledDSMSensor* psensor = *pi;
        psensor->close();
        delete psensor;
    }

    list<DSMSensor*>::const_iterator si;
    for (si = _allSensors.begin(); si != _allSensors.end(); ++si)
        delete *si;

    if (_epollfd >= 0) ::close(_epollfd);

    delete [] _events;
}

void SensorHandler::signalHandler(int sig, siginfo_t*)
{
    DLOG(("SensorHandler::signalHandler(), sig=%s (%d)",strsignal(sig),sig));
}

void SensorHandler::calcStatistics(dsm_time_t tnow)
{
    _sensorStatsTime += _sensorStatsInterval;
    if (_sensorStatsTime < tnow) {
        // cerr << "tnow-_sensorStatsTime=" << (tnow - _sensorStatsTime) << endl;
        _sensorStatsTime = n_u::timeCeiling(tnow, _sensorStatsInterval);
    }
    list<DSMSensor*> allCopy = getAllSensors();
    list<DSMSensor*>::const_iterator si;

    for (si = allCopy.begin(); si != allCopy.end(); ++si) {
        DSMSensor *sensor = *si;
        sensor->calcStatistics(_sensorStatsInterval);
    }
}

void SensorHandler::checkTimeouts(dsm_time_t tnow)
{
    _sensorCheckTime += _sensorCheckIntervalUsecs;
    if (_sensorCheckTime < tnow) {
        // cerr << "tnow-_sensorStatsTime=" << (tnow - _sensorStatsTime) << endl;
        _sensorCheckTime = n_u::timeCeiling(tnow, _sensorCheckIntervalUsecs);
    }

    // reading _polledSensors from my thread, no need for lock

    list<EpolledDSMSensor*> timedout;
    list<EpolledDSMSensor*>::iterator pi;

    // closeAndReopen(psensor) changes _polledSensors, so we have to
    // put the timed out sensors in another list.
    for (pi = _polledSensors.begin(); pi != _polledSensors.end(); ++pi ) {
        EpolledDSMSensor* psensor = *pi;
        if (psensor->checkTimeout()) timedout.push_back(psensor);
    }

    for (pi = timedout.begin(); pi != timedout.end(); ++pi ) {
        EpolledDSMSensor* psensor = *pi;
        closeAndReopen(psensor);
    }
}

/* returns a copy of our sensor list. */
list<DSMSensor*> SensorHandler::getAllSensors() const
{
    n_u::Synchronized autosync(_pollingMutex);
    return _allSensors;
}

/* returns a copy of our opened sensors. */
list<DSMSensor*> SensorHandler::getOpenedSensors() const
{
    n_u::Synchronized autosync(_pollingMutex);
    return _openedSensors;
}

SensorHandler::EpolledDSMSensor::EpolledDSMSensor(DSMSensor* sensor,
        SensorHandler* handler) throw(n_u::IOException): 
    _sensor(sensor),_handler(handler),
    _nTimeouts(0), _nTimeoutsMax(-1), _lastCheckInterval(0)
{
    epoll_event event;

#ifdef TEST_EDGE_TRIGGERED_EPOLL
    if (::fcntl(_sensor->getReadFd(),F_SETFL,O_NONBLOCK) < 0)
        throw n_u::IOException(getName(),"fcntl O_NONBLOCK",errno);
    event.events = EPOLLIN | EPOLLET;
#else
    event.events = EPOLLIN;
#endif

    event.data.ptr = (EpollFd*)this;

    if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_ADD,_sensor->getReadFd(),&event) < 0)
        throw n_u::IOException(getName(),"EPOLL_CTL_ADD",errno);
}

void SensorHandler::EpolledDSMSensor::close() throw(n_u::IOException)
{
    if (_sensor->getReadFd() >= 0) {
        if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_DEL,_sensor->getReadFd(),NULL) < 0) {
            n_u::IOException e(getName(),"EPOLL_CTL_DEL",errno);
            _sensor->close();
            throw e;
        }
    }
    _sensor->close();
}

void SensorHandler::EpolledDSMSensor::handleEpollEvents(uint32_t events) throw()
{
    if (events & EPOLLIN) {
        try {
            _sensor->readSamples();
            _nTimeouts = 0;
        }
        catch(n_u::IOException & ioe) {
            // report timeouts as a notice, not an error
            if (ioe.getErrno() == ETIMEDOUT)
                NLOG(("%s: %s",getName().c_str(), ioe.what()));
            else
                PLOG(("%s: %s", getName().c_str(), ioe.what()));
            if (_sensor->reopenOnIOException())
                _handler->closeAndReopen(this);
            else
                _handler->close(this);
            return;
        }
    }
#ifdef EPOLLRDHUP
    if (events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP))
#else
    if (events & (EPOLLERR | EPOLLHUP))
#endif
    {
#ifdef EPOLLRDHUP
        if (events & EPOLLRDHUP)
            NLOG(("%s: EPOLLRDHUP event", getName().c_str()));
#endif
        if (events & EPOLLERR)
            WLOG(("%s: EPOLLERR event", getName().c_str()));
        if (events & EPOLLHUP)
            NLOG(("%s: EPOLLHUP event", getName().c_str()));
        if (_sensor->reopenOnIOException())
            _handler->closeAndReopen(this); // Try to reopen
        else
            _handler->close(this);    // report error but don't reopen
    }
}
bool SensorHandler::EpolledDSMSensor::checkTimeout()
{
    if (_nTimeouts++ == _nTimeoutsMax) {
        _sensor->incrementTimeoutCount();
        if ((_sensor->getTimeoutCount() % 10) == 0)
            WLOG(("%s: timeout #%d (%.3f sec)",
                    getName().c_str(),
                    _sensor->getTimeoutCount(),
                    (float)_sensor->getTimeoutMsecs()/ MSECS_PER_SEC));
        _nTimeouts = 0;
        return true;
    }
    return false;
}

void SensorHandler::EpolledDSMSensor::setupTimeouts(int checkIntervalMsecs)
{
    if (checkIntervalMsecs <= 0) {  // not monitoring timeouts
        _nTimeouts = 0;
        _lastCheckInterval = checkIntervalMsecs;
        return;
    }

    if (checkIntervalMsecs == _lastCheckInterval) return;

    int sto = _sensor->getTimeoutMsecs();
    if (sto > 0) {
        _nTimeoutsMax = (sto + checkIntervalMsecs - 1) / checkIntervalMsecs;
        if (_nTimeoutsMax < 1) _nTimeoutsMax = 1;
    }
    else _nTimeoutsMax = -1;

    // if the new check interval is smaller than the last one
    if (checkIntervalMsecs < _lastCheckInterval)
        _nTimeouts *= (_lastCheckInterval / checkIntervalMsecs);
    else if (_lastCheckInterval > 0)
        _nTimeouts /= (checkIntervalMsecs / _lastCheckInterval);
    
    _lastCheckInterval = checkIntervalMsecs;
}

#ifndef HAS_EPOLL_PWAIT

SensorHandler::NotifyPipe::NotifyPipe(SensorHandler* handler)
    throw(n_u::IOException) : _fds(),_handler(handler)
{

   if (::pipe(_fds) < 0)
        throw n_u::IOException("SensorHandler", "pipe", errno);

    epoll_event event;

#ifdef TEST_EDGE_TRIGGERED_EPOLL
    if (::fcntl(_fds[0],F_SETFL,O_NONBLOCK) < 0)
        throw n_u::IOException("SensorHandler NotifyPipe","fcntl O_NONBLOCK",errno);
    event.events = EPOLLIN | EPOLLET;
#else
    event.events = EPOLLIN;
#endif

    event.data.ptr = (EpollFd*)this;

    if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_ADD,_fds[0],&event) < 0)
        throw n_u::IOException("SensorHandler::NotifyPipe","EPOLL_CTL_ADD",errno);
}

SensorHandler::NotifyPipe::~NotifyPipe()
{
    try {
        close();
    }
    catch (const n_u::IOException& e) {
        PLOG(("%s", e.what()));
    }
}

void SensorHandler::NotifyPipe::close()
{
    int fd0= _fds[0];
    _fds[0] = -1;
    int fd1= _fds[1];
    _fds[1] = -1;
    if (fd0 >= 0) {
        if (::epoll_ctl(_handler->getEpollFd(),EPOLL_CTL_DEL,fd0,NULL) < 0) {
            n_u::IOException e("SensorHandler::NotifyPipe","EPOLL_CTL_DEL",errno);
            ::close(fd0);
            if (fd1 >= 0) ::close(fd1);
            throw e;
        }
        if (::close(fd0) == -1) {
            if (fd1 >= 0) ::close(fd1);
            throw n_u::IOException("SensorHandler::NotifyPipe","close",errno);
        }
    }
    if (fd1 >= 0) ::close(fd1);
}

void SensorHandler::NotifyPipe::handleEpollEvents(uint32_t events) throw()
{
    if (events & EPOLLIN) {
        char buf[4];
        ssize_t l = read(_fds[0], buf, sizeof(buf));
        if (l < 0) {
            n_u::IOException e("SensorHandler::NotifyPipe","read",errno);
            PLOG(("%s",e.what()));
        }
    }
#ifdef EPOLLRDHUP
    if (events & (EPOLLRDHUP | EPOLLERR | EPOLLHUP))
#else
    if (events & (EPOLLERR | EPOLLHUP))
#endif
        PLOG(("%s", "SensorHandler::NotifyPipe epoll exception"));
}

void SensorHandler::NotifyPipe::notify() throw()
{
    if (::write(_fds[1],this,1) < 0) {
        n_u::IOException e("SensorHandler::NotifyPipe","write",errno);
        WLOG(("%s",e.what()));
    }
}


#endif

/**
 * Thread function, epoll loop.
 */
int SensorHandler::run() throw(n_u::Exception)
{

    dsm_time_t rtime = 0;

    if (_epollfd < 0) {
        _epollfd = epoll_create(10);
        if (_epollfd == -1)
            throw n_u::IOException("SensorHandler", "epoll_create", errno);
    }

#ifdef HAS_EPOLL_PWAIT
    // get the existing signal mask
    sigset_t sigmask;
    pthread_sigmask(SIG_BLOCK,NULL,&sigmask);
    // unblock SIGUSR1 in epoll_pwait
    sigdelset(&sigmask,SIGUSR1);
#else
    delete _notifyPipe;
    _notifyPipe = 0;
    _notifyPipe = new NotifyPipe(this);
#endif

    _opener.start();

    delete _rserial;
    _rserial = 0;
    if (_remoteSerialSocketPort > 0) {
        try {
            _rserial = new RemoteSerialListener(_remoteSerialSocketPort,this);
        }
        catch(const n_u::IOException & e)
        {
            n_u::Logger::getInstance()->log(LOG_WARNING,
                                            "%s: continuing anyhow",
                                            e.what());
        }
    }

    unsigned int nsamplesAlloc = 0;
    _pollingChanged = true;

    for (;!isInterrupted();) {

        if (_pollingChanged)
            handlePollingChange();

        // cerr << "_epollfd=" << _epollfd << ", _nevents=" << _nevents << endl;
#ifdef HAS_EPOLL_PWAIT
        int nfdpoll =::epoll_pwait(_epollfd, _events, _nevents,
                _sensorCheckIntervalMsecs,&sigmask);
#else
        int nfdpoll =::epoll_wait(_epollfd, _events, _nevents,
                _sensorCheckIntervalMsecs);
#endif

        rtime = n_u::getSystemTime();
        if (nfdpoll <= 0) {      // poll error or timeout
            if (nfdpoll < 0) {
                // When SIGHUP1 is sent to this thread, errno will be EINTR.
                // It is sent in order to interrupt the epoll_pwait, when either
                // the set of file descriptors to be polled has changed,
                // or the run method should be interrupted.
                if (errno == EINTR) continue;
                n_u::IOException e("SensorHandler", "epoll_wait", errno);
                PLOG(("%s",e.what()));
                break;
            }
            if (_sensorCheckIntervalMsecs > 0 && rtime > _sensorCheckTime)
                checkTimeouts(rtime);
            if (rtime > _sensorStatsTime) calcStatistics(rtime);
            continue;
        }

        struct epoll_event* event = _events;

        for (int ifd = 0; ifd < nfdpoll; ifd++,event++) {
            // EpollFd* efd = reinterpret_cast<EpollFd*>(event->data.ptr);
            EpollFd* efd = (EpollFd*)event->data.ptr;
            efd->handleEpollEvents(event->events);
        }

        if (_sensorCheckIntervalMsecs > 0 && rtime > _sensorCheckTime)
            checkTimeouts(rtime);

        if (rtime > _sensorStatsTime) {
            calcStatistics(rtime);

            // watch for sample memory leaks
            unsigned int nsamp = 0;
            list<SamplePoolInterface*> pools =
                SamplePools::getInstance()->getPools();
            for (list<SamplePoolInterface*>::const_iterator pi =
                 pools.begin(); pi != pools.end(); ++pi) {
                SamplePoolInterface *pool = *pi;
                nsamp += pool->getNSamplesAlloc();
            }
            if (nsamp > 20 && nsamp > (nsamplesAlloc + nsamplesAlloc / 2)) {
                for (list<SamplePoolInterface*>::const_iterator pi =
                     pools.begin(); pi != pools.end(); ++pi) {
                    SamplePoolInterface *pool = *pi;
                    n_u::Logger::getInstance()->log(LOG_INFO,
                        "pool nsamples alloc=%d, nsamples out=%d",
                        pool->getNSamplesAlloc(), pool->getNSamplesOut());
                }
                nsamplesAlloc = nsamp;
            }
        }
    }                           // poll loop until interrupt

    _pollingMutex.lock();
    list<RemoteSerialConnection*> conns = _activeRserialConns;
    _pollingMutex.unlock();

    list<RemoteSerialConnection*>::iterator ci;
    for (ci = conns.begin(); ci != conns.end(); ++ci)
        removeRemoteSerialConnection(*ci);

    list<DSMSensor*> tsensors = getOpenedSensors();

    // don't use list<>::size()
    int n = 0;
    list<DSMSensor*>::const_iterator si;
    for (si = tsensors.begin(); si != tsensors.end(); ++si,n++)
        closeSensor(*si);

    n_u::Logger::getInstance()->log(LOG_INFO,
            "SensorHandler finished, closing remaining %d sensors ",n);

    handlePollingChange();

    if (_opener.isRunning()) _opener.interrupt();

    if (_rserial) _rserial->close();

#ifndef HAS_EPOLL_PWAIT
    _notifyPipe->close();
#endif

    return RUN_OK;
}

/*
 * Interrupt this thread.  We catch this
 * interrupt so that we can pass it on the SensorOpener.
 */
void SensorHandler::interrupt()
{
    Thread::interrupt();
#ifdef HAS_EPOLL_PWAIT
    kill(SIGUSR1);
#else
    _notifyPipe->notify();
#endif
}

/*
 * Join this thread and join the SensorOpener.
 */
int SensorHandler::join() throw(nidas::util::Exception)
{
    if (!_opener.isJoined())
         _opener.join();
    int res = Thread::join();
    return res;
}

/*
 * Called from the main thread.
 */
void SensorHandler::addSensor(DSMSensor * sensor)
{
    _pollingMutex.lock();
    _allSensors.push_back(sensor);
    _pollingMutex.unlock();
    _opener.openSensor(sensor);

}

/*
 * Called from the SensorOpener thread indicating a sensor is
 * opened and ready.
 */
void SensorHandler::sensorOpen(DSMSensor * sensor)
{
    _pollingMutex.lock();
    _newOpenedSensors.push_back(sensor);
    _pollingChanged = true;
    _pollingMutex.unlock();

#ifdef HAS_EPOLL_PWAIT
    kill(SIGUSR1);
#else
    _notifyPipe->notify();
#endif
}

/*
 * Public method to add DSMSensor to the list of DSMSensors to be
 * closed later, and not reopened.
 */
void SensorHandler::closeSensor(DSMSensor * sensor)
{
    _pollingMutex.lock();
    _pendingSensorClosures.push_back(sensor);
    _pollingChanged = true;
    _pollingMutex.unlock();
#ifdef HAS_EPOLL_PWAIT
    kill(SIGUSR1);
#else
    _notifyPipe->notify();
#endif
}

/*
 * Private method to close a sensor.
 */
void SensorHandler::close(EpolledDSMSensor* psensor) throw()
{
    try {
        psensor->close();
    }
    catch(const n_u::IOException & e) {
        PLOG(("%s: %s", psensor->getName().c_str(), e.what()));
    }

    DSMSensor* sensor = psensor->getDSMSensor();

    _pollingMutex.lock();
    list<DSMSensor*>::iterator si;
    si = find(_openedSensors.begin(), _openedSensors.end(), sensor);
    assert(si != _openedSensors.end());
    _openedSensors.erase(si);

    list<EpolledDSMSensor*>::iterator pi;
    pi = find(_polledSensors.begin(), _polledSensors.end(), psensor);
    assert(pi != _polledSensors.end());
    _polledSensors.erase(pi);
    _pollingMutex.unlock();
    delete psensor;
}

/*
 * Private method to close a sensor.
 */
void SensorHandler::close(DSMSensor* sensor) throw()
{

    EpolledDSMSensor* psensor = 0;

    _pollingMutex.lock();
    list<EpolledDSMSensor*>::iterator pi = _polledSensors.begin();

    for ( ; pi != _polledSensors.end(); ++pi) {
        EpolledDSMSensor* ptmp = *pi;
        if (ptmp->getDSMSensor() == sensor) {
            psensor = ptmp;
            break;
        }
    }
    assert(psensor);
    _pollingMutex.unlock();

    close(psensor);
}

/*
 * Private method to close DSMSensor, then schedule it to be reopened.
 */
void SensorHandler::closeAndReopen(EpolledDSMSensor* psensor) throw()
{
    DSMSensor* sensor = psensor->getDSMSensor();

    close(psensor);

    _opener.reopenSensor(sensor);

#ifdef HAS_EPOLL_PWAIT
    kill(SIGUSR1);
#else
    _notifyPipe->notify();
#endif
}

/*
 * Protected method to add a RemoteSerial connection
 */
void SensorHandler::addRemoteSerialConnection(RemoteSerialConnection* conn) throw()
{
    try {
        conn->readSensorName();

        n_u::Synchronized autosync(_pollingMutex);
        list<DSMSensor*>::const_iterator si;
        for (si = _openedSensors.begin(); si != _openedSensors.end(); ++si) {
            DSMSensor *sensor = *si;
            if (sensor->getDeviceName() == conn->getSensorName()) {
                conn->setDSMSensor(sensor); // may throw n_u::IOException
                _activeRserialConns.push_back(conn);
                NLOG(("added rserial connection for device %s",
                                                conn->getSensorName().c_str()));
                return;
            }
        }
        conn->sensorNotFound();
    }
    catch(const n_u::IOException & e) {
        PLOG(("%s", e.what()));
    }
    try {
        conn->close();
    }
    catch(const n_u::IOException & e) {
        PLOG(("%s", e.what()));
    }
    delete conn;
}

/*
 * Remove a RemoteSerialConnection from the current list.
 * This doesn't close or delete the connection, but puts
 * it in the _pendingRserialClosures list.
 */
void SensorHandler::removeRemoteSerialConnection(RemoteSerialConnection *
                                                 conn)
{
    _pollingMutex.lock();
    _pendingRserialClosures.push_back(conn);
    _pollingChanged = true;
    _pollingMutex.unlock();
#ifdef HAS_EPOLL_PWAIT
    kill(SIGUSR1);
#else
    _notifyPipe->notify();
#endif
}

void SensorHandler::addPolledSensor(DSMSensor* sensor) throw()
{
    // _pollingMutex is locked prior to entering this method
    try {
        EpolledDSMSensor* psensor = new EpolledDSMSensor(sensor,this);
        _openedSensors.push_back(sensor);
        _polledSensors.push_back(psensor);
    }
    catch(const n_u::IOException & e) {
        PLOG(("%s: %s", sensor->getName().c_str(), e.what()));
    }
}

void SensorHandler::setupTimeouts(int sensorCheckIntervalMsecs)
{
    // reading _openedSensors from my thread, no need for lock
    list<EpolledDSMSensor*>::iterator pi;
    for (pi = _polledSensors.begin(); pi != _polledSensors.end(); ++pi ) {
        EpolledDSMSensor* psensor = *pi;
        psensor->setupTimeouts(sensorCheckIntervalMsecs);
    }
}

void SensorHandler::handlePollingChange()
{
    _pollingMutex.lock();
    bool changed = _pollingChanged;
    _pollingChanged = false;
    _pollingMutex.unlock();

    int nsensors = 0;

    if (changed) {

        _pollingMutex.lock();
        list<DSMSensor*> tmpsensors = _pendingSensorClosures;
        _pendingSensorClosures.clear();
        _pollingMutex.unlock();

        list<DSMSensor*>::iterator si = tmpsensors.begin();
        for (; si != tmpsensors.end(); ++si) {
            DSMSensor *sensor = *si;
            close(sensor);
        }

        _pollingMutex.lock();
        for (si = _newOpenedSensors.begin();
             si != _newOpenedSensors.end(); ++si) {
            DSMSensor *sensor = *si;
            addPolledSensor(sensor);
        }
        _newOpenedSensors.clear();
        _pollingMutex.unlock();

        unsigned int minTimeoutMsecs = UINT_MAX;

        // reading _openedSensors from my thread, no need for lock
        si = _openedSensors.begin();
        for (; si != _openedSensors.end(); ++si) {
            DSMSensor *sensor = *si;
            nsensors++;
            unsigned int sto = sensor->getTimeoutMsecs();
            // For now, don't check more than once a second
            if (sto > 0 && sto < minTimeoutMsecs)
                minTimeoutMsecs = std::max(sto,(unsigned int)MSECS_PER_SEC);
        }

        if (minTimeoutMsecs < UINT_MAX) {
            _sensorCheckIntervalMsecs = minTimeoutMsecs;
            _sensorCheckIntervalUsecs = _sensorCheckIntervalMsecs * USECS_PER_MSEC;
            _sensorCheckTime = n_u::timeCeiling(n_u::getSystemTime(), _sensorCheckIntervalUsecs);
        }
        else {
            _sensorCheckIntervalMsecs = -1;
            _sensorCheckIntervalUsecs = 0;
        }
        setupTimeouts(_sensorCheckIntervalMsecs);

        n_u::Logger::getInstance()->log(LOG_INFO, "%d active sensors",nsensors);

        _pollingMutex.lock();
        list<RemoteSerialConnection*> tmprserials = _pendingRserialClosures;
        _pendingRserialClosures.clear();
        _pollingMutex.unlock();

        list<RemoteSerialConnection*>::iterator ci =
            tmprserials.begin();
        for (; ci != tmprserials.end(); ++ci) {
            RemoteSerialConnection *conn = *ci;

            list<RemoteSerialConnection*>::iterator ci2;

            _pollingMutex.lock();
            ci2 = find(_activeRserialConns.begin(), _activeRserialConns.end(),conn);

            if (ci2 != _activeRserialConns.end())
                _activeRserialConns.erase(ci2);
            else
                WLOG(("%s",
                    "SensorHandler::removeRemoteSerialConnection couldn't find connection for %s",
                                    conn->getSensorName().c_str()));
            _pollingMutex.unlock();

            try {
                conn->close();
            }
            catch(const n_u::IOException & e) {
                PLOG(("%s", e.what()));
            }
            delete conn;
        }

        int nfds = 1 + nsensors;
        if (nfds > _nevents) {
            delete [] _events;
            _nevents = nfds + 5;
            _events = new epoll_event[_nevents];
        }
    }
}
