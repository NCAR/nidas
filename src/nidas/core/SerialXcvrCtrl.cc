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
using namespace nidas::util;

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

void XcvrConfig::print()
{
    std::cout << "Port" << port << ": " << SerialXcvrCtrl::portTypeToStr(portType) 
                                << " | " << SerialXcvrCtrl::termToStr(termination)
                                << " | " << SerialXcvrCtrl::powerStateToStr(sensorPower)
              << std::endl;
}

std::ostream& operator <<(std::ostream& rOutStrm, const XcvrConfig& rObj)
{
    rOutStrm << "Port" << rObj.port << ": " << SerialXcvrCtrl::portTypeToStr(rObj.portType)
                                << " | " << SerialXcvrCtrl::termToStr(rObj.termination)
                                << " | " << SerialXcvrCtrl::powerStateToStr(rObj.sensorPower);

    return rOutStrm;
}



SerialXcvrCtrl::SerialXcvrCtrl(const PORT_DEFS portId)
: _xcvrConfig(portId, RS232, NO_TERM), _rawXcvrConfig(0),
  _busAddr(1), _deviceAddr(6), _pContext(ftdi_new()), _gpioOpen(false)
{
}

SerialXcvrCtrl::SerialXcvrCtrl(const PORT_DEFS portId, 
                               const PORT_TYPES portType, 
                               const TERM termination,
                               const SENSOR_POWER_STATE pwrState)
: _xcvrConfig(portId, portType, termination, pwrState), _rawXcvrConfig(0), 
  _busAddr(1), _deviceAddr(6), _pContext(ftdi_new()), _gpioOpen(false)
{
}

SerialXcvrCtrl::SerialXcvrCtrl(const XcvrConfig initXcvrConfig)
: _xcvrConfig(initXcvrConfig), _rawXcvrConfig(0), 
  _busAddr(1), _deviceAddr(6), _pContext(ftdi_new()), _gpioOpen(false)
{
}

SerialXcvrCtrl::~SerialXcvrCtrl()
{
    ftdi_free(_pContext);
}

void SerialXcvrCtrl::setXcvrConfig(const PORT_TYPES portType, 
                                              const TERM term, 
                                              const SENSOR_POWER_STATE powerState)
{
    _xcvrConfig.portType = portType;
    _xcvrConfig.termination = term;
    _xcvrConfig.sensorPower = powerState;
}

void SerialXcvrCtrl::applyXcvrConfig(const bool readDevice)
{
    bool ifWeOpenedIt = gpioOpen();

    if (readDevice) {
        VLOG(("SerialXcvrCtrl: Reading GPIO pin state before adjusting them."));
        readXcvrConfig();
    }

    DLOG(("Working on PORT") << (int)(_xcvrConfig.port));
    DLOG(("Applying port type: ") << portTypeToStr(_xcvrConfig.portType));
    DLOG(("Applying termination: ") << termToStr(_xcvrConfig.termination));
    DLOG(("Applying power: ") << powerStateToStr(_xcvrConfig.sensorPower));

    DLOG(("Raw xcvr config: 0X%02X", _rawXcvrConfig));
    _rawXcvrConfig &= ~adjustBitPosition(0xF);
    DLOG(("Raw xcvr config: 0X%02X", _rawXcvrConfig));
    _rawXcvrConfig |= adjustBitPosition(assembleBits(_xcvrConfig.portType, _xcvrConfig.termination , _xcvrConfig.sensorPower));
	DLOG(("Raw xcvr config: 0X%02X", _rawXcvrConfig));

    if (gpioIsOpen()) {
        DLOG(("Writing xcvr config to FT4232H"));
        // Call FTDI API to set the desired port types
        if (!ftdi_write_data(_pContext, &_rawXcvrConfig, 1)) {
            throw n_u::Exception("SerialXcvrCtrl: cannot write the GPIO pins "
                                "on previously opened USB device");        
        }

        // re-read to compare....
        unsigned char checkConfig = 0;
        if (ftdi_read_pins(_pContext, &checkConfig)) {
            throw n_u::Exception("SerialXcvrCtrl: cannot read the GPIO pins "
                                "after writing the GPIO");        
        }

        if (checkConfig != _rawXcvrConfig) {
            throw n_u::Exception("SerialXcvrCtrl: the pins written to the GPIO "
                                "do not match the pins read from the GPIO");        
        }

        DLOG(("Written config matches desired config."));

        if (!gpioClose(ifWeOpenedIt)) {
            throw n_u::Exception("SerialXcvrCtrl: cannot close "
                                 "previously opened USB device");
        }
    }
    else {
        gpioClose(ifWeOpenedIt);
        throw n_u::Exception(std::string("SerialXcvrCtrl::applyXcvrConfig(): Couldn't open device: ")
                                            + ftdi_get_error_string(_pContext));
    }
}

void SerialXcvrCtrl::initFtdi()
{
    bool ifWeOpenedIt = false;
    if (_pContext) {
        enum ftdi_interface iface = port2iface();
        if (iface != INTERFACE_ANY) {
            ftdi_set_interface(_pContext, iface);
            // set bit bang mode if not already in that mode
            ifWeOpenedIt = gpioOpen();
            if (gpioIsOpen()) {
                const char* ifaceIdx = (iface == 1 ? "A" : iface == 2 ? "B" :
                                        iface == 3 ? "C" :
                                        iface == 4 ? "D" : "?!?");
                VLOG(
                        ("SerialXcvrCtrl: initFtdi(): Successfully opened GPIO on INTERFACE_") << ifaceIdx);
                if (!_pContext->bitbang_enabled) {
                    // Now initialize the chosen device for bit-bang mode, all outputs
                    if (!ftdi_set_bitmode(_pContext, 0xFF, BITMODE_BITBANG)) {
                        NLOG(
                                ("SerialXcvrCtrl: initFtdi(): Successfully set GPIO on INTERFACE_") << ifaceIdx << " to bitbang mode");
                    }
                    else {
                        gpioClose(ifWeOpenedIt);
                        throw n_u::Exception(
                                std::string(
                                        "SerialXcvrCtrl: initFtdi() error: Couldn't set bitbang mode: ")
                                        + ftdi_get_error_string(_pContext));
                    }
                }
                else {
                    VLOG(
                            ("SerialXcvrCtrl: initFtdi(): Already in bitbang mode; proceed to set port config."));
                }
                // And while we're at it, set the port type and termination.
                // Don't need to open the device this time.
                applyXcvrConfig(true);
                gpioClose(ifWeOpenedIt);
            }
            else {
                gpioClose(ifWeOpenedIt);
                throw n_u::Exception(
                        std::string(
                                "SerialXcvrCtrl: initFtdi() error: Couldn't open device: ")
                                + ftdi_get_error_string(_pContext));
            }
        }
        else {
            throw n_u::Exception(
                    "SerialXcvrCtrl: initFtdi(): portId arg is not valid");
        }
    }
    else {
        throw n_u::Exception(
                "SerialXcvrCtrl: initFtdi(): failed to allocate ftdi_struct");
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
    unsigned char bits = 0xFF;
    switch (portType) {
        case RS422:
        //case RS485_FULL:
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
    switch (bits & 0x03) {
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

unsigned char SerialXcvrCtrl::adjustBitPosition(const unsigned char bits ) 
{
    // adjust port shift to always be 0 or 4, assuming 4 bits per port configuration.
    unsigned char portShift = (_xcvrConfig.port % PORT2)*4;
    return bits << portShift;
}

enum ftdi_interface SerialXcvrCtrl::port2iface()
{
    enum ftdi_interface iface = INTERFACE_ANY;
    switch ( _xcvrConfig.port ) 
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
        //case RS485_FULL:
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

const std::string SerialXcvrCtrl::rawTermToStr(unsigned char termCfg)
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

const std::string SerialXcvrCtrl::rawPowerToStr(unsigned char powerCfg)
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

bool SerialXcvrCtrl::gpioOpen()
{
    if (!gpioIsOpen()) {
        VLOG(("Attempting to open FT4232H GPIO device..."));
        if (!ftdi_usb_open_bus_addr(_pContext, _busAddr, _deviceAddr)) {

            VLOG(("Successfully opened FT4232H GPIO device..."));
            // we opened, so we should close it
            _gpioOpen = true;
            return true;
        }
        else {
            // we opened it unsuccessfully
            VLOG(("Failed to open FT4232H GPIO device..."));
            return false;
        }
    }

    // we didn't open, so we're not going to close it.
    return false;
}

bool SerialXcvrCtrl::gpioClose(bool ifWeOpenedIt)
{
    if (gpioIsOpen()) {
        if (ifWeOpenedIt) {
            VLOG(("Attempting to close FT4232H GPIO device..."));
            if (!ftdi_usb_close(_pContext)) {
                _gpioOpen = false;
                // we successfully closed it when we should have.
                VLOG(("Successfully closed FT4232H GPIO device..."));
                return true;
            }
            else {
                // we unsuccessfully closed it
                VLOG(("Failed to close FT4232H GPIO device..."));
                return false;
            }
        }
        else {
            // we successfully didn't close it when we shouldn't have
            VLOG(("Successfully did not close FT4232H GPIO device because we didn't open it..."));
            return true;
        }
    }
    else {
        // we successfully didn't close it because it was already closed
        VLOG(("Did not close FT4232H GPIO device because it was already..."));
        return true;
    }
}


void SerialXcvrCtrl::readXcvrConfig() 
{
    bool ifWeOpenedIt = false;
    ifWeOpenedIt = gpioOpen();

    if (gpioIsOpen()) {
        enum ftdi_interface iface = port2iface();
        const char* ifaceIdx = (iface==INTERFACE_A ? "A" : iface==INTERFACE_B ? "B" 
                                : iface==INTERFACE_C ? "C" : iface==INTERFACE_D ? "D" : "?!?");
        VLOG(("SerialXcvrCtrl: Successfully opened GPIO on INTERFACE_") << ifaceIdx);
        if (!_pContext->bitbang_enabled) {
            // Now initialize the chosen device for bit-bang mode, all outputs
            if (!ftdi_set_bitmode(_pContext, 0xFF, BITMODE_BITBANG)) {
                VLOG(("SerialXcvrCtrl: Successfully set GPIO on INTERFACE_")
                        << ifaceIdx << " to bitbang mode");
            }
            else {
                throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't set bitbang mode: ") 
                                            + ftdi_get_error_string(_pContext));
            }
        }
        else {
            VLOG(("SerialXcvrCtrl: Already in bitbang mode; proceed to read current GPIO state."));
        }

        VLOG(("Attemping to read the current FT4232H GPIO pin settings"));
        if (ftdi_read_pins(_pContext, &_rawXcvrConfig)) {
            throw n_u::Exception("SerialXcvrCtrl: cannot read the GPIO pins "
                                "on previously opened USB device");
        }

        VLOG(("Successfully read FT4232H GPIO pin settings. Now closing device."));
        if (!gpioClose(ifWeOpenedIt)) {
            throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't close USB device: ") 
                                        + ftdi_get_error_string(_pContext));
        }
    }

    else {
        gpioClose(ifWeOpenedIt);
        throw n_u::Exception(std::string("SerialXcvrCtrl: read error: Couldn't open device")
                                            + ftdi_get_error_string(_pContext));
    }
}

void SerialXcvrCtrl::printXcvrConfig(const bool addNewline, const bool readFirst)
{
    if (readFirst) {
        VLOG(("SerialXcvrCtrl: Reading GPIO pin state before reporting them."));
        readXcvrConfig();

    unsigned char tmpPortConfig = _rawXcvrConfig;
    if (_xcvrConfig.port % 2) tmpPortConfig >>= 4;
    std::cout << "Port" << _xcvrConfig.port << ": " << portTypeToStr(bits2PortType(tmpPortConfig & RS422_RS485_BITS)) 
                                << " | " << rawTermToStr(tmpPortConfig & TERM_120_OHM_BIT)
                                << " | " << rawPowerToStr(tmpPortConfig & SENSOR_POWER_ON_BIT);
    }
    else {
        _xcvrConfig.print();
    }

    if (addNewline) {
        std::cout << std::endl;
    }
}

}} // namespace { nidas namespace core {
