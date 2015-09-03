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

#include <nidas/dynld/UDPSocketSensor.h>
#include <nidas/core/UDPSocketIODevice.h>

#include <nidas/util/Logger.h>

#include <sstream>
#include <iomanip>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(UDPSocketSensor)

UDPSocketSensor::UDPSocketSensor()
{
}

IODevice* UDPSocketSensor::buildIODevice() throw(n_u::IOException)
{
    UDPSocketIODevice* dev = new UDPSocketIODevice();
    return dev;
}

SampleScanner* UDPSocketSensor::buildSampleScanner()
    throw(n_u::InvalidParameterException)
{
    DatagramSampleScanner* scanner = new DatagramSampleScanner();
    scanner->setNullTerminate(doesAsciiSscanfs());
    return scanner;
}
