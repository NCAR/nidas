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

#include <nidas/util/Logger.h>
#include <nidas/util/time_constants.h>

#include <cmath>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

void SerialPortIODevice::open(int flags) throw(n_u::IOException)
{
    UnixIODevice::open(flags);
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

