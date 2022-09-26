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
#ifndef NIDAS_UTIL_DSMPOWERCTRL_H
#define NIDAS_UTIL_DSMPOWERCTRL_H

#include "GpioIF.h"
#include "PowerCtrlIf.h"

namespace nidas { namespace util {


/*
 *  This class specializes PowerCtrlIF in order to provide a facade for to
 *  enabling/disabling power control depending on what sort of GPIO interface
 *  is available on the DSM
 */
class DSMPowerCtrl : public PowerCtrlIf
{
public:
    DSMPowerCtrl(GPIO_PORT_DEFS gpio);

    virtual ~DSMPowerCtrl();

    virtual void pwrOn();

    virtual void pwrOff();

    virtual void updatePowerState();

    virtual void print();

    virtual bool ifaceAvailable();

    virtual void enablePwrCtrl(bool enable);

    virtual bool pwrCtrlEnabled();

    virtual void setPower(POWER_STATE newPwrState);

    virtual void setPowerState(POWER_STATE pwrState);

    virtual POWER_STATE getPowerState();

    virtual void pwrReset(uint32_t pwrOnDelayMs=0, uint32_t pwrOffDelayMs=0);

    virtual bool pwrIsOn();

    DSMPowerCtrl(const DSMPowerCtrl& rRight) = delete;
    DSMPowerCtrl& operator=(const DSMPowerCtrl& rRight) = delete;

private:
    PowerCtrlIf* _pPwrCtrl;

};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_DSMPOWERCTRL_H
