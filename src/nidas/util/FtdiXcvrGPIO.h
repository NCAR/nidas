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
#ifndef NIDAS_UTIL_XCVRGPIO_H
#define NIDAS_UTIL_XCVRGPIO_H

#include "FtdiHW.h"

namespace nidas { namespace util {

// deduces the FT4232H GPIO interface form the port in _xcvrConfig.
// need to select the interface based on the specified port
// at present, assume 4 bits per port definition
inline enum ftdi_interface port2iface(GPIO_PORT_DEFS port)
{
    enum ftdi_interface iface = INTERFACE_ANY;
    switch ( port )
    {
        case SER_PORT0:
        case SER_PORT1:
            iface = INTERFACE_A;
            break;
        case SER_PORT2:
        case SER_PORT3:
            iface = INTERFACE_B;
            break;

        case SER_PORT4:
        case SER_PORT5:
            iface = INTERFACE_C;
            break;
        case SER_PORT6:
        case SER_PORT7:
            iface = INTERFACE_D;
            break;

        default:
            break;
    }

    return iface;
}

const unsigned char XCVR_BITS_PORT_TYPE = 0b00000011;
const unsigned char XCVR_BITS_TERM =      0b00000100;
const unsigned char SENSOR_BITS_POWER =     0b00001000;

/*
 *  Class FtdiXcvrGPIO provides the means to access the FTDI FT4232H device
 *  which is designated for serial transceiver and power control. If a FT4232H
 *  device is not found, then a simple shadow register will be used for testing
 *  purposes only.
 */
class FtdiXcvrGPIO : public GpioIF
{
public:
    FtdiXcvrGPIO(GPIO_PORT_DEFS port)
    : _pFtdiDevice(0), _port(port), _shadow(0)
    {
        _pFtdiDevice = getFtdiDevice(FTDI_GPIO, port2iface(port));

        if (ifaceFound()) {
            _pFtdiDevice->setMode(0xFF, BITMODE_BITBANG);
        }
    }

    virtual ~FtdiXcvrGPIO()
    {
        DLOG(("FtdiXcvrGPIO::~FtdiXcvrGPIO(): destructing..."));
        // don't delete _pFtdiDevice, because someone else may be using it
        _pFtdiDevice = 0;
    }

    GPIO_PORT_DEFS getPort() {return _port;}

    bool ifaceFound()
    {
        return _pFtdiDevice->ifaceFound();
    }

    ftdi_interface getInterface()
    {
        return _pFtdiDevice->getInterface();
    }

    virtual void write(unsigned char bits)
    {
        _pFtdiDevice->write(bits);
    }

    virtual void write(unsigned char bits, unsigned char mask)
    {
        unsigned char rawBits = _shadow;
        if (ifaceFound()) {
            rawBits = _pFtdiDevice->read();
            DLOG(("FtdiXcvrGPIO::write(): Raw bits: 0x%0x", rawBits));
            rawBits &= ~adjustBitPosition(mask);
            rawBits |= adjustBitPosition(bits);
            DLOG(("FtdiXcvrGPIO::write(): New bits: 0x%0x", rawBits));
            write(rawBits);
        }
        else {
            rawBits &= ~adjustBitPosition(mask);
            rawBits |= adjustBitPosition(bits);
            _shadow = rawBits;
        }
    }

    virtual unsigned char read()
    {
        unsigned char retval = adjustBitPosition(_shadow, true);
        if (ifaceFound()) {
            retval = adjustBitPosition(_pFtdiDevice->read(), true);
            DLOG(("FtdiXcvrGPIO::read(): Actually read the device and the value is: 0x%02x", retval));
        }
        return retval;
    }

protected:
    unsigned char adjustBitPosition(const unsigned char bits, bool read=false)
    {
        // adjust port shift to always be 0 or 4, assuming 4 bits per port configuration.
        unsigned char portShift = (_port % SER_PORT2)*4;
        unsigned char retVal = bits << portShift;
        if (read) {
            retVal = (bits >> portShift) & 0x0F;
        }

        return retVal;
    }

private:
    FtdiHwIF* _pFtdiDevice;
    GPIO_PORT_DEFS _port;

    // only used for testing
    unsigned char _shadow;

    /*
     *  No copying
     */

    FtdiXcvrGPIO(const FtdiXcvrGPIO&);
    FtdiXcvrGPIO& operator=(FtdiXcvrGPIO&);
    const FtdiXcvrGPIO& operator=(const FtdiXcvrGPIO&);
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_XCVRGPIO_H
