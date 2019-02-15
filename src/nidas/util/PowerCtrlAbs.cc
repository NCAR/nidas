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

#include "PowerCtrlAbs.h"

namespace nidas { namespace util {

void PowerCtrlAbs::setPower(POWER_STATE newPwrState)
{
    newPwrState == POWER_ON ? pwrOn() : pwrOff();
}

void PowerCtrlAbs::pwrReset(uint32_t pwrOnDelayMs, uint32_t pwrOffDelayMs)
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
        DLOG(("PowerCtrlAbs::pwrReset(): Power control is not enabled"));
    }
}
void PowerCtrlAbs::print()
{
    std::cout << "Power state: " << powerStateToStr(_pwrState) << std::endl;
}



}} //namespace nidas { namespace util {
