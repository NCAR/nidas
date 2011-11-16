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

#include <nidas/core/DSMTime.h>

#include <nidas/core/DSMSensor.h>
#include <nidas/core/SensorOpener.h>
#include <nidas/core/RemoteSerialListener.h>
#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/IOException.h>

#include <sys/time.h>
#include <sys/select.h>

#include <vector>
#include <set>

namespace nidas { namespace core {

/**
 * SensorHandler implements a DSMSensor event loop. It does a
 * select() system call on the file descriptors of one or more
 * DSMSensors, and calls their readSample methods when data is
 * available to be read.  The select() loop is implemented in
 * the Thread::run method of the SensorHandler.
 *
 * select also detects connections to RemoteSerialListener.
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

    void addRemoteSerialConnection(RemoteSerialConnection *)
     throw(nidas::util::IOException);
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

    /**
     * Set the sensor check period.
     *
     * @param val Period, in milliseconds.
     *
     */
    void setSensorCheckIntervalMsecs(int val);
    /**
     * Get the sensor check period.
     * @return Period, in milliseconds.
     */
    int getSensorCheckIntervalMsecs() const;

    int getSensorCheckIntervalUsecs() const;

    int getSelectErrors() const
    {
        return _selectErrors;
    }
    int getRemoteSerialListenErrors() const
    {
        return _rserialListenErrors;
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
     * Interrupt this thread.  We override this method
     * so that we can pass it on the SensorOpener.
     */
    void interrupt();

    /**
     * Join this thread and join the SensorOpener.
     */
    int join() throw(nidas::util::Exception);

private:

    /**
     * Called when something has changed in our collection
     * of sensors. Mainly this maintains the set of 
     * file descriptors used by the select() system call.
     */
    void handleChangedSensors();

    /**
     * Utility function that calls fstat on a file descriptor and
     * returns true if fstat succeeds, indicating that the file
     * descriptor is useable.
     */
    static bool goodFd(int fd,
                       const std::string & devname) throw();

    /**
     * Close, then pass this sensor to the SensorOpener to be reopened.
     */
    void closeReopenSensor(DSMSensor * sensor);

    void calcStatistics(dsm_time_t);

    void checkTimeouts(dsm_time_t);

    mutable nidas::util::Mutex _sensorsMutex;

    std::list<DSMSensor*> _allSensors;

    /**
     * Collection of DSMSensors which have been opened.
     * After a call to handleChangedSensors, then activeSensors
     * will contain the same sensors as openedSensors.
     */
    std::list<DSMSensor*> _openedSensors;

    std::list<DSMSensor*> _closedSensors;

    std::list<DSMSensor*> _pendingSensorClosures;

    int* _activeSensorFds;

    DSMSensor** _activeSensors;

    unsigned int _nActiveSensors;

    unsigned int _nActiveSensorsAlloc;

    bool _sensorsChanged;

    unsigned short _remoteSerialSocketPort;

    RemoteSerialListener *_rserial;

    nidas::util::Mutex _rserialConnsMutex;
    std::list<RemoteSerialConnection*> _pendingRserialConns;
    std::list<RemoteSerialConnection*> _pendingRserialClosures;
    std::list<RemoteSerialConnection*> _activeRserialConns;

    bool _rserialConnsChanged;

    fd_set _readfdset;
    int _selectn;

    int _selectErrors;
    int _rserialListenErrors;

    struct timeval _selectTimeoutVal;
    dsm_time_t _sensorCheckTime;
    dsm_time_t _sensorStatsTime;

    /**
     * Interval for checking on each sensor, in microseconds.
     */
    unsigned int _sensorCheckIntervalUsecs;

    /**
     * Interval for calculcating through-put statistics on each sensor,
     * in microseconds.
     */
    unsigned int _sensorStatsInterval;

    SensorOpener _opener;

    /*
     * Pipe read/write file descriptors used to notify the select() call
     * that action is needed.
     */
    int _notifyPipe[2];

    /**
     * FD_SET used to indicate whether data has been received
     * on each active file descriptor since the last
     * _sensorCheckIntervalUsecs.
     */
    fd_set _rcvdData;

    /**
     * For each active sensor, how many successive time periods of
     * length _sensorCheckIntervalUsecs have elapsed with no data.
     */
    std::vector<int> _noDataCounts;

    /**
     * Once _noDataCounts reaches _noDataCountsMax, then
     * we have a data timeout on this sensor.
     */
    std::vector<int> _noDataCountsMax;

    /** No copy. */
    SensorHandler(const SensorHandler&);

    /** No assignment. */
    SensorHandler& operator=(const SensorHandler&);
};

}}                              // namespace nidas namespace core

#endif
