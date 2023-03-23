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
#include <iomanip>
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

NidasAppArg List("--list", "",
                 "List all known devices with descriptions.", "");

std::string Device;
std::string Operation;

void
usage()
{
    std::cerr
        << R""""(
Usage: pio [device [op]]]

  device:

    Device id, use --list to list known devices.

  op: {on|off|switch}

    Turn the output on or off, or wait for a switch to be pressed.
    If not specified, show the current status of just that output.

Query and control power relays, sensor power, LEDs, and pushbutton switches.
If no arguments, show the status of all output devices.
)""""
        << app.usage()
        << R""""()

Examples:

  pio              - Show status of all output devices.
  pio --list       - List devices with descriptions.
  pio 0 off        - Turn off power to the sensor in port 0.
  pio wifi on      - Turn on the LED for the wifi button.
  pio aux off      - Turn off the auxiliary power port.
)"""";
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Help | List);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        usage();
        return 1;
    }

    // Get positional args
    ArgVector pargs = app.unparsedArgs();
    if (pargs.size() > 2)
    {
        std::cerr << "Too many arguments.  Use -h for help." << endl;
        return 1;
    }
    if (pargs.size() > 0)
        Device = pargs[0];
    if (pargs.size() > 1)
    {
        Operation = pargs[1];
        if (Operation != "on" && Operation != "off" && Operation != "switch")
        {
            std::cerr << "unknown operation: " << Operation << endl;
            return 1;
        }
    }
    return 0;
}


void
print_status(HardwareDevice& device)
{
    cout << std::setw(10) << device.id() << "  ";
    if (auto oi = device.iOutput())
        cout << oi->getState();
    else
        cout << "could-not-open";
    cout << endl;
}


void printAll()
{
    // Print the current state for all power outputs.
    auto hwi = HardwareInterface::getHardwareInterface();
    for (auto& device: hwi->devices())
    {
        print_status(device);
    }
}


int main(int argc, char* argv[]) {

    if (parseRunString(argc, argv))
        exit(1);

    HardwareInterface::selectInterface("ftdi");
    auto hwi = HardwareInterface::getHardwareInterface();

    if (List.asBool())
    {
        // Dump a list of devies with descriptions.
        for (auto& device: hwi->devices())
        {
            cout << std::setw(10) << device.id() << ":  "
                 << device.description() << endl;
        }
        return 0;
    }

    if (Device.empty())
    {
        printAll();
        return 0;
    }

    HardwareDevice device;
    device = hwi->lookupDevice(Device);
    if (device.isEmpty())
    {
        std::cerr << "unrecognized device: " << Device << endl;
        return 2;
    }

    auto ioutput = device.iOutput();
    auto ibutton = device.iButton();
    if (ioutput)
    {
        if (Operation == "on")
            ioutput->on();
        else if (Operation == "off")
            ioutput->off();
        // print current status, whether just changed or not.
        print_status(device);

        if (Operation.empty())
            return 0;
    }

    if (Operation == "switch" && !ibutton)
    {
        std::cerr << "device " << device.id() << " is not a switch." << endl;
        return 1;
    }

    if (Operation == "switch" && ibutton)
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
        }
        while (ibutton->isDown() && decaySecs < 10);

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
        std::cerr << "pio: failed to instantiate either power or switch "
                  << "control object for: " << device.id() << endl;
        return 9;
    }

    // all good, return 0
    return 0;
}
