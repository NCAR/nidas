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
#ifndef NIDAS_UTIL_POWERCTRLABS_H
#define NIDAS_UTIL_POWERCTRLABS_H

#include "Logger.h"
#include "UTime.h"
#include "PowerCtrlIf.h"

namespace nidas { namespace util {

/*
 *  This class specializes PowerCtrlIF by providing a manual means to enable/disable power control
 *  and to maintain the current power state.
 */
class PowerCtrlAbs : public PowerCtrlIf
{
public:
    // This utility converts a string to the SENSOR_POWER_STATE enum
    static POWER_STATE strToPowerState(const std::string powerStr);
    // This utility converts a binary power configuration to a string
    static const std::string powerStateToStr(POWER_STATE pwrState);

    static const char* STR_POWER_ON;
    static const char* STR_POWER_OFF;


    PowerCtrlAbs() : _pwrCtrlEnabled(false), _pwrState(ILLEGAL_POWER) {}
    virtual ~PowerCtrlAbs() {}

    virtual void enablePwrCtrl(bool enable) {_pwrCtrlEnabled = enable;}
    virtual bool pwrCtrlEnabled() {return _pwrCtrlEnabled;}
    virtual void setPower(POWER_STATE newPwrState);
    virtual void setPowerState(POWER_STATE newPwrState) {_pwrState = newPwrState;}
    virtual POWER_STATE getPowerState() {return _pwrState;}
    virtual void pwrReset(uint32_t pwrOnDelayMs=0, uint32_t pwrOffDelayMs=0);
    virtual bool pwrIsOn() {return getPowerState() == POWER_ON;}
    virtual void print();

private:
    bool _pwrCtrlEnabled;
    POWER_STATE _pwrState;
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_POWERCTRLIF_H
