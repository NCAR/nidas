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

namespace nidas { namespace util {

const char* SensorPowerCtrl::STR_POWER_ON = "POWER_ON";
const char* SensorPowerCtrl::STR_POWER_OFF = "POWER_OFF";

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

SENSOR_POWER_STATE SensorPowerCtrl::rawPowerToState(unsigned char powerCfg)
{
    SENSOR_POWER_STATE retval = SENSOR_POWER_OFF;
    if (powerCfg & BITS_POWER) {
        retval = SENSOR_POWER_ON;
    }

    return retval;
}

// This utility converts a string to the SENSOR_POWER_STATE enum
SENSOR_POWER_STATE SensorPowerCtrl::strToPowerState(const std::string powerStr)
{
    if (powerStr == std::string(STR_POWER_OFF)) {
        return SENSOR_POWER_OFF;
    }

    if (powerStr == std::string(STR_POWER_ON)) {
        return SENSOR_POWER_ON;
    }

    return (SENSOR_POWER_STATE)-1;
}

const std::string SensorPowerCtrl::powerStateToStr(SENSOR_POWER_STATE sensorState)
{
    switch (sensorState) {
        case SENSOR_POWER_OFF:
            return std::string(STR_POWER_OFF);
            break;
        case SENSOR_POWER_ON:
            return std::string(STR_POWER_ON);
            break;
        default:
            std::stringstream sstrm("Unknown sensor power state: ");
            sstrm << sensorState;
            return sstrm.str();
            break;
    }
}

SensorPowerCtrl::SensorPowerCtrl(PORT_DEFS port)
: SerialGPIO(port2iface(port)), _port(port), _pwrState(SENSOR_POWER_OFF)
{
    getPowerState();
}

void SensorPowerCtrl::pwrOn()
{
    if (pwrCtrlEnabled()) {
        unsigned char pins = readInterface();
        pins |= BITS_POWER;
        writeInterface(pins);
        getPowerState();
    }
    else {
        DLOG(("SerialPowerCtrl::SerialPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
}

void SensorPowerCtrl::pwrOff()
{
    if (pwrCtrlEnabled()) {
        unsigned char pins = readInterface();
        pins &= ~BITS_POWER;
        writeInterface(pins);
        getPowerState();
    }
    else {
        DLOG(("SerialPowerCtrl::SerialPowerCtrl(): Power control for device: ") << _port << " is not enabled");
    }
}

void SensorPowerCtrl::pwrReset(uint32_t pwrOnDelayMs, uint32_t pwrOffDelayMs)
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

bool SensorPowerCtrl::pwrIsOn()
{
    return _pwrState == SENSOR_POWER_ON;
}

void SensorPowerCtrl::getPowerState()
{
    unsigned char ifaceState = readInterface();
    DLOG(("current interface state: %x", int(ifaceState)));
    if (_port % 2) ifaceState >>= 4;
    DLOG(("interface state after shift: %x", (int)ifaceState));
    _pwrState = rawPowerToState(ifaceState);
    DLOG(("power state: %s", powerStateToStr(_pwrState).c_str()));
}

void SensorPowerCtrl::print()
{
    std::cout << "Power state: " << powerStateToStr(_pwrState) << std::endl;
}

}} //namespace nidas { namespace util {
