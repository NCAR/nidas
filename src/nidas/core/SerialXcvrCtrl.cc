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
#include <limits>

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


SerialXcvrCtrl::SerialXcvrCtrl(const GPIO_PORT_DEFS portId)
: _xcvrConfig(portId, RS232, NO_TERM), _pXcvrGPIO(new FtdiXcvrGPIO(portId))
{
    if (_pXcvrGPIO) {
        DLOG(("SerialXcvrCtrl(): SeriaPortGPIO object constructed and device found..."));
        applyXcvrConfig();
        DLOG(("SerialXcvrCtrl(): applied XcvrConfig..."));
    }

    else
    {
        throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't find GPIO device: "));
    }
}

SerialXcvrCtrl::SerialXcvrCtrl(const GPIO_PORT_DEFS portId, 
                               const PORT_TYPES portType, 
                               const TERM termination)
: _xcvrConfig(portId, portType, termination), _pXcvrGPIO(new FtdiXcvrGPIO(portId))
{
    if (_pXcvrGPIO) {
        DLOG(("SerialXcvrCtrl(): SeriaPortGPIO object constructed and device found..."));
        applyXcvrConfig();
        DLOG(("SerialXcvrCtrl(): applied XcvrConfig..."));
    }

    else
    {
        throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't find GPIO device: "));
    }
}

SerialXcvrCtrl::SerialXcvrCtrl(const XcvrConfig initXcvrConfig)
: _xcvrConfig(initXcvrConfig), _pXcvrGPIO(new FtdiXcvrGPIO(initXcvrConfig.port))
{
    if (_pXcvrGPIO) {
        DLOG(("SerialXcvrCtrl(): SeriaPortGPIO object constructed and device found..."));
        applyXcvrConfig();
        DLOG(("SerialXcvrCtrl(): applied XcvrConfig..."));
    }

    else
    {
        throw n_u::Exception(std::string("SerialXcvrCtrl: ctor error: Couldn't find GPIO device: "));
    }
}

SerialXcvrCtrl::~SerialXcvrCtrl()
{
    DLOG(("SerialXcvrCtrl::~SerialXcvrCtrl(): destructing..."));
    delete _pXcvrGPIO;
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
        DLOG(("Pointing FtdiXcvrGPIO object to new port"));
        if (_pXcvrGPIO) {
            delete _pXcvrGPIO;
            _pXcvrGPIO = 0;
            _pXcvrGPIO = new FtdiXcvrGPIO(_xcvrConfig.port);
        }
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

void SerialXcvrCtrl::applyXcvrConfig()
{
    DLOG(("Working on PORT") << (int)(_xcvrConfig.port));
    DLOG(("Applying port type: ") << portTypeToStr(_xcvrConfig.portType));
    DLOG(("Applying termination: ") << termToStr(_xcvrConfig.termination));

    unsigned char desiredConfig = assembleBits(_xcvrConfig.portType, _xcvrConfig.termination);

    DLOG(("Writing xcvr config to FT4232H"));
    // Call FTDI API to set the desired port types
    _pXcvrGPIO->write(desiredConfig, XCVR_BITS_PORT_TYPE|XCVR_BITS_TERM);

    if ((_pXcvrGPIO->read() & ~SENSOR_BITS_POWER) != desiredConfig) {
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

n_u::GPIO_PORT_DEFS SerialXcvrCtrl::devName2PortDef(std::string devName)
{
    unsigned int portID = std::numeric_limits<uint32_t>::max();
    // assume that all DSM ports are tty[DSM|USB][0-7]
    std::string ttyEndChars = "MB";
    std::size_t foundAt = devName.find_last_of(ttyEndChars);
    if (foundAt != std::string::npos) {
        std::string portChar = devName.substr(foundAt+1, 1);
        VLOG(("SerialXcvrCtrl::devName2PortDef(): Found port char, ") << portChar << ", in " << devName);
        std::istringstream portStream(portChar);

        try {
            portStream >> portID;
        }
        catch (std::exception& e) {
            throw n_u::Exception("SerialPortIODevice: device name arg "
                                "cannot be parsed for canonical port ID");
        }
    }

    return int2PortDef(portID);
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

void SerialXcvrCtrl::printXcvrConfig(const bool addNewline, const bool readFirst)
{
    if (readFirst) {
        DLOG(("SerialXcvrCtrl: Reading GPIO pin state before reporting them."));

        unsigned char tmpPortConfig = _pXcvrGPIO->read();
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
