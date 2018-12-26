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

const unsigned char BITS_PORT_TYPE = 0b00000011;
const unsigned char BITS_TERM =      0b00000100;
const unsigned char BITS_POWER =     0b00001000;

/*
 *  Class SerialGPIO provides the means to access the FTDI FT4232H device
 *  which is designated for serial transceiver and power control.
 */
class SerialGPIO : public FtdiDevice
{
public:
    /*
     *  Because multiple specializations may exist on a single FTDI device interface
     *  (Xcvr control and power control, for instance), Sync selects one mutex per interface.
     *
     *  Specializations of SerialGPIO should use the Sync class to protect their operations on
     *  the interface which they are concerned.
     */
    class Sync : public Synchronized
    {
    public:
        Sync(SerialGPIO* me) : Synchronized(Sync::selectIfaceMutex(me->getInterface())), _me(me)
        {
            ftdi_interface iface = me->getInterface();
            DLOG(("Synced on interface %c", iface == INTERFACE_A ? 'A' : iface == INTERFACE_B ? 'B' :
                                            iface == INTERFACE_C ? 'C' : iface == INTERFACE_D ? 'D' : '?'));
        }
        ~Sync()
        {
            ftdi_interface iface = _me->getInterface();
            DLOG(("Sync released on interface %c", iface == INTERFACE_A ? 'A' : iface == INTERFACE_B ? 'B' :
                                                    iface == INTERFACE_C ? 'C' : iface == INTERFACE_D ? 'D' : '?'));
            _me = 0;
        }
    private:
        static Mutex _ifaceAMutex;
        static Mutex _ifaceBMutex;
        static Mutex _ifaceCMutex;
        static Mutex _ifaceDMutex;
        SerialGPIO* _me;

        static Mutex& selectIfaceMutex(ftdi_interface iface)
        {
            Mutex* pMutex = 0;
            switch (iface) {
            case INTERFACE_A:
                pMutex = &_ifaceAMutex;
                break;
            case INTERFACE_B:
                pMutex = &_ifaceBMutex;
                break;
            case INTERFACE_C:
                pMutex = &_ifaceCMutex;
                break;
            case INTERFACE_D:
                pMutex = &_ifaceDMutex;
                break;
            default:
                throw Exception("selectIfaceMutex(): Unknown FTDI interface value");
            }

            return *pMutex;
        }

        // no copying
        Sync(const Sync& rRight);
        Sync& operator=(const Sync& rRight);
        Sync& operator=(Sync& rRight);
    };

    SerialGPIO(ftdi_interface iface) : FtdiDevice(std::string("UCAR"), std::string("GPIO"), iface)
    {
        setMode(0xFF, BITMODE_BITBANG);
    }
    virtual ~SerialGPIO(){}
};

}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_AUTOCONFIGHW_H
