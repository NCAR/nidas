
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
     * Set the length of time to wait in the select.
     * This shouldn't be set too long during the time
     * one is opening and adding sensors to the SensorHandler.
     * @param val length of time, in milliseconds.
     */
    void setTimeout(int val);

    int getTimeout() const;

    /**
     * Check on each sensor. Currently this means checking
     * whether a timeout has occured and calculating
     * statistics on the data received from the sensor.
     */
    void checkSensors(dsm_time_t);

    /**
     * Set the sensor check period.
     *
     * @param val Period, in milliseconds.
     *
     */
    void setSensorCheckInterval(int val)
    {
        _sensorCheckInterval = val * USECS_PER_MSEC;
    }
    /**
     * Get the sensor check period.
     * @return Period, in milliseconds.
     */
    int getSensorCheckInterval() const
    {
        return _sensorCheckInterval / USECS_PER_MSEC;
    }

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
    virtual int run() throw(nidas::util::Exception);

     std::list<DSMSensor*> getAllSensors() const;

     std::list<DSMSensor*> getOpenedSensors() const;

    /**
     * Cancel this SensorHandler. We catch this
     * cancel so that we can pass it on the SensorOpener.
     */
    void cancel() throw(nidas::util::Exception)
    {
        if (_opener.isRunning())
            _opener.cancel();
        Thread::cancel();
    }

    /**
     * Interrupt this thread.  We catch this
     * interrupt so that we can pass it on the SensorOpener.
     */
    void interrupt();

    /**
     * Join this thread.
     */
    int join() throw(nidas::util::Exception)
    {
        int res = Thread::join();
        if (!_opener.isJoined())
             _opener.join();
         return res;
    }

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

    std::vector<int> _activeSensorFds;

    std::vector<DSMSensor*> _activeSensors;

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

    unsigned int _timeoutMsec;
    struct timeval _timeoutVal;
    dsm_time_t _sensorCheckTime;

    /**
     * Interval for checking on each sensor, in microseconds.
     */
    unsigned int _sensorCheckInterval;

    SensorOpener _opener;

    /*
     * Pipe read/write file descriptors used to notify the select() call
     * that action is needed.
     */
    int _notifyPipe[2];

};

}}                              // namespace nidas namespace core

#endif
