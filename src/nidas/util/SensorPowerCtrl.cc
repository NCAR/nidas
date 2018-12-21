// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2018, Copyright University Corporation for Atmospheric Research
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

#include "SensorPowerCtrl.h"
#include "Logger.h"
#include "UTime.h"
#include "SerialXcvrCtrl.h"

using namespace nidas::core;

namespace nidas { namespace util {

SensorPowerCtrl::SensorPowerCtrl(PORT_DEFS port)
: SerialGPIO(port2iface(port)), _port(port), _pwrIsOn(false)
{
    // Intentionall left blank
}

virtual void SensorPowerCtrl::pwrOn()
{
    if (pwrCtrlEnabled()) {
        unsigned char pins = readInterface();
        pins |= BITS_POWER;
        writeInterface(pins);
        _pwrIsOn = true;
    }
    else {
        DLOG(("SerialPowerCtrl::SerialPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
}

virtual void SensorPowerCtrl::pwrOff()
{
    if (pwrCtrlEnabled()) {
        unsigned char pins = readInterface();
        pins &= ~BITS_POWER;
        writeInterface(pins);
        _pwrIsOn = false;
    }
    else {
        DLOG(("SerialPowerCtrl::SerialPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
}

virtual void SensorPowerCtrl::pwrReset(uint32_t pwrOnDelayMs=0, uint32_t pwrOffDelayMs=0)
{

    if (pwrCtrlEnabled()) {
        if (pwrOffDelayMs) {
            sleepUntil(pwrOffDelayMs);
        }
        pwrOff();

        if (pwrOnDelayMs) {
            sleepUntil(pwrOnDelayMs);
        }
        pwrOn();
    }
    else {
        DLOG(("SerialPowerCtrl::SerialPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
}

virtual bool SensorPowerCtrl::pwrIsOn()
{
    return _pwrIsOn;
}

}} //namespace nidas { namespace util {
