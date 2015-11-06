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

#include <nidas/core/SerialPortIODevice.h>
#include <nidas/core/Looper.h>
#include <nidas/core/Prompt.h>

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
        // clear RTS
        if (::ioctl(_fd, TIOCMBIC, &bits) < 0)
            throw n_u::IOException(getName(),"ioctl TIOCMBIC",errno);
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

