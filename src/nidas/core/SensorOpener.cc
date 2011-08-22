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
#include <nidas/util/IOTimeoutException.h>

#include <cerrno>
#include <unistd.h>
#include <csignal>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SensorOpener::SensorOpener(SensorHandler* s):
  Thread("SensorOpener"),selector(s)
{
  blockSignal(SIGINT);
  blockSignal(SIGHUP);
  blockSignal(SIGTERM);
  blockSignal(SIGUSR2);
  unblockSignal(SIGUSR1);
}

/**
 * Close any remaining sensors. Before this is called
 * the run method should be finished.
 */
SensorOpener::~SensorOpener()
{
}

/**
 * Called from the main thread
 */
void SensorOpener::openSensor(DSMSensor *sensor)
{
    sensorCond.lock();
    sensors.push_back(sensor);
    sensorCond.signal();
    sensorCond.unlock();
}

/**
 * Called from the main thread
 */
void SensorOpener::reopenSensor(DSMSensor *sensor)
{
    sensorCond.lock();
    problemSensors.push_back(sensor);
    sensorCond.signal();
    sensorCond.unlock();
}

/**
 * Interrupt this SensorOpener. Send a sensorCond.signal()
 * so it will see the interrupt.
 */
void SensorOpener::interrupt()
{
    Thread::interrupt();
    sensorCond.lock();
    sensorCond.signal();
    sensorCond.unlock();
#ifdef DO_KILL
    // It may be in the middle of initialization I/O to a sensor,
    // so send it a signal which should cause a EINTR
    try {
        kill(SIGUSR1);
    }
    catch(const n_u::Exception& e) {
        WLOG(("%s",e.what()));
    }
#else
    try {
        cancel();
    }
    catch(const n_u::Exception& e) {
        WLOG(("%s",e.what()));
    }
#endif
}

/**
 * Thread function, open sensors.
 */
int SensorOpener::run() throw(n_u::Exception)
{

    // This thread can be canceled, so don't have sensorCond locked
    // when executing a cancelation point, such as amInterupted(),
    // or sleeps, or the sensor open.

    for (;;) {
	sensorCond.lock();
	while (!isInterrupted() && (sensors.size() + problemSensors.size()) == 0) {
            sensorCond.wait();
        }

	if (isInterrupted()) break;

	DSMSensor* sensor = 0;
	if (sensors.size() > 0) {
	    sensor = sensors.front();
	    sensors.pop_front();
	}
	else {
	    // don't pound on the recalcitrant sensors too fast

	    sensorCond.unlock();
	    struct timespec sleepPeriod = {10,0};
	    nanosleep(&sleepPeriod,0);
	    sensorCond.lock();

	    sensor = problemSensors.front();
	    problemSensors.pop_front();
	}

	if (isInterrupted()) break;
	sensorCond.unlock();

	try {
	    sensor->open(sensor->getDefaultMode());
	    selector->sensorOpen(sensor);
	}
	catch(const n_u::IOException& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		  sensor->getName().c_str(),e.what());

	    if (dynamic_cast<const n_u::IOTimeoutException*>(&e))
		sensor->incrementTimeoutCount();

            // file descriptor may still be open if the
            // error happened after the libc ::open
            // during some initialization.
            try {
                sensor->close();
            }
            catch(const n_u::IOException& e) {
                n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
                      sensor->getName().c_str(),e.what());
            }
	    sensorCond.lock();
	    problemSensors.push_back(sensor);
	    sensorCond.unlock();
	}
	// On InvalidParameterException, report the error
	// and don't try to open again.  Time will
	// not likely fix an InvalidParameterException,
	// it needs human interaction.
	catch(const n_u::InvalidParameterException& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		  sensor->getName().c_str(),e.what());
	    
	}
    }
    sensorCond.unlock();
    return RUN_OK;
}

