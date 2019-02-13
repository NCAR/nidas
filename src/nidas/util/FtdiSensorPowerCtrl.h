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
#ifndef NIDAS_UTIL_FTDISERIALPOWERCTRL_H
#define NIDAS_UTIL_FTDISERIALPOWERCTRL_H

#include "XcvrGPIO.h"
#include "PowerCtrlAbs.h"

namespace nidas { namespace util {

/*
 *  This class specializes PowerCtrlIF by providing a manual means to enable/disable power control
 */
class FtdiSensorPowerCtrl : public XcvrGPIO, public PowerCtrlAbs
{
public:
    FtdiSensorPowerCtrl(GPIO_PORT_DEFS port);
    virtual ~FtdiSensorPowerCtrl()
    {
        DLOG(("FtdiSensorPowerCtrl::~FtdiSensorPowerCtrl(): destructing..."));
    }

    virtual void pwrOn();
    virtual void pwrOff();
    virtual void print()
    {
        std::cout << "Port" << _port << " ";
        PowerCtrlAbs::print();
    }

    std::string getPowerStateStr() {
        unsigned char rawPwr = read();
        return rawPowerToStr(rawPwr);
    }

    void updatePowerState();

    // This utility converts a binary power configuration to a string
    static const std::string rawPowerToStr(unsigned char powerCfg);
    // This utility converts a binary power configuration to state representation
    static POWER_STATE rawPowerToState(unsigned char powerCfg);

private:
    GPIO_PORT_DEFS _port;

    /*
     *  No copying
     */
    FtdiSensorPowerCtrl(const FtdiSensorPowerCtrl& rRight);
    FtdiSensorPowerCtrl(FtdiSensorPowerCtrl& rRight);
    const FtdiSensorPowerCtrl& operator=(const FtdiSensorPowerCtrl& rRight);
    FtdiSensorPowerCtrl & operator=(FtdiSensorPowerCtrl& rRight);

};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_FTDIPOWERCTRLIF_H
