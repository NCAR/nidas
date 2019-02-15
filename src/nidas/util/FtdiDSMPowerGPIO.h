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
#ifndef NIDAS_UTIL_POWERGPIO_H
#define NIDAS_UTIL_POWERGPIO_H

#include "FtdiHW.h"

namespace nidas { namespace util {

/*
 *  Class PowerGPIO provides the means to access the FTDI FT4232H device
 *  which is designated for i2c and DSM power control. If a FT4232H
 *  device is not found, then a simple shadow register will be used for testing
 *  purposes only.
 */
class FtdiPowerGPIO
{
public:
    /*
     *  Because multiple specializations may exist on a single FTDI device interface
     *  (Xcvr control and power control, for instance), Sync selects one mutex per interface.
     *
     *  Specializations of PowerGPIO should use the Sync class to protect their operations on
     *  the interface which they are concerned.
     */
    class Sync : public Synchronized
    {
    public:
        // TODO: When the reworked FTDI USB Serial Interface board comes out then the FTDI interface used
        //       for DSM power control will be INTERFACE_C
        Sync(FtdiPowerGPIO* me) : Synchronized(_ifaceACondVar), _me(me)
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
        FtdiPowerGPIO* _me;


        // no copying
        Sync(const Sync& rRight);
        Sync& operator=(const Sync& rRight);
        Sync& operator=(Sync& rRight);
    };

    FtdiPowerGPIO()
    : _pFtdiDevice(0), _shadow(0)
    {
        try {
            // See TODO above
            _pFtdiDevice = getFtdiDevice(FTDI_GPIO, INTERFACE_A);
        }
        catch (InvalidParameterException& e) {
            _pFtdiDevice = 0;
        }

        if (ifaceFound()) {
            _pFtdiDevice->setMode(0xFF, BITMODE_BITBANG);
        }
    }

    virtual ~FtdiPowerGPIO()
    {
        DLOG(("PowerGPIO::~PowerGPIO(): destructing..."));
        // don't delete _pFtdiDevice, because someone else may be using it
        _pFtdiDevice = 0;
    }

    virtual void write(unsigned char bits, unsigned char mask)
    {
        unsigned char rawBits = _shadow;
        if (ifaceFound()) {
            rawBits = _pFtdiDevice->read();
            DLOG(("PowerGPIO::write(): Raw bits: 0x%0x", rawBits));
            rawBits &= mask;
            rawBits |= bits;
            DLOG(("PowerGPIO::write(): New bits: 0x%0x", rawBits));
            _pFtdiDevice->write(rawBits);
        }
        else {
            rawBits &= mask;
            rawBits |= bits;
            _shadow = rawBits;
        }
    }

    virtual unsigned char read()
    {
        unsigned char retval = _shadow;
        if (ifaceFound()) {
            retval = _pFtdiDevice->read();
        }
        return retval;
    }

    ftdi_interface getInterface()
    {
        ftdi_interface retval = INTERFACE_A;
        if (_pFtdiDevice) {
            retval =_pFtdiDevice->getInterface();
        }
        return retval;
    }


protected:
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

    FtdiPowerGPIO(const FtdiPowerGPIO&);
    FtdiPowerGPIO& operator=(FtdiPowerGPIO&);
    const FtdiPowerGPIO& operator=(const FtdiPowerGPIO&);
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_POWERGPIO_H
