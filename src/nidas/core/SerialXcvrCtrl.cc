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
#include <nidas/util/Logger.h>
#include "SerialXcvrCtrl.h"

#include <algorithm>
#include <sstream>

namespace n_u = nidas::util;
namespace nidas { namespace core {

const char* SerialXcvrCtrl::STR_LOOPBACK = "LOOPBACK";
const char* SerialXcvrCtrl::STR_RS232 = "RS232";
const char* SerialXcvrCtrl::STR_RS422 = "RS422";
const char* SerialXcvrCtrl::STR_RS485_HALF = "RS485_HALF";
const char* SerialXcvrCtrl::STR_RS485_FULL = "RS485_FULL";
const char* SerialXcvrCtrl::STR_NO_TERM = "NO_TERM";
const char* SerialXcvrCtrl::STR_TERM_120_OHM = "TERM_120_OHM";
const char* SerialXcvrCtrl::STR_POWER_ON = "POWER_ON";
const char* SerialXcvrCtrl::STR_POWER_OFF = "POWER_OFF";


SerialXcvrCtrl::SerialXcvrCtrl(const PORT_DEFS portId)
: _portID(portId), _portType(LOOPBACK), _term(NO_TERM), _powerstate(SENSOR_POWER_ON), 
  _portConfig(0), _busAddr(1), _deviceAddr(6), _pContext(ftdi_new())
{
    if (_pContext)
    {
        enum ftdi_interface iface = port2iface(portId);
        if (iface != INTERFACE_ANY) {
            ftdi_set_interface(_pContext, iface);

            // set bit bang mode if not already in that mode
            if (!ftdi_usb_open_bus_addr(_pContext, _busAddr, _deviceAddr))
            {
                const char* ifaceIdx = (iface==1 ? "A" : iface==2 ? "B" : iface==3 ? "C" : iface==4 ? "D" : "?!?");
                NLOG(("SerialXcvrCtrl: Successfully opened GPIO on INTERFACE_") << ifaceIdx);
                if (!_pContext->bitbang_enabled) {
                    // Now initialize the chosen device for bit-bang mode, all outputs
                    if (!ftdi_set_bitmode(_pContext, 0xFF, BITMODE_BITBANG)) {
                        NLOG(("SerialXcvrCtrl: Successfully set GPIO on INTERFACE_") 
                              << ifaceIdx << "to bitbang mode");

                        if (ftdi_usb_close(_pContext)) {
                            throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't close USB device: ") 
                                                        + ftdi_get_error_string(_pContext));
                        }
                    }
                    else
                    {
                        throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't set bitbang mode: ") 
                                                    + ftdi_get_error_string(_pContext));
                    }
                }
                else {
                    NLOG(("SerialXcvrCtrl: Already in bitbang mode; proceed to set port config."));    
                }
            }

            else
            {
                throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't open device")
                                                    + ftdi_get_error_string(_pContext));
            }
        }

        else
        {
            std::ostringstream errString("SerialXcvrCtrl: Failed to get a valid interface ID: ctor portId arg is not valid: ");
            errString << portId;
            throw n_u::Exception(errString.str());
        }
    }

    else 
    {
        throw n_u::Exception("SerialXcvrCtrl: ctor failed to allocate ftdi_struct");
    }
}

SerialXcvrCtrl::SerialXcvrCtrl(const PORT_DEFS portId, 
                                                     const PORT_TYPES portType, 
                                                     const TERM termination,
                                                     const SENSOR_POWER_STATE pwrState)
: _portID(portId), _portType(portType), _term(termination), _powerstate(pwrState), 
  _portConfig(0), _busAddr(1), _deviceAddr(6), _pContext(ftdi_new())
{
    if (_pContext)
    {
        enum ftdi_interface iface = port2iface(portId);
        if (iface != INTERFACE_ANY) {
            ftdi_set_interface(_pContext, iface);

            // set bit bang mode if not already in that mode
            if (!ftdi_usb_open_bus_addr(_pContext, _busAddr, _deviceAddr))
            {
                const char* ifaceIdx = (iface==1 ? "A" : iface==2 ? "B" : iface==3 ? "C" : iface==4 ? "D" : "?!?");
                NLOG(("SerialXcvrCtrl: Successfully opened GPIO on INTERFACE_") << ifaceIdx);
                if (!_pContext->bitbang_enabled) {
                    // Now initialize the chosen device for bit-bang mode, all outputs
                    if (!ftdi_set_bitmode(_pContext, 0xFF, BITMODE_BITBANG)) {
                        NLOG(("SerialXcvrCtrl: Successfully set GPIO on INTERFACE_") 
                              << ifaceIdx << "to bitbang mode");
                    }
                    else
                    {
                        throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't set bitbang mode: ") 
                                                    + ftdi_get_error_string(_pContext));
                    }
                }
                else {
                    NLOG(("SerialXcvrCtrl: Already in bitbang mode; proceed to set port config."));    
                }
                // And while we're at it, set the port type and termination.
                // Don't need to open the device this time.
                applyPortConfig(false);

                // All done, close'er up.
                ftdi_usb_close(_pContext);
            }

            else
            {
                throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't open device")
                                                    + ftdi_get_error_string(_pContext));
            }
        }

        else
        {
            throw n_u::Exception("SerialXcvrCtrl: ctor portId arg is not valid");
        }
    }

    else 
    {
        throw n_u::Exception("SerialXcvrCtrl: ctor failed to allocate ftdi_struct");
    }
}

SerialXcvrCtrl::~SerialXcvrCtrl()
{
    ftdi_free(_pContext);
}

void SerialXcvrCtrl::setPortConfig(const PORT_TYPES portType, 
                                              const TERM term, 
                                              const SENSOR_POWER_STATE powerState)
{
    _portType = portType;
    _term = term;
    _powerstate = powerState;
}

void SerialXcvrCtrl::applyPortConfig(const bool openDevice)
{
    if (openDevice) {
        if (ftdi_usb_open_bus_addr(_pContext, _busAddr, _deviceAddr))
        {
            throw n_u::Exception("SerialXcvrCtrl: cannot open USB device");
        }
    }

    unsigned char _portConfig;
    // get the current port definitions
    if (ftdi_read_pins(_pContext, &_portConfig)) {
        throw n_u::Exception("SerialXcvrCtrl: cannot read the GPIO pins "
                             "on previously opened USB device");

    }

    NLOG(("Applying port type: ") << portTypeToStr(_portType));

    _portConfig &= ~adjustBitPosition(_portID, 0xF);
    _portConfig |= adjustBitPosition(_portID, assembleBits(_portType, _term , _powerstate));

    // Call FTDI API to set the desired port types
    if (!ftdi_write_data(_pContext, &_portConfig, 1)) {
        throw n_u::Exception("SerialXcvrCtrl: cannot write the GPIO pins "
                             "on previously opened USB device");        
    }

    unsigned char checkConfig = 0;

    if (ftdi_read_pins(_pContext, &checkConfig)) {
        throw n_u::Exception("SerialXcvrCtrl: cannot read the GPIO pins "
                             "after writing the GPIO");        
    }

    if (checkConfig != _portConfig) {
        throw n_u::Exception("SerialXcvrCtrl: the pins written to the GPIO "
                             "do not match the pins read from the GPIO");        
    }

    if (openDevice) {
        if (ftdi_usb_close(_pContext)) {
            throw n_u::Exception("SerialXcvrCtrl: cannot close "
                                 "previously opened USB device");
        }
    }
}

void SerialXcvrCtrl::setBusAddress(const int busId, const int deviceId)
{
    _busAddr = busId;
    _deviceAddr = deviceId;
}

unsigned char SerialXcvrCtrl::assembleBits(const PORT_TYPES portType, 
                                                      const TERM term, 
                                                      const SENSOR_POWER_STATE powerState)
{
    unsigned char bits = portType2Bits(portType);

    if (portType == RS422 || portType == RS485_HALF || portType == RS485_FULL) {
        if (term == TERM_120_OHM) {
            bits |= TERM_120_OHM_BIT;
        }
    }

    if (powerState == SENSOR_POWER_ON) {
        bits |= SENSOR_POWER_ON_BIT;
    }

    return bits;
}

unsigned char SerialXcvrCtrl::portType2Bits(const PORT_TYPES portType) 
{
    unsigned char bits = 0b11111111;
    switch (portType) {
        case RS422:
        case RS485_FULL:
            bits = RS422_RS485_BITS;
            break;

        case RS485_HALF:
            bits = RS485_HALF_BITS;
            break;
        
        case RS232:
            bits = RS232_BITS;
            break;

        case LOOPBACK:
        default:
            bits = LOOPBACK_BITS;
            break;
    }

    return bits;
}

PORT_TYPES SerialXcvrCtrl::bits2PortType(const unsigned char bits) 
{
    PORT_TYPES portType = static_cast<PORT_TYPES>(-1);
    switch (bits & 0b00000011) {
        case RS422_RS485_BITS:
            portType = RS422;
            break;

        case RS485_HALF_BITS:
            portType = RS485_HALF;
            break;
        
        case RS232_BITS:
            portType = RS232;
            break;

        case LOOPBACK_BITS:
        default:
            portType = LOOPBACK;
            break;
    }

    return portType;
}

unsigned char SerialXcvrCtrl::adjustBitPosition(const PORT_DEFS port, const unsigned char bits ) 
{
    // adjust port shift to always be 0 or 4, assuming 4 bits per port configuration.
    unsigned char portShift = (port % PORT2)*4;
    return bits << portShift;
}

enum ftdi_interface SerialXcvrCtrl::port2iface(const unsigned int port)
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

const std::string SerialXcvrCtrl::portTypeToStr(const PORT_TYPES portType)
{
    std::string portTypeStr("");
    switch (portType) {
        case LOOPBACK:
            portTypeStr.append(STR_LOOPBACK);
            break;
        case RS232:
            portTypeStr.append(STR_RS232);
            break;
        case RS485_HALF:
            portTypeStr.append(STR_RS485_HALF);
            break;
        case RS422:
        case RS485_FULL:
            portTypeStr.append(STR_RS422);
            portTypeStr.append("/");
            portTypeStr.append(STR_RS485_FULL);
            break;
        default:
            break;
    }

    return portTypeStr;
}

PORT_TYPES SerialXcvrCtrl::strToPortType(const char* portStr) {
    PORT_TYPES retval = LOOPBACK;
    std::string portString(portStr);
    std::transform(portString.begin(), portString.end(), portString.begin(), ::toupper);

    std::cout << "SerialXcvrCtrl::strToPortType: portStr arg: " << portStr << std::endl;

    if (portString == std::string(STR_RS232)) {
        retval = RS232;
    } else if (portString == std::string(STR_RS422) || portString == std::string(STR_RS485_FULL)) {
        retval = RS422;
    } else if (portString == std::string(STR_RS485_HALF)) {
        retval = RS485_HALF;
    }

    return retval;
}

const std::string SerialXcvrCtrl::termToStr(unsigned char termCfg) const
{
    std::string termStr("");
    switch (termCfg) {
        case TERM_120_OHM_BIT:
            termStr.append(STR_TERM_120_OHM);
            break;
        case NO_TERM:
        default:
            termStr.append(STR_NO_TERM);
            break;
    }

    return termStr;
}

const std::string SerialXcvrCtrl::powerToStr(unsigned char powerCfg) const 
{
    std::string powerStr("");
    switch (powerCfg) {
        case SENSOR_POWER_ON_BIT:
            powerStr.append(STR_POWER_ON);
            break;
        case SENSOR_POWER_OFF:
        default:
            powerStr.append(STR_POWER_OFF);
            break;
    }

    return powerStr;
}

void SerialXcvrCtrl::printPortConfig(const PORT_DEFS port, const bool addNewline, const bool readFirst)
{
    if (readFirst) {
        NLOG(("SerialXcvrCtrl: Reading GPIO pin state before reporting them."));
        if (!ftdi_usb_open_bus_addr(_pContext, _busAddr, _deviceAddr)) {
            enum ftdi_interface iface = port2iface(port);
            const char* ifaceIdx = (iface==INTERFACE_A ? "A" : iface==INTERFACE_B ? "B" 
                                    : iface==INTERFACE_C ? "C" : iface==INTERFACE_D ? "D" : "?!?");
            NLOG(("SerialXcvrCtrl: Successfully opened GPIO on INTERFACE_") << ifaceIdx);
            if (!_pContext->bitbang_enabled) {
                // Now initialize the chosen device for bit-bang mode, all outputs
                if (!ftdi_set_bitmode(_pContext, 0xFF, BITMODE_BITBANG)) {
                    NLOG(("SerialXcvrCtrl: Successfully set GPIO on INTERFACE_") 
                            << ifaceIdx << " to bitbang mode");
                }
                else {
                    throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't set bitbang mode: ") 
                                                + ftdi_get_error_string(_pContext));
                }
            }
            else {
                NLOG(("SerialXcvrCtrl: Already in bitbang mode; proceed to read current GPIO state."));    
            }

            if (ftdi_read_pins(_pContext, &_portConfig)) {
                throw n_u::Exception("SerialXcvrCtrl: cannot read the GPIO pins "
                                    "on previously opened USB device");

            }
        }
        else {
            throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't open device")
                                                + ftdi_get_error_string(_pContext));
        }
    }

    unsigned char tmpPortConfig = _portConfig;
    if (port % 2) tmpPortConfig >>= 4;
    std::cout << "Port" << port << ": " << portTypeToStr(bits2PortType(tmpPortConfig & RS422_RS485_BITS)) 
                                << " | " << termToStr(tmpPortConfig & TERM_120_OHM_BIT)
                                << " | " << powerToStr(tmpPortConfig & SENSOR_POWER_ON_BIT);
    if (addNewline) {
        std::cout << std::endl;
    }

    if (readFirst) {
        if (ftdi_usb_close(_pContext)) {
            throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't close USB device: ") 
                                        + ftdi_get_error_string(_pContext));
        }
    }
}





}} // namespace { nidas namespace core {
