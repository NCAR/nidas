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
#ifndef NIDAS_UTIL_DSMSWITCHCTRL_H
#define NIDAS_UTIL_DSMSWITCHCTRL_H

#include "FtdiSwitchGPIO.h"

namespace nidas { namespace util {

static const unsigned char DEFAULT_SW_BIT = 0x01;
static const unsigned char WIFI_SW_BIT = 0x02;
static const unsigned char DEFAULT_LED_BIT = 0x03;
static const unsigned char WIFI_LED_BIT = 0x04;

/*
 *  This class specializes PowerCtrlIF in order to provide a facade for to enabling/disabling power control
 *  depending on what sort of GPIO interface is available on the DSM
 */
class DSMSwitchCtrl : public FtdiSwitchGPIO
{
public:
    DSMSwitchCtrl(GPIO_PORT_DEFS gpio)
    : FtdiSwitchGPIO(), _switchBit(0), _ledBit(0)
    {
        DLOG(("DSMSwitchCtrl::DSMSwitchCtrl(): setting up the switch and LED bits..."));
        switch (gpio) {
            case DEFAULT_SW:
                _switchBit = DEFAULT_SW_BIT;
                _ledBit = DEFAULT_LED_BIT;
                break;
            case WIFI_SW:
                _switchBit = WIFI_SW_BIT;
                _ledBit = WIFI_LED_BIT;
                break;
            default:
                throw InvalidParameterException("DSMSwitchCtrl", "DSMSwitchCtrl()", 
                                                "Illegal value for gpio parameter. "
                                                "Should be either DEFAULT_SW or WIFI_SW");
                break;
        }
    }

    virtual ~DSMSwitchCtrl()
    {
        DLOG(("DSMSwitchCtrl::~DSMSwitchCtrl(): destructing..."));
    }

    void ledOn()
    {
        setBit(_ledBit);
    }

    void ledOff()
    {
        resetBit(_ledBit);
    }

    void print()
    {
    }

    bool switchIsPressed()
    {
        return readBit(_switchBit);
    }

    bool ledIsOn()
    {
        return readBit(_ledBit);
    }

private:
    unsigned char _switchBit;
    unsigned char _ledBit;

    /*
     *  No copying...
     */
    DSMSwitchCtrl(const DSMSwitchCtrl& rRight);
    DSMSwitchCtrl& operator=(const DSMSwitchCtrl& rRight);
    DSMSwitchCtrl& operator=(DSMSwitchCtrl& rRight);

};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_DSMPOWERCTRL_H
