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
#ifndef NIDAS_UTIL_GPIOIF_H
#define NIDAS_UTIL_GPIOIF_H

#include <string>
#include <ftdi.h>

#include "Logger.h"

#include "ThreadSupport.h"
#include "IOException.h"
#include "Logger.h"
#include "InvalidParameterException.h"

namespace nidas { namespace util {

/*
 * This enum specifies all the ports in the DSM which are controlled in some way by GPIO.
 */
enum GPIO_PORT_DEFS {ILLEGAL_PORT=-1,
                     SER_PORT0=0, SER_PORT1, SER_PORT2, SER_PORT3,
                     SER_PORT4, SER_PORT5, SER_PORT6, SER_PORT7,
                     PWR_DCDC, PWR_AUX, PWR_BANK1, PWR_BANK2, PWR_BTCON,
                     DEFAULT_SW, WIFI_SW};
// At present there are only 7 available ports on a DSM
const GPIO_PORT_DEFS MAX_SER_PORT = SER_PORT7;

static const char* GPIO_STR_ILLEGAL =   "GPIO_ILLEGAL";
static const char* GPIO_STR_SER_PORT0 = "GPIO_SER_PORT0";
static const char* GPIO_STR_SER_PORT1 = "GPIO_SER_PORT1";
static const char* GPIO_STR_SER_PORT2 = "GPIO_SER_PORT2";
static const char* GPIO_STR_SER_PORT3 = "GPIO_SER_PORT3";
static const char* GPIO_STR_SER_PORT4 = "GPIO_SER_PORT4";
static const char* GPIO_STR_SER_PORT5 = "GPIO_SER_PORT5";
static const char* GPIO_STR_SER_PORT6 = "GPIO_SER_PORT6";
static const char* GPIO_STR_SER_PORT7 = "GPIO_SER_PORT7";
static const char* GPIO_STR_DCDC =      "GPIO_PWR_DCDC";
static const char* GPIO_STR_AUX =       "GPIO_PWR_AUX";
static const char* GPIO_STR_BANK1 =     "GPIO_PWR_BANK1";
static const char* GPIO_STR_BANK2 =     "GPIO_PWR_BANK2";
static const char* GPIO_STR_BTCON =     "GPIO_PWR_BTCON";
static const char* GPIO_STR_DEFAULT_SW1 = "GPIO_DEFAULT_SW1";
static const char* GPIO_STR_WIFI_SW2 =    "GPIO_WIFI_SW2";

inline std::string gpio2Str(GPIO_PORT_DEFS gpio) {
    std::string retval;

    switch (gpio) {
    case SER_PORT0:
        retval.append(GPIO_STR_SER_PORT0);
        break;
    case SER_PORT1:
        retval.append(GPIO_STR_SER_PORT1);
        break;
    case SER_PORT2:
        retval.append(GPIO_STR_SER_PORT2);
        break;
    case SER_PORT3:
        retval.append(GPIO_STR_SER_PORT3);
        break;
    case SER_PORT4:
        retval.append(GPIO_STR_SER_PORT4);
        break;
    case SER_PORT5:
        retval.append(GPIO_STR_SER_PORT5);
        break;
    case SER_PORT6:
        retval.append(GPIO_STR_SER_PORT6);
        break;
    case SER_PORT7:
        retval.append(GPIO_STR_SER_PORT7);
        break;
    case PWR_DCDC:
        retval.append(GPIO_STR_DCDC);
        break;
    case PWR_AUX:
        retval.append(GPIO_STR_AUX);
        break;
    case PWR_BANK1:
        retval.append(GPIO_STR_BANK1);
        break;
    case PWR_BANK2:
        retval.append(GPIO_STR_BANK2);
        break;
    case PWR_BTCON:
        retval.append(GPIO_STR_BTCON);
        break;
    case DEFAULT_SW:
        retval.append(GPIO_STR_DEFAULT_SW1);
        break;
    case WIFI_SW:
        retval.append(GPIO_STR_WIFI_SW2);
        break;
    case ILLEGAL_PORT:
    default:
        retval.append(GPIO_STR_ILLEGAL);
        DLOG(("pwrDevice2Bits(): Unknown power GPIO ID") << gpio);
        break;
    }

    return retval;
}



/*
 *  FtdiDevice interface class
 */
class GpioIF
{
public:
    virtual ~GpioIF() {}

    /*
     *  Method return whether the interface was found on the platform.
     */
    virtual bool ifaceFound() = 0;

    /*
     *  Method reads the interface and returns the value of the port pin(s) last written.
     */
    virtual unsigned char read() = 0;

    /*
     *  Method writes the pin(s) to the pre-selected interface.
     */
    virtual void write(unsigned char pins) = 0;
};


}} //namespace nidas { namespace util {

#endif //NIDAS_UTIL_GPIOIF_H
