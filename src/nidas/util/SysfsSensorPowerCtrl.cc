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

#include "SysfsSensorPowerCtrl.h"
#include "util.h"

#include <string>

namespace nidas { namespace util {

std::string SysfsSensorPowerCtrl::rawPowerToStr(unsigned char powerCfg)
{
    return powerStateToStr((powerCfg == '1') ? POWER_ON : POWER_OFF);
}

POWER_STATE SysfsSensorPowerCtrl::rawPowerToState(unsigned char powerCfg)
{
    DLOG(("SysfsSensorPowerCtrl::rawPowerToState(): powerCfg = 0x%02X", powerCfg));
    return (powerCfg == '1') ? POWER_ON : POWER_OFF;
}

SysfsSensorPowerCtrl::SysfsSensorPowerCtrl(GPIO_PORT_DEFS port)
: SysfsGpio(gpioPort2RpiGpio(port)), PowerCtrlAbs(), _port(port)
{
    if (!RANGE_CHECK_INC(SER_PORT0, port, SER_PORT7)) {
        throw IOException("SysfsSensorPowerCtrl::SysfsSensorPowerCtrl()", "Illegal/Unknown sensor port definition", port);
    }

    DLOG(("SysfsSensorPowerCtrl::SysfsSensorPowerCtrl(): SysfsGpio instantiated, updating power state"));
    updatePowerState();
}

void SysfsSensorPowerCtrl::pwrOn()
{
    if (pwrCtrlEnabled()) {
        Sync sync(this);
        write('1');
    }
    else {
        ILOG(("SysfsSensorPowerCtrl::SysfsSensorPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
    updatePowerState();
}

void SysfsSensorPowerCtrl::pwrOff()
{
    if (pwrCtrlEnabled()) {
        Sync sync(this);
        write('0');
    }
    else {
        ILOG(("SysfsSensorPowerCtrl::SysfsSensorPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
    updatePowerState();
}

void SysfsSensorPowerCtrl::updatePowerState()
{
    Sync sync(this);
    setPowerState(rawPowerToState(read()));
    DLOG(("power state: %s", powerStateToStr(getPowerState()).c_str()));
}

}} //namespace nidas { namespace util {
