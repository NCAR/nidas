
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
class SensorHandler : public nidas::util::Thread {
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
    void addSensor(DSMSensor *sensor);

    /**
     * Request that SensorHandler close the sensor.
     */
    void closeSensor(DSMSensor *sensor);

    /**
     * After SensorOpener has opened the sensor, it will
     * notify SensorHandler via this method that the
     * sensor is open.
     */
    void sensorOpen(DSMSensor *sensor);

    void addRemoteSerialConnection(RemoteSerialConnection*)
    	throw(nidas::util::IOException);
    void removeRemoteSerialConnection(RemoteSerialConnection*);

    /**
     * Set the length of time to wait in the select.
     * This shouldn't be set too long during the time
     * one is opening and adding sensors to the SensorHandler.
     * @param val length of time, in milliseconds.
     */
    void setTimeout(int val);

    int getTimeout() const;

    /**
     * Set the timeout warning period. After no data has
     * been received on any port for this period, issue a warning.
     * @param val length of time, in milliseconds.
     */
    void setTimeoutWarning(int val);

    int getTimeoutWarning() const;

    void calcStatistics(dsm_time_t);

    /**
     * Set the statistics period.
     * @param val Period, in milliseconds.
     */
    void setStatisticsPeriod(int val) { statisticsPeriod = val * USECS_PER_MSEC; }
    /**
     * Get the statistics period.
     * @return Period, in milliseconds.
     */
    int getStatisticsPeriod() const { return statisticsPeriod / USECS_PER_MSEC; }

    int getSelectErrors() const { return selectErrors; }
    int getRemoteSerialListenErrors() const { return rserialListenErrors; }

      void handleRemoteSerial(int fd,DSMSensor* sensor)
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
        if (opener.isRunning()) opener.cancel();
	Thread::cancel();
    }

    /**
     * Interrupt this thread.  We catch this
     * interrupt so that we can pass it on the SensorOpener.
     */
    void interrupt()
    {
        if (opener.isRunning()) opener.cancel();
	Thread::interrupt();
    }

    /**
     * Join this thread.
     */
    int join() throw(nidas::util::Exception)
    {
	int res = Thread::join();
        if (!opener.isJoined()) opener.join();
	return res;
    }

private:

    void handleChangedSensors();

    /**
     * Utility function that calls fstat on a file descriptor and
     * returns true if fstat succeeds, indicating that the file
     * descriptor is useable.
     */
    static bool goodFd(int fd, const std::string& devname) throw();

    /**
     * Pass this sensor to the SensorOpener to be reopened.
     */
    void reopenSensor(DSMSensor *sensor);

    mutable nidas::util::Mutex sensorsMutex;

    std::list<DSMSensor*> allSensors;

    std::list<DSMSensor*> pendingSensors;

    std::list<DSMSensor*> pendingSensorClosures;

    std::vector<int> activeSensorFds;

    std::vector<DSMSensor*> activeSensors;

    bool sensorsChanged;

    unsigned short remoteSerialSocketPort;

    RemoteSerialListener* rserial;

    nidas::util::Mutex rserialConnsMutex;
    std::list<RemoteSerialConnection*> pendingRserialConns;
    std::list<RemoteSerialConnection*> pendingRserialClosures;
    std::list<RemoteSerialConnection*> activeRserialConns;

    bool rserialConnsChanged;

    fd_set readfdset;
    int selectn;

    int selectErrors;
    int rserialListenErrors;

    size_t timeoutMsec;
    struct timeval timeoutVal;
    size_t timeoutWarningMsec;

    dsm_time_t statisticsTime;

    /**
     * Statistics period, in microseconds.
     */
    unsigned long statisticsPeriod;

    SensorOpener opener;

};

}}	// namespace nidas namespace core

#endif
