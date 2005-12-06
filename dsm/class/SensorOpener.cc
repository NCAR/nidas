/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-11-20 08:58:12 -0700 (Sun, 20 Nov 2005) $

    $LastChangedRevision: 3133 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/PortSelector.cc $
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

SensorOpener::SensorOpener(PortSelector* s):
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
    Thread::interrupt();
    sensorCond.signal();
}

/**
 * Cancel this SensorOpener. Since Cond::wait
 * is not interrupted by a cancel, we must do
 * a sensorCond.signal() first.
 */
void SensorOpener::cancel() throw(atdUtil::Exception)
{
    interrupt();
    Thread::cancel();
}


/**
 * Thread function, open sensors.
 */
int SensorOpener::run() throw(atdUtil::Exception)
{

    for (;;) {
	sensorCond.lock();
	while (!isInterrupted() &&
		(sensors.size() + problemSensors.size()) == 0)
		sensorCond.wait();
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

	sensorCond.unlock();
	if (amInterrupted()) break;

	try {
	    sensor->open(sensor->getDefaultMode());
	    selector->sensorOpen(sensor);
	}
	catch(const atdUtil::IOException& e) {
	    atdUtil::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		  sensor->getName().c_str(),e.what());

	    sensorCond.lock();
	    problemSensors.push_back(sensor);
	    sensorCond.unlock();
	}
	// On InvalidParameterException, report the error
	// and don't try to open again.  Time will
	// not likely fix an InvalidParameterException,
	// it needs human interaction.
	catch(const atdUtil::InvalidParameterException& e) {
	    atdUtil::Logger::getInstance()->log(LOG_ERR,"%s: %s",
		  sensor->getName().c_str(),e.what());
	    
	}
    }
    atdUtil::Logger::getInstance()->log(LOG_INFO,"%s: run method finished",
	  getName().c_str());
    return RUN_OK;
}

