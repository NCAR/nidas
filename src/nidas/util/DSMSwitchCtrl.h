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

static const unsigned char WIFI_SW_BIT =     0b00010000;
static const unsigned char DEFAULT_SW_BIT =  0b00100000;
static const unsigned char WIFI_LED_BIT =    0b01000000;
static const unsigned char DEFAULT_LED_BIT = 0b10000000;

static const std::string WIFI_SW_BIT_STR("WIFI_SW_BIT");
static const std::string DEFAULT_SW_BIT_STR("DEFAULT_SW_BIT");
static const std::string WIFI_LED_BIT_STR("WIFI_LED_BIT");
static const std::string DEFAULT_LED_BIT_STR("DEFAULT_LED_BIT");

const std::string bit2str(unsigned char bit) 
{
    std::string retval;
    switch (bit) {
        case WIFI_SW_BIT:
            retval = WIFI_SW_BIT_STR;
            break;
        case DEFAULT_SW_BIT:
            retval = DEFAULT_SW_BIT_STR;
            break;
        case WIFI_LED_BIT:
            retval = WIFI_LED_BIT_STR;
            break;
        case DEFAULT_LED_BIT:
            retval = DEFAULT_LED_BIT_STR;
            break;
        default:
            throw InvalidParameterException(std::string("bit2str() : Illegal value for bit parameter."));
            break;
    }

    return retval;
}

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
        DLOG(("DSMSwitchCtrl::DSMSwitchCtrl(): Switch bit: ") << bit2str(_switchBit)
                                                              << " : " << _switchBit 
                                                              << "; LED bit: " 
                                                              << bit2str(_ledBit)
                                                              << " : " << _ledBit);
    }

    virtual ~DSMSwitchCtrl()
    {
        DLOG(("DSMSwitchCtrl::~DSMSwitchCtrl(): destructing..."));
    }

    void ledOn()
    {
        setBit(_ledBit);
        DLOG(("DSMSwitchCtrl::ledOn(): turned on LED bit: ") << bit2str(_ledBit) << " : " << (long)_ledBit);
    }

    void ledOff()
    {
        resetBit(_ledBit);
        DLOG(("DSMSwitchCtrl::ledOff(): turned off LED bit: ") << bit2str(_ledBit) << " : " << (long)_ledBit);
    }

    void print()
    {
    }

    bool switchIsPressed()
    {
        bool swPressed = (readBit(_switchBit) != 0);
        unsigned char raw = readBit(_switchBit);
        char rawBuf[5] = {0,0,0,0,0};
        sprintf(rawBuf, "0x%0X", raw);
        std::string rawStr(rawBuf);
        char bitBuf[5] = {0,0,0,0,0};
        sprintf(bitBuf, "0X%0X", _switchBit);
        std::string bitStr(bitBuf);
        DLOG(("DSMSwitchCtrl::switchIsPressed(): ") << "raw: " << rawStr 
                                                    << "; logical: " << bit2str(_switchBit) 
                                                    << " : " << bitStr << " : is " 
                                                    << (swPressed ? "ON" : "OFF"));
        return (swPressed);
    }

    bool ledIsOn()
    {
        bool ledOn = (readBit(_ledBit) != 0);
        unsigned char raw = readBit(_ledBit);
        char rawBuf[5] = {0,0,0,0,0};
        sprintf(rawBuf, "0x%0X", raw);
        std::string rawStr(rawBuf);
        char bitBuf[5] = {0,0,0,0,0};
        sprintf(bitBuf, "0X%0X", _switchBit);
        std::string bitStr(bitBuf);
        DLOG(("DSMSwitchCtrl::ledIsOn(): ") << "raw: " << rawStr 
                                            << "; logical: " 
                                            << bit2str(_ledBit) 
                                            << " : " << bitStr << " : is " 
                                            << (ledOn ? "ON" : "OFF"));
        return ledOn;
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
