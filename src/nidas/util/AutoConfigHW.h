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
#ifndef NIDAS_UTIL_AUTOCONFIGHW_H
#define NIDAS_UTIL_AUTOCONFIGHW_H

#include "FtdiHW.h"

namespace nidas { namespace util {

/*
 * This enum specifies the ports in the DSM.
 */
enum PORT_DEFS {ILLEGAL_PORT=-1, PORT0=0, PORT1, PORT2, PORT3, PORT4, PORT5, PORT6, PORT7};
// At present there are only 7 available ports on a DSM
const PORT_DEFS MAX_PORT = PORT7;

// deduces the FT4232H GPIO interface form the port in _xcvrConfig.
// need to select the interface based on the specified port
// at present, assume 4 bits per port definition
inline enum ftdi_interface port2iface(PORT_DEFS port)
{
    enum ftdi_interface iface = INTERFACE_ANY;
    switch ( port )
    {
        case PORT0:
        case PORT1:
            iface = INTERFACE_A;
            break;
        case PORT2:
        case PORT3:
            iface = INTERFACE_B;
            break;

        case PORT4:
        case PORT5:
            iface = INTERFACE_C;
            break;
        case PORT6:
        case PORT7:
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
 *  Class XcvrGPIO provides the means to access the FTDI FT4232H device
 *  which is designated for serial transceiver and power control. If a FT4232H
 *  device is not found, then a simple shadow register will be used for testing
 *  purposes only.
 */
class XcvrGPIO
{
public:
    /*
     *  Because multiple specializations may exist on a single FTDI device interface
     *  (Xcvr control and power control, for instance), Sync selects one mutex per interface.
     *
     *  Specializations of XcvrGPIO should use the Sync class to protect their operations on
     *  the interface which they are concerned.
     */
    class Sync : public Synchronized
    {
    public:
        Sync(XcvrGPIO* me) : Synchronized(Sync::selectIfaceLock(port2iface(me->getPort()))), _me(me)
        {
            ftdi_interface iface = port2iface(me->getPort());
            DLOG(("Synced on interface %c", iface == INTERFACE_A ? 'A' : iface == INTERFACE_B ? 'B' :
                                            iface == INTERFACE_C ? 'C' : iface == INTERFACE_D ? 'D' : '?'));
        }
        ~Sync()
        {
            ftdi_interface iface = port2iface(_me->getPort());
            DLOG(("Sync released on interface %c", iface == INTERFACE_A ? 'A' : iface == INTERFACE_B ? 'B' :
                                                    iface == INTERFACE_C ? 'C' : iface == INTERFACE_D ? 'D' : '?'));
            _me = 0;
        }
    private:
        static Cond _ifaceACondVar;
        static Cond _ifaceBCondVar;
        static Cond _ifaceCCondVar;
        static Cond _ifaceDCondVar;
        XcvrGPIO* _me;

        static Cond& selectIfaceLock(ftdi_interface iface)
        {
            Cond* pCond = 0;
            switch (iface) {
            case INTERFACE_A:
                pCond = &_ifaceACondVar;
                break;
            case INTERFACE_B:
                pCond = &_ifaceBCondVar;
                break;
            case INTERFACE_C:
                pCond = &_ifaceCCondVar;
                break;
            case INTERFACE_D:
                pCond = &_ifaceDCondVar;
                break;
            default:
                throw InvalidParameterException("Sync::selectIfaceMutex(): Unknown FTDI interface value");
            }

            return *pCond;
        }

        // no copying
        Sync(const Sync& rRight);
        Sync& operator=(const Sync& rRight);
        Sync& operator=(Sync& rRight);
    };

    XcvrGPIO(PORT_DEFS port)
    : _pFtdiDevice(0), _port(port), _shadow(0)
    {
        try {
            _pFtdiDevice = getFtdiDevice(port2iface(port), std::string("UCAR"), std::string("GPIO"));
        }
        catch (InvalidParameterException& e) {
            _pFtdiDevice = 0;
        }

        if (deviceFound()) {
            _pFtdiDevice->setMode(0xFF, BITMODE_BITBANG);
        }
    }

    virtual ~XcvrGPIO()
    {
        DLOG(("XcvrGPIO::~XcvrGPIO(): destructing..."));
        // don't delete _pFtdiDevice, because someone else may be using it
        _pFtdiDevice = 0;
    }

    virtual void write(unsigned char bits, unsigned char mask)
    {
        unsigned char rawBits = _shadow;
        if (deviceFound()) {
            rawBits = _pFtdiDevice->readInterface();
            DLOG(("XcvrGPIO::write(): Raw bits: 0x%0x", rawBits));
            rawBits &= ~adjustBitPosition(mask);
            rawBits |= adjustBitPosition(bits);
            DLOG(("XcvrGPIO::write(): New bits: 0x%0x", rawBits));
            _pFtdiDevice->writeInterface(rawBits);
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
        if (deviceFound()) {
            retval = adjustBitPosition(_pFtdiDevice->readInterface(), true);
        }
        return retval;
    }

    PORT_DEFS getPort() {return _port;}

    ftdi_interface getInterface()
    {
        ftdi_interface retval = port2iface(_port);
        if (_pFtdiDevice) {
            retval =_pFtdiDevice->getInterface();
        }
        return retval;
    }


protected:
    bool deviceFound()
    {
        bool retval = false;
        if (_pFtdiDevice) {
            retval = _pFtdiDevice->deviceFound();
        }
        return retval;
    }

    unsigned char adjustBitPosition(const unsigned char bits, bool read=false)
    {
        // adjust port shift to always be 0 or 4, assuming 4 bits per port configuration.
        unsigned char portShift = (_port % PORT2)*4;
        unsigned char retVal = bits << portShift;
        if (read) {
            retVal = (bits >> portShift) & 0x0F;
        }

        return retVal;
    }

private:
    FtdiDeviceIF* _pFtdiDevice;
    PORT_DEFS _port;

    // only used for testing
    unsigned char _shadow;

    /*
     *  No copying
     */

    XcvrGPIO(const XcvrGPIO&);
    XcvrGPIO& operator=(XcvrGPIO&);
    const XcvrGPIO& operator=(const XcvrGPIO&);
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_AUTOCONFIGHW_H
