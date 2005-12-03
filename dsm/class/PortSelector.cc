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
  remoteSerialSocketPort(rserialPort),rserial(0),rserialConnsChanged(false),
  selectn(0),selectErrors(0),rserialListenErrors(0),opener(this)
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
  setTimeout(MSECS_PER_SEC / 50);
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
    for (unsigned int i = 0; i < activeSensors.size(); i++)
	activeSensors[i]->close();

    list<DSMSensor*>::const_iterator si;
    for (si = allSensors.begin(); si != allSensors.end(); ++si)
	delete *si;
}


void PortSelector::setTimeout(int val) {
    timeoutMsec = val;
    timeoutVal.tv_sec = val / MSECS_PER_SEC;
    timeoutVal.tv_usec = (val % MSECS_PER_SEC) * USECS_PER_MSEC;
}

int PortSelector::getTimeout() const {
  return timeoutMsec;
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


  list<DSMSensor*>::const_iterator si;
  for (si = allSensors.begin(); si != allSensors.end(); ++si) {
    DSMSensor *sensor = *si;
    sensor->calcStatistics(statisticsPeriod);
  }
}

/* returns a copy of our sensor list. */
list<DSMSensor*> PortSelector::getAllSensors() const
{
    Synchronized autosync(sensorsMutex);
    return allSensors;
}
/* returns a copy of our opened sensors. */
list<DSMSensor*> PortSelector::getOpenedSensors() const
{
    Synchronized autosync(sensorsMutex);
    return pendingSensors;
}

/**
 * Thread function, select loop.
 */
int PortSelector::run() throw(atdUtil::Exception)
{

    dsm_time_t rtime = 0;
    struct timeval tout;
    unsigned long timeoutSumMsec = 0;

    opener.start();

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
	tout = timeoutVal;
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

	for (unsigned int ifd = 0; ifd < activeSensorFds.size(); ifd++) {
	    fd = activeSensorFds[ifd];
	    if (FD_ISSET(fd,&rset)) {
		DSMSensor *sensor = activeSensors[ifd];
		try {
		  rtime = sensor->readSamples(dater);
		}
		// log the error but don't exit
		catch (IOException &ioe) {
		  Logger::getInstance()->log(LOG_ERR,"%s",ioe.toString().c_str());
		  try {
		      sensor->close();
		  }
		  catch (IOException &e) {
		    Logger::getInstance()->log(LOG_ERR,"%s",
		    	e.toString().c_str());
		  }
		  reopenSensor(sensor);
		}
		if (++nfd == nfdsel) break;
	    }
	    // log the error but don't exit
	    if (FD_ISSET(fd,&eset)) {
		DSMSensor *sensor = activeSensors[ifd];
		Logger::getInstance()->log(LOG_ERR,
		      "PortSelector select reports exception for %s",
		      sensor->getDeviceName().c_str());
		if (++nfd == nfdsel) break;
	    }
	}
	if (rtime > statisticsTime) {
	    calcStatistics(rtime);
	    list<SamplePoolInterface*> pools =
	    	SamplePools::getInstance()->getPools();
	    for (list<SamplePoolInterface*>::const_iterator pi = pools.begin();
	    	pi != pools.end(); ++pi) 
	    {
		SamplePoolInterface* pool = *pi;
	        Logger::getInstance()->log(LOG_INFO,
			"pool nsamples alloc=%d, nsamples out=%d",
			pool->getNSamplesAlloc(),pool->getNSamplesOut());
	    }
	}

	if (nfd == nfdsel) continue;

	list<RemoteSerialConnection*>::iterator ci;
	for (ci = activeRserialConns.begin();
		ci != activeRserialConns.end(); ++ci) {
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
		    RemoteSerialConnection* rsconn =
		    	rserial->acceptConnection();
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
    list<RemoteSerialConnection*> conns = pendingRserialConns;
    rserialConnsMutex.unlock();
	  
    list<RemoteSerialConnection*>::iterator ci;
    for (ci = conns.begin(); ci != conns.end(); ++ci)
	removeRemoteSerialConnection(*ci);

    // Logger::getInstance()->log(LOG_INFO,
	// "PortSelector finished, closing remaining %d sensors ",activeSensors.size());

    sensorsMutex.lock();
    list<DSMSensor*> tsensors = pendingSensors;
    sensorsMutex.unlock();

    list<DSMSensor*>::const_iterator si;
    for (si = tsensors.begin(); si != tsensors.end(); ++si)
	closeSensor(*si);

    handleChangedSensors();

    return RUN_OK;
}

/*
 * Called from the main thread.
 */
void PortSelector::addSensor(DSMSensor *sensor)
{
    sensorsMutex.lock();
    allSensors.push_back(sensor);
    sensorsMutex.unlock();

    opener.openSensor(sensor);
}

/*
 * Called from the SensorOpener thread indicating a sensor is
 * opened and ready.
 */
void PortSelector::sensorOpen(DSMSensor *sensor)
{
    // cerr << "PortSelector::sensorOpen" << endl;
    Synchronized autosync(sensorsMutex);
    pendingSensors.push_back(sensor);
    // cerr << "PortSelector::sensorOpen pendingSensors.size="  <<
    // 	pendingSensors.size() << endl;
    sensorsChanged = true;
}

/*
 * Add DSMSensor to my list of DSMSensors to be closed, and not reopened.
 */
void PortSelector::closeSensor(DSMSensor *sensor)
{
    Synchronized autosync(sensorsMutex);

    list<DSMSensor*>::iterator si = find(pendingSensors.begin(),
    	pendingSensors.end(),sensor);
    if (si != pendingSensors.end()) pendingSensors.erase(si);

    pendingSensorClosures.push_back(sensor);
    sensorsChanged = true;
}

/*
 * Make request to SensorOpener thread that DSMSensor be reopened.
 */
void PortSelector::reopenSensor(DSMSensor *sensor)
{
    Synchronized autosync(sensorsMutex);

    list<DSMSensor*>::iterator si = find(pendingSensors.begin(),
    	pendingSensors.end(),sensor);
    if (si != pendingSensors.end()) pendingSensors.erase(si);

    opener.reopenSensor(sensor);
    sensorsChanged = true;
}

/*
 * Protected method to add a RemoteSerial connection
 */
void PortSelector::addRemoteSerialConnection(RemoteSerialConnection* conn)
	throw(atdUtil::IOException)
{
    conn->readSensorName();

    Synchronized autosync(sensorsMutex);
    list<DSMSensor*>::const_iterator si;
    for (si = pendingSensors.begin(); si != pendingSensors.end(); ++si) {
	DSMSensor* sensor = *si;
	if (!sensor->getDeviceName().compare(conn->getSensorName())) {
	    conn->setDSMSensor(sensor);	// may throw IOException

	    rserialConnsMutex.lock();
	    pendingRserialConns.push_back(conn);
	    rserialConnsChanged = true;
	    rserialConnsMutex.unlock();

	    Logger::getInstance()->log(LOG_NOTICE,
	      "added rserial connection for device %s",
		      conn->getSensorName().c_str());
	    return;
	}
    }
    conn->sensorNotFound();
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
    list<RemoteSerialConnection*>::iterator ci;
    ci = find(pendingRserialConns.begin(),pendingRserialConns.end(),conn);

    if (ci != pendingRserialConns.end()) pendingRserialConns.erase(ci);
    else Logger::getInstance()->log(LOG_WARNING,"%s",
	"PortSelector::removeRemoteSerialConnection couldn't find connection for %s",
		conn->getSensorName().c_str());
    pendingRserialClosures.push_back(conn);
    rserialConnsChanged = true;
}


void PortSelector::handleChangedSensors() {
    // cerr << "handleChangedSensors" << endl;
    unsigned int i;
    if (sensorsChanged) {
	Synchronized autosync(sensorsMutex);

	activeSensors.clear();
	activeSensorFds.clear();

	// cerr << "handleChangedSensors, pendingSensors.size()" <<
	// 	pendingSensors.size() << endl;
	list<DSMSensor*>::iterator si;
	for (si = pendingSensors.begin();
		si != pendingSensors.end(); ) {
	    DSMSensor* sensor = *si;
	    if (sensor->getReadFd() >= 0) {
		// cerr << "readfd=" << sensor->getReadFd() << endl;
		activeSensors.push_back(sensor);
		activeSensorFds.push_back(sensor->getReadFd());
		++si;
	    }
	    else {
	        opener.reopenSensor(sensor);
		si = pendingSensors.erase(si);
	    }
	}

	// close any sensors
	for (si = pendingSensorClosures.begin();
		si != pendingSensorClosures.end(); ++si) {
	    DSMSensor* sensor = *si;
	    sensor->close();
	}
	pendingSensorClosures.clear();
	sensorsChanged = false;
	atdUtil::Logger::getInstance()->log(LOG_INFO,"%d active sensors",
		activeSensors.size());
    }

    if (rserialConnsChanged) {
	Synchronized autosync(rserialConnsMutex);
	activeRserialConns = pendingRserialConns;
	// close and delete any pending remote serial connections
	list<RemoteSerialConnection*>::iterator ci;
	for (ci = pendingRserialClosures.begin();
		ci != pendingRserialClosures.end(); ++ci) {
	    RemoteSerialConnection* conn = *ci;
	    conn->close();
	    delete conn;
	}
	pendingRserialClosures.clear();
	rserialConnsChanged = false;
    }

    selectn = 0;
    FD_ZERO(&readfdset);
    for (i = 0; i < activeSensorFds.size(); i++) {
	selectn = std::max(activeSensorFds[i]+1,selectn);
	FD_SET(activeSensorFds[i],&readfdset);
    }

    list<RemoteSerialConnection*>::iterator ci;
    for (ci = activeRserialConns.begin();
    	ci != activeRserialConns.end(); ++ci) {
	RemoteSerialConnection* conn = *ci;
	if (conn->getFd() >= 0) {
	    selectn = std::max(conn->getFd()+1,selectn);
	    FD_SET(conn->getFd(),&readfdset);
	}
	else removeRemoteSerialConnection(conn);
    }

    if (rserial) {
	int fd = rserial->getFd();
	FD_SET(fd,&readfdset);
	selectn = std::max(fd+1,selectn);
    }
}

