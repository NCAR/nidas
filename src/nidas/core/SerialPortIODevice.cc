// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/

#include <nidas/core/SerialPortIODevice.h>
#include <nidas/core/Looper.h>
#include <nidas/core/Prompt.h>

#include <nidas/util/Logger.h>

#include <cmath>

#include <iostream>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

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

