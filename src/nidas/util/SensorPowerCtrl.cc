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
#include "FtdiSensorPowerCtrl.h"
#include "SysfsSensorPowerCtrl.h"

namespace nidas { namespace util {

const std::string SensorPowerCtrl::rawPowerToStr(unsigned char powerCfg)
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

POWER_STATE SensorPowerCtrl::rawPowerToState(unsigned char powerCfg)
{
    POWER_STATE retval = POWER_OFF;
    if (powerCfg & SENSOR_BITS_POWER) {
        retval = POWER_ON;
    }

    return retval;
}


SensorPowerCtrl::SensorPowerCtrl(GPIO_PORT_DEFS port)
: PowerCtrlIf(), _port(port), _pPwrCtrl(0)
{
    // always assume new USB FTDI board first...
    _pPwrCtrl = new FtdiSensorPowerCtrl(port);
    if (_pPwrCtrl) {
        if (!_pPwrCtrl->ifaceAvailable()) {
            delete _pPwrCtrl;

            _pPwrCtrl = new SysfsSensorPowerCtrl(port);
            if (!_pPwrCtrl) {
                DLOG(("SensorPowerCtrl::SensorPowerCtrl(): Failed to instantiate SysfsSensorPowerCtrl object!!"));
                throw Exception("SensorPowerCtrl::SensorPowerCtrl()", "Failed to reserve memory for SysfsSensorPowerCtrl object.");
            }
        }
    }
    else {
        DLOG(("SensorPowerCtrl::SensorPowerCtrl(): Failed to instantiate FtdiSensorPowerCtrl object!!"));
        throw Exception("SensorPowerCtrl::SensorPowerCtrl()", "Failed to reserve memory for FtdiSensorPowerCtrl object.");
    }

    updatePowerState();
}

SensorPowerCtrl::~SensorPowerCtrl()
{
    DLOG(("SensorPowerCtrl::~SensorPowerCtrl(): destructing..."));
    delete _pPwrCtrl;
}

void SensorPowerCtrl::enablePwrCtrl(bool enable)
{
    if (_pPwrCtrl) {
        _pPwrCtrl->enablePwrCtrl(enable);
    }
}

bool SensorPowerCtrl::pwrCtrlEnabled()
{
    bool retval = false;
    if (_pPwrCtrl) {
        retval = _pPwrCtrl->pwrCtrlEnabled();
    }
    return retval;
}

void SensorPowerCtrl::setPower(POWER_STATE pwrState)
{
    if (_pPwrCtrl) {
        _pPwrCtrl->setPower(pwrState);
    }
}

void SensorPowerCtrl::setPowerState(POWER_STATE pwrState)
{
    if (_pPwrCtrl) {
        _pPwrCtrl->setPowerState(pwrState);
    }
}

POWER_STATE SensorPowerCtrl::getPowerState()
{
    POWER_STATE retval = ILLEGAL_POWER;
    if (_pPwrCtrl) {
        retval = _pPwrCtrl->getPowerState();
    }
    return retval;
}

void SensorPowerCtrl::pwrOn()
{
    if (_pPwrCtrl) {
        _pPwrCtrl->pwrOn();
    }
}

void SensorPowerCtrl::pwrOff()
{
    if (_pPwrCtrl) {
        _pPwrCtrl->pwrOff();
    }
}

void SensorPowerCtrl::pwrReset(uint32_t pwrOnDelayMs, uint32_t pwrOffDelayMs)
{
    if (_pPwrCtrl) {
        _pPwrCtrl->pwrReset(pwrOnDelayMs, pwrOffDelayMs);
    }
}

bool SensorPowerCtrl::pwrIsOn()
{
    bool retval = false;
    if (_pPwrCtrl) {
        retval = _pPwrCtrl->pwrIsOn();
    }
    return retval;
}

void SensorPowerCtrl::print()
{
    if (_pPwrCtrl) {
        _pPwrCtrl->print();
    }
}

bool SensorPowerCtrl::ifaceAvailable()
{
    return _pPwrCtrl ? _pPwrCtrl->ifaceAvailable() : false;
}

void SensorPowerCtrl::updatePowerState()
{
    if (_pPwrCtrl) {
        _pPwrCtrl->updatePowerState();
        setPowerState(_pPwrCtrl->getPowerState());
    }
}


}} //namespace nidas { namespace util {
