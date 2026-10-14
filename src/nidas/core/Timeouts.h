// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_CORE_TIMEOUTS_H
#define NIDAS_CORE_TIMEOUTS_H

#include <nidas/util/time_constants.h>
#include <string>
#include <iosfwd>

namespace nidas { namespace core {

/**
 * Timeouts contains settings and state to implement sensor timeouts. If a
 * sensor does not report any data after the timeout period, stored in
 * milliseconds, then it can be reopened according to the other settings in
 * this class.
 * 
 * There is a counter for the number of timeouts that have occurred, and that
 * counter determines when a power cycle should be triggered and how long to
 * delay each reopen based on the backoff rate.
 */
class Timeouts
{
public:
    static const int MAXDELAY_LIMIT_MSECS{300*MSECS_PER_SEC};
    static const int DELAY_MSECS_DEFAULT{10*MSECS_PER_SEC};
    static const int MINDELAY_LIMIT_MSECS{1*MSECS_PER_SEC};

    /**
     * Construct a Timeouts instance.
     *
     * @param timeoutMsecs The timeout period in milliseconds.
     * @param delayMsecs The initial delay before a reopen after a timeout.
     * @param backoffRate The rate at which the delay increases with each
     * timeout.
     * @param maxDelayMsecs The maximum allowed delay before a reopen.
     * @param cycleNumTimeouts The number of timeouts before a power cycle is
     * triggered.
     * @param warmupMsecs The warmup period after a power cycle before
     * reopening.
     * 
     * The default delay matches original behavior where sensors were
     * automatically delayed by 10 seconds to avoid runaway reopen attempts.
     * The minimum delay is fixed at 1 second to prevent excessively rapid
     * reopen attempts.
     */
    Timeouts(int timeoutMsecs=0, int delayMsecs=DELAY_MSECS_DEFAULT,
             float backoffRate=1.0,
             int maxDelayMsecs=MAXDELAY_LIMIT_MSECS, int cycleNumTimeouts=0,
             int warmupMsecs=0, int nTimeouts=0);

    Timeouts& setPowerCycle(int cycleNumTimeouts, int warmupMsecs=0);

    void incrementTimeouts();

    void setTimeoutMsecs(int val);

    /**
     * Return true if the sensor should be power cycled on this timeout, false
     * otherwise.  For example, if the cycle_n_timeouts is set to 3, the
     * sensor should be power cycled on every third timeout.  The expected
     * states are timeout -> power off -> wait for getDelayMsecs() -> power on
     * -> wait for getWarmupMsecs() -> reopen.
     */
    bool cyclePowerThisTimeout() const;

    int getNumTimeouts() const;

    /**
     * Return the number of power cycles that should have occurred based on
     * the number of timeouts, ie, this is the number of times
     * cyclePowerThisTimeout() would have returned true for the current number
     * of timeouts.
     */
    int getNumPowerCycles() const;

    int getTimeoutMsecs() const;

    int getCycleNumTimeouts() const;

    /**
     * Get the milliseconds to delay before a reopen after a timeout.  This is
     * adjusted according to the backoff rate, number of timeouts so far, and
     * the maximum allowed delay.  If there have been no timeouts, then the
     * delay is zero.
     */
    int getDelayMsecs() const;

    /**
     * Return an additional delay in milliseconds to wait after powering on
     * the sensor before opening the device.
     */
    int getWarmupMsecs() const;

    /**
     * Parse a string @p value containing timeout settings in this syntax:
     * 
     * <timeout>[,<delay>[,<backoff>[,<maxdelay>]]] [<cycle_n_timeouts>[,<warmup>]]
     * 
     * The timeout, delay, and warmup times are specified in floating point
     * seconds and will be converted to integer milliseconds.  As much as
     * possible of the string is parsed and modifies this object, even if the
     * parsing eventually fails.  Additional fields are ignored to allow
     * forward compatibility.
     *
     * @return True if the parsing was successful, false otherwise.
     */
    bool parse(const std::string& value);

    static int seconds_to_msecs(float seconds);

    void nanosleep_msecs(int msecs);

    std::string toString() const;

    bool operator==(const Timeouts& other) const;

private:

    bool checkValid();

    /**
     * Timeout period in milliseconds. If the sensor does not report any data
     * within this period, it may be reopened.
     */
    int timeout_msecs;

    /**
     * Initial delay before attempting to reopen the sensor after a timeout.
     */
    int delay_msecs;

    /**
     * Backoff rate for increasing the delay between successive reopen
     * attempts.
     */
    float backoff_rate;

    /**
     * Maximum delay between successive reopen attempts.  This is currently
     * limited to MAXDELAY_LIMIT_MSECS, which is hardcoded at 5 minutes,
     * because all the reopens run in the same thread. Delaying one sensor
     * reopen too long could also delay other reopens.
     */
    int maxdelay_msecs;

    /**
     * Cycle the sensor power after every interval of this many timeouts.
     */
    int cycle_n_timeouts;

    /**
     * How long to delay opening the sensor device after powering it back on.
     * There is no harm in this being short unless the sensor requires an init
     * string and must reach a state to read it.  The shorter the warmup, the
     * more likely the sensor startup messages can be recorded.
     */
    int warmup_msecs;

    /**
     * Number of timeouts that have occurred.
     */
    int n_timeouts;
};


std::ostream& operator<<(std::ostream& os, const Timeouts& timeouts);


}} // namespace nidas::core

#endif // NIDAS_CORE_TIMEOUTS_H
