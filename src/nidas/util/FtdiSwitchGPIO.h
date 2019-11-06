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
#ifndef NIDAS_UTIL_FTDISWITCHGPIO_H
#define NIDAS_UTIL_FTDISWITCHGPIO_H

#include "FtdiHW.h"

namespace nidas { namespace util {


/*
 *  Class FtdiSwitchGPIO provides the means to access the FTDI FT4232H device
 *  which is designated for i2c, DSM power control and monitoring momentary switch
 *  inputs. In particular, this class is interested in two switches on the FTDI 
 *  USB Serial board: SW3 and SW4.
 * 
 *  If a FT4232H device is not found, then a simple shadow register will be 
 *  used for testing purposes only.
 */
class FtdiSwitchGPIO : public GpioIF
{
public:
    FtdiSwitchGPIO()
    : _pFtdiDevice(0), _shadow(0)
    {
        // This will throw an exception if it fails. We don't really want to 
        // catch it, because something is really wrong if it does fail.
        _pFtdiDevice = getFtdiDevice(FTDI_I2C, INTERFACE_C);
    }

    virtual ~FtdiSwitchGPIO()
    {
        DLOG(("FtdiSwitchGPIO::~FtdiSwitchGPIO(): destructing..."));
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
        _shadow = bits;
        _pFtdiDevice->write(bits);
    }

    void setBit(unsigned char bit)
    {
        unsigned char rawBits = _shadow;
        if (ifaceFound()) {
            rawBits = _pFtdiDevice->read();
            DLOG(("FtdiSwitchGPIO::write(): Raw bits: 0x%0x", rawBits));
            rawBits |= bit;
            DLOG(("FtdiSwitchGPIO::write(): New bits: 0x%0x", rawBits));
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
            DLOG(("FtdiSwitchGPIO::write(): Raw bits: 0x%0x", rawBits));
            rawBits &= ~bit;
            DLOG(("FtdiSwitchGPIO::write(): New bits: 0x%0x", rawBits));
            write(rawBits);
        }
        else {
            rawBits &= ~bit;
        }

        _shadow = rawBits;
    }

    ftdi_interface getInterface()
    {
        ftdi_interface retval = INTERFACE_A;
        if (_pFtdiDevice) {
            retval =_pFtdiDevice->getInterface();
        }
        return retval;
    }

    bool ifaceFound()
    {
        bool retval = false;
        if (_pFtdiDevice) {
            retval = _pFtdiDevice->ifaceFound();
        }
        return retval;
    }

private:
    FtdiHwIF* _pFtdiDevice;

    // only used for testing
    unsigned char _shadow;

    /*
     *  No copying
     */

    FtdiSwitchGPIO(const FtdiSwitchGPIO&);
    FtdiSwitchGPIO& operator=(FtdiSwitchGPIO&);
    const FtdiSwitchGPIO& operator=(const FtdiSwitchGPIO&);
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_FTDISWITCHGPIO_H
