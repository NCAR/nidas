
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
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

    PortSelector();
    ~PortSelector();

    void addSensorPort(DSMSensor *port);
    void closeSensorPort(DSMSensor *port);

    void addRemoteSerialConnection(RemoteSerialConnection*);
    void removeRemoteSerialConnection(RemoteSerialConnection*);

    void setTimeoutMsec(int val);
    int getTimeoutMsec() const;

    void setTimeoutWarningMsec(int val);
    int getTimeoutWarningMsec() const;

    void calcStatistics(dsm_sys_time_t);

    void setStatisticsPeriodInSecs(int val) { statisticsPeriod = val * 1000; }
    int getStatisticsPeriodInSecs() const { return statisticsPeriod / 1000; }

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

    atdUtil::Mutex rserialConnsMutex;
    std::vector<RemoteSerialConnection*> pendingRserialConns;
    std::vector<RemoteSerialConnection*> pendingRserialClosures;
    std::vector<RemoteSerialConnection*> activeRserialConns;

    bool portsChanged;
    bool rserialConnsChanged;

    struct timeval tval;
    fd_set readfdset;
    int selectn;

    RemoteSerialListener rserial;

    int selectErrors;
    int rserialListenErrors;

    size_t timeoutMsec;
    size_t timeoutSec;
    size_t timeoutUsec;
    size_t timeoutWarningMsec;

    dsm_sys_time_t statisticsTime;

    unsigned long statisticsPeriod;

};
}
#endif
