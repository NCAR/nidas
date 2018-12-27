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

namespace nidas { namespace util {

const std::string SensorPowerCtrl::rawPowerToStr(unsigned char powerCfg)
{
    std::string powerStr("");
    if (powerCfg & BITS_POWER) {
        powerStr.append(STR_POWER_ON);
    }
    else {
        powerStr.append(STR_POWER_OFF);
    }

    return powerStr;
}

POWER_STATE SensorPowerCtrl::rawPowerToState(unsigned char powerCfg)
{
    POWER_STATE retval = POWER_OFF;
    if (powerCfg & BITS_POWER) {
        retval = POWER_ON;
    }

    return retval;
}


SensorPowerCtrl::SensorPowerCtrl(PORT_DEFS port)
: SerialGPIO(port2iface(port)), PowerCtrlAbs(), _port(port)
{
    retrievePowerState();
}

void SensorPowerCtrl::pwrOn()
{
    if (deviceFound() && pwrCtrlEnabled()) {
        Sync sync(this);
        unsigned char pins = readInterface();
        pins |= BITS_POWER;
        writeInterface(pins);
    }
    else {
        DLOG(("SerialPowerCtrl::SerialPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
    retrievePowerState();
}

void SensorPowerCtrl::pwrOff()
{
    if (deviceFound() && pwrCtrlEnabled()) {
        Sync sync(this);
        unsigned char pins = readInterface();
        pins &= ~BITS_POWER;
        writeInterface(pins);
    }
    else {
        DLOG(("SerialPowerCtrl::SerialPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
    retrievePowerState();
}

void SensorPowerCtrl::retrievePowerState()
{
    if (deviceFound()) {
        Sync sync(this);
        unsigned char ifaceState = readInterface();

        DLOG(("current interface state: %x", int(ifaceState)));
        if (_port % 2) ifaceState >>= 4;
        DLOG(("interface state after shift: %x", (int)ifaceState));
        setPowerState(rawPowerToState(ifaceState));
        DLOG(("power state: %s", powerStateToStr(getPowerState()).c_str()));
    }
}

}} //namespace nidas { namespace util {
