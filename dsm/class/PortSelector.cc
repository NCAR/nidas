/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <PortSelector.h>
#include <atdUtil/Logger.h>

#include <errno.h>
#include <unistd.h>
#include <signal.h>

using namespace std;
using namespace atdUtil;
using namespace dsm;

PortSelector::PortSelector() :
  Thread("PortSelector"),portsChanged(false),
  rserial(0),rserialConnsChanged(false),
  statisticsPeriod(300000)
{
  /* start out with a 1/10 second select timeout.
   * While we're adding sensors we want select to
   * timeout fairly quickly, so when new SensorPorts are opened
   * and added that they are read from without much time delay.
   * Otherwise if you add a sensor that isn't transmitting,
   * then one which is generating alot of output, the buffers for
   * the second sensor may fill before the first timeout.
   * Later it can be increased, but there may not be much to 
   * gain from increasing it.
   */
  setTimeoutMsec(100);
  setTimeoutWarningMsec(300000);
  FD_ZERO(&readfdset);
  statisticsTime = timeCeiling(getCurrentTimeInMillis(),
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
    cerr << "deleting activeSensorPorts" << endl;
    for (unsigned int i = 0; i < activeSensorPorts.size(); i++) {
	activeSensorPorts[i]->close();
	delete activeSensorPorts[i];
    }
}


void PortSelector::setTimeoutMsec(int val) {
  timeoutMsec = val;
  timeoutSec = val / 1000;
  timeoutUsec = (val % 1000) * 1000;
}

int PortSelector::getTimeoutMsec() const {
  return timeoutSec * 1000 + timeoutUsec / 1000;
}

void PortSelector::setTimeoutWarningMsec(int val) {
  timeoutWarningMsec = val;
}

int PortSelector::getTimeoutWarningMsec() const {
  return timeoutWarningMsec;
}

void PortSelector::calcStatistics(dsm_sys_time_t tnowMsec)
{
  statisticsTime += statisticsPeriod;
  if (statisticsTime < tnowMsec)
    statisticsTime = timeCeiling(tnowMsec,statisticsPeriod);

  for (unsigned int ifd = 0; ifd < activeSensorPortFds.size(); ifd++) {
    DSMSensor *port = activeSensorPorts[ifd];
    port->calcStatistics(statisticsPeriod);
  }
}


/**
 * Thread function, select loop.
 */
int PortSelector::run() throw(atdUtil::Exception)
{

  /* ignore SIGPIPE signals (they come from rserial) */
  struct sigaction act;
  memset(&act,0,sizeof (struct sigaction));
  act.sa_flags = 0;
  act.sa_handler = SIG_IGN;
  sigaction(SIGPIPE,&act,(struct sigaction *)0);

  dsm_sys_time_t rtime = 0;
  struct timeval tout;
  unsigned long timeoutSumMsec = 0;

  delete rserial;
  rserial = 0;
  try {
      rserial = new RemoteSerialListener();
  }
  catch (const atdUtil::IOException& e) {
      Logger::getInstance()->log(LOG_WARNING,"%s: continuing anyhow",e.what());
  }
  for (;;) {
    if (amInterrupted()) break;

    if (portsChanged || rserialConnsChanged)
    	handleChangedPorts();

    fd_set rset = readfdset;
    fd_set eset = readfdset;
    tout.tv_sec = timeoutSec;
    tout.tv_usec = timeoutUsec;
    int nfdsel = ::select(selectn,&rset,0,&eset,&tout);
    if (amInterrupted()) break;

    if (nfdsel <= 0) {
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
      rtime = getCurrentTimeInMillis();
      if (rtime > statisticsTime) calcStatistics(rtime);
      continue;
    }

    timeoutSumMsec = 0;

    int nfd = 0;
    int fd = 0;

    for (unsigned int ifd = 0; ifd < activeSensorPortFds.size(); ifd++) {
      fd = activeSensorPortFds[ifd];
      if (FD_ISSET(fd,&rset)) {
	DSMSensor *port = activeSensorPorts[ifd];
	try {
	  rtime = port->readSamples();
	}
	// log the error but don't exit
	catch (IOException &ioe) {
	  Logger::getInstance()->log(LOG_ERR,"%s",ioe.toString().c_str());
	}
	if (++nfd == nfdsel) break;
      }
      // log the error but don't exit
      if (FD_ISSET(fd,&eset)) {
	DSMSensor *port = activeSensorPorts[ifd];
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
      "PortSelector finished, closing remaining %d sensors ",activeSensorPorts.size());

  rserialConnsMutex.lock();
  std::vector<RemoteSerialConnection*> conns = pendingRserialConns;
  rserialConnsMutex.unlock();
	
  for (unsigned int i = 0; i < conns.size(); i++)
    removeRemoteSerialConnection(conns[i]);

  portsMutex.lock();
  std::vector<DSMSensor*> tports = pendingSensorPorts;
  portsMutex.unlock();

  for (unsigned int i = 0; i < tports.size(); i++)
    closeSensorPort(tports[i]);

  portsChanged = true;
  handleChangedPorts();

  return RUN_OK;
}

/**
 * Called from the main thread
 */
void PortSelector::addSensorPort(DSMSensor *port)
{

  int fd = port->getReadFd();

  portsMutex.lock();

  unsigned int i;
  for (i = 0; i < pendingSensorPorts.size(); i++) {
    // new file descriptor for this port
    if (pendingSensorPorts[i] == port) {
      pendingSensorPortFds[i] = fd;
      portsChanged = true;
      portsMutex.unlock();
      return;
    }
  }
  pendingSensorPortFds.push_back(fd);
  pendingSensorPorts.push_back(port);
  portsChanged = true;
  portsMutex.unlock();

}

void PortSelector::closeSensorPort(DSMSensor *port)
{

  Synchronized autosync(portsMutex);

  for (unsigned int i = 0; i < pendingSensorPorts.size(); i++) {
    if (pendingSensorPorts[i] == port) {
      pendingSensorPortFds.erase(pendingSensorPortFds.begin()+i);
      pendingSensorPorts.erase(pendingSensorPorts.begin()+i);
      break;
    }
  }
  pendingSensorPortClosures.push_back(port);
  portsChanged = true;
}

/**
 * Protected method to add an RemoteSerial connection
 */
void PortSelector::addRemoteSerialConnection(RemoteSerialConnection* conn)
{
  Synchronized autosync(portsMutex);
  for (unsigned int i = 0; i < pendingSensorPorts.size(); i++) {
    if (!pendingSensorPorts[i]->getDeviceName().compare(
    	conn->getSensorName())) {
      rserialConnsMutex.lock();
      pendingRserialConns.push_back(conn);
      conn->setSensor(pendingSensorPorts[i]);
      rserialConnsChanged = true;
      rserialConnsMutex.unlock();
      Logger::getInstance()->log(LOG_INFO,
	"added rserial connection for device %s",
		conn->getSensorName().c_str());
      break;
    }
  }
}

/**
 * this is called by a SensorPort from this (PortSelector) thread.
 */
void PortSelector::removeRemoteSerialConnection(RemoteSerialConnection* conn)
{
  Synchronized autosync(rserialConnsMutex);
  for (unsigned int i = 0; i < pendingRserialConns.size(); i++) {
    if (pendingRserialConns[i] == conn) {
        conn->setSensor(0);
	pendingRserialConns.erase(pendingRserialConns.begin()+i);
    }
  }
  pendingRserialClosures.push_back(conn);
  rserialConnsChanged = true;
  Logger::getInstance()->log(LOG_INFO,"%s",
      "PortSelector::removeRemoteSerialConnection");
}


void PortSelector::handleChangedPorts() {
  unsigned int i;
  if (portsChanged) {
    Synchronized autosync(portsMutex);
    activeSensorPortFds = pendingSensorPortFds;
    activeSensorPorts = pendingSensorPorts;
    // close any ports
    for (i = 0; i < pendingSensorPortClosures.size(); i++) {
	pendingSensorPortClosures[i]->close();
	delete pendingSensorPortClosures[i];
    }
    pendingSensorPortClosures.clear();
    portsChanged = false;
  }

  selectn = 0;
  FD_ZERO(&readfdset);
  for (i = 0; i < activeSensorPortFds.size(); i++) {
    if (activeSensorPortFds[i] > selectn) selectn = activeSensorPortFds[i];
    FD_SET(activeSensorPortFds[i],&readfdset);
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

