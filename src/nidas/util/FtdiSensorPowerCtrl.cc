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

#include "FtdiSensorPowerCtrl.h"

#include <string>

namespace nidas { namespace util {

const std::string FtdiSensorPowerCtrl::rawPowerToStr(unsigned char powerCfg)
{
    std::string powerStr("");
    if (powerCfg & SENSOR_BITS_POWER) {
        powerStr.append(STR_POWER_ON);
    }
    else {
        powerStr.append(STR_POWER_OFF);
    }

    return powerStr;
}

POWER_STATE FtdiSensorPowerCtrl::rawPowerToState(unsigned char powerCfg)
{
    POWER_STATE retval = POWER_OFF;
    if (powerCfg & SENSOR_BITS_POWER) {
        retval = POWER_ON;
    }

    return retval;
}


FtdiSensorPowerCtrl::FtdiSensorPowerCtrl(GPIO_PORT_DEFS port)
: FtdiXcvrGPIO(port), PowerCtrlAbs(), _port(port)
{
    updatePowerState();
}

void FtdiSensorPowerCtrl::pwrOn()
{
    if (pwrCtrlEnabled()) {
        write(SENSOR_BITS_POWER, SENSOR_BITS_POWER);
    }
    else {
        ILOG(("FtdiSensorPowerCtrl::FtdiSensorPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
    updatePowerState();
}

void FtdiSensorPowerCtrl::pwrOff()
{
    if (pwrCtrlEnabled()) {
        write(0, SENSOR_BITS_POWER);
    }
    else {
        ILOG(("FtdiSensorPowerCtrl::FtdiSensorPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
    updatePowerState();
}

void FtdiSensorPowerCtrl::updatePowerState()
{
    setPowerState(rawPowerToState(read()));
    DLOG(("power state: %s", powerStateToStr(getPowerState()).c_str()));
}

}} //namespace nidas { namespace util {
