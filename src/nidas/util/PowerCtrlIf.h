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

#include <algorithm>
#include <stdint.h>

namespace nidas { namespace util {

/*
 * Sensor power setting
 */
typedef enum {ILLEGAL_POWER=-1, POWER_OFF=0, POWER_ON, POWER_RESETTING} POWER_STATE;

static const char* STR_POWER_ON = "POWER_ON";
static const char* STR_POWER_OFF = "POWER_OFF";

// This utility converts a string to the POWER_STATE enum
inline POWER_STATE strToPowerState(const std::string powerStr)
{
    std::string xformStr(powerStr);
    std::transform(powerStr.begin(), powerStr.end(), xformStr.begin(), ::toupper);
    if (xformStr == std::string(STR_POWER_OFF)
        || xformStr == std::string("OFF")
        || xformStr == std::string("0")) {
        return POWER_OFF;
    }

    if (powerStr == std::string(STR_POWER_ON)
        || xformStr == std::string("ON")
        || xformStr == std::string("1")) {
        return POWER_ON;
    }

    return ILLEGAL_POWER;
}

inline const std::string powerStateToStr(POWER_STATE sensorState)
{
    switch (sensorState) {
        case POWER_OFF:
            return std::string(STR_POWER_OFF);
            break;
        case POWER_ON:
            return std::string(STR_POWER_ON);
            break;
        default:
            std::stringstream sstrm("Unknown sensor power state: ");
            sstrm << sensorState;
            return sstrm.str();
            break;
    }
}


/*
 *  This class provides the standard interface for controlling power to
 *  individual HW objects in NIDAS projects, such as DSM, etc.
 */
class PowerCtrlIf {
public:
    virtual ~PowerCtrlIf() {}
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
