
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
#include <RemoteSerialListener.h>
#include <atdUtil/Thread.h>
#include <atdUtil/ThreadSupport.h>
#include <atdUtil/IOException.h>

#include <sys/time.h>
#include <sys/select.h>

#include <vector>

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

    void addSensorPort(DSMSensor *port);
    void closeSensorPort(DSMSensor *port);

    void addRemoteSerialConnection(RemoteSerialConnection*);
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

      void handleRemoteSerial(int fd,DSMSensor* port)
      	throw(atdUtil::IOException);
    /**
     * Thread function.
     */
    virtual int run() throw(atdUtil::Exception);

protected:

    void handleChangedPorts();

    atdUtil::Mutex portsMutex;
    std::vector<int> pendingSensorPortFds;
    std::vector<DSMSensor*> pendingSensorPorts;
    std::vector<DSMSensor*> pendingSensorPortClosures;

    std::vector<int> activeSensorPortFds;
    std::vector<DSMSensor*> activeSensorPorts;

    bool portsChanged;

    unsigned short remoteSerialSocketPort;

    RemoteSerialListener* rserial;

    atdUtil::Mutex rserialConnsMutex;
    std::vector<RemoteSerialConnection*> pendingRserialConns;
    std::vector<RemoteSerialConnection*> pendingRserialClosures;
    std::vector<RemoteSerialConnection*> activeRserialConns;

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

};
}
#endif
