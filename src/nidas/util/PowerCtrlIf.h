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
#ifndef NIDAS_UTIL_POWERCTRLIF_H
#define NIDAS_UTIL_POWERCTRLIF_H

#include <string>

namespace nidas { namespace util {

/**
 * Sensor power setting
 */
typedef enum
{
    ILLEGAL_POWER=-1,
    POWER_OFF=0,
    POWER_ON,
    POWER_RESETTING
}
POWER_STATE;

// This utility converts a string to the POWER_STATE enum
POWER_STATE strToPowerState(const std::string& powerStr);

std::string powerStateToStr(POWER_STATE sensorState);

/**
 *  This class provides the standard interface for controlling power to
 *  individual HW objects in NIDAS projects, such as DSM, etc.
 */
class PowerCtrlIf {
public:
    virtual ~PowerCtrlIf();
    virtual void enablePwrCtrl(bool enable) = 0;
    virtual bool pwrCtrlEnabled() = 0;
    virtual void setPower(POWER_STATE newPwrState) = 0;
    virtual void setPowerState(POWER_STATE newPwrState) = 0;
    virtual POWER_STATE getPowerState() = 0;
    virtual void pwrOn() = 0;
    virtual void pwrOff() = 0;
    virtual void pwrReset(uint32_t pwrOnDelayMs=0, uint32_t pwrOffDelayMs=0) = 0;
    virtual bool pwrIsOn() = 0;
    virtual void print() = 0;
    virtual bool ifaceAvailable() = 0;
    virtual void updatePowerState() = 0;
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_POWERCTRLIF_H
