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

#ifndef NIDAS_CORE_PORTSELECTOR_H
#define NIDAS_CORE_PORTSELECTOR_H

#include <nidas/core/DSMSensor.h>
#include <nidas/core/SensorOpener.h>
#include <nidas/core/RemoteSerialListener.h>
#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/IOException.h>

#include <sys/time.h>
#include <sys/epoll.h>

#include <vector>
#include <set>

namespace nidas { namespace core {

/**
 * SensorHandler implements a DSMSensor event loop. It does a
 * epoll_wait() system call on the file descriptors of one or more
 * DSMSensors, and calls their readSample methods when data is
 * available to be read.  The epoll loop is implemented in
 * the Thread::run method of the SensorHandler.
 *
 * epoll also detects connections to RemoteSerialListener.
 * Once is socket connection is established for a given sensor, then
 * data is then passed back and forth between the socket
 * connection and the DSMSensor.  This path is separate
 * from the normal Sample data path.  It allows remote
 * direct control of serial sensors.
 */
class SensorHandler:public nidas::util::Thread
{
public:

    /**
     * Constructor.
     * @param rserialPort TCP socket port to listen for incoming
     *		requests to the rserial service. 0=don't listen.
     */
    SensorHandler(unsigned short rserialPort = 0);
    ~SensorHandler();

    /**
     * Override default implementation of Thread::signalHandler().
     */
    void signalHandler(int sig, siginfo_t*);

    /**
     * Add an unopened sensor to the SensorHandler. SensorHandler
     * will then own the DSMSensor.
     */
    void addSensor(DSMSensor * sensor);

    /**
     * Request that SensorHandler close the sensor.
     */
    void closeSensor(DSMSensor * sensor);

    /**
     * After SensorOpener has opened the sensor, it will
     * notify SensorHandler via this method that the
     * sensor is open.
     */
    void sensorOpen(DSMSensor * sensor);

    void addRemoteSerialConnection(RemoteSerialConnection *) throw();

    void removeRemoteSerialConnection(RemoteSerialConnection *);

    /**
     * Check on each sensor. Currently this means checking
     * whether a timeout has occured and calculating
     * statistics on the data received from the sensor.
     */
    void checkSensors(dsm_time_t);

    /**
     * Set the sensor statistics calculation period.
     *
     * @param val Period, in milliseconds.
     *
     */
    void setSensorStatsInterval(int val)
    {
        _sensorStatsInterval = val * USECS_PER_MSEC;
    }
    /**
     * Get the sensor check period.
     * @return Period, in milliseconds.
     */
    int getSensorStatsInterval() const
    {
        return _sensorStatsInterval / USECS_PER_MSEC;
    }

    void handleRemoteSerial(int fd, DSMSensor * sensor)
     throw(nidas::util::IOException);

    /**
     * Thread function.
     */
    int run() throw(nidas::util::Exception);

    std::list<DSMSensor*> getAllSensors() const;

    std::list<DSMSensor*> getOpenedSensors() const;

    /**
     * Interrupt polling.
     */
    void interrupt();

    /**
     * Join this thread and join the SensorOpener.
     */
    int join() throw(nidas::util::Exception);

    int getEpollFd() const { return _epollfd; }

private:

    class EpolledDSMSensor: public EpollFd 
    {
    public:
        EpolledDSMSensor(DSMSensor* sensor,SensorHandler* handler)
            throw(nidas::util::IOException);

        /**
         * Destructor does not close().
         */
        ~EpolledDSMSensor() {}

        void handleEpollEvents(uint32_t events) throw();

        DSMSensor* getDSMSensor() { return _sensor; }

        const std::string getName() const { return _sensor->getName(); }

        /**
         * SensorHandler implements a fairly crude way to detect
         * timeouts on data read from sensors. It determines the
         * minimum timeout value for the currently opened sensors.
         * This value, but not less than 1000 milliseconds, is the parameter
         * msecs, which is passed here to each opened sensor
         * from time to time as the group of sensors changes.
         *
         * In this simple implementation, this method sets the value
         * of _nTimeoutsMax to:
         * (int)(DSMSensor::getTimeoutMsecs() + msecs -1)/msecs
         */
        void setupTimeouts(int msecs);

        /**
         * The SensorHandler will call this method of each opened
         * sensor at the interval specified previously in the call to
         * setupTimeouts(int), or perhaps somewhat less often, due
         * to normal overhead. 
         * If handleEpollEvents(events) with an event mask of EPOLLEDIN
         * has not bee called since the last call to checkTimeout,
         * then increment _nTimeouts.
         * @return: true if _nTimeouts is equal to _nTimeoutsMax.
         *  false otherwise.
         */
        bool checkTimeout();

        /**
         * Remove this DSMSensor from those being polled, 
         * then call its close() method.
         */
        void close() throw(nidas::util::IOException);

    private:
        DSMSensor* _sensor;

        SensorHandler* _handler;

        int _nTimeouts;

        int _nTimeoutsMax;

        int _lastCheckInterval;

        // no copying
        EpolledDSMSensor(const EpolledDSMSensor&);

        // no assignment
        EpolledDSMSensor& operator = (const EpolledDSMSensor&);

    };

    // friend class EpolledDSMSensor;

#ifndef HAS_EPOLL_PWAIT
    class NotifyPipe: public EpollFd
    {
    public:
        NotifyPipe(SensorHandler* handler) throw(nidas::util::IOException);

        ~NotifyPipe();

        void handleEpollEvents(uint32_t events) throw();

        void close();

        void notify() throw();

    private:
        int _fds[2];
        SensorHandler* _handler;

        // no copying
        NotifyPipe(const NotifyPipe&);

        // no assignment
        NotifyPipe& operator = (const NotifyPipe&);
    };
#endif

    void addPolledSensor(DSMSensor* sensor) throw();

    /**
     * Close the EpolledDSMSensor, which removes it from the
     * polling list, then delete it. The associated
     * DSMSensor will be removed from _openedSensors.
     */
    void close(EpolledDSMSensor* sensor) throw();

    /**
     * Find the corresponding EpolledDSMSensor for this DSMSensor,
     * and call the close method on it.
     */
    void close(DSMSensor* sensor) throw();

    /**
     * Close, then pass this sensor to the SensorOpener to be reopened.
     */
    void closeAndReopen(EpolledDSMSensor * sensor) throw();

    /**
     * Called when something has changed in our collection
     * of sensors. Mainly this maintains the set of 
     * file descriptors used by the epoll() system functions.
     */
    void handlePollingChange();

    void calcStatistics(dsm_time_t);

    void setupTimeouts(int sensorCheckIntervalMsecs);

    void checkTimeouts(dsm_time_t);

    mutable nidas::util::Mutex _pollingMutex;

    std::list<DSMSensor*> _allSensors;

    /**
     * Collection of DSMSensors which have been opened.
     */
    std::list<DSMSensor*> _openedSensors;

    std::list<EpolledDSMSensor*> _polledSensors;

    /**
     * epoll file descriptor.
     */
    int _epollfd;

    struct epoll_event* _events;

    int _nevents;

    std::list<DSMSensor*> _newOpenedSensors;

    std::list<DSMSensor*> _pendingSensorClosures;

    bool _pollingChanged;

    unsigned short _remoteSerialSocketPort;

    RemoteSerialListener *_rserial;

    std::list<RemoteSerialConnection*> _pendingRserialClosures;

    std::list<RemoteSerialConnection*> _activeRserialConns;

    dsm_time_t _sensorCheckTime;

    dsm_time_t _sensorStatsTime;

    /**
     * Interval for checking for timeouts on each sensor, in milliseconds.
     * Will be -1 if no sensors have a timeout.
     */
    int _sensorCheckIntervalMsecs;

    /**
     * Same as _sensorCheckIntervalMsecs, but in microseconds.
     */
    unsigned int _sensorCheckIntervalUsecs;

    /**
     * Interval for calculcating through-put statistics on each sensor,
     * in microseconds.
     */
    unsigned int _sensorStatsInterval;

    SensorOpener _opener;

#ifndef HAS_EPOLL_PWAIT
    NotifyPipe* _notifyPipe;
#endif

    /** No copy. */
    SensorHandler(const SensorHandler&);

    /** No assignment. */
    SensorHandler& operator=(const SensorHandler&);
};

}}                              // namespace nidas namespace core

#endif
