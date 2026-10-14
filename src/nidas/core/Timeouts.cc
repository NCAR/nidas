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

#include "Timeouts.h"

#include <cmath>
#include <ctime>
#include <ostream>
#include <sstream>

namespace nidas { namespace core {

// Definitions for the static constants initialized in the class declaration,
// needed when they are bound to a reference.
const int Timeouts::MAXDELAY_LIMIT_MSECS;
const int Timeouts::DELAY_MSECS_DEFAULT;
const int Timeouts::MINDELAY_LIMIT_MSECS;

Timeouts::Timeouts(int timeoutMsecs, int delayMsecs, float backoffRate,
                   int maxDelayMsecs, int cycleNumTimeouts,
                   int warmupMsecs):
    timeout_msecs(timeoutMsecs),
    delay_msecs(delayMsecs),
    backoff_rate(backoffRate),
    maxdelay_msecs(maxDelayMsecs),
    cycle_n_timeouts(cycleNumTimeouts),
    warmup_msecs(warmupMsecs),
    n_timeouts(0),
    total_n_timeouts(0),
    total_n_power_cycles(0)
{
    // force valid values for all fields.
    checkValid();
}

Timeouts& Timeouts::setPowerCycle(int cycleNumTimeouts, int warmupMsecs)
{
    cycle_n_timeouts = cycleNumTimeouts;
    warmup_msecs = warmupMsecs;
    checkValid();
    return *this;
}

void Timeouts::incrementTimeouts()
{
    ++n_timeouts;
    ++total_n_timeouts;
    if (cyclePowerThisTimeout()) {
        ++total_n_power_cycles;
    }
}

void Timeouts::setTimeoutMsecs(int val)
{
    timeout_msecs = val;
    checkValid();
}

void Timeouts::setDelayMsecs(int val)
{
    delay_msecs = val;
    checkValid();
}

bool Timeouts::cyclePowerThisTimeout() const
{
    return n_timeouts > 0 && cycle_n_timeouts > 0 &&
        n_timeouts % cycle_n_timeouts == 0;
}

int Timeouts::getNumTimeouts() const
{
    return n_timeouts;
}

int Timeouts::getTotalNumTimeouts() const
{
    return total_n_timeouts;
}

int Timeouts::getTotalNumPowerCycles() const
{
    return total_n_power_cycles;
}

int Timeouts::getTimeoutMsecs() const
{
    return timeout_msecs;
}

int Timeouts::getCycleNumTimeouts() const
{
    return cycle_n_timeouts;
}

int Timeouts::getDelayMsecs() const
{
    return delay_msecs;
}

int Timeouts::getAdjustedDelayMsecs() const
{
    // if no timeouts, then no delay, such as on the initial sensor open().
    // on the first timeout, the delay is just the initial delay
    // (delay_msecs).  i think it's safe to assume this will never overflow,
    // but that's a good reason to keep the minimum delay check here.
    float delay{0};
    if (n_timeouts > 0)
    {
        delay = (backoff_rate * (n_timeouts - 1) + 1.0) * delay_msecs;
        if (delay > maxdelay_msecs) {
            delay = maxdelay_msecs;
        }
        if (delay > MAXDELAY_LIMIT_MSECS) {
            delay = MAXDELAY_LIMIT_MSECS;
        }
        if (delay < MINDELAY_LIMIT_MSECS) {
            delay = MINDELAY_LIMIT_MSECS;
        }
    }
    return static_cast<int>(delay);
}

int Timeouts::getWarmupMsecs() const
{
    return warmup_msecs;
}

namespace {

// If the next character is a comma (after skipping all whitespace), then
// accept it as part of the current field and parse it.  If a number is not
// present, ok is set to false.
bool parse_optional(std::istringstream& ist, bool& ok, float& value)
{
    if (ok) {
        ist >> std::ws;
    }
    if (ok && ist.peek() == ',') {
        float temp;
        ist.get();
        ok = bool(ist >> temp);
        if (ok) {
            value = temp;
        }
        return ok;
    }
    // the value was not present and should not be changed
    return false;
}

bool parse_optional_msecs(std::istringstream& ist, bool& ok, int& msecs)
{
    float value{0};
    if (parse_optional(ist, ok, value))
    {
        msecs = Timeouts::seconds_to_msecs(value);
        return true;
    }
    return false;
}

} // anonymous namespace

bool Timeouts::parse(const std::string& value)
{
    // Need at least one value for the timeout in seconds.
    std::istringstream ist(value);
    float timeout{0};
    bool ok = bool(ist >> timeout);
    if (ok) {
        timeout_msecs = seconds_to_msecs(timeout);
    }
    parse_optional_msecs(ist, ok, delay_msecs);
    parse_optional(ist, ok, backoff_rate);
    parse_optional_msecs(ist, ok, maxdelay_msecs);

    // Now should either be at the end or a power cycle field.  Read the
    // cycle count as a float so a fractional count is an error rather than
    // being truncated, which would also leave the rest of the field unparsed.
    float temp;
    if (ok && !(ist >> temp)) {
        ok = ist.eof();
    }
    else if (ok && temp != std::floor(temp)) {
        ok = false;
    }
    else if (ok) {
        cycle_n_timeouts = static_cast<int>(temp);
        parse_optional_msecs(ist, ok, warmup_msecs);
    }
    // force all fields to be valid, but fail if any were not
    ok = checkValid() && ok;
    return ok;
}

int Timeouts::seconds_to_msecs(float seconds)
{
    return (int)rint(seconds * MSECS_PER_SEC);
}

void Timeouts::nanosleep_msecs(int msecs)
{
    if (msecs <= 0)
        return;
    struct timespec ts;
    ts.tv_sec = msecs / MSECS_PER_SEC;
    ts.tv_nsec = (msecs % MSECS_PER_SEC) * NSECS_PER_MSEC;
    nanosleep(&ts, 0);
}

std::string Timeouts::toString() const
{
    std::ostringstream oss;
    oss << "timeout_msecs=" << timeout_msecs
        << ", delay_msecs=" << delay_msecs
        << ", backoff_rate=" << backoff_rate
        << ", maxdelay_msecs=" << maxdelay_msecs
        << ", cycle_n_timeouts=" << cycle_n_timeouts
        << ", warmup_msecs=" << warmup_msecs
        << ", n_timeouts=" << n_timeouts
        << ", total_n_timeouts=" << total_n_timeouts
        << ", total_n_power_cycles=" << total_n_power_cycles;
    return oss.str();
}

bool Timeouts::operator==(const Timeouts& other) const
{
    return timeout_msecs == other.timeout_msecs &&
           delay_msecs == other.delay_msecs &&
           backoff_rate == other.backoff_rate &&
           maxdelay_msecs == other.maxdelay_msecs &&
           cycle_n_timeouts == other.cycle_n_timeouts &&
           warmup_msecs == other.warmup_msecs &&
           n_timeouts == other.n_timeouts &&
           total_n_timeouts == other.total_n_timeouts &&
           total_n_power_cycles == other.total_n_power_cycles;
}

bool Timeouts::checkValid()
{
    bool ok = true;
    if (timeout_msecs < 0) {
        timeout_msecs = 0;
        ok = false;
    }
    if (delay_msecs < MINDELAY_LIMIT_MSECS) {
        delay_msecs = MINDELAY_LIMIT_MSECS;
        ok = false;
    }
    if (delay_msecs > MAXDELAY_LIMIT_MSECS) {
        delay_msecs = MAXDELAY_LIMIT_MSECS;
        ok = false;
    }
    if (maxdelay_msecs < MINDELAY_LIMIT_MSECS) {
        maxdelay_msecs = MINDELAY_LIMIT_MSECS;
        ok = false;
    }
    if (maxdelay_msecs > MAXDELAY_LIMIT_MSECS) {
        maxdelay_msecs = MAXDELAY_LIMIT_MSECS;
        ok = false;
    }
    if (backoff_rate < 0) {
        backoff_rate = 0;
        ok = false;
    }
    if (cycle_n_timeouts < 0) {
        cycle_n_timeouts = 0;
        ok = false;
    }
    if (warmup_msecs < 0) {
        warmup_msecs = 0;
        ok = false;
    }
    return ok;
}

std::ostream& operator<<(std::ostream& os, const Timeouts& timeouts)
{
    os << timeouts.toString();
    return os;
}

}} // namespace nidas::core
