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
    /*
     *  Because multiple specializations may exist on a single FTDI device interface
     *  (Xcvr control and power control, for instance), Sync selects one mutex per interface.
     *
     *  Specializations of FtdiDSMPowerGPIO should use the Sync class to protect their operations on
     *  the interface which they are concerned.
     */
    class Sync : public Synchronized
    {
    public:
        // TODO: When the reworked FTDI USB Serial Interface board comes out then the FTDI interface used
        //       for DSM power control will be INTERFACE_C
        Sync(FtdiDSMPowerGPIO* me) : Synchronized(_ifaceACondVar), _me(me)
        {
            DLOG(("Synced on interface A"));
        }
        ~Sync()
        {
            DLOG(("Sync released on interface A"));
            _me = 0;
        }
    private:
        static Cond _ifaceACondVar;
        FtdiDSMPowerGPIO* _me;


        // no copying
        Sync(const Sync& rRight);
        Sync& operator=(const Sync& rRight);
        Sync& operator=(Sync& rRight);
    };

    FtdiDSMPowerGPIO()
    : _pFtdiDevice(0), _shadow(0)
    {
        try {
            // See TODO above
            _pFtdiDevice = getFtdiDevice(FTDI_I2C, INTERFACE_A);
        }
        catch (InvalidParameterException& e) {
            _pFtdiDevice = 0;
        }

        if (ifaceFound()) {
            _pFtdiDevice->setMode(0xFF, BITMODE_BITBANG);
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

    virtual void write(unsigned char bits)
    {
        if (ifaceFound()) {
            _pFtdiDevice->write(bits);
        }
    }

    virtual void write(unsigned char bits, unsigned char mask)
    {
        unsigned char rawBits = _shadow;
        if (ifaceFound()) {
            rawBits = _pFtdiDevice->read();
            DLOG(("FtdiDSMPowerGPIO::write(): Raw bits: 0x%0x", rawBits));
            rawBits &= ~mask;
            rawBits |= bits;
            DLOG(("FtdiDSMPowerGPIO::write(): New bits: 0x%0x", rawBits));
            write(rawBits);
            _shadow = rawBits;
        }
        else {
            rawBits &= ~mask;
            rawBits |= bits;
            _shadow = rawBits;
        }
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

    FtdiDSMPowerGPIO(const FtdiDSMPowerGPIO&);
    FtdiDSMPowerGPIO& operator=(FtdiDSMPowerGPIO&);
    const FtdiDSMPowerGPIO& operator=(const FtdiDSMPowerGPIO&);
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_FTDIDSMPOWERGPIO_H
