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

SensorOpener::SensorOpener(SensorHandler* s):
  Thread("SensorOpener"),selector(s)
{
  blockSignal(SIGINT);
  blockSignal(SIGHUP);
  blockSignal(SIGTERM);
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
    cerr << "SensorOpener::interrupt" << endl;
    Thread::interrupt();
    sensorCond.signal();
}

/**
 * Cancel this SensorOpener. Since Cond::wait
 * is not interrupted by a cancel, we must do
 * a sensorCond.signal() first.
 */
void SensorOpener::cancel() throw(n_u::Exception)
{
    cerr << "SensorOpener::cancel" << endl;
    interrupt();
    Thread::cancel();
}


/**
 * Thread function, open sensors.
 */
int SensorOpener::run() throw(n_u::Exception)
{

    for (;;) {
	sensorCond.lock();
	while (!amInterrupted() &&
		(sensors.size() + problemSensors.size()) == 0)
		sensorCond.wait();
	if (amInterrupted()) break;

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

	sensorCond.unlock();
	if (amInterrupted()) break;

	try {
	    sensor->open(sensor->getDefaultMode());
	    selector->sensorOpen(sensor);
	}
	catch(const n_u::IOException& e) {
	    n_u::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		  sensor->getName().c_str(),e.what());

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
    n_u::Logger::getInstance()->log(LOG_INFO,"%s: run method finished",
	  getName().c_str());
    return RUN_OK;
}

