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

// global bool used to determine whether to use the old /proc filesystem control, or the new FTDI control.
bool useFTDICtrl = true;

typedef std::map<std::string, int> DeviceArgMapType;
typedef std::pair<std::string, int> DeviceArgMapPair;

DeviceArgMapType deviceArgMap;

static const char* ARG_AUX_DEVICE = "AUX";

NidasApp app("pio");

NidasAppArg Device("-d,--device-id", "<0>",
        		 "DSM devices which power setting are managed - \n"
		         "     0-7   - Sensor ports 0-7\n"
                 "     28V   - 28 volt power port\n"
                 "     aux   - auxiliary power port - typically used to power another DSM\n"
                 "     bank1 - bank1 12V power port - powers Serial IO Panel\n"
                 "     bank2 - bank2 12V power port\n",
                 "");
NidasAppArg Map("-m,--map", "",
			      "Output the devices which power can be controlled and exit", "");
NidasAppArg View("-v,--view", "<blank>",
			      "Output the current power settings for one or all devices and exit"
                  "    all - all devices"
                  "    <blank> - device specified by -d",
                  "");
NidasAppArg Power("-p,--power", "<on>",
                  "Controls whether the DSM sends power to the DSM device - \n"
                  "    (0|off|power_off)\n"
                  "    (1|on|power_on)", "power_on");

int usage(const char* argv0)
{
    std::cerr
<< argv0 << "is a utility to control the power of various DSM devices." << std::endl
<< "It detects the presence of the FTDI USB Serial Interface board and used it if " << std::endl
<< "it is available. Otherwise, it attempts to use the Rpi GPIO script to do the same " << std::endl
<< "function. If the utility is not executed on a Rpi device, it will exit with an error." << std::endl
<< std::endl
<< "Usage: " << argv0 << "-d <device ID> -p <power state> -l <log level>" << std::endl
<< "       " << argv0 << "-d <device ID> -l <log level>" << std::endl
<< "       " << argv0 << "-d <device ID> -l <log level>" << std::endl
<< "       " << argv0 << "-m -l <log level>" << std::endl << std::endl
<< app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Version | app.Help
    		            | Device | Map | View | Power);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        return usage(argv[0]);
    }
    return 0;
}

/*************************************************
 *
 * TODO: Move all rpi2_gpio into the NIDAS util lib
 *
 *************************************************/
int executeRpiGPIO(int argc, char* argv[])
{
   return exec("/usr/bin/rpi2_gpio", argc, argv);
}

void printDevice(std::string device, int port)
{
    if (device.length() == 1) {
        // see if it's a numeral indicating a sensor
        std::ostringstream ostrm(device);
        int sensorID;
    }
    else {
        // check to see if it's one of the other DSM power interfaces
    }
}

void printAll()
{
    std::cout << "Current Power Settings" << std::endl
              << "----------------------" << std::endl
              << "Device          Setting"<< std::endl;
    for (DeviceArgMapType::iterator iter = deviceArgMap.begin;
         iter != deviceArgMap.end();
         iter++) {
        printDevice(iter->first, iter->second);
    }
}

int main(int argc, char* argv[]) {
    deviceArgMap.insert(DeviceArgMapPair(std::string("0"), PORT0));
    deviceArgMap.insert(DeviceArgMapPair(std::string("1"), PORT1));
    deviceArgMap.insert(DeviceArgMapPair(std::string("2"), PORT2));
    deviceArgMap.insert(DeviceArgMapPair(std::string("3"), PORT3));
    deviceArgMap.insert(DeviceArgMapPair(std::string("4"), PORT4));
    deviceArgMap.insert(DeviceArgMapPair(std::string("5"), PORT5));
    deviceArgMap.insert(DeviceArgMapPair(std::string("6"), PORT6));
    deviceArgMap.insert(DeviceArgMapPair(std::string("7"), PORT7));
    deviceArgMap.insert(DeviceArgMapPair(std::string("28V"), PWR_DEVICE_28V));
    deviceArgMap.insert(DeviceArgMapPair(std::string("AUX"), PWR_DEVICE_AUX));
    deviceArgMap.insert(DeviceArgMapPair(std::string("BANK1"), PWR_DEVICE_BANK1));
    deviceArgMap.insert(DeviceArgMapPair(std::string("BANK2"), PWR_DEVICE_BANK2));

    if (parseRunString(argc, argv))
        exit(1);

    // check the options first
    DLOG(("Map Option Flag Set: ") << Map.specified() ? "true" : "false");
    if (Map.specified()) {
        Device.usage();
        return 0;
    }

    DLOG(("View Option Flag/Value: ") << (View.specified() ? View.getValue() : "no value"));
    if (View.specified()) {
        std::string value(View.getValue());
        std::transform(value.begin(), value.end(), value.begin(), ::toupper);

        if (value == "ALL") {
            printAll();
            return 0;
        }
    }

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

    if (View.specified()) {
        return 0;
    }

    if (Power.specified()) {
        std::string pwrStr(Power.getValue());
        DLOG(("Power State Option Flag/Value: ") << Power.getFlag() << ": " << pwrStr);
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
