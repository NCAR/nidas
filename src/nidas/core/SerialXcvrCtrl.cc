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
#include <libusb.h>

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

void XcvrConfig::print()
{
    std::cout << "Port" << port << ": " << SerialXcvrCtrl::portTypeToStr(portType) 
                                << " | " << SerialXcvrCtrl::termToStr(termination)
              << std::endl;
}

std::ostream& operator <<(std::ostream& rOutStrm, const XcvrConfig& rObj)
{
    rOutStrm << "Port" << rObj.port << ": " << SerialXcvrCtrl::portTypeToStr(rObj.portType)
                                << " | " << SerialXcvrCtrl::termToStr(rObj.termination);

    return rOutStrm;
}


SerialXcvrCtrl::SerialXcvrCtrl(const PORT_DEFS portId)
: _xcvrConfig(portId, RS232, NO_TERM), _rawXcvrConfig(0), _pSerialGPIO(new SerialGPIO(port2iface()))
{
    if (_pSerialGPIO && _pSerialGPIO->deviceFound()) {
        DLOG(("SerialXcvrCtrl(): SeriaPortGPIO object constructed and device found..."));
        applyXcvrConfig(true);
        DLOG(("SerialXcvrCtrl(): applied XcvrConfig..."));
    }

    else
    {
        throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't find GPIO device: "));
    }
}

SerialXcvrCtrl::SerialXcvrCtrl(const PORT_DEFS portId, 
                               const PORT_TYPES portType, 
                               const TERM termination)
: _xcvrConfig(portId, portType, termination), _rawXcvrConfig(0), _pSerialGPIO(new SerialGPIO(port2iface()))
{
    if (_pSerialGPIO && _pSerialGPIO->deviceFound()) {
        DLOG(("SerialXcvrCtrl(): SeriaPortGPIO object constructed and device found..."));
        applyXcvrConfig(true);
        DLOG(("SerialXcvrCtrl(): applied XcvrConfig..."));
    }

    else
    {
        throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't find GPIO device: "));
    }
}

SerialXcvrCtrl::SerialXcvrCtrl(const XcvrConfig initXcvrConfig)
: _xcvrConfig(initXcvrConfig), _rawXcvrConfig(0), _pSerialGPIO(new SerialGPIO(n_u::port2iface(initXcvrConfig.port)))
{
    if (_pSerialGPIO && _pSerialGPIO->deviceFound()) {
        DLOG(("SerialXcvrCtrl(): SeriaPortGPIO object constructed and device found..."));
        applyXcvrConfig(true);
        DLOG(("SerialXcvrCtrl(): applied XcvrConfig..."));
    }

    else
    {
        throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't find GPIO device: "));
    }
}

SerialXcvrCtrl::~SerialXcvrCtrl()
{
//    ftdi_free(_pContext);
}

void SerialXcvrCtrl::setXcvrConfig(const PORT_TYPES portType, 
                                   const TERM term)
{
    _xcvrConfig.portType = portType;
    _xcvrConfig.termination = term;
}

void SerialXcvrCtrl::setXcvrConfig(const XcvrConfig& newXcvrConfig)
{
    if (newXcvrConfig.port != _xcvrConfig.port) {
        DLOG(("Current port: ") << _xcvrConfig.port << " - New port: " << newXcvrConfig.port);
        _xcvrConfig.port = newXcvrConfig.port;
        enum ftdi_interface iface = port2iface();
        DLOG(("Setting the FTDI BitBang interface to INTERFACE_") << (iface==INTERFACE_A ? "A" :
                                                                        iface==INTERFACE_B ? "B" :
                                                                        iface==INTERFACE_C ? "C" :
                                                                        iface==INTERFACE_D ? "D" : "???"));
    }

    _xcvrConfig = newXcvrConfig;
}

unsigned char SerialXcvrCtrl::assembleBits(const PORT_TYPES portType,
                                           const TERM term)
{
    unsigned char bits = portType2Bits(portType);

    if (portType == RS422 || portType == RS485_HALF || portType == RS485_FULL) {
        if (term == TERM_120_OHM) {
            bits |= TERM_120_OHM_BIT;
        }
    }

    return bits;
}

void SerialXcvrCtrl::applyXcvrConfig(const bool readDevice)
{
    if (readDevice) {
        DLOG(("SerialXcvrCtrl::applyXcvrConfig(): Reading GPIO pin state before adjusting them."));
        readXcvrConfig();
    }

    DLOG(("Working on PORT") << (int)(_xcvrConfig.port));
    DLOG(("Applying port type: ") << portTypeToStr(_xcvrConfig.portType));
    DLOG(("Applying termination: ") << termToStr(_xcvrConfig.termination));
//    DLOG(("Applying power: ") << powerStateToStr(_xcvrConfig.sensorPower));

    DLOG(("Raw xcvr config: 0X%02X", _rawXcvrConfig));
    _rawXcvrConfig &= ~adjustBitPosition(0b00000111);
    DLOG(("Raw xcvr config after mask: 0X%02X", _rawXcvrConfig));

    _rawXcvrConfig |= adjustBitPosition(assembleBits(_xcvrConfig.portType, _xcvrConfig.termination));// , _xcvrConfig.sensorPower));
	DLOG(("New raw xcvr config: 0X%02X", _rawXcvrConfig));

    DLOG(("Writing xcvr config to FT4232H"));
    // Call FTDI API to set the desired port types
    _pSerialGPIO->writeInterface(_rawXcvrConfig);

    // re-read to compare....
    unsigned char checkConfig = _pSerialGPIO->readInterface();

    if (checkConfig != _rawXcvrConfig) {
        throw n_u::Exception("SerialXcvrCtrl: the pins written to the GPIO "
                            "do not match the pins read from the GPIO");
    }

    DLOG(("Written config matches desired config."));
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
    return n_u::port2iface(_xcvrConfig.port);
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

void SerialXcvrCtrl::readXcvrConfig() 
{
    DLOG(("Attempting to read the current FT4232H GPIO pin settings"));
    _rawXcvrConfig = _pSerialGPIO->readInterface();
    DLOG(("Successfully read FT4232H GPIO pin settings. Now closing device."));
}

void SerialXcvrCtrl::printXcvrConfig(const bool addNewline, const bool readFirst)
{
    if (readFirst) {
        DLOG(("SerialXcvrCtrl: Reading GPIO pin state before reporting them."));
        readXcvrConfig();

    unsigned char tmpPortConfig = _rawXcvrConfig;
    if (_xcvrConfig.port % 2) tmpPortConfig >>= 4;
    std::cout << "Port" << _xcvrConfig.port << ": " << portTypeToStr(bits2PortType(tmpPortConfig & RS422_RS485_BITS)) 
                                << " | " << rawTermToStr(tmpPortConfig & TERM_120_OHM_BIT);
    }
    else {
        _xcvrConfig.print();
    }

    if (addNewline) {
        std::cout << std::endl;
    }
}

}} // namespace { nidas namespace core {
