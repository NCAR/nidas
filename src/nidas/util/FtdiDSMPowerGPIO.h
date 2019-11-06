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
#ifndef NIDAS_UTIL_FTDIDSMPOWERGPIO_H
#define NIDAS_UTIL_FTDIDSMPOWERGPIO_H

#include "FtdiHW.h"

namespace nidas { namespace util {

/*
 *  Class FtdiDSMPowerGPIO provides the means to access the FTDI FT4232H device
 *  which is designated for i2c and DSM power control. If a FT4232H
 *  device is not found, then a simple shadow register will be used for testing
 *  purposes only.
 */
class FtdiDSMPowerGPIO : public GpioIF
{
public:
    FtdiDSMPowerGPIO()
    : _pFtdiDevice(0), _shadow(0)
    {
        try {
            _pFtdiDevice = getFtdiDevice(FTDI_I2C, INTERFACE_C);
        }
        catch (InvalidParameterException& e) {
            _pFtdiDevice = 0;
        }
    }

    virtual ~FtdiDSMPowerGPIO()
    {
        DLOG(("FtdiDSMPowerGPIO::~FtdiDSMPowerGPIO(): destructing..."));
        // don't delete _pFtdiDevice, because someone else may be using it
        _pFtdiDevice = 0;
    }

    virtual unsigned char read()
    {
        unsigned char retval = _shadow;
        if (ifaceFound()) {
            retval = _pFtdiDevice->read();
        }
        return retval;
    }

    unsigned char readBit(unsigned char bit) 
    {
        return read() & bit;
    }

    virtual void write(unsigned char bits)
    {
        _pFtdiDevice->write(bits);
    }

    void setBit(unsigned char bit)
    {
        unsigned char rawBits = _shadow;
        if (ifaceFound()) {
            rawBits = _pFtdiDevice->read();
            DLOG(("FtdiDSMPowerGPIO::setBit(): Raw bits: 0x%0x", rawBits));
            rawBits |= bit;
            DLOG(("FtdiDSMPowerGPIO::setBit(): New bits: 0x%0x", rawBits));
            write(rawBits);
            _shadow = rawBits;
        }
        else {
            rawBits |= bit;
            _shadow = rawBits;
        }
    }

    void resetBit(unsigned char bit)
    {
        unsigned char rawBits = _shadow;
        if (ifaceFound()) {
            rawBits = _pFtdiDevice->read();
            DLOG(("FtdiDSMPowerGPIO::resetBit(): Raw bits: 0x%0x", rawBits));
            rawBits &= ~bit;
            DLOG(("FtdiDSMPowerGPIO::resetBit(): New bits: 0x%0x", rawBits));
            write(rawBits);
            _shadow = rawBits;
        }
        else {
            rawBits &= ~bit;
            _shadow = rawBits;
        }
    }

    ftdi_interface getInterface()
    {
        return _pFtdiDevice->getInterface();
    }

    bool ifaceFound()
    {
        return _pFtdiDevice->ifaceFound();
    }

private:
    // Does the work of actually opening, reading, writing and closing device
    FtdiHwIF* _pFtdiDevice;

    // only used for testing when there is no actual device.
    unsigned char _shadow;

    /*
     *  No copying
     */

    FtdiDSMPowerGPIO(const FtdiDSMPowerGPIO&);
    FtdiDSMPowerGPIO& operator=(FtdiDSMPowerGPIO&);
    const FtdiDSMPowerGPIO& operator=(const FtdiDSMPowerGPIO&);
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_FTDIDSMPOWERGPIO_H
