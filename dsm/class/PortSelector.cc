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
  Thread("PortSelector"),portsChanged(false),
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
    DSMSensor *port = activeDSMSensors[ifd];
    port->calcStatistics(statisticsPeriod);
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

    if (portsChanged || rserialConnsChanged)
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
	DSMSensor *port = activeDSMSensors[ifd];
	try {
	  rtime = port->readSamples(dater);
	}
	// log the error but don't exit
	catch (IOException &ioe) {
	  Logger::getInstance()->log(LOG_ERR,"%s",ioe.toString().c_str());
	}
	if (++nfd == nfdsel) break;
      }
      // log the error but don't exit
      if (FD_ISSET(fd,&eset)) {
	DSMSensor *port = activeDSMSensors[ifd];
	Logger::getInstance()->log(LOG_ERR,
	      "PortSelector select reports exception for %s",
	      port->getDeviceName().c_str());
	if (++nfd == nfdsel) break;
      }
    }
    if (rtime > statisticsTime) calcStatistics(rtime);

    if (nfd == nfdsel) continue;

    for (unsigned int ifd = 0; ifd < activeRserialConns.size(); ifd++) {
      fd = activeRserialConns[ifd]->getFd();
      if (FD_ISSET(fd,&rset)) {
	try {
	    activeRserialConns[ifd]->read();
	}
	// log the error but don't exit
	catch (IOException &ioe) {
	  Logger::getInstance()->log(LOG_ERR,"rserial: %s",ioe.toString().c_str());
	  removeRemoteSerialConnection(activeRserialConns[ifd]);
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

  Logger::getInstance()->log(LOG_INFO,
      "PortSelector finished, closing remaining %d sensors ",activeDSMSensors.size());

  rserialConnsMutex.lock();
  std::vector<RemoteSerialConnection*> conns = pendingRserialConns;
  rserialConnsMutex.unlock();
	
  for (unsigned int i = 0; i < conns.size(); i++)
    removeRemoteSerialConnection(conns[i]);

  portsMutex.lock();
  std::vector<DSMSensor*> tports = pendingDSMSensors;
  portsMutex.unlock();

  for (unsigned int i = 0; i < tports.size(); i++)
    closeDSMSensor(tports[i]);

  portsChanged = true;
  handleChangedSensors();

  return RUN_OK;
}

/**
 * Called from the main thread
 */
void PortSelector::addDSMSensor(DSMSensor *port)
{

  int fd = port->getReadFd();

  portsMutex.lock();

  unsigned int i;
  for (i = 0; i < pendingDSMSensors.size(); i++) {
    // new file descriptor for this port
    if (pendingDSMSensors[i] == port) {
      pendingDSMSensorFds[i] = fd;
      portsChanged = true;
      portsMutex.unlock();
      return;
    }
  }
  pendingDSMSensorFds.push_back(fd);
  pendingDSMSensors.push_back(port);
  portsChanged = true;
  portsMutex.unlock();

}

void PortSelector::closeDSMSensor(DSMSensor *port)
{

  Synchronized autosync(portsMutex);

  for (unsigned int i = 0; i < pendingDSMSensors.size(); i++) {
    if (pendingDSMSensors[i] == port) {
      pendingDSMSensorFds.erase(pendingDSMSensorFds.begin()+i);
      pendingDSMSensors.erase(pendingDSMSensors.begin()+i);
      break;
    }
  }
  pendingDSMSensorClosures.push_back(port);
  portsChanged = true;
}

/**
 * Protected method to add a RemoteSerial connection
 */
void PortSelector::addRemoteSerialConnection(RemoteSerialConnection* conn)
	throw(atdUtil::IOException)
{
    Synchronized autosync(portsMutex);
    for (unsigned int i = 0; i < pendingDSMSensors.size(); i++) {
	if (!pendingDSMSensors[i]->getDeviceName().compare(
	    conn->getSensorName())) {

	    conn->setDSMSensor(pendingDSMSensors[i]);	// may throw IOException

	    Synchronized rserialLock(rserialConnsMutex);
	    pendingRserialConns.push_back(conn);
	    rserialConnsChanged = true;

	    Logger::getInstance()->log(LOG_INFO,
	      "added rserial connection for device %s",
		      conn->getSensorName().c_str());
	    return;
	}
    }
    Logger::getInstance()->log(LOG_INFO,
      "PortSelector::addRemoteSerialConnection: cannot find sensor %s",
	      conn->getSensorName().c_str());
}

/**
 * this is called by a DSMSensor from this (PortSelector) thread.
 */
void PortSelector::removeRemoteSerialConnection(RemoteSerialConnection* conn)
{
  Synchronized autosync(rserialConnsMutex);
  for (unsigned int i = 0; i < pendingRserialConns.size(); i++) {
    if (pendingRserialConns[i] == conn) {
        conn->setDSMSensor(0);
	pendingRserialConns.erase(pendingRserialConns.begin()+i);
    }
  }
  pendingRserialClosures.push_back(conn);
  rserialConnsChanged = true;
  Logger::getInstance()->log(LOG_INFO,"%s",
      "PortSelector::removeRemoteSerialConnection");
}


void PortSelector::handleChangedSensors() {
  unsigned int i;
  if (portsChanged) {
    Synchronized autosync(portsMutex);
    activeDSMSensorFds = pendingDSMSensorFds;
    activeDSMSensors = pendingDSMSensors;
    // close any ports
    for (i = 0; i < pendingDSMSensorClosures.size(); i++) {
	pendingDSMSensorClosures[i]->close();
	delete pendingDSMSensorClosures[i];
    }
    pendingDSMSensorClosures.clear();
    portsChanged = false;
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
    // close any ports
    for (i = 0; i < pendingRserialClosures.size(); i++)
	delete pendingRserialClosures[i];
    pendingRserialClosures.clear();
    rserialConnsChanged = false;
  }

  for (i = 0; i < activeRserialConns.size(); i++) {
    if (activeRserialConns[i]->getFd() > selectn) selectn =
    	activeRserialConns[i]->getFd();
    FD_SET(activeRserialConns[i]->getFd(),&readfdset);
  }

  if (rserial) {
      int fd = rserial->getFd();
      FD_SET(fd,&readfdset);
      if (fd > selectn) selectn = fd;
  }

  selectn++;
}

