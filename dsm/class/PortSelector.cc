/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <PortSelector.h>
#include <DSMEngine.h>
#include <atdUtil/Logger.h>

#include <errno.h>
#include <unistd.h>
#include <signal.h>

using namespace std;
using namespace atdUtil;
using namespace dsm;

PortSelector::PortSelector(unsigned short rserialPort) :
  Thread("PortSelector"),sensorsChanged(false),
  remoteSerialSocketPort(rserialPort),rserial(0),rserialConnsChanged(false)
{
  setStatisticsPeriod(60 * MSECS_PER_SEC);

  /* start out with a 1/10 second select timeout.
   * While we're adding sensors we want select to
   * timeout fairly quickly, so when new DSMSensors are opened
   * and added they are read from without much time delay.
   * Otherwise if you add a sensor that isn't transmitting,
   * then one which is generating alot of output, the buffers for
   * the second sensor may fill before the first timeout.
   * Later it can be increased, but there may not be much to 
   * gain from increasing it.
   */
  setTimeout(MSECS_PER_SEC / 10);
  setTimeoutWarning(300 * MSECS_PER_SEC);
  FD_ZERO(&readfdset);
  statisticsTime = timeCeiling(getSystemTime(),
  	statisticsPeriod);
  blockSignal(SIGINT);
  blockSignal(SIGHUP);
  blockSignal(SIGTERM);
}

/**
 * Close any remaining sensors. Before this is called
 * the run method should be finished.
 */
PortSelector::~PortSelector()
{
    delete rserial;
    cerr << "deleting activeSensors" << endl;
    for (unsigned int i = 0; i < activeDSMSensors.size(); i++) {
	activeDSMSensors[i]->close();
	delete activeDSMSensors[i];
    }
}


void PortSelector::setTimeout(int val) {
  timeoutMsec = val;
  timeoutSec = val / MSECS_PER_SEC;
  timeoutUsec = (val % MSECS_PER_SEC) * USECS_PER_MSEC;
}

int PortSelector::getTimeout() const {
  return timeoutSec * MSECS_PER_SEC + timeoutUsec / USECS_PER_MSEC;
}

void PortSelector::setTimeoutWarning(int val) {
  timeoutWarningMsec = val;
}

int PortSelector::getTimeoutWarning() const {
  return timeoutWarningMsec;
}

void PortSelector::calcStatistics(dsm_time_t tnow)
{
  statisticsTime += statisticsPeriod;
  if (statisticsTime < tnow)
    statisticsTime = timeCeiling(tnow,statisticsPeriod);

  for (unsigned int ifd = 0; ifd < activeDSMSensorFds.size(); ifd++) {
    DSMSensor *sensor = activeDSMSensors[ifd];
    sensor->calcStatistics(statisticsPeriod);
  }
}


/**
 * Thread function, select loop.
 */
int PortSelector::run() throw(atdUtil::Exception)
{

    dsm_time_t rtime = 0;
    struct timeval tout;
    unsigned long timeoutSumMsec = 0;

    delete rserial;
    rserial = 0;
    if (remoteSerialSocketPort > 0) {
	try {
	    rserial = new RemoteSerialListener(remoteSerialSocketPort);
	}
	catch (const atdUtil::IOException& e) {
	    Logger::getInstance()->log(LOG_WARNING,"%s: continuing anyhow",e.what());
	}
    }
    for (;;) {
	if (amInterrupted()) break;

	if (sensorsChanged || rserialConnsChanged)
	    handleChangedSensors();

	fd_set rset = readfdset;
	fd_set eset = readfdset;
	tout.tv_sec = timeoutSec;
	tout.tv_usec = timeoutUsec;
	int nfdsel = ::select(selectn,&rset,0,&eset,&tout);
	if (amInterrupted()) break;

	if (nfdsel <= 0) {		// select error
	    if (nfdsel < 0) {
		// Create and report but don't throw IOException.
		// Likely this is a EINTR (interrupted) error, in which case
		// the signal handler has set the amInterrupted() flag, which
		// will be caught at the top of the loop.
		IOException e("PortSelector","select",errno);
		Logger::getInstance()->log(LOG_ERR,"%s",e.toString().c_str());
		if (errno != EINTR) selectErrors++;
		timeoutSumMsec = 0;
	    }
	    else {
		timeoutSumMsec += timeoutMsec;
		if (timeoutSumMsec >= timeoutWarningMsec) {
		  Logger::getInstance()->log(LOG_INFO,
			"PortSelector select timeout %d msecs",timeoutSumMsec);
		  timeoutSumMsec = 0;
		}
	    }
	    rtime = getSystemTime();
	    if (rtime > statisticsTime) calcStatistics(rtime);
	    continue;
	}				// end of select error section

	timeoutSumMsec = 0;

	int nfd = 0;
	int fd = 0;

	SampleDater* dater = DSMEngine::getInstance()->getSampleDater();

	for (unsigned int ifd = 0; ifd < activeDSMSensorFds.size(); ifd++) {
	    fd = activeDSMSensorFds[ifd];
	    if (FD_ISSET(fd,&rset)) {
		DSMSensor *sensor = activeDSMSensors[ifd];
		try {
		  rtime = sensor->readSamples(dater);
		}
		// log the error but don't exit
		catch (IOException &ioe) {
		  Logger::getInstance()->log(LOG_ERR,"%s",ioe.toString().c_str());
		}
		if (++nfd == nfdsel) break;
	    }
	    // log the error but don't exit
	    if (FD_ISSET(fd,&eset)) {
		DSMSensor *sensor = activeDSMSensors[ifd];
		Logger::getInstance()->log(LOG_ERR,
		      "PortSelector select reports exception for %s",
		      sensor->getDeviceName().c_str());
		if (++nfd == nfdsel) break;
	    }
	}
	if (rtime > statisticsTime) calcStatistics(rtime);

	if (nfd == nfdsel) continue;

	std::list<RemoteSerialConnection*>::iterator ci;
	for (ci = activeRserialConns.begin(); ci != activeRserialConns.end(); ++ci) {
	    RemoteSerialConnection* conn = *ci;
	    fd = conn->getFd();
	    if (FD_ISSET(fd,&rset)) {
		try {
		    conn->read();
		}
		// log the error but don't exit
		catch (IOException &ioe) {
		  Logger::getInstance()->log(LOG_ERR,"rserial: %s",ioe.toString().c_str());
		  removeRemoteSerialConnection(conn);
		}
		if (++nfd == nfdsel) break;
	    }
	    if (FD_ISSET(fd,&eset)) {
		Logger::getInstance()->log(LOG_ERR,"%s",
		  "PortSelector select reports exception for rserial socket ");
		if (++nfd == nfdsel) break;
	    }
	}
	if (nfd == nfdsel) continue;
	if (rserial) {
	    fd = rserial->getFd();
	    if (FD_ISSET(fd,&rset)) {
		try {
		    RemoteSerialConnection* rsconn = rserial->acceptConnection();
		    addRemoteSerialConnection(rsconn);
		}
		catch (IOException &ioe) {
		    Logger::getInstance()->log(LOG_ERR,
			    "PortSelector rserial: %s",ioe.toString().c_str());
		    rserialListenErrors++;
		}
		if (++nfd == nfdsel) continue;
	    }
	    if (FD_ISSET(fd,&eset)) {
		Logger::getInstance()->log(LOG_ERR,"%s",
		  "PortSelector select reports exception for rserial listen socket ");
		  rserialListenErrors++;
		if (++nfd == nfdsel) continue;
	    }
	}
    }

    rserialConnsMutex.lock();
    std::list<RemoteSerialConnection*> conns = pendingRserialConns;
    rserialConnsMutex.unlock();
	  
    std::list<RemoteSerialConnection*>::iterator ci;
    for (ci = conns.begin(); ci != conns.end(); ++ci)
	removeRemoteSerialConnection(*ci);

    // Logger::getInstance()->log(LOG_INFO,
	// "PortSelector finished, closing remaining %d sensors ",activeDSMSensors.size());

    sensorsMutex.lock();
    std::vector<DSMSensor*> tsensors = pendingDSMSensors;
    sensorsMutex.unlock();

    for (unsigned int i = 0; i < tsensors.size(); i++)
	closeDSMSensor(tsensors[i]);

    handleChangedSensors();

    return RUN_OK;
}

/**
 * Called from the main thread
 */
void PortSelector::addDSMSensor(DSMSensor *sensor)
{
    int fd = sensor->getReadFd();

    sensorsMutex.lock();

    unsigned int i;
    for (i = 0; i < pendingDSMSensors.size(); i++) {
	// new file descriptor for this sensor
	if (pendingDSMSensors[i] == sensor) {
	    pendingDSMSensorFds[i] = fd;
	    sensorsChanged = true;
	    sensorsMutex.unlock();
	    return;
	}
    }
    pendingDSMSensorFds.push_back(fd);
    pendingDSMSensors.push_back(sensor);
    sensorsChanged = true;
    sensorsMutex.unlock();

}

/*
 * Add DSMSensor to my list of DSMSensors to be closed.
 */
void PortSelector::closeDSMSensor(DSMSensor *sensor)
{
    Synchronized autosync(sensorsMutex);

    for (unsigned int i = 0; i < pendingDSMSensors.size(); i++) {
	if (pendingDSMSensors[i] == sensor) {
	    pendingDSMSensorFds.erase(pendingDSMSensorFds.begin()+i);
	    pendingDSMSensors.erase(pendingDSMSensors.begin()+i);
	    break;
	}
    }
    pendingDSMSensorClosures.push_back(sensor);
    sensorsChanged = true;
}

/**
 * Protected method to add a RemoteSerial connection
 */
void PortSelector::addRemoteSerialConnection(RemoteSerialConnection* conn)
	throw(atdUtil::IOException)
{
    Synchronized autosync(sensorsMutex);
    cerr << "requested rserial device= \"" << conn->getSensorName() << "\"" << endl;
    for (unsigned int i = 0; i < pendingDSMSensors.size(); i++) {
	cerr << "pending sensor= \"" <<
		pendingDSMSensors[i]->getDeviceName() << "\"" << endl;
	if (!pendingDSMSensors[i]->getDeviceName().compare(
	    conn->getSensorName())) {

	    conn->setDSMSensor(pendingDSMSensors[i]);	// may throw IOException

	    Synchronized rserialLock(rserialConnsMutex);
	    pendingRserialConns.push_back(conn);
	    rserialConnsChanged = true;

	    Logger::getInstance()->log(LOG_NOTICE,
	      "added rserial connection for device %s",
		      conn->getSensorName().c_str());
	    return;
	}
    }
    Logger::getInstance()->log(LOG_WARNING,
      "PortSelector::addRemoteSerialConnection: cannot find sensor %s",
	      conn->getSensorName().c_str());
}

/*
 * Remove a RemoteSerialConnection from the current list.
 * This doesn't close or delete the connection, but puts
 * it in the pendingRserialClosures list.
 */
void PortSelector::removeRemoteSerialConnection(RemoteSerialConnection* conn)
{
    Synchronized autosync(rserialConnsMutex);
    bool found = false;

    std::list<RemoteSerialConnection*>::iterator ci;
    for (ci = pendingRserialConns.begin();
    	ci != pendingRserialConns.end(); ++ci) {
	if (conn == *ci) {
	    conn->setDSMSensor(0);
	    pendingRserialConns.erase(ci);
	    found = true;
	    break;
	}
    }
    if (!found) Logger::getInstance()->log(LOG_WARNING,"%s",
	"PortSelector::removeRemoteSerialConnection couldn't find connection for %s",
		conn->getSensorName().c_str());
    pendingRserialClosures.push_back(conn);
    rserialConnsChanged = true;
}


void PortSelector::handleChangedSensors() {
    unsigned int i;
    if (sensorsChanged) {
	Synchronized autosync(sensorsMutex);
	activeDSMSensorFds = pendingDSMSensorFds;
	activeDSMSensors = pendingDSMSensors;
	// close any sensors
	for (i = 0; i < pendingDSMSensorClosures.size(); i++) {
	    pendingDSMSensorClosures[i]->close();
	    delete pendingDSMSensorClosures[i];
	}
	pendingDSMSensorClosures.clear();
	sensorsChanged = false;
    }

    selectn = 0;
    FD_ZERO(&readfdset);
    for (i = 0; i < activeDSMSensorFds.size(); i++) {
	if (activeDSMSensorFds[i] > selectn) selectn = activeDSMSensorFds[i];
	FD_SET(activeDSMSensorFds[i],&readfdset);
    }

    if (rserialConnsChanged) {
	Synchronized autosync(rserialConnsMutex);
	activeRserialConns = pendingRserialConns;
	// close any pending remote serial connections
	std::list<RemoteSerialConnection*>::iterator ci;
	for (ci = pendingRserialClosures.begin();
		ci != pendingRserialClosures.end(); ++ci) delete *ci;
	pendingRserialClosures.clear();
	rserialConnsChanged = false;
    }

    std::list<RemoteSerialConnection*>::iterator ci;
    for (ci = activeRserialConns.begin(); ci != activeRserialConns.end(); ++ci) {
	RemoteSerialConnection* conn = *ci;
	if (conn->getFd() > selectn) selectn = conn->getFd();
	FD_SET(conn->getFd(),&readfdset);
    }

    if (rserial) {
	int fd = rserial->getFd();
	FD_SET(fd,&readfdset);
	if (fd > selectn) selectn = fd;
    }

    selectn++;
}

