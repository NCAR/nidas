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

#include "FtdiHW.h"


namespace nidas { namespace util {

static const char* i2cStr= "FTDI_I2C";
static const char* gpioStr= "FTDI_GPIO";
static const char* p0p3Str= "FTDI_P0_P3";
static const char* p4p7Str= "FTDI_P4_P7";
static const char* illegalStr= "FTDI_ILLEGAL_DEVICE";

const char* device2Str(FTDI_DEVICES device)
{
    switch (device) {
        case FTDI_I2C:
            return i2cStr;
            break;
        case FTDI_GPIO:
            return gpioStr;
            break;
        case FTDI_P0_P3:
            return p0p3Str;
            break;
        case FTDI_P4_P7:
            return p4p7Str;
            break;
        case FTDI_ILLEGAL_DEVICE:
        default:
            break;
    }

    return illegalStr;
}

static const char* ifaceAStr = "IFACE_A";
static const char* ifaceBStr = "IFACE_B";
static const char* ifaceCStr = "IFACE_C";
static const char* ifaceDStr = "IFACE_D";
static const char* ifaceAnyStr = "IFACE_ANY";
static const char* ifaceIllegalStr = "IFACE_ILLEGAL";

const char* iface2Str(ftdi_interface iface)
{
    switch (iface) {
        case INTERFACE_A:
            return ifaceAStr;
            break;
        case INTERFACE_B:
            return ifaceBStr;
            break;
        case INTERFACE_C:
            return ifaceCStr;
            break;
        case INTERFACE_D:
            return ifaceDStr;
            break;
        case INTERFACE_ANY:
            return ifaceAnyStr;
            break;
        default:
            break;
    }

    return ifaceIllegalStr;
}


}} // namespace nidas { namespace util {
