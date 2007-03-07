/*
********************************************************************
Copyright 2005 UCAR, NCAR, All Rights Reserved

$LastChangedDate$

$LastChangedRevision$

$LastChangedBy$

$HeadURL$
********************************************************************

*/

#include <nidas/core/SensorHandler.h>
#include <nidas/core/DSMEngine.h>
#include <nidas/util/Logger.h>

#include <cerrno>
#include <unistd.h>
#include <csignal>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SensorHandler::SensorHandler(unsigned short rserialPort) :
   Thread("SensorHandler"),sensorsChanged(false),
   remoteSerialSocketPort(rserialPort),rserial(0),rserialConnsChanged(false),
   selectn(0),selectErrors(0),rserialListenErrors(0),opener(this)
{
    setStatisticsPeriod(10 * MSECS_PER_SEC);

    setTimeout(MSECS_PER_SEC * 60);     // one minute select timeout
    FD_ZERO(&readfdset);
    statisticsTime = timeCeiling(getSystemTime(),
                                statisticsPeriod);
    blockSignal(SIGINT);
    blockSignal(SIGHUP);
    blockSignal(SIGTERM);

    if (::pipe(notifyPipe) < 0) {
        // Can't throw exception, but report it. Check at beginning
        // of thread that notifyPipe[0,1] are OK
        n_u::IOException e("SensorHandler","pipe",errno);
        n_u::Logger::getInstance()->log(LOG_WARNING,"%s",e.what());
        notifyPipe[0] = -1;
        notifyPipe[1] = -1;
    }
}

/**
 * Close any remaining sensors. Before this is called
 * the run method should be finished.
 */
SensorHandler::~SensorHandler()
{
   delete rserial;
   for (unsigned int i = 0; i < activeSensors.size(); i++)
      activeSensors[i]->close();

   list<DSMSensor*>::const_iterator si;
   for (si = allSensors.begin(); si != allSensors.end(); ++si)
      delete *si;

    if (notifyPipe[0] >= 0) ::close(notifyPipe[0]);
    if (notifyPipe[1] >= 0) ::close(notifyPipe[1]);
}


void SensorHandler::setTimeout(int val) {
   timeoutMsec = val;
   timeoutVal.tv_sec = val / MSECS_PER_SEC;
   timeoutVal.tv_usec = (val % MSECS_PER_SEC) * USECS_PER_MSEC;
}

int SensorHandler::getTimeout() const {
   return timeoutMsec;
}

void SensorHandler::calcStatistics(dsm_time_t tnow)
{
   statisticsTime += statisticsPeriod;
   if (statisticsTime < tnow) {
      // cerr << "tnow-statisticsTime=" << (tnow - statisticsTime) << endl;
      statisticsTime = timeCeiling(tnow,statisticsPeriod);
   }


   list<DSMSensor*>::const_iterator si;
   for (si = allSensors.begin(); si != allSensors.end(); ++si) {
      DSMSensor *sensor = *si;
      sensor->calcStatistics(statisticsPeriod);
   }
}

/* returns a copy of our sensor list. */
list<DSMSensor*> SensorHandler::getAllSensors() const
{
   n_u::Synchronized autosync(sensorsMutex);
   return allSensors;
}
/* returns a copy of our opened sensors. */
list<DSMSensor*> SensorHandler::getOpenedSensors() const
{
   n_u::Synchronized autosync(sensorsMutex);
   return pendingSensors;
}

/**
 * Thread function, select loop.
 */
int SensorHandler::run() throw(n_u::Exception)
{

   dsm_time_t rtime = 0;
   struct timeval tout;

   if (notifyPipe[0] < 0 || notifyPipe[1] < 0) {
        n_u::IOException e("SensorHandler cmd pipe","pipe",EBADF);
        n_u::Logger::getInstance()->log(LOG_ERR,"%s",e.what());
        throw e;
   }

   opener.start();

   delete rserial;
   rserial = 0;
   if (remoteSerialSocketPort > 0) {
      try {
         rserial = new RemoteSerialListener(remoteSerialSocketPort);
      }
      catch (const n_u::IOException& e) {
         n_u::Logger::getInstance()->log(LOG_WARNING,"%s: continuing anyhow",e.what());
      }
   }

   size_t nsamplesAlloc = 0;
   sensorsChanged = true;

   for (;;) {
      if (amInterrupted()) break;

      if (sensorsChanged || rserialConnsChanged)
         handleChangedSensors();

      fd_set rset = readfdset;
      fd_set eset = readfdset;
      tout = timeoutVal;
      int nfdsel = ::select(selectn,&rset,0,&eset,&tout);
      if (amInterrupted()) break;

      if (nfdsel <= 0) {              // select error
         if (nfdsel < 0) {
            // Create and report but don't throw IOException.
            // Likely this is a EINTR (interrupted) error, in which case
            // the signal handler has set the amInterrupted() flag, which
            // will be caught at the top of the loop.
            n_u::IOException e("SensorHandler","select",errno);
            n_u::Logger::getInstance()->log(LOG_ERR,"%s",e.toString().c_str());
            sensorsChanged = rserialConnsChanged = true;
            if (errno != EINTR) selectErrors++;
         }
         else n_u::Logger::getInstance()->log(LOG_INFO,
                   "SensorHandler select timeout %d msecs",timeoutMsec);
         rtime = getSystemTime();
         if (rtime > statisticsTime) calcStatistics(rtime);
         continue;
      }                               // end of select error section

      int nfd = 0;
      int fd = 0;

      // check sensors
      for (unsigned int ifd = 0; ifd < activeSensorFds.size(); ifd++) {
         fd = activeSensorFds[ifd];
         if (FD_ISSET(fd,&rset)) {
            DSMSensor *sensor = activeSensors[ifd];
            try {
               rtime = sensor->readSamples();
            }
            catch (n_u::IOException &ioe) {
               n_u::Logger::getInstance()->log(LOG_ERR,"%s",ioe.toString().c_str());
               // Try to reopen
               if (sensor->reopenOnIOException()) {
                  try {
                     sensor->close();
                  }
                  catch (n_u::IOException &e) {
                     n_u::Logger::getInstance()->log(LOG_ERR,"%s",
                                                     e.toString().c_str());
                  }
                  reopenSensor(sensor);
               }
               // report error but don't reopen
               else closeSensor(sensor);
            }
            if (++nfd == nfdsel) break;
         }
         // log the error but don't exit
         if (FD_ISSET(fd,&eset)) {
            DSMSensor *sensor = activeSensors[ifd];
            n_u::Logger::getInstance()->log(LOG_ERR,
                                            "SensorHandler select reports exception for %s",
                                            sensor->getDeviceName().c_str());
            if (++nfd == nfdsel) break;
         }
      }
      if (rtime > statisticsTime) {
         calcStatistics(rtime);

         // watch for sample memory leaks
         size_t nsamp = 0;
         list<SamplePoolInterface*> pools =
            SamplePools::getInstance()->getPools();
         for (list<SamplePoolInterface*>::const_iterator pi = pools.begin();
              pi != pools.end(); ++pi) 
         {
            SamplePoolInterface* pool = *pi;
            nsamp += pool->getNSamplesAlloc();
         }
         if (nsamp > nsamplesAlloc) {
            for (list<SamplePoolInterface*>::const_iterator pi =
                    pools.begin(); pi != pools.end(); ++pi) {
               SamplePoolInterface* pool = *pi;
               n_u::Logger::getInstance()->log(LOG_INFO,
                                               "pool nsamples alloc=%d, nsamples out=%d",
                                               pool->getNSamplesAlloc(),pool->getNSamplesOut());
            }
            nsamplesAlloc = nsamp;
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
            catch (n_u::IOException &ioe) {
               n_u::Logger::getInstance()->log(LOG_ERR,"rserial: %s",ioe.toString().c_str());
               removeRemoteSerialConnection(conn);
            }
            if (++nfd == nfdsel) break;
         }
         if (FD_ISSET(fd,&eset)) {
            n_u::Logger::getInstance()->log(LOG_ERR,"%s",
                                            "SensorHandler select reports exception for rserial socket ");
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
            catch (n_u::IOException &ioe) {
               n_u::Logger::getInstance()->log(LOG_ERR,
                   "SensorHandler rserial: %s",ioe.toString().c_str());
               rserialListenErrors++;
            }
            if (++nfd == nfdsel) continue;
         }
         if (FD_ISSET(fd,&eset)) {
            n_u::Logger::getInstance()->log(LOG_ERR,"%s",
                "SensorHandler select reports exception for rserial listen socket ");
            rserialListenErrors++;
            if (++nfd == nfdsel) continue;
         }
      }
      if (notifyPipe[0] >= 0 && FD_ISSET(notifyPipe[0],&rset)) {
          char z;
          if (::read(notifyPipe[0],&z,1) < 0) {
               n_u::IOException e("SensorHandler cmd pipe","read",errno);
               n_u::Logger::getInstance()->log(LOG_ERR,
                   "SensorHandler rserial: %s",e.what());
          }
          // Don't need to do anything, next loop will check
          // for sensorsChanged.
          if (++nfd == nfdsel) continue;
      }
   }

   rserialConnsMutex.lock();
   list<RemoteSerialConnection*> conns = pendingRserialConns;
   rserialConnsMutex.unlock();
          
   list<RemoteSerialConnection*>::iterator ci;
   for (ci = conns.begin(); ci != conns.end(); ++ci)
      removeRemoteSerialConnection(*ci);

   n_u::Logger::getInstance()->log(LOG_INFO,
                                   "SensorHandler finished, closing remaining %d sensors ",activeSensors.size());

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
void SensorHandler::addSensor(DSMSensor *sensor)
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
void SensorHandler::sensorOpen(DSMSensor *sensor)
{
   // cerr << "SensorHandler::sensorOpen" << endl;
   n_u::Synchronized autosync(sensorsMutex);
   pendingSensors.push_back(sensor);
   // cerr << "SensorHandler::sensorOpen pendingSensors.size="  <<
     //   pendingSensors.size() << endl;
   sensorsChanged = true;
   // Write a byte on the notifyPipe to wake up select.
   char dummy = 0;
   if (notifyPipe[1] >= 0) ::write(notifyPipe[1],&dummy,1);
}

/*
 * Add DSMSensor to my list of DSMSensors to be closed, and not reopened.
 */
void SensorHandler::closeSensor(DSMSensor *sensor)
{
   n_u::Synchronized autosync(sensorsMutex);

   list<DSMSensor*>::iterator si = find(pendingSensors.begin(),
                                        pendingSensors.end(),sensor);
   if (si != pendingSensors.end()) pendingSensors.erase(si);

   pendingSensorClosures.push_back(sensor);
   sensorsChanged = true;
   // Write a byte on the notifyPipe to wake up select.
   char dummy = 0;
   if (notifyPipe[1] >= 0) ::write(notifyPipe[1],&dummy,1);
}

/*
 * Make request to SensorOpener thread that DSMSensor be reopened.
 */
void SensorHandler::reopenSensor(DSMSensor *sensor)
{
   n_u::Synchronized autosync(sensorsMutex);

   list<DSMSensor*>::iterator si = find(pendingSensors.begin(),
                                        pendingSensors.end(),sensor);
   if (si != pendingSensors.end()) pendingSensors.erase(si);

   opener.reopenSensor(sensor);
   sensorsChanged = true;
   // Write a byte on the notifyPipe to wake up select.
   char dummy = 0;
   if (notifyPipe[1] >= 0) ::write(notifyPipe[1],&dummy,1);
}

/*
 * Protected method to add a RemoteSerial connection
 */
void SensorHandler::addRemoteSerialConnection(RemoteSerialConnection* conn)
   throw(n_u::IOException)
{
   conn->readSensorName();

   n_u::Synchronized autosync(sensorsMutex);
   list<DSMSensor*>::const_iterator si;
   for (si = pendingSensors.begin(); si != pendingSensors.end(); ++si) {
      DSMSensor* sensor = *si;
      if (!sensor->getDeviceName().compare(conn->getSensorName())) {
         conn->setDSMSensor(sensor); // may throw n_u::IOException

         rserialConnsMutex.lock();
         pendingRserialConns.push_back(conn);
         rserialConnsChanged = true;
         rserialConnsMutex.unlock();

         n_u::Logger::getInstance()->log(LOG_NOTICE,
                                         "added rserial connection for device %s",
                                         conn->getSensorName().c_str());
         return;
      }
   }
   conn->sensorNotFound();
   n_u::Logger::getInstance()->log(LOG_WARNING,
                                   "SensorHandler::addRemoteSerialConnection: cannot find sensor %s",
                                   conn->getSensorName().c_str());
}

/*
 * Remove a RemoteSerialConnection from the current list.
 * This doesn't close or delete the connection, but puts
 * it in the pendingRserialClosures list.
 */
void SensorHandler::removeRemoteSerialConnection(RemoteSerialConnection* conn)
{
   n_u::Synchronized autosync(rserialConnsMutex);
   list<RemoteSerialConnection*>::iterator ci;
   ci = find(pendingRserialConns.begin(),pendingRserialConns.end(),conn);

   if (ci != pendingRserialConns.end()) pendingRserialConns.erase(ci);
   else n_u::Logger::getInstance()->log(LOG_WARNING,"%s",
                                        "SensorHandler::removeRemoteSerialConnection couldn't find connection for %s",
                                        conn->getSensorName().c_str());
   pendingRserialClosures.push_back(conn);
   rserialConnsChanged = true;
}

/* static */
bool SensorHandler::goodFd(int fd, const string& devname) throw()
{
   if (fd < 0) return false;
   struct stat statbuf;
   if (::fstat(fd,&statbuf) < 0) {
      n_u::IOException ioe(devname,
                           "handleChangedSensors fstat",errno);
      n_u::Logger::getInstance()->log(LOG_INFO,"%s",
                                      ioe.what());
      return false;
   }
   return true;
}

void SensorHandler::handleChangedSensors() {
   // cerr << "handleChangedSensors" << endl;
   unsigned int i;
   if (sensorsChanged) {
      n_u::Synchronized autosync(sensorsMutex);

      activeSensors.clear();
      activeSensorFds.clear();

      // cerr << "handleChangedSensors, pendingSensors.size()" <<
      //      pendingSensors.size() << endl;
      list<DSMSensor*>::iterator si = pendingSensors.begin();
      for ( ; si != pendingSensors.end(); ) {
         DSMSensor* sensor = *si;
         if (goodFd(sensor->getReadFd(),sensor->getName())) {
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
      n_u::Logger::getInstance()->log(LOG_INFO,"%d active sensors",
                                      activeSensors.size());
   }

   if (rserialConnsChanged) {
      n_u::Synchronized autosync(rserialConnsMutex);
      activeRserialConns = pendingRserialConns;
      // close and delete any pending remote serial connections
      list<RemoteSerialConnection*>::iterator ci = pendingRserialClosures.begin();
      for (; ci != pendingRserialClosures.end(); ++ci) {
         RemoteSerialConnection* conn = *ci;
         conn->close();
         delete conn;
      }
      pendingRserialClosures.clear();
      rserialConnsChanged = false;
   }

   selectn = 0;
   FD_ZERO(&readfdset);
   if (notifyPipe[0] >= 0) FD_SET(notifyPipe[0],&readfdset);
   selectn = std::max(notifyPipe[0]+1,selectn);

   for (i = 0; i < activeSensorFds.size(); i++) {
      FD_SET(activeSensorFds[i],&readfdset);
      selectn = std::max(activeSensorFds[i]+1,selectn);
   }

   list<RemoteSerialConnection*>::iterator ci = activeRserialConns.begin();
   for (; ci != activeRserialConns.end(); ++ci) {
      RemoteSerialConnection* conn = *ci;
      if (goodFd(conn->getFd(),conn->getName())) {
         FD_SET(conn->getFd(),&readfdset);
         selectn = std::max(conn->getFd()+1,selectn);
      }
      else removeRemoteSerialConnection(conn);
   }

   if (rserial) {
      int fd = rserial->getFd();
      if (goodFd(fd,"rserial server socket")) {
         FD_SET(fd,&readfdset);
         selectn = std::max(fd+1,selectn);
      }
   }
}

