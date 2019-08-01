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

#include "FtdiDSMPowerCtrl.h"

namespace nidas { namespace util {

const std::string FtdiDSMPowerCtrl::rawPowerToStr(unsigned char powerCfg)
{
    std::string powerStr("");
    if (powerCfg & _pwrBit) {
        powerStr.append(STR_POWER_ON);
    }
    else {
        powerStr.append(STR_POWER_OFF);
    }

    return powerStr;
}

POWER_STATE FtdiDSMPowerCtrl::rawPowerToState(unsigned char powerCfg)
{
    POWER_STATE retval = POWER_OFF;
    if (powerCfg & _pwrBit) {
        retval = POWER_ON;
    }

    return retval;
}


FtdiDSMPowerCtrl::FtdiDSMPowerCtrl(GPIO_PORT_DEFS gpio)
: FtdiDSMPowerGPIO(), PowerCtrlAbs(), _pwrPort(gpio), _pwrBit(pwrGpio2bits(gpio))
{
    updatePowerState();
}

void FtdiDSMPowerCtrl::pwrOn()
{
    if (pwrCtrlEnabled()) {
        setBit(_pwrBit);
    }
    else {
        ILOG(("FtdiDSMPowerCtrl::FtdiDSMPowerCtrl(): Power control for device: ") << gpio2Str(getPwrPort())
        																  << " is not enabled");
    }
    updatePowerState();
}

void FtdiDSMPowerCtrl::pwrOff()
{
    if (pwrCtrlEnabled()) {
        resetBit(_pwrBit);
    }
    else {
        ILOG(("FtdiDSMPowerCtrl::FtdiDSMPowerCtrl(): Power control for device: ") << gpio2Str(getPwrPort())
        																  << " is not enabled");
    }
    updatePowerState();
}

void FtdiDSMPowerCtrl::updatePowerState()
{
    setPowerState(rawPowerToState(read()));
    DLOG(("power state: %s", powerStateToStr(getPowerState()).c_str()));
}

}} //namespace nidas { namespace util {
