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

    User space code to manage the power state of the sensor serial ports, as
    well as other power controllable entities in the DSM.

    Original Author: Paul Ourada

*/

#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <time.h>
#include <unistd.h>

#include "nidas/core/NidasApp.h"
#include "nidas/util/SensorPowerCtrl.h"
#include "nidas/util/DSMPowerCtrl.h"
#include "nidas/util/DSMSwitchCtrl.h"
#include "nidas/util/util.h"

using namespace nidas::core;
using namespace nidas::util;

typedef std::map<std::string, GPIO_PORT_DEFS> DeviceArgMapType;
typedef std::pair<std::string, GPIO_PORT_DEFS> DeviceArgMapPair;

DeviceArgMapType deviceArgMap;

NidasApp app("pio");

NidasAppArg Device("-d,--device-id", "<blank>|0-7|dcdc|aux|bank1|bank2|default_sw|wifi_sw",
        		 "DSM devices for which power setting is managed - \n"
		         "     0-7         - Sensors 0-7 port power \n"
                 "     dcdc|DCDC   - programmable DC power port\n"
                 "     aux|AUX     - auxiliary power port - typically used to power another DSM\n"
                 "     bank1|BANK1 - bank1 12V power port - powers Serial IO Panel\n"
                 "     bank2|BANK2 - bank2 12V power port - resets the RPi\n"
                 "     def_sw      - detects default switch, SW1 on FTDI USB Serial Board\n"
                 "     wifi_sw  -    detects wifi switch, SW2 on FTDI USB Serial Board\n",
                 "");
NidasAppArg Map("-m,--map", "",
			      "Output the devices for which power can be controlled and exit", "");
NidasAppArg View("-v,--view", "",
			      "Output the current power settings for all devices and exit",
                  "");
NidasAppArg Power("-p,--power", "<on>",
                  "Controls whether the DSM sends power to the DSM device - \n"
                  "    (0|off|power_off)\n"
                  "    (1|on|power_on)", "power_on");

int usage(const char* argv0)
{
    std::cerr
<< argv0 << " is a utility to control the power of various DSM devices." << std::endl
<< "It detects the presence of the FTDI USB Serial Interface board and uses it if " << std::endl
<< "it is available. Otherwise, it attempts to use the Rpi GPIO script to do the same " << std::endl
<< "function. If the utility is not executed on a Rpi device, it will exit with an error." << std::endl
<< std::endl
<< "Usage: " << argv0 << " -d <device ID> -p <power state> -l <log level>" << std::endl
<< "       " << argv0 << " -d <device ID> -l <log level>" << std::endl
<< "       " << argv0 << " -d <device ID> -l <log level>" << std::endl
<< "       " << argv0 << " -m -l <log level>" << std::endl
<< "       " << argv0 << " # same as " << argv0 << " --view" << std::endl << std::endl
<< app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Help
    		            | Device | Map | View | Power);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        return usage(argv[0]);
    }

    if (argc == 1) {
        View.parse(ArgVector{"-v"});
    }

    // implement positional arguments, as per Jira ticket ISFS-410
    if ( !(Map.specified() || View.specified() || Device.specified() || Power.specified()) ) {
        ArgVector unknowns = app.unparsedArgs();
        if (unknowns.size() >= 2) {
            Device.parse(ArgVector{"-d", unknowns[0]});
            Power.parse(ArgVector{"-p", unknowns[1]});
        }
    }

    return 0;
}

void printDevice(GPIO_PORT_DEFS port)
{
    if (port < PWR_DCDC) {
        SensorPowerCtrl(port).print();
    }
    else {
        DSMPowerCtrl(port).print();
    }
}

void printAll()
{
    std::cout << "Current Power Settings" << std::endl
              << "----------------------" << std::endl
              << "Device          Setting"<< std::endl;
    // Print out serial port power settings first
    for (DeviceArgMapType::iterator iter = deviceArgMap.begin();
         iter != deviceArgMap.end();
         iter++) {
        if (iter->second < PWR_DCDC) {
            printDevice(iter->second);
        }
    }

    // Then print out DSM power settings
    for (DeviceArgMapType::iterator iter = deviceArgMap.begin();
         iter != deviceArgMap.end();
         iter++) {
        if (iter->second > SER_PORT7) {
            printDevice(iter->second);
        }
    }
}

int main(int argc, char* argv[]) {
    deviceArgMap.insert(DeviceArgMapPair(std::string("0"), SER_PORT0));
    deviceArgMap.insert(DeviceArgMapPair(std::string("1"), SER_PORT1));
    deviceArgMap.insert(DeviceArgMapPair(std::string("2"), SER_PORT2));
    deviceArgMap.insert(DeviceArgMapPair(std::string("3"), SER_PORT3));
    deviceArgMap.insert(DeviceArgMapPair(std::string("4"), SER_PORT4));
    deviceArgMap.insert(DeviceArgMapPair(std::string("5"), SER_PORT5));
    deviceArgMap.insert(DeviceArgMapPair(std::string("6"), SER_PORT6));
    deviceArgMap.insert(DeviceArgMapPair(std::string("7"), SER_PORT7));
    deviceArgMap.insert(DeviceArgMapPair(std::string("DCDC"), PWR_DCDC));
    deviceArgMap.insert(DeviceArgMapPair(std::string("AUX"), PWR_AUX));
    deviceArgMap.insert(DeviceArgMapPair(std::string("BANK1"), PWR_BANK1));
    deviceArgMap.insert(DeviceArgMapPair(std::string("BANK2"), PWR_BANK2));
    deviceArgMap.insert(DeviceArgMapPair(std::string("DEF_SW"), DEFAULT_SW));
    deviceArgMap.insert(DeviceArgMapPair(std::string("WIFI_SW"), WIFI_SW));

    if (parseRunString(argc, argv))
        exit(1);

    // check the options first
    DLOG(("Map Option Flag Set: ") << (Map.specified() ? "true" : "false"));
    if (Map.specified()) {
        std::cout << Device.usage();
        return 0;
    }

    DLOG(("View Option Flag/Value: ") << (View.specified() ? View.getValue() : "no value"));
    if (View.specified()) {
        printAll();
        return 0;
    }

    DLOG(("Device Option Flag/Value: ") << Device.getFlag() << ": " << Device.getValue());
    GPIO_PORT_DEFS deviceArg = ILLEGAL_PORT;
    if (Device.specified()) {
        std::string deviceArgStr(Device.getValue());
        std::transform(deviceArgStr.begin(), deviceArgStr.end(), deviceArgStr.begin(), ::toupper);
        if (deviceArgMap.find(deviceArgStr) != deviceArgMap.end()) {
            deviceArg = deviceArgMap[deviceArgStr];
            DLOG(("deviceArg == %d", deviceArg));
            DLOG(("Found %s in deviceArgMap...", gpio2Str(deviceArg).c_str()));
            if (!(RANGE_CHECK_INC(SER_PORT0, deviceArg, WIFI_SW))) {
                std::cerr << "Something went wrong, as the device arg wasn't recognized" << std::endl;
                usage(argv[0]);
                return 2;
            }
        }
        else {
            std::cerr << deviceArgStr << " is not a valid device type!" << std::endl;
            usage(argv[0]);
            return 6;
        }
    }

    else 
    {
        std::cerr << "Must provide the device ID option on the command line.\n" << std::endl;
        usage(argv[0]);
        return 3;
    }

    PowerCtrlIf* pPwrCtrl = 0;
    DSMSwitchCtrl* pSwCtrl = 0;
    if (deviceArg < PWR_DCDC) {
        DLOG(("Instantiating SensorPowerCtrl object..."));
        pPwrCtrl = new SensorPowerCtrl(deviceArg);
    }
    else if (deviceArg < DEFAULT_SW) {
        DLOG(("Instantiating DSMPowerCtrl object..."));
        pPwrCtrl = new DSMPowerCtrl(deviceArg);
    }

    else {
        DLOG(("Instantiating DSMSwitchCtrl object..."));
        pSwCtrl = new DSMSwitchCtrl(deviceArg);
    }

    if (pPwrCtrl) {
        PowerCtrlIf& rPwrCtrl = *pPwrCtrl;

        // print out the existing power state of the device
        std::cout << std::endl << "Current Device Power State"
                << std::endl << "========================"
                << std::endl;
        rPwrCtrl.print();

        // just display power state if -p X is not provided
        if (!Power.specified()) {
            return 0;
        }

        else {
            std::string pwrStr(Power.getValue());
            DLOG(("Power State Option Flag/Value: ") << Power.getFlag() << ": " << pwrStr);
            std::transform(pwrStr.begin(), pwrStr.end(), pwrStr.begin(), ::toupper);
            POWER_STATE power = strToPowerState(pwrStr);
            DLOG(("Transformed Power State Value: ") << powerStateToStr(power));
            if (power != ILLEGAL_POWER) {
                rPwrCtrl.enablePwrCtrl(true);
                power == POWER_ON ? rPwrCtrl.pwrOn() : rPwrCtrl.pwrOff();
            }
            else
            {
                std::cerr << "Unknown/Illegal/Missing power state argument: " << Power.getValue() << std::endl;
                usage(argv[0]);
                return 5;
            }
        }

        // print out the new power state
        std::cout << std::endl << "New Power State"
                << std::endl << "===================="
                << std::endl;
        rPwrCtrl.print();
    }
    else if (pSwCtrl) {
        timespec decayStart, decayStop, pressWaitStart, pressWaitNow;
        std::cout << "Waiiting for " << gpio2Str(deviceArg) << " switch to be pressed..." << std::endl;
        int waiting = 0;
        clock_gettime(CLOCK_MONOTONIC_RAW, &pressWaitStart);
        // Wait for the switch to be pressed...
        bool switchPressed = pSwCtrl->switchIsPressed();
        while (!switchPressed) {
            switchPressed = pSwCtrl->switchIsPressed();
            // continously get start of decay until switch is pressed
            // for better accuracy.
            clock_gettime(CLOCK_MONOTONIC_RAW, &decayStart);

            clock_gettime(CLOCK_MONOTONIC_RAW, &pressWaitNow);
            // we only need approx time for waiting...
            int currently_waiting = pressWaitNow.tv_sec - pressWaitStart.tv_sec;
            if ( currently_waiting > waiting) {
                waiting = currently_waiting;
                std::cout << "\rWaiting " << waiting << " seconds for switch press..." << std::flush;
            }

            if (waiting >= 60) {
                std::cout << std::endl;
                break;
            }
        }

        if (!switchPressed) {
            std::cout << "Did not detect a switch pressed..." << std::endl;
            return 7;
        }
        else {
            std::cout << std::endl << "Detected " << gpio2Str(deviceArg) << " switch pressed..." << std::endl;
        }

        pSwCtrl->ledOn();
        int decaySecs = 0;
        int decayMSecs = 0;
        bool switchIsPressed = false;
        do {
            switchIsPressed = pSwCtrl->switchIsPressed();
            timespec requestedSleep = {0, NSECS_PER_MSEC*10};
            nanosleep(&requestedSleep, 0);

            clock_gettime(CLOCK_MONOTONIC_RAW, &decayStop);
            decaySecs = decayStop.tv_sec - decayStart.tv_sec;
            int nsec = decayStop.tv_nsec - decayStart.tv_nsec;
            if (nsec < 0) {
                decaySecs--;
                nsec += NSECS_PER_SEC;
            }
            decayMSecs = nsec/NSECS_PER_MSEC;
        } while (switchIsPressed && decaySecs < 10);
        pSwCtrl->ledOff();

        if (pSwCtrl->switchIsPressed()) {
            std::cout << "Switch RC decay time > 10 Sec" << std::endl;
        }
        else {
            std::cout << "Switch RC decay time: " 
                      << decaySecs << "." << decayMSecs << " Sec" << std::endl;
        }
    }
    else {
        std::cerr << "pio: failed to instantiate either power or switch control object for: " << gpio2Str(deviceArg);
        return -1;
    }

    // all good, return 0
    return 0;
}
