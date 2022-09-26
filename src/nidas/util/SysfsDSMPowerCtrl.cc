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

#include "SysfsDSMPowerCtrl.h"
#include "util.h"

namespace nidas { namespace util {


const std::string SysfsDSMPowerCtrl::rawPowerToStr(unsigned char powerCfg)
{
    return powerStateToStr((powerCfg == '1') ? POWER_ON : POWER_OFF);
}


POWER_STATE SysfsDSMPowerCtrl::rawPowerToState(unsigned char powerCfg)
{
    return (powerCfg == '1') ? POWER_ON : POWER_OFF;
}


SysfsDSMPowerCtrl::SysfsDSMPowerCtrl(GPIO_PORT_DEFS gpio)
: SysfsGpio(gpioPort2RpiGpio(gpio)), PowerCtrlAbs(), _iface(gpio)
{
    if (!RANGE_CHECK_INC(PWR_28V, gpio, PWR_BTCON)) {
        throw IOException("SysfsDSMPowerCtrl::SysfsDSMPowerCtrl()", "Illegal/Unknown DSM power control GPIO definition", gpio);
    }

    DLOG(("SysfsDSMPowerCtrl::SysfsDSMPowerCtrl(): SysfsGpio instantiated, updating power state"));
    updatePowerState();
}

void SysfsDSMPowerCtrl::pwrOn()
{
    if (pwrCtrlEnabled()) {
        Sync sync(this);
        write('1');
    }
    else {
        ILOG(("SysfsDSMPowerCtrl::SysfsDSMPowerCtrl(): Power control for device: ") << gpio2Str(getPwrIface())
        																  << " is not enabled");
    }
    updatePowerState();
}

void SysfsDSMPowerCtrl::pwrOff()
{
    if (pwrCtrlEnabled()) {
        Sync sync(this);
        write('0');
    }
    else {
        ILOG(("SysfsDSMPowerCtrl::SysfsDSMPowerCtrl(): Power control for device: ") << gpio2Str(getPwrIface())
        																  << " is not enabled");
    }
    updatePowerState();
}

void SysfsDSMPowerCtrl::updatePowerState()
{
    Sync sync(this);
    setPowerState(rawPowerToState(read()));
    DLOG(("power state: %s", powerStateToStr(getPowerState()).c_str()));
}

}} //namespace nidas { namespace util {
