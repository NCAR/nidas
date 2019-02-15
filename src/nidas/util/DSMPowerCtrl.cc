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

#include "DSMPowerCtrl.h"
#include "FtdiDSMPowerCtrl.h"
#include "SysfsDSMPowerCtrl.h"

namespace nidas { namespace util {

DSMPowerCtrl::DSMPowerCtrl(GPIO_PORT_DEFS gpio)
: PowerCtrlIf(), _pPwrCtrl(0)
{
    _pPwrCtrl = new FtdiDSMPowerCtrl(gpio);
    if (_pPwrCtrl) {
        if (!_pPwrCtrl->ifaceAvailable()) {
            delete _pPwrCtrl;
            _pPwrCtrl = new SysfsDSMPowerCtrl(gpio);
            if (!_pPwrCtrl) {
                DLOG(("DSMPowerCtrl::DSMPowerCtrl(): Failed to instantiate SysfsDSMPowerCtrl object!!"));
                throw Exception("DSMPowerCtrl::DSMPowerCtrl()", "Failed to reserve memory for SysfsDSMPowerCtrl object.");
            }
        }
    }
    else {
        DLOG(("DSMPowerCtrl::DSMPowerCtrl(): Failed to instantiate FtdiDSMPowerCtrl object!!"));
        throw Exception("DSMPowerCtrl::DSMPowerCtrl()", "Failed to reserve memory for FtdiDSMPowerCtrl object.");
    }
    updatePowerState();
}

}} //namespace nidas { namespace util {
