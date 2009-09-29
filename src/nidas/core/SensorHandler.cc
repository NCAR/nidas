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

#include <cerrno>
#include <unistd.h>
#include <csignal>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SensorHandler::
SensorHandler(unsigned short rserialPort):Thread("SensorHandler"),
    _activeSensorFds(0),_activeSensors(0),
    _nActiveSensors(0),_nActiveSensorsAlloc(0),
    _sensorsChanged(false),
    _remoteSerialSocketPort(rserialPort), _rserial(0),
    _rserialConnsChanged(false), _selectn(0), _selectErrors(0),
    _rserialListenErrors(0), _opener(this)

{
    setSensorStatsInterval(5 * MSECS_PER_SEC);
    setSensorCheckIntervalMsecs(5 * MSECS_PER_SEC);

    FD_ZERO(&_readfdset);
    FD_ZERO(&_rcvdData);
    _sensorStatsTime = timeCeiling(getSystemTime(), _sensorStatsInterval);
    _sensorCheckTime = timeCeiling(getSystemTime(), getSensorCheckIntervalUsecs());
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);

    if (::pipe(_notifyPipe) < 0) {
        // Can't throw exception, but report it. Check at beginning
        // of thread that _notifyPipe[0,1] are OK
        n_u::IOException e("SensorHandler", "pipe", errno);
        n_u::Logger::getInstance()->log(LOG_WARNING, "%s", e.what());
        _notifyPipe[0] = -1;
        _notifyPipe[1] = -1;
    }
}

/**
 * Close any remaining sensors. Before this is called
 * the run method should be finished.
 */
SensorHandler::~SensorHandler()
{
    delete _rserial;
    for (unsigned int i = 0; i < _nActiveSensors; i++)
        _activeSensors[i]->close();

    list<DSMSensor*>::const_iterator si;
    for (si = _allSensors.begin(); si != _allSensors.end(); ++si)
        delete *si;

    if (_notifyPipe[0] >= 0)
        ::close(_notifyPipe[0]);
    if (_notifyPipe[1] >= 0)
        ::close(_notifyPipe[1]);
    delete [] _activeSensorFds;
    delete [] _activeSensors;
}

void SensorHandler::setSensorCheckIntervalMsecs(int val)
{
    _sensorCheckIntervalUsecs = val * USECS_PER_MSEC;
    _selectTimeoutVal.tv_sec = val / MSECS_PER_SEC;
    _selectTimeoutVal.tv_usec = (val % MSECS_PER_SEC) * USECS_PER_MSEC;
}

int SensorHandler::getSensorCheckIntervalMsecs() const
{
    return _sensorCheckIntervalUsecs / USECS_PER_MSEC;
}

int SensorHandler::getSensorCheckIntervalUsecs() const
{
    return _sensorCheckIntervalUsecs;
}

void SensorHandler::calcStatistics(dsm_time_t tnow)
{
    _sensorStatsTime += _sensorStatsInterval;
    if (_sensorStatsTime < tnow) {
        // cerr << "tnow-_sensorStatsTime=" << (tnow - _sensorStatsTime) << endl;
        _sensorStatsTime = timeCeiling(tnow, _sensorStatsInterval);
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
    _sensorCheckTime += getSensorCheckIntervalUsecs();
    if (_sensorCheckTime < tnow) {
        // cerr << "tnow-_sensorStatsTime=" << (tnow - _sensorStatsTime) << endl;
        _sensorCheckTime = timeCeiling(tnow, getSensorCheckIntervalUsecs());
    }
    for (unsigned int i = 0; i < _nActiveSensors; i++) {
        int fd = _activeSensorFds[i];
        if (!FD_ISSET(fd,&_rcvdData)) {
            if (++_noDataCounts[i] >= _noDataCountsMax[i]) {
                DSMSensor *sensor = _activeSensors[i];
                sensor->incrementTimeoutCount();
                if ((sensor->getTimeoutCount() % 10) == 0)
                    n_u::Logger::getInstance()->log(LOG_WARNING,
                    "timeout #%d: %s (%.3f sec)",
                        sensor->getTimeoutCount(),sensor->getName().c_str(),
                        (float)sensor->getTimeoutMsecs()/ MSECS_PER_SEC);
                closeReopenSensor(sensor);
            }
        }
        else _noDataCounts[i] = 0;
    }
    FD_ZERO(&_rcvdData);
}

/* returns a copy of our sensor list. */
list<DSMSensor*> SensorHandler::getAllSensors() const
{
    n_u::Synchronized autosync(_sensorsMutex);
    return _allSensors;
}

/* returns a copy of our opened sensors. */
list<DSMSensor*> SensorHandler::getOpenedSensors() const
{
    n_u::Synchronized autosync(_sensorsMutex);
    return _openedSensors;
}

/**
 * Thread function, select loop.
 */
int SensorHandler::run() throw(n_u::Exception)
{

    dsm_time_t rtime = 0;
    struct timeval tout;

    if (_notifyPipe[0] < 0 || _notifyPipe[1] < 0) {
        n_u::IOException e("SensorHandler cmd pipe", "pipe", EBADF);
        n_u::Logger::getInstance()->log(LOG_ERR, "%s", e.what());
        throw e;
    }

    _opener.start();

    delete _rserial;
    _rserial = 0;
    if (_remoteSerialSocketPort > 0) {
        try {
            _rserial = new RemoteSerialListener(_remoteSerialSocketPort);
        }
        catch(const n_u::IOException & e)
        {
            n_u::Logger::getInstance()->log(LOG_WARNING,
                                            "%s: continuing anyhow",
                                            e.what());
        }
    }

    unsigned int nsamplesAlloc = 0;
    _sensorsChanged = true;

    for (;;) {
        if (amInterrupted())
            break;

        if (_sensorsChanged || _rserialConnsChanged)
            handleChangedSensors();

        fd_set rset = _readfdset;
        fd_set eset = _readfdset;
        tout = _selectTimeoutVal;
        int nfdsel =::select(_selectn, &rset, 0, &eset, &tout);
        if (amInterrupted())
            break;

        rtime = getSystemTime();
        if (nfdsel <= 0) {      // select error
            if (nfdsel < 0) {
                // Create and report but don't throw IOException.
                // Likely this is a EINTR (interrupted) error, in which case
                // the signal handler has set the amInterrupted() flag, which
                // will be caught at the top of the loop.
                n_u::IOException e("SensorHandler", "select", errno);
                n_u::Logger::getInstance()->log(LOG_ERR, "%s",
                                                e.toString().c_str());
                _sensorsChanged = _rserialConnsChanged = true;
                if (errno != EINTR)
                    _selectErrors++;
            }
            if (rtime > _sensorCheckTime) checkTimeouts(rtime);
            if (rtime > _sensorStatsTime) calcStatistics(rtime);
            continue;
        }                       // end of select error section

        int nfd = 0;
        int fd = 0;

        // check sensor fds
        for (unsigned int ifd = 0; ifd < _nActiveSensors; ifd++) {
            fd = _activeSensorFds[ifd];
            if (FD_ISSET(fd, &rset)) {
                FD_SET(fd,&_rcvdData);
                DSMSensor *sensor = _activeSensors[ifd];
                try {
                    sensor->readSamples();
                }
                catch(n_u::IOException & ioe) {
                    n_u::Logger::getInstance()->log(LOG_ERR, "%s: %s",
                                                    sensor->getName().
                                                    c_str(),
                                                    ioe.toString().
                                                    c_str());
                    if (sensor->reopenOnIOException())
                        closeReopenSensor(sensor); // Try to reopen
                    else
                        closeSensor(sensor);    // report error but don't reopen
                }
                if (++nfd == nfdsel)
                    break;
            }
            // log the error but don't exit
            if (FD_ISSET(fd, &eset)) {
                DSMSensor *sensor = _activeSensors[ifd];
                n_u::Logger::getInstance()->log(LOG_ERR,
                                                "SensorHandler select reports exception for %s",
                                                sensor->getName().c_str());
                if (++nfd == nfdsel)
                    break;
            }
        }
        if (rtime > _sensorCheckTime) checkTimeouts(rtime);

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

        if (nfd == nfdsel)
            continue;

        list<RemoteSerialConnection*>::iterator ci;
        for (ci = _activeRserialConns.begin();
             ci != _activeRserialConns.end(); ++ci) {
            RemoteSerialConnection *conn = *ci;
            fd = conn->getFd();
            if (FD_ISSET(fd, &rset)) {
                try {
                    conn->read();
                }
                catch(n_u::EOFException & ioe) {
                    removeRemoteSerialConnection(conn);
                }
                // log the error but don't exit
                catch(n_u::IOException & ioe) {
                    n_u::Logger::getInstance()->log(LOG_INFO, "rserial: %s",
                                                    ioe.toString().
                                                    c_str());
                    removeRemoteSerialConnection(conn);
                }
                if (++nfd == nfdsel)
                    break;
            }
            if (FD_ISSET(fd, &eset)) {
                n_u::Logger::getInstance()->log(LOG_ERR, "%s",
                                                "SensorHandler select reports exception for rserial socket ");
                if (++nfd == nfdsel)
                    break;
            }
        }
        if (nfd == nfdsel)
            continue;
        if (_rserial) {
            fd = _rserial->getFd();
            if (FD_ISSET(fd, &rset)) {
                try {
                    RemoteSerialConnection *rsconn =
                        _rserial->acceptConnection();
                    addRemoteSerialConnection(rsconn);
                }
                catch(n_u::IOException & ioe) {
                    n_u::Logger::getInstance()->log(LOG_ERR,
                                                    "SensorHandler rserial: %s",
                                                    ioe.toString().
                                                    c_str());
                    _rserialListenErrors++;
                }
                if (++nfd == nfdsel)
                    continue;
            }
            if (FD_ISSET(fd, &eset)) {
                n_u::Logger::getInstance()->log(LOG_ERR, "%s",
                                                "SensorHandler select reports exception for rserial listen socket ");
                _rserialListenErrors++;
                if (++nfd == nfdsel)
                    continue;
            }
        }
        if (_notifyPipe[0] >= 0 && FD_ISSET(_notifyPipe[0], &rset)) {
            char z[4];
            if (::read(_notifyPipe[0], z, sizeof(z)) < 0) {
                n_u::IOException e("SensorHandler cmd pipe", "read",
                                   errno);
                n_u::Logger::getInstance()->log(LOG_ERR,
                                                "SensorHandler rserial: %s",
                                                e.what());
            }
            // Don't need to do anything, next loop will check
            // for _sensorsChanged.
            if (++nfd == nfdsel)
                continue;
        }
    }                           // select loop forever

    _rserialConnsMutex.lock();
    list<RemoteSerialConnection*> conns = _pendingRserialConns;
    _rserialConnsMutex.unlock();

    list<RemoteSerialConnection*>::iterator ci;
    for (ci = conns.begin(); ci != conns.end(); ++ci)
        removeRemoteSerialConnection(*ci);

    n_u::Logger::getInstance()->log(LOG_INFO,
            "SensorHandler finished, closing remaining %d sensors ",
            _nActiveSensors);

    list<DSMSensor*> tsensors = getOpenedSensors();

    list<DSMSensor*>::const_iterator si;
    for (si = tsensors.begin(); si != tsensors.end(); ++si)
        closeSensor(*si);

    handleChangedSensors();

    return RUN_OK;
}

/*
 * Interrupt this thread.  We catch this
 * interrupt so that we can pass it on the SensorOpener.
 */
void SensorHandler::interrupt()
{
    if (_opener.isRunning())
        _opener.interrupt();
    Thread::interrupt();
    // send a byte on the _notifyPipe to wake up select.
    if (_notifyPipe[1] >= 0)
        ::write(_notifyPipe[1], this, 1);
}

/*
 * Called from the main thread.
 */
void SensorHandler::addSensor(DSMSensor * sensor)
{
    _sensorsMutex.lock();
    _allSensors.push_back(sensor);
    _sensorsMutex.unlock();
    _opener.openSensor(sensor);

}

/*
 * Called from the SensorOpener thread indicating a sensor is
 * opened and ready.
 */
void SensorHandler::sensorOpen(DSMSensor * sensor)
{
    _sensorsMutex.lock();
    _openedSensors.push_back(sensor);
    _sensorsChanged = true;
    _sensorsMutex.unlock();

    // Write a byte on the _notifyPipe to wake up select.
    char dummy = 0;
    if (_notifyPipe[1] >= 0)
        ::write(_notifyPipe[1], &dummy, 1);
}

/*
 * Add DSMSensor to my list of DSMSensors to be closed, and not reopened.
 */
void SensorHandler::closeSensor(DSMSensor * sensor)
{

    _sensorsMutex.lock();
    list<DSMSensor*>::iterator si = find(_openedSensors.begin(),
                                             _openedSensors.end(), sensor);
    if (si != _openedSensors.end())
        _openedSensors.erase(si);

    _pendingSensorClosures.push_back(sensor);
    _sensorsMutex.unlock();
    _sensorsChanged = true;
    // Write a byte on the _notifyPipe to wake up select.
    char dummy = 0;
    if (_notifyPipe[1] >= 0)
        ::write(_notifyPipe[1], &dummy, 1);
}

/*
 * Close DSMSensor, then make request to SensorOpener thread that
 * it be reopened.
 */
void SensorHandler::closeReopenSensor(DSMSensor * sensor)
{

    try {
        sensor->close();
    }
    catch(n_u::IOException & e) {
        n_u::Logger::getInstance()->log(LOG_ERR, "%s: %s",
                                        sensor->getName().c_str(),
                                        e.toString().c_str());
    }
    _sensorsMutex.lock();
    list<DSMSensor*>::iterator si = find(_openedSensors.begin(),
                                             _openedSensors.end(), sensor);
    if (si != _openedSensors.end())
        _openedSensors.erase(si);
    _opener.reopenSensor(sensor);
    _sensorsChanged = true;
    _sensorsMutex.unlock();

    // Write a byte on the _notifyPipe to wake up select.
    char dummy = 0;
    if (_notifyPipe[1] >= 0)
        ::write(_notifyPipe[1], &dummy, 1);
}

/*
 * Protected method to add a RemoteSerial connection
 */
void SensorHandler::addRemoteSerialConnection(RemoteSerialConnection *
                                              conn) throw(n_u::IOException)
{
    conn->readSensorName();

    n_u::Synchronized autosync(_sensorsMutex);
    list<DSMSensor*>::const_iterator si;
    for (si = _openedSensors.begin(); si != _openedSensors.end(); ++si) {
        DSMSensor *sensor = *si;
        if (!sensor->getDeviceName().compare(conn->getSensorName())) {
            conn->setDSMSensor(sensor); // may throw n_u::IOException

            _rserialConnsMutex.lock();
            _pendingRserialConns.push_back(conn);
            _rserialConnsChanged = true;
            _rserialConnsMutex.unlock();

            n_u::Logger::getInstance()->log(LOG_NOTICE,
                                            "added rserial connection for device %s",
                                            conn->getSensorName().c_str());
            return;
        }
    }
    conn->sensorNotFound();
    n_u::Logger::getInstance()->log(LOG_WARNING,
                                    "SensorHandler::addRemoteSerialConnection: cannot find sensor %s",
                                    conn->getSensorName().c_str());
}

/*
 * Remove a RemoteSerialConnection from the current list.
 * This doesn't close or delete the connection, but puts
 * it in the _pendingRserialClosures list.
 */
void SensorHandler::removeRemoteSerialConnection(RemoteSerialConnection *
                                                 conn)
{
    n_u::Synchronized autosync(_rserialConnsMutex);
    list<RemoteSerialConnection*>::iterator ci;
    ci = find(_pendingRserialConns.begin(), _pendingRserialConns.end(),
              conn);

    if (ci != _pendingRserialConns.end())
        _pendingRserialConns.erase(ci);
    else
        n_u::Logger::getInstance()->log(LOG_WARNING, "%s",
                                        "SensorHandler::removeRemoteSerialConnection couldn't find connection for %s",
                                        conn->getSensorName().c_str());
    _pendingRserialClosures.push_back(conn);
    _rserialConnsChanged = true;
}

/* static */
bool SensorHandler::goodFd(int fd, const string & devname) throw()
{
    if (fd < 0)
        return false;
    struct stat statbuf;
    if (::fstat(fd, &statbuf) < 0) {
        n_u::IOException ioe(devname, "handleChangedSensors fstat", errno);
        n_u::Logger::getInstance()->log(LOG_INFO, "%s", ioe.what());
        return false;
    }
    return true;
}

void SensorHandler::handleChangedSensors()
{
    // cerr << "handleChangedSensors" << endl;
    unsigned int i;
    if (_sensorsChanged) {
        n_u::Synchronized autosync(_sensorsMutex);

        map<DSMSensor*,int> saveTimeoutCounts;
        for (unsigned int i = 0; i < _nActiveSensors; i++)
            saveTimeoutCounts[_activeSensors[i]] = _noDataCounts[i];

        _noDataCountsMax.clear();
        _noDataCounts.clear();

        int minTimeoutMsecs = 10 * MSECS_PER_SEC;

        vector<DSMSensor*> activeSensors;
        vector<int> activeSensorFds;

        // cerr << "handleChangedSensors, _openedSensors.size()" <<
        //      _openedSensors.size() << endl;
        list<DSMSensor*>::iterator si = _openedSensors.begin();
        for (; si != _openedSensors.end();) {
            DSMSensor *sensor = *si;
            if (goodFd(sensor->getReadFd(), sensor->getName())) {
                activeSensors.push_back(sensor);
                activeSensorFds.push_back(sensor->getReadFd());
                _noDataCountsMax.push_back(1);
                // will return 0 if this sensor wasn't active before
                _noDataCounts.push_back(saveTimeoutCounts[sensor]);
                int sto = sensor->getTimeoutMsecs();
                // For now, don't check more than once a second
                if (sto > 0 && sto < minTimeoutMsecs)
                    minTimeoutMsecs = std::max(sto,MSECS_PER_SEC);
                ++si;
            } else {
                si = _openedSensors.erase(si);
                _opener.reopenSensor(sensor);
            }
        }
        if (activeSensorFds.size() > _nActiveSensorsAlloc) {
            delete [] _activeSensors;
            delete [] _activeSensorFds;
            _nActiveSensorsAlloc = activeSensorFds.size() + 5;
            _activeSensors = new DSMSensor*[_nActiveSensorsAlloc];
            _activeSensorFds = new int[_nActiveSensorsAlloc];
        }
        _nActiveSensors = activeSensorFds.size();
        std::copy(activeSensors.begin(),activeSensors.end(),
            _activeSensors);
        std::copy(activeSensorFds.begin(),activeSensorFds.end(),
            _activeSensorFds);

        setSensorCheckIntervalMsecs(minTimeoutMsecs);

        // close any sensors
        for (si = _pendingSensorClosures.begin();
             si != _pendingSensorClosures.end(); ++si) {
            DSMSensor *sensor = *si;
            sensor->close();
        }
        _pendingSensorClosures.clear();
        _sensorsChanged = false;
        n_u::Logger::getInstance()->log(LOG_INFO, "%d active sensors",
                            _nActiveSensors);
    }

    if (_rserialConnsChanged) {
        n_u::Synchronized autosync(_rserialConnsMutex);
        _activeRserialConns = _pendingRserialConns;
        // close and delete any _pending remote serial connections
        list<RemoteSerialConnection*>::iterator ci =
            _pendingRserialClosures.begin();
        for (; ci != _pendingRserialClosures.end(); ++ci) {
            RemoteSerialConnection *conn = *ci;
            n_u::Logger::getInstance()->log(LOG_NOTICE,
                                            "closing rserial connection for device %s",
                                            conn->getSensorName().c_str());
            conn->close();
            delete conn;
        }
        _pendingRserialClosures.clear();
        _rserialConnsChanged = false;
    }

    _selectn = 0;
    FD_ZERO(&_readfdset);
    if (_notifyPipe[0] >= 0)
        FD_SET(_notifyPipe[0], &_readfdset);
    _selectn = std::max(_notifyPipe[0] + 1, _selectn);

    for (i = 0; i < _nActiveSensors; i++) {
        int fd = _activeSensorFds[i];
        FD_SET(fd, &_readfdset);
        _selectn = std::max(fd + 1, _selectn);
        int sto = _activeSensors[i]->getTimeoutMsecs();
        if (sto > 0) {
            _noDataCountsMax[i] = (sto + (sto / 2)) / getSensorCheckIntervalMsecs();
            if (_noDataCountsMax[i] < 1) _noDataCountsMax[i] = 1;
        }
        else
            _noDataCountsMax[i] = INT_MAX;
    }

    list<RemoteSerialConnection*>::iterator ci =
        _activeRserialConns.begin();
    for (; ci != _activeRserialConns.end(); ++ci) {
        RemoteSerialConnection *conn = *ci;
        if (goodFd(conn->getFd(), conn->getName())) {
            FD_SET(conn->getFd(), &_readfdset);
            _selectn = std::max(conn->getFd() + 1, _selectn);
        } else
            removeRemoteSerialConnection(conn);
    }

    if (_rserial) {
        int fd = _rserial->getFd();
        if (goodFd(fd, "rserial server socket")) {
            FD_SET(fd, &_readfdset);
            _selectn = std::max(fd + 1, _selectn);
        }
    }
}
