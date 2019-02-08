/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
// #define DEBUGFILT
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
/* pio.cc

    User space code that uses libusb and libftdi to bit bang the two FT4243 devices in
    the DSM which are reserved for GPIO. It uses these devices to programmatically manage
    the power state of the sensor serial ports, and other power controllable entities in
    the DSM.

    Original Author: Paul Ourada

*/

#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <libusb-1.0/libusb.h>
#include <libftdi1/ftdi.h>
#include "nidas/core/NidasApp.h"
#include "nidas/util/SensorPowerCtrl.h"
#include "nidas/util/DSMPowerCtrl.h"

using namespace nidas::core;
using namespace nidas::util;

static const char* ARG_AUX_DEVICE = "AUX";

NidasApp app("pio");

NidasAppArg Device("-d,--device-id", "<0>",
        		 "Specifies the DSM device whose power setting will change - \n"
		         "     0-7 - Sensor ports 0-7\n"
				 "     aux - auxiliary power port\n", "");
NidasAppArg Print("-p,--print", "",
			      "Output the current power settings for all devices and exit", "");
NidasAppArg State("-s,--state", "<POWER_ON>",
                  "Controls whether the DSM sends power to the DSM device - \n"
                  "    POWER_OFF\n"
                  "    POWER_ON", "POWER_ON");

int usage(const char* argv0)
{
    std::cerr << "\
Usage: " << argv0 << "-d <device ID> -s <power state> -l <log level>]" << std::endl << "       "
		 << argv0 << "-p <port ID> -d" << std::endl << std::endl
         << app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Version | app.Help
    		            | Device | Print | State);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        return usage(argv[0]);
    }
    return 0;
}

// global bool used to determine whether to use the old /proc filesystem control, or the new FTDI control.
bool useFTDICtrl = true;

int main(int argc, char* argv[]) {

    if (parseRunString(argc, argv))
        exit(1);

    // check the options first to set up the port and port control
    DLOG(("Device Option Flag/Value: ") << Device.getFlag() << ": " << Device.getValue());
    if (Device.specified()) {
        if (!(1)) {
            std::cerr << "Something went wrong, as the device arg wasn't recognized" << std::endl;
            return 2;
        }
    }

    else 
    {
        std::cerr << "Must device ID option on the command line.\n" << std::endl;
        usage(argv[0]);
        return 3;
    }

    // print out the existing power state of the device
    std::cout << std::endl << "Current Device Power State" << std::endl << "========================" << std::endl;
    SensorPowerCtrl sensrPwrCtrl((n_u::PORT_DEFS)Port.asInt());
    sensrPwrCtrl.print();

    if (Print.specified()) {
        return 0;
    }

    if (State.specified()) {
        std::string pwrStr(State.getValue());
        DLOG(("Power State Option Flag/Value: ") << State.getFlag() << ": " << pwrStr);
        std::transform(pwrStr.begin(), pwrStr.end(), pwrStr.begin(), ::toupper);
        POWER_STATE power = PowerCtrlAbs::strToPowerState(pwrStr);
        if (power != ILLEGAL_POWER) {
            sensrPwrCtrl.enablePwrCtrl(true);
            power == POWER_ON ? sensrPwrCtrl.pwrOn() : sensrPwrCtrl.pwrOff();
        }
        else
        {
            std::cerr << "Unknown/Illegal/Missing power state argument: " << State.getValue() << std::endl;
            usage(argv[0]);
            return 5;
        }
    }

    // print out the new power state
    std::cout << std::endl << "New Power State" << std::endl << "====================" << std::endl;
    sensrPwrCtrl.print();

    // all good, return 0
    return 0;
}
