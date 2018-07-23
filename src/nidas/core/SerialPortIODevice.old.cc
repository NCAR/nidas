// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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

#include "SerialPortIODevice.h"
#include "Looper.h"
#include "Prompt.h"
#include "SerialXcvrCtrl.h"

#include <nidas/util/Logger.h>
#include <nidas/util/time_constants.h>
#include <nidas/util/Exception.h>

#include <cmath>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

SerialPortIODevice::SerialPortIODevice(const std::string& name, const PORT_TYPES portType, 
                                       const TERM term):
    UnixIODevice(name),_termios(),_rts485(0),_usecsperbyte(0),
    _portType(portType),_term(term),_pSerialControl(0)
{
    _termios.setRaw(true);
    _termios.setRawLength(1);
    _termios.setRawTimeout(0);

    // Determine if this needs SP339 port type control
    std::string ttyBase = "/dev/ttyUSB";
    std::size_t foundAt = name.find(ttyBase);
    if (foundAt != std::string::npos) {
        NLOG(("SerialPortIODevice: Device needs SerialXcvrCtrl object: ") << name);
        const char* nameStr = name.c_str();
        const char* portChar = &nameStr[ttyBase.length()];
        unsigned int portID = UINT32_MAX;
        istringstream portStream(portChar);
        //portStream << portChar;

        try {
            portStream >> portID;
        }
        catch (exception e) {
            throw n_u::Exception("SerialPortIODevice: device name arg "
                                "cannot be parsed for canonical port ID");
        }

        NLOG(("SerialPortIODevice: Instantiating SerialXcvrCtrl object on PORT") << portID 
            << "; Port type: " << _portType);
        _pSerialControl = new SerialXcvrCtrl(static_cast<PORT_DEFS>(portID), 
                                                        _portType, _term);
        if (_pSerialControl == 0)
        {
            throw n_u::Exception("SerialPortIODevice: Cannot construct "
                                    "SerialXcvrCtrl object");
        }
    }
}

void SerialPortIODevice::open(int flags) throw(n_u::IOException)
{
    UnixIODevice::open(flags);
    applyPortType();
    applyTermios();

    // If the remote device is 485, clear RTS, which on many serial interfaces
    // shuts down the transmitter. This is usually necessary to be able to read
    // data from the remote device.  See the discussion about setRTS485() in the
    // header.
    if (_rts485) {
        int bits = TIOCM_RTS;
        if (_rts485 > 0) {
            // clear RTS
            if (::ioctl(_fd, TIOCMBIC, &bits) < 0)
                throw n_u::IOException(getName(),"ioctl TIOCMBIC",errno);
        }
        else {
            // set RTS
            if (::ioctl(_fd, TIOCMBIS, &bits) < 0)
                throw n_u::IOException(getName(),"ioctl TIOCMBIS",errno);
        }
        _usecsperbyte = getUsecsPerByte();
    }
}

int SerialPortIODevice::getUsecsPerByte() const
{
    int usecs = 0;
    if (::isatty(_fd)) {
        int bits = _termios.getDataBits() + _termios.getStopBits() + 1;
        switch(_termios.getParity()) {
        case n_u::Termios::ODD:
        case n_u::Termios::EVEN:
            bits++;
            break;
        case n_u::Termios::NONE:
            break;
        }
        usecs = (bits * USECS_PER_SEC + _termios.getBaudRate() / 2) / _termios.getBaudRate();
    }
    return usecs;
}

void SerialPortIODevice::applyPortType() throw(nidas::util::IOException)
{
    if (_portType == RS232 || _portType == RS422 || _portType == RS485_FULL || _portType == RS485_HALF) {
        /**
         * TODO - is this an IOCTL operation?
         */
    }
    else {
        throw nidas::util::IOException(getName(),"_portType illegal value",_portType);
    }
}

void SerialPortIODevice::setRTS485(int val) throw(nidas::util::IOException)
{
    _rts485 = val;
    if (_rts485 && _fd >= 0) {
        int bits = TIOCM_RTS;
        if (_rts485 > 0) {
            // clear RTS
            if (::ioctl(_fd, TIOCMBIC, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIC",errno);
        }
        else {
            // set RTS
            if (::ioctl(_fd, TIOCMBIS, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIS",errno);
        }
        _usecsperbyte = getUsecsPerByte();
    }
}

/**
 * Write to the device.
 */
size_t SerialPortIODevice::write(const void *buf, size_t len) throw(nidas::util::IOException)
{
    ssize_t result;
    int bits;


    if (_rts485) {
        // see the above discussion about RTS and 485. Here we
        // try an in-exact set/clear of RTS on either side of a write.
        bits = TIOCM_RTS;
        if (_rts485 > 0) {
            // set RTS before write
            if (_fd >= 0 && ::ioctl(_fd, TIOCMBIS, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIS",errno);
        }
        else {
            // clear RTS before write
            if (_fd >= 0 && ::ioctl(_fd, TIOCMBIC, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIS",errno);
        }
    }

    if ((result = ::write(_fd,buf,len)) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) result = 0;
        else throw nidas::util::IOException(getName(),"write",errno);
    }

    if (_rts485) {
        // Sleep until we think the last bit has been transmitted.
        // Add a fudge-factor of one quarter of a character.
        ::usleep(len * _usecsperbyte + _usecsperbyte/4);
        if (_rts485 > 0) {
            // then clear RTS
            if (_fd >= 0 && ::ioctl(_fd, TIOCMBIC, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIC",errno);
        }
        else {
            // then set RTS
            if (_fd >= 0 && ::ioctl(_fd, TIOCMBIS, &bits) < 0)
                throw nidas::util::IOException(getName(),"ioctl TIOCMBIS",errno);
        }
    }
    return result;
}

