// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
#ifndef TEST_SERIAL_XCVR_CTRL_H
#define MOCK_SERIAL_XCVR_CTRL_H
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

#include "SerialXcvrCtrl.h"

using namespace nidas::core;

class MockSerialXcvrCtrl : public SerialXcvrCtrl
{
public:
    // Constructor needs to know what port is being controlled
    // Constructor uses portID to decide which FTDI interface to
    // use to control the port type.
    // Bus address is the means by which the USB device is opened.
    // So default them to the values known today, but may be
    // overridden later.
    MockSerialXcvrCtrl(const PORT_DEFS portId) : SerialXcvrCtrl(portId), _gpioOpen(false), _rawXcvrConfig(0) {}
    MockSerialXcvrCtrl(const PORT_DEFS portId, const PORT_TYPES portType, const TERM termination=NO_TERM) //,
//                       const SENSOR_POWER_STATE powerState=SENSOR_POWER_ON)
        : SerialXcvrCtrl(portId, portType, termination, powerState), _gpioOpen(false), _rawXcvrConfig(0) {}
    MockSerialXcvrCtrl(const XcvrConfig initXcvrConfig)
        : SerialXcvrCtrl(initXcvrConfig), _gpioOpen(false), _rawXcvrConfig(0) {}

    // Destructor
    virtual ~MockSerialXcvrCtrl() {};

    virtual void initFtdi() {}

    // This is the primary client API that does all the heavy lifting
    // to actually change the SP339 driver port type/mode (RS232, RS422, etc).
    void applyXcvrConfig(const bool /*readDevice=true*/)
    {
        _rawXcvrConfig &= ~adjustBitPosition(0xF);
        _rawXcvrConfig |= adjustBitPosition(
                            assembleBits(getXcvrConfig().portType, getXcvrConfig().termination , getXcvrConfig().sensorPower));
    };
    // Returns the raw bits already reported by readXcvrConfig() indicating current state of
    // the port mode, including termination and sensor power
    unsigned char getRawXcvrConfig() {return _rawXcvrConfig;};
//    // Returns the raw bits indicating current state of the port mode, including sensor power
//    XcvrConfig& getXcvrConfig() {return SerialXcvrCtrl::getXcvrConfig();};
    // Reads the xcvr config from the FTDI chip and put it in _rawXcvrConfig
    void readXcvrConfig() {}

protected:
    virtual bool gpioOpen()
    {
        bool weOpenedIt = false;
        if (!gpioIsOpen()) {
            weOpenedIt = true;
            _gpioOpen = true;
        }

        return weOpenedIt;
    };
    // safe FT4232H close - must bracket all gpio operations!!!
    // returns true if already closed or successfully closed, false if attempted close fails.
    virtual bool gpioClose(bool weOpenedIt)
    {
        if (gpioIsOpen()) {
            if (weOpenedIt) {
                _gpioOpen = false;
                return true;
            }
        }

        return true;
    }
    virtual bool gpioIsOpen() {return _gpioOpen;}

private:
    // keeps track of whether the FT4232H GPIO is open
    bool _gpioOpen;
    unsigned char _rawXcvrConfig;

    // never use default constructor, copy constructors, operator=
    MockSerialXcvrCtrl();
    MockSerialXcvrCtrl(const MockSerialXcvrCtrl& rRight);
    MockSerialXcvrCtrl(MockSerialXcvrCtrl& rRight);
    const MockSerialXcvrCtrl& operator=(const MockSerialXcvrCtrl& rRight);
    MockSerialXcvrCtrl& operator=(MockSerialXcvrCtrl& rRight);
};

#endif //TEST_SERIAL_XCVR_CTRL_H
