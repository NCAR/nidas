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

#include "PowerCtrlIf.h"

#include <algorithm>
#include <sstream>

namespace nidas { namespace util {

static const char* STR_POWER_ON = "POWER_ON";
static const char* STR_POWER_OFF = "POWER_OFF";

// This utility converts a string to the POWER_STATE enum
POWER_STATE strToPowerState(const std::string& powerStr)
{
    std::string xformStr(powerStr);
    std::transform(powerStr.begin(), powerStr.end(), xformStr.begin(), ::toupper);
    if (xformStr == std::string(STR_POWER_OFF)
        || xformStr == std::string("OFF")
        || xformStr == std::string("0")) {
        return POWER_OFF;
    }

    if (powerStr == std::string(STR_POWER_ON)
        || xformStr == std::string("ON")
        || xformStr == std::string("1")) {
        return POWER_ON;
    }

    return ILLEGAL_POWER;
}

std::string powerStateToStr(POWER_STATE sensorState)
{
    switch (sensorState) {
        case POWER_OFF:
            return std::string(STR_POWER_OFF);
            break;
        case POWER_ON:
            return std::string(STR_POWER_ON);
            break;
        default:
            std::ostringstream sstrm("Unknown sensor power state: ");
            sstrm << sensorState;
            return sstrm.str();
            break;
    }
}

PowerCtrlIf::~PowerCtrlIf() {}


}} //namespace nidas { namespace util {
