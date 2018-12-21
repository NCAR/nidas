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
#ifndef NIDAS_UTIL_SERIALPOWERCTRL_H
#define NIDAS_UTIL_SERIALPOWERCTRL_H

#include "AutoConfigHW.h"
#include "PowerCtrlAbs.h"

namespace nidas { namespace util {
/*
 * Sensor power setting
 */
typedef enum {SENSOR_POWER_OFF, SENSOR_POWER_ON} SENSOR_POWER_STATE;


/*
 *  This class specializes PowerCtrlIF by providing a manual means to enable/disable power control
 */
class SensorPowerCtrl : public SerialGPIO, public PowerCtrlAbs
{
public:
    SensorPowerCtrl(PORT_DEFS port);
    virtual ~SensorPowerCtrl() {}

    virtual void pwrOn();
    virtual void pwrOff();
    virtual void pwrReset(uint32_t pwrOnDelayMs=0, uint32_t pwrOffDelayMs=0);
    virtual bool pwrIsOn();
    void print();
    void getPowerState();

    // This utility converts a string to the SENSOR_POWER_STATE enum
    static SENSOR_POWER_STATE strToPowerState(const std::string powerStr);
    // This utility converts a binary power configuration to a string
    static const std::string rawPowerToStr(unsigned char powerCfg);
    // This utility converts a binary power configuration to state representation
    static SENSOR_POWER_STATE rawPowerToState(unsigned char powerCfg);
    // This utility converts a binary power configuration to a string
    static const std::string powerStateToStr(SENSOR_POWER_STATE sensorState);

    static const char* STR_POWER_ON;
    static const char* STR_POWER_OFF;

private:
    PORT_DEFS _port;
    SENSOR_POWER_STATE _pwrState;
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_POWERCTRLIF_H
