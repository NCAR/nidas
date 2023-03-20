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
#include "nidas/core/HardwareInterface.h"

using namespace nidas::core;
using namespace nidas::util;

using std::cout;
using std::endl;

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
NidasAppArg View("-v,--view", "",
			      "Output the current power settings for all devices and exit",
                  "");
NidasAppArg Power("-p,--power", "<on>",
                  "Controls whether the DSM sends power to the DSM device - \n"
                  "    (0|off|power_off)\n"
                  "    (1|on|power_on)", "power_on");

int usage(const char* argv0)
{
    const char* text = R""""(
Query and control power relays, sensor power, LEDs, and pushbutton switches.

Examples:
  Turn off power to the sensor in port 0: pio 0 off
  Turn on the LED for the wifi button: pio wifi on
  Turn off the auxiliary power port: pio aux off
)"""";
    std::cerr << text << app.usage();
    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Help
    		            | Device | View | Power);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        return usage(argv[0]);
    }

    ArgVector unknowns = app.unparsedArgs();
    if (unknowns.empty()) {
        View.parse(ArgVector{"-v"});
    }

    // implement positional arguments, as per Jira ticket ISFS-410
    if ( !(View.specified() || Device.specified() || Power.specified()) ) {
        if (unknowns.size() >= 2) {
            Device.parse(ArgVector{"-d", unknowns[0]});
            Power.parse(ArgVector{"-p", unknowns[1]});
        }
    }

    return 0;
}


void printAll()
{
    std::cout << "Current Output Settings" << std::endl
              << "-----------------------" << std::endl
              << "Device          Setting"<< std::endl;
    // Print the current state for all power outputs.
    HardwareInterface* hwi = HardwareInterface::getHardwareInterface();
    for (auto device: hwi->devices())
    {
        if (auto oi = device.iOutput())
        {
            cout << device.id() << " " << oi->getState() << endl;
        }
    }
}


int main(int argc, char* argv[]) {

    if (parseRunString(argc, argv))
        exit(1);

    HardwareInterface::selectInterface("ftdi");

    DLOG(("View Option Flag/Value: ") << (View.specified() ? View.getValue() : "no value"));
    if (View.specified()) {
        printAll();
        return 0;
    }

    DLOG(("Device Option Flag/Value: ") << Device.getFlag() << ": " << Device.getValue());
    HardwareDevice device;
    if (Device.specified()) {
        HardwareInterface* hwi = HardwareInterface::getHardwareInterface();
        device = hwi->lookupDevice(Device.getValue());
        if (device.isEmpty())
        {
            std::cerr << "Device '" << Device.getValue() << "' not recognized." << endl;
            usage(argv[0]);
            return 2;
        }
    }
    else 
    {
        std::cerr << "Must provide the device ID option on the command line.\n" << std::endl;
        usage(argv[0]);
        return 3;
    }

    auto ioutput = device.iOutput();
    auto ibutton = device.iButton();
    if (ioutput)
    {
        // print out the existing power state of the device
        std::cout << std::endl << "Current Device Power State"
                << std::endl << "========================"
                << std::endl;
        std::cout << device.id() << " " << ioutput->getState() << endl;

        // just display power state if -p X is not provided
        if (!Power.specified()) {
            return 0;
        }
        OutputInterface::STATE power = OutputInterface::stringToState(Power.getValue());
        if (power != OutputInterface::UNKNOWN)
        {
            ioutput->setState(power);
        }
        else
        {
            std::cerr << "Unknown/Illegal/Missing power state argument: " << Power.getValue() << std::endl;
            usage(argv[0]);
            return 5;
        }

        // print out the new power state
        std::cout << std::endl << "New Power State"
                << std::endl << "===================="
                << std::endl;
        std::cout << device.id() << " " << ioutput->getState() << endl;
    }

    if (false)
    {
        timespec decayStart, decayStop, pressWaitStart, pressWaitNow;
        std::cout << "Waiiting for " << device.id() << " switch to be pressed..." << std::endl;
        int waiting = 0;
        bool timeout = false;
        clock_gettime(CLOCK_MONOTONIC_RAW, &pressWaitStart);
        // Wait for the switch to be pressed...
        do
        {
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
                timeout = true;
            }
        }
        while (!ibutton->isDown() && !timeout);

        if (timeout)
        {
            std::cout << "Did not detect a switch pressed..." << std::endl;
            return 7;
        }
        else {
            std::cout << std::endl << "Detected " << device.id() << " switch pressed..." << std::endl;
        }

        if (ioutput)
            ioutput->on();
        int decaySecs = 0;
        int decayMSecs = 0;
        do {
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
        } while (ibutton->isDown() && decaySecs < 10);

        if (ioutput)
            ioutput->off();

        if (decaySecs >= 10)
        {
            std::cout << "Switch RC decay time > 10 Sec" << std::endl;
        }
        else {
            std::cout << "Switch RC decay time: " 
                      << decaySecs << "." << decayMSecs << " Sec" << std::endl;
        }
    }

    if (!ibutton && !ioutput)
    {
        std::cerr << "pio: failed to instantiate either power or switch control object for: " << device.id();
        return -1;
    }

    // all good, return 0
    return 0;
}
