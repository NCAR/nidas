// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 8; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "SensorHandler.h"
#include "DSMEngine.h"
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
        // It is now up to the config to specify how fast is too fast to
        // reopen a sensor, and how quickly to back off on reopen attempts.
        sensor = _problemSensors.front();
        _problemSensors.pop_front();
    }

        if (isInterrupted()) break;
        _sensorCond.unlock();

        try {
            // First wait for the adjusted delay.  There is no delay if this
            // sensor has never timed out, ie, it is being opened for the
            // first time.
            Timeouts& timeouts = sensor->getTimeouts();
            int delay = timeouts.getAdjustedDelayMsecs();
            if (delay > 0) {
                DLOG(("") << sensor->getName() << " delaying opening: "
                          << delay << " ms");
                timeouts.nanosleep_msecs(delay);
            }

            // If the sensor was turned off and it has a warmup time, then
            // turn it back on here and give it time to power up before
            // opening it.  If there is no warmup time needed, then leave the
            // sensor off and let it be powered on in the open() call if the
            // device open succeeds.
            SerialSensor* serial = dynamic_cast<SerialSensor*>(sensor);
            if (serial && timeouts.cyclePowerThisTimeout() &&
                timeouts.getWarmupMsecs() > 0)
            {
                serial->powerOn();
                DLOG(("") << sensor->getName() << " warmup delay: "
                          << timeouts.getWarmupMsecs() << " ms");
                timeouts.nanosleep_msecs(timeouts.getWarmupMsecs());
            }
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
            sensor->setSensorState(SENSOR_OPEN);
            _selector->sensorIsOpen(sensor);
        }
        catch(const n_u::IOException& e) {
            // Treat reopen failures as timeouts so subsequent reopens follow
            // the same delay and backoff settings as for sensor timeouts. If
            // a sensor ends up here without a timeout, it means the open is
            // failing, likely because the device does not exist. In that
            // case, the historical automatic and fixed delay was 10 seconds.
            // Now, the default Timeouts for a sensor allows the reopens to
            // slow down based on the default backoff rate.  If this sensor
            // open is going to fail persistently, then it will probably reach
            // the max delay and slow down reopens on other sensors, probably
            // surprising someone.  Maybe sensors should be removed after a
            // certain number of open failures, until it's possible to block
            // on a single sensor open without blocking other sensors. Or
            // maybe open failures should be tracked and handled differently
            // than timeouts.
            sensor->incrementTimeoutCount();
            if (dynamic_cast<const n_u::IOTimeoutException*>(&e)) {
                // log timeouts as warnings, not errors
                WLOG(("") << sensor->getName() << ": " << e.what());
            }
            else {
                PLOG(("") << sensor->getName() << ": " << e.what());
            }

            // file descriptor may be open if the error happened in
            // some initialization code after the libc ::open.
            try {
                sensor->close();
            }
            catch(const n_u::IOException& e) {
                PLOG(("") << sensor->getName() << ": " << e.what());
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
        PLOG(("") << sensor->getName() << ": " << e.what());
            try {
                sensor->close();
            }
            catch(const n_u::IOException& e) {
                PLOG(("") << sensor->getName() << ": " << e.what());
            }
    }
    }
    _sensorCond.unlock();
    return RUN_OK;
}
