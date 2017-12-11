// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_PORTSELECTOR_H
#define NIDAS_CORE_PORTSELECTOR_H

#include <nidas/Config.h>

#include "DSMSensor.h"
#include "Polled.h"
#include "SensorOpener.h"
#include "RemoteSerialListener.h"
#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/IOException.h>

#include <sys/time.h>

#include <vector>
#include <set>

/**
 * If this thread cannot block and then atomically catch a signal in its
 * polling method then a pipe must be used to notify the polling loop of
 * changes to the polled set of descriptors.
 *
 * A pipe is used in these situations:
 * 1. polling method is epoll, but don't have epoll_pwait
 * 2. polling method is poll, but don't have ppoll.
 * otherwise a SIGUSR1 signal is sent to this thread.
 */
#if (((POLLING_METHOD == POLL_EPOLL_ET || POLLING_METHOD == POLL_EPOLL_LT) && !defined(HAVE_EPOLL_PWAIT)) || (POLLING_METHOD == POLL_POLL && !defined(HAVE_PPOLL)))
#define USE_NOTIFY_PIPE
#endif

namespace nidas { namespace core {

/**
 * SensorHandler implements a DSMSensor event loop.
 * It uses one of the available system calls, depending
 * on the value of POLLING_METHOD, to check for events
 * on the file descriptors of one or more PolledDSMSensors
 * and RemoteSerial sockets.
 *
 * The handlePollEvents() method is called on each active
 * file descriptor.  The polling loop is implemented in
 * the Thread::run method of the SensorHandler.
 *
 * This code has supported legacy systems which did not have the
 * epoll() API, and used select/pselect or poll/ppoll instead.
 * So, this code supports all the polling APIs, and one is
 * selected using POLLING_METHOD. As of 2017 all known systems
 * support epoll(), so the old select/poll code could be removed.
 *
 * When an incoming socket connection is accepted on
 * RemoteSerialListener, a RemoteSerialConnection is
 * established, which first requests the name of a
 * DSMSensor on the connected socket. If that sensor 
 * is found then data are then passed back and forth
 * between the socket and the DSMSensor. This data path
 * is separate from the normal Sample data path.  It
 * allows remote, direct control of serial sensors.
 */
class SensorHandler:public nidas::util::Thread
{
public:

    friend class RemoteSerialConnection;
    friend class RemoteSerialListener;

    /**
     * Constructor.
     * @param rserialPort TCP socket port to listen for incoming
     *		requests to the rserial service. 0=don't listen.
     */
    SensorHandler(unsigned short rserialPort = 0);

    ~SensorHandler();

    /**
     * Override default implementation of Thread::signalHandler().
     * The default implementation sets the interrupted flag,
     * which we don't want.
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
    void sensorIsOpen(DSMSensor * sensor) throw();

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

#if POLLING_METHOD == POLL_EPOLL_ET || POLLING_METHOD == POLL_EPOLL_LT
    int getEpollFd() const { return _epollfd; }
#endif

    /**
     * Called by a DSMSensor to indicate that a read did not
     * consume all available data.
     */
    void incrementFullBufferReads(const DSMSensor* sensor);


    /** 
     * Tell the SensorHandler that one or more sensor timeouts have changed.
     */
    void updateTimeouts() 
    {
        _pollingChanged = true;
    }

private:

    class PolledDSMSensor : public Polled 
    {
    public:
        PolledDSMSensor(DSMSensor* sensor,SensorHandler* handler)
            throw(nidas::util::IOException);

        /**
         * Destructor does not close().
         */
        ~PolledDSMSensor() {}

        /**
         * @return: true: read consumed all available data.
         */
        bool handlePollEvents(uint32_t events) throw();

        DSMSensor* getDSMSensor() { return _sensor; }

        int getFd() const { return _sensor->getReadFd(); }

        const std::string getName() const { return _sensor->getName(); }

        int getTimeoutMsecs() const { return _sensor->getTimeoutMsecs(); }

        /**
         * SensorHandler implements a fairly crude way to detect
         * timeouts on data read from sensors. It determines the
         * minimum timeout value for the currently opened sensors.
         * This value, but not less than 1000 milliseconds, is the parameter
         * msecs, which is passed here to each opened sensor
         * from time to time as the group of sensors changes.
         *
         * In this simple implementation, this method sets the value
         * of _nTimeoutChecksMax to:
         * getTimeoutMsecs() + msecs -1)/msecs
         */
        void setupTimeouts(int checkIntervalMsecs);

        /**
         * The SensorHandler will call this method of each opened
         * sensor at the interval specified previously in the call to
         * setupTimeouts(int), or perhaps somewhat less often, due
         * to normal overhead. 
         * If handlePollEvents(events) with an event mask of EPOLLEDIN
         * has not bee called since the last call to checkTimeout,
         * then increment _nTimeoutChecks.
         * @return: true if _nTimeoutChecks is equal to _nTimeoutChecksMax.
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

        /**
         * How many times this sensor has been checked for timeouts since
         * the last time data was reads.
         */
        int _nTimeoutChecks;

        /**
         * How many timeout checks constitute an timeout on this sensor.
         */
        int _nTimeoutChecksMax;

        /**
         *  What the previous timeout check interval was. This value is
         *  used to scale _nTimeoutChecks if the check interval changes,
         *  which happens when the list of polled sensors changes.
         */
        int _lastCheckInterval;

        // no copying
        PolledDSMSensor(const PolledDSMSensor&);

        // no assignment
        PolledDSMSensor& operator = (const PolledDSMSensor&);

    };

#ifdef USE_NOTIFY_PIPE
    class NotifyPipe: public Polled
    {
    public:
        NotifyPipe(SensorHandler* handler) throw(nidas::util::IOException);

        ~NotifyPipe();

        bool handlePollEvents(uint32_t events) throw();

        void close() throw(nidas::util::IOException);

        /**
         * Used in public methods of SensorHandler which are called
         * from other threads to notify the SensorHandler that
         * the collection of polled objects has changed.
         */
        void notify() throw();

        int getFd() const { return _fds[0]; }

    private:
        int _fds[2];
        SensorHandler* _handler;

        // no copying
        NotifyPipe(const NotifyPipe&);

        // no assignment
        NotifyPipe& operator = (const NotifyPipe&);
    };
#endif

    /**
     * Internal private method to create a PolledDSMSensor from
     * a DSMSensor and add it to the list of currently active
     * sensors.
     */
    void add(DSMSensor* sensor) throw();

    /**
     * Internal private method to close the PolledDSMSensor.
     * Remove it from the polling list, then delete it.
     * The associated DSMSensor will be removed from _openedSensors.
     */
    void remove(PolledDSMSensor* sensor) throw();

    /**
     * Schedule this PolledDSMSensor to be closed
     * when convenient.
     */
    void scheduleClose(PolledDSMSensor*) throw();

    /**
     * Schedule this PolledDSMSensor to be closed and reopened.
     */
    void scheduleReopen(PolledDSMSensor*) throw();

    /**
     * Internal private method to add a remote serial
     * connection to a sensor.
     */
    void add(RemoteSerialConnection *) throw();

    void remove(RemoteSerialConnection *) throw();

    /**
     * Schedule this remote serial connection to be closed
     * when convenient.
     */
    void scheduleAdd(RemoteSerialConnection*) throw();

    void scheduleClose(RemoteSerialConnection*) throw();

    /**
     * Called when something has changed in our collection
     * of sensors. Mainly this maintains the set of 
     * file descriptors used by the epoll() system functions.
     */
    void handlePollingChange();

    void calcStatistics(dsm_time_t);

    void setupTimeouts(int sensorCheckIntervalMsecs);

    void checkTimeouts(dsm_time_t);

    /**
     * The collection of DSMSensors to be handled.
     */
    std::list<DSMSensor*> _allSensors;

    mutable nidas::util::Mutex _pollingMutex;

    /**
     * A change in the polling file descriptors needs
     * to be handled.
     */
    bool _pollingChanged;

    /**
     * Collection of DSMSensors which have been opened.
     */
    std::list<DSMSensor*> _openedSensors;

    /**
     * Those DSMSensors currently being polled.
     */
    std::list<PolledDSMSensor*> _polledSensors;

    /**
     * Newly opened DSMSensors, which are to be added
     * to the list of file descriptors to be polled.
     */
    std::list<DSMSensor*> _newOpenedSensors;

    /**
     * Sensors to be closed, probably due to a read error
     * or a timeout. Defined as a set to avoid bugs
     * where a sensor might be added twice.
     */
    std::set<PolledDSMSensor*> _pendingSensorClosures;

    /**
     * Sensors to be closed and then reopened. Also probably
     * due to a read error or a timeout.
     */
    std::set<PolledDSMSensor*> _pendingSensorReopens;

    unsigned short _remoteSerialSocketPort;

    RemoteSerialListener *_rserial;

    /**
     * Newly opened RemoteSerialConnections, which are to be added
     * to the list of file descriptors to be polled.
     */
    std::list<RemoteSerialConnection*> _newRserials;

    /**
     * RemoteSerialConnections to be closed, probably because
     * the socket connection closed.
     */
    std::set<RemoteSerialConnection*> _pendingRserialClosures;

    std::list<RemoteSerialConnection*> _activeRserialConns;

#if POLLING_METHOD == POLL_EPOLL_ET || POLLING_METHOD == POLL_EPOLL_LT
    /**
     * epoll file descriptor.
     */
    int _epollfd;

    struct epoll_event* _events;

    int _nevents;

#elif POLLING_METHOD == POLL_PSELECT

    fd_set _rfdset;

    fd_set _efdset;

    /**
     * Argument to select, one greater than maximum filedescriptor value.
     */
    int _nselect;

    /**
     * array of active file descriptors.
     */
    int* _fds;

#elif POLLING_METHOD == POLL_POLL

    struct pollfd* _fds;

#endif

#if POLLING_METHOD == POLL_PSELECT || POLLING_METHOD == POLL_POLL
    /**
     * array of active Polled objects.
     */
    Polled** _polled;

#if POLLING_METHOD == POLL_POLL
    nfds_t _nfds;
#else
    unsigned int _nfds;
#endif

    unsigned int _nAllocFds;

    struct timespec _timeout;

#endif

#ifdef USE_NOTIFY_PIPE
    NotifyPipe* _notifyPipe;
#endif

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

    /**
     * For each sensor, number of times that the input buffer was
     * filled in a read, i.e. the read did not consume all the data
     * available.
     */
    std::map<const DSMSensor*,unsigned int> _fullBufferReads;

    bool _acceptingOpens;

    /** No copy. */
    SensorHandler(const SensorHandler&);

    /** No assignment. */
    SensorHandler& operator=(const SensorHandler&);
};

}}                              // namespace nidas namespace core

#endif
