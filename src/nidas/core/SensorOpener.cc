// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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
    Thread("SensorOpener"),_selector(s),
    _sensors(),_problemSensors(),_sensorCond()
{
    blockSignal(SIGUSR1);
}

/**
 * Note that SensorOpener does not own the sensors, and does not delete them.
 * The SensorHander is responsible for deleting them.
 */
SensorOpener::~SensorOpener()
{
}

/**
 * Called from the main thread
 */
void SensorOpener::openSensor(DSMSensor *sensor)
{
    _sensorCond.lock();
    _sensors.push_back(sensor);
    _sensorCond.signal();
    _sensorCond.unlock();
}

/**
 * Called from the main thread
 */
void SensorOpener::reopenSensor(DSMSensor *sensor)
{
    _sensorCond.lock();
    _problemSensors.push_back(sensor);
    _sensorCond.signal();
    _sensorCond.unlock();
}

/**
 * Interrupt this SensorOpener. Send a _sensorCond.signal()
 * so it will see the interrupt.
 */
void SensorOpener::interrupt()
{
    Thread::interrupt();
    _sensorCond.lock();
    _sensorCond.signal();
    _sensorCond.unlock();
    // This thread may be in the middle of an sensor->open(), which may
    // do a fair amount of initialization, including I/O.
    // 
    // We block SIGUSR1 in this thread, so that it can be
    // caught by pselect/ppoll. If sensors do blocking reads
    // in their open method, they should use readBuffer() with
    // a timeout, which does a pselect while atomically unblocking
    // SIGUSR1.
    // We're trying to avoid using cancel(), since it is so hard to
    // make sure things are cleaned up, and we have little control over
    // what is done in the sensor open methods.
    try {
        kill(SIGUSR1);
    }
    catch(const n_u::Exception& e) {
        WLOG(("%s",e.what()));
    }
}

/**
 * Thread function, open sensors.
 */
int SensorOpener::run() throw(n_u::Exception)
{

    // If cancel() is used in the interrupt() method,
    // don't have _sensorCond locked when executing a cancelation
    // point, such as amInterupted(), or sleeps, or the sensor open.

    for (;;) {
	_sensorCond.lock();
	while (!isInterrupted() && (_sensors.size() + _problemSensors.size()) == 0) {
            _sensorCond.wait();
        }

	if (isInterrupted()) break;

	DSMSensor* sensor = 0;
	if (_sensors.size() > 0) {
	    sensor = _sensors.front();
	    _sensors.pop_front();
	}
	else {
	    // don't pound on the recalcitrant sensors too fast

	    _sensorCond.unlock();
	    struct timespec sleepPeriod = {10,0};
	    nanosleep(&sleepPeriod,0);
	    _sensorCond.lock();

	    sensor = _problemSensors.front();
	    _problemSensors.pop_front();
	}

        if (isInterrupted()) break;
        _sensorCond.unlock();

        try {
            sensor->open(sensor->getDefaultMode());

            // sensor->open might take a while, so check for interrupted again.
            _sensorCond.lock();
            if (isInterrupted()) {
                try {
                    sensor->close();
                }
                catch(const n_u::IOException& e) {
                    PLOG(("%s: %s", sensor->getName().c_str(),e.what()));
                }
                break;  // _sensorCond is unlocked after the for loop
            }
            _sensorCond.unlock();

            // It is tempting to make this call to sensorIsOpen() while
            // _sensorCond is locked.  Then the SensorHandler could be sure
            // that sensorIsOpen() is not called after calling interrupt()
            // on this thread.  However since sensorIsOpen() holds a lock
            // in the SensorHander it would be too difficult to prevent
            // a thread deadlock bug.
            _selector->sensorIsOpen(sensor);
        }
        catch(const n_u::IOException& e) {
            if (dynamic_cast<const n_u::IOTimeoutException*>(&e)) {
                sensor->incrementTimeoutCount();
                // log timeouts as warnings, not errors
                WLOG(("%s: %s",sensor->getName().c_str(),e.what()));
            }
            else {
                PLOG(("%s: %s",sensor->getName().c_str(),e.what()));
            }

            // file descriptor may be open if the error happened in
            // some initialization code after the libc ::open.
            try {
                sensor->close();
            }
            catch(const n_u::IOException& e) {
                PLOG(("%s: %s", sensor->getName().c_str(),e.what()));
            }
	    _sensorCond.lock();
	    _problemSensors.push_back(sensor);
	    _sensorCond.unlock();
	}
	// On InvalidParameterException, report the error
	// and don't try to open again.  Time will
	// not likely fix an InvalidParameterException,
	// it needs human interaction.
	catch(const n_u::InvalidParameterException& e) {
	    PLOG(("%s: %s", sensor->getName().c_str(),e.what()));
            try {
                sensor->close();
            }
            catch(const n_u::IOException& e) {
                PLOG(("%s: %s", sensor->getName().c_str(),e.what()));
            }
	}
    }
    _sensorCond.unlock();
    return RUN_OK;
}
