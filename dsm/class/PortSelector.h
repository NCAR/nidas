
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#ifndef DSM_PORTSELECTOR_H
#define DSM_PORTSELECTOR_H

#include <DSMTime.h>

#include <DSMSensor.h>
#include <SensorOpener.h>
#include <RemoteSerialListener.h>
#include <atdUtil/Thread.h>
#include <atdUtil/ThreadSupport.h>
#include <atdUtil/IOException.h>

#include <sys/time.h>
#include <sys/select.h>

#include <vector>
#include <set>

namespace dsm {

/**
 * PortSelector implements a DSMSensor event loop. It does a
 * select() system call on the file descriptors of one or more
 * DSMSensors, and calls their readSample methods when data is
 * available to be read.  The select() loop is implemented in
 * the Thread::run method of the PortSelector.
 *
 * select also detects connections to RemoteSerialListener.
 * Once is socket connection is established for a given sensor, then
 * data is then passed back and forth between the socket
 * connection and the DSMSensor.  This path is separate
 * from the normal Sample data path.  It allows remote
 * direct control of serial sensors.
 */
class PortSelector : public atdUtil::Thread {
public:

    /**
     * Constructor.
     * @param rserialPort TCP socket port to listen for incoming
     *		requests to the rserial service. 0=don't listen.
     */
    PortSelector(unsigned short rserialPort = 0);
    ~PortSelector();

    /**
     * Add an unopened sensor to the PortSelector. PortSelector
     * will then own the DSMSensor.
     */
    void addSensor(DSMSensor *sensor);

    /**
     * Request that PortSelector close the sensor.
     */
    void closeSensor(DSMSensor *sensor);

    /**
     * After SensorOpener has opened the sensor, it will
     * notify PortSelector via this method that the
     * sensor is open.
     */
    void sensorOpen(DSMSensor *sensor);

    void addRemoteSerialConnection(RemoteSerialConnection*)
    	throw(atdUtil::IOException);
    void removeRemoteSerialConnection(RemoteSerialConnection*);

    /**
     * Set the length of time to wait in the select.
     * This shouldn't be set too long during the time
     * one is opening and adding sensors to the PortSelector.
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
      	throw(atdUtil::IOException);
    /**
     * Thread function.
     */
    virtual int run() throw(atdUtil::Exception);
    
    std::list<DSMSensor*> getSensors() const;

    std::set<DSMSensor*> getOpenedSensors() const;

    /**
     * Cancel this PortSelector. We catch this
     * cancel so that we can pass it on the SensorOpener.
     */
    void cancel() throw(atdUtil::Exception)
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
    int join() throw(atdUtil::Exception)
    {
	int res = Thread::join();
        if (!opener.isJoined()) opener.join();
	return res;
    }

protected:

    void handleChangedSensors();

    /**
     * Pass this sensor to the SensorOpener to be reopened.
     */
    void reopenSensor(DSMSensor *sensor);

    mutable atdUtil::Mutex sensorsMutex;

    std::list<DSMSensor*> allSensors;

    std::set<DSMSensor*> pendingSensors;

    std::set<DSMSensor*> pendingSensorClosures;

    std::vector<int> activeSensorFds;

    std::vector<DSMSensor*> activeSensors;

    bool sensorsChanged;

    unsigned short remoteSerialSocketPort;

    RemoteSerialListener* rserial;

    atdUtil::Mutex rserialConnsMutex;
    std::list<RemoteSerialConnection*> pendingRserialConns;
    std::list<RemoteSerialConnection*> pendingRserialClosures;
    std::list<RemoteSerialConnection*> activeRserialConns;

    bool rserialConnsChanged;

    struct timeval tval;
    fd_set readfdset;
    int selectn;

    int selectErrors;
    int rserialListenErrors;

    size_t timeoutMsec;
    size_t timeoutSec;
    size_t timeoutUsec;
    size_t timeoutWarningMsec;

    dsm_time_t statisticsTime;

    /**
     * Statistics period, in microseconds.
     */
    unsigned long statisticsPeriod;

    SensorOpener opener;

};
}
#endif
