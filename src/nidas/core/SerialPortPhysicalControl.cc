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
#include <nidas/util/Exception.h>

#include "SerialPortPhysicalControl.h"

namespace n_u = nidas::util;
namespace nidas { namespace core {

SerialPortPhysicalControl::SerialPortPhysicalControl(const PORT_DEFS portId, 
                                                     const PORT_TYPES portType, 
                                                     const TERM termination)
: _portID(portId), _portType(portType), _term(termination), _powerstate(SENSOR_POWER_ON), 
  _portConfig(0), _busAddr(1), _deviceAddr(6), _pContext(ftdi_new())
{
    if (_pContext)
    {
        enum ftdi_interface iface = port2iface(portId);
        if (iface != INTERFACE_ANY) {
            ftdi_set_interface(_pContext, iface);

            // set bit bang mode if not already in that mode
            if (_pContext->bitbang_mode != BITMODE_BITBANG) {
                if (!ftdi_usb_open_bus_addr(_pContext, _busAddr, _deviceAddr))
                {
                    // Now initialize the chosen device for bit-bang mode, all outputs
                    if (!ftdi_set_bitmode(_pContext, 0xFF, BITMODE_BITBANG)) {
                        
                        // And while we're at it, set the port type and termination.
                        // Don't need to open the device this time.
                        applyPortConfig(false);

                        // All done, close'er up.
                        ftdi_usb_close(_pContext);
                    }

                    else
                    {
                        throw n_u::Exception(std::string("SerialPortPhysicalControl: ctor error: Couldn't set bitbang mode: ") 
                                                    + ftdi_get_error_string(_pContext));
                    }
                }

                else
                {
                    throw n_u::Exception(std::string("SerialPortPhysicalControl: ctor error: Couldn't open device")
                                                     + ftdi_get_error_string(_pContext));
                }
            }
        }

        else
        {
            throw n_u::Exception("SerialPortPhysicalControl: ctor portId arg is not valid");
        }
    }

    else 
    {
        throw n_u::Exception("SerialPortPhysicalControl: ctor failed to allocate ftdi_struct");
    }
}

SerialPortPhysicalControl::~SerialPortPhysicalControl()
{
    ftdi_free(_pContext);
}

void SerialPortPhysicalControl::setPortConfig(const PORT_TYPES portType, 
                                              const TERM term, 
                                              const SENSOR_POWER_STATE powerState)
{
    _portType = portType;
    _term = term;
    _powerstate = powerState;
}

void SerialPortPhysicalControl::applyPortConfig(const bool openDevice)
{
    if (openDevice) {
        if (ftdi_usb_open_bus_addr(_pContext, _busAddr, _deviceAddr))
        {
            throw n_u::Exception("SerialPortPhysicalControl: cannot open USB device");
        }
    }

    unsigned char portConfig;
    // get the current port definitions
    ftdi_read_pins(_pContext, &portConfig);

    portConfig &= ~adjustBitPosition(_portID, 0xF);
    portConfig |= adjustBitPosition(_portID, assembleBits(_portType, _term , _powerstate));

    // Call FTDI API to set the desired port types
    ftdi_write_data(_pContext, &portConfig, 1);

    if (openDevice) {
        if (ftdi_usb_close(_pContext)) {
            throw n_u::Exception("SerialPortPhysicalControl: cannot close "
                                 "previously opened USB device");
        }
    }
}

void SerialPortPhysicalControl::setBusAddress(const int busId, const int deviceId)
{
    _busAddr = busId;
    _deviceAddr = deviceId;
}

unsigned char SerialPortPhysicalControl::assembleBits(const PORT_TYPES portType, 
                                                      const TERM term, 
                                                      const SENSOR_POWER_STATE powerState)
{
    unsigned char bits = portType << 2;
    if (term == TERM_96k_OHM) {
        bits |= 1 << 1;
    }

    if (powerState) {
        bits |= 1;
    }

    return bits;
}

unsigned char SerialPortPhysicalControl::adjustBitPosition(const PORT_DEFS port, const unsigned char bits ) 
{
    // adjust port shift to always be 0 or 4, assuming 4 bits per port configuration.
    unsigned char portShift = (port % PORT2)*4;
    return bits << portShift;
}

enum ftdi_interface SerialPortPhysicalControl::port2iface(const unsigned int port)
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




}} // namespace { nidas namespace core {
