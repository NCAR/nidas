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

    Refactored later to use the HardwareInterface API and incorporate the
    functionality of the dsm_port_config utility, so pio now can set the
    transceiver mode and termination for DSM sensor ports. dsm_port_config, in
    addition to controlling transceiver settings, would change and report the
    RTS line status, so pio can do that also.  However, I'm not sure how
    useful this is, because it seems like RTS is always set when the terminal
    device is opened, even if cleared in a previous run.

*/

#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <nidas/core/NidasApp.h>
#include <nidas/core/HardwareInterface.h>
#include <nidas/util/Termios.h>


using namespace nidas::core;
using namespace nidas::util;

using std::cerr;
using std::cout;
using std::endl;
using std::setw;
using std::left;

NidasApp app("pio");

// set for the list, status, or switch operations
std::string Operation;

// if set, act on one device
std::string Device;
// if set, perform the output setting on the named device.
std::string Output;
// if set, apply the port configs to the named device.
std::string Serial;


PortType PTYPE(PortType::RS232);
PortTermination PTERM(PortTermination::NO_TERM);
std::string RTS;


void
usage()
{
    std::cerr
        << R""""(
Usage: pio [list|status] [device [op ...]]

Query and control power relays, sensor power, LEDs, serial ports, and
pushbutton switches.  If no arguments, show the status of all devices.

  {list|status}:

    List the known devices, without opening the hardware, or else
    open all the hardware devices and show their status.
    If no other devices or operations specified, the default is 'status'.

  device:

    Device name.  Use 'list' to show the known device names.

  op: {on|off|switch}

    Turn the output on or off, wait for a switch to be pressed, or set the
    serial port mode.
    If not specified, show the current status of just that output.

  op: [{232|422|485h|485f|loop}] [{term|noterm}] [{rts0|rts1}]

    Configure a serial port with (term) or without (noterm) 120 Ohm
    termination, and set the signal protocol: RS232, RS422, RS485 half duplex,
    RS485 full duplex, loopback.  Set (rts1) or clear (rts0) the RTS bit.
    Protocol and termination are always set together, so if only
    one is specified, the other defaults to noterm or rs232.
)""""
        << app.usage()
        << R""""()

Examples:

  pio              - Show status of all devices.
  pio list         - List devices with descriptions.
  pio 0 off        - Turn off power to the sensor in port 0.
  pio wifi on      - Turn on the LED for the wifi button.
  pio aux off      - Turn off the auxiliary power port.
  pio 0 232 on     - Put port 0 in RS232 mode and turn on sensor power.
  pio 7 485h       - Put port 7 in RS485 half-duplex mode, no termination.
  pio 7            - Show the status of just port7.

)"""";
}


int toomany(const std::string& msg)
{
    cerr << msg << ": too many arguments.  Use -h for help." << endl;
    return 1;
}


int parseRunString(int argc, char* argv[])
{

    app.enableArguments(app.loggingArgs() | app.Help);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        usage();
        return 1;
    }

    // Get positional args
    ArgVector pargs = app.unparsedArgs();
    Operation = "status";

    for (auto& arg: pargs)
    {
        if (arg == "list" || arg == "status")
        {
            Operation = arg;
            if (pargs.size() > 1)
            {
                return toomany(Operation);
            }
        }
        if(arg=="wifi_test")
        {
            Operation=arg;
            Device="wifi";
            continue;
        }
        if (Device.empty())
        {
            Device = arg;
            continue;
        }
        Operation = arg;
        if (Operation == "on" || Operation == "off")
        {
            Output = Operation;
            continue;
        }
        if (Operation == "switch")
        {
            // it doesn't really make sense to set anything else when testing
            // a switch.
            if (pargs.size() > 2)
            {
                return toomany(Operation);
            }
            continue;
        }
        
        if (arg == "rts0" || arg == "rts1")
        {
            RTS = arg;
            continue;
        }
        if (PTYPE.parse(arg) || PTERM.parse(arg))
        {
            Serial = arg;
            continue;
        }
        std::cerr << "operation unknown: " << arg << endl;
        return 1;
    }
    return 0;
}


int unsupported(const HardwareDevice& device, const std::string& op)
{
    cerr << "unsupported operation on device "
         << device << ": " << op << endl;
    return 1;
}


bool
set_rts(int fd, bool rts)
{
    if (fd < 0)
        return false;
    struct termios attr;
    tcgetattr(fd, &attr);
    attr.c_cflag |= CRTSCTS | CLOCAL;
    if (tcflush(fd, TCIOFLUSH) == -1) {
        perror("RTS: tcflush()");
        return false;
    }
    if (tcsetattr(fd, TCSANOW, &attr) == -1) {
        perror("RTS: tcsetattr()");
        return false;
    }

    int cmd = TIOCMBIC;
    int bit = TIOCM_RTS;

    if (rts) {
        cmd = TIOCMBIS;
    }

    if (ioctl(fd, cmd, &bit) == -1) {
        std::string errStr("set_rts(): ");
        if (cmd == TIOCMBIC) {
            errStr.append("TIOCMBIC");
        }
        else {
            errStr.append("TIOCMBIS");
        }
        perror(errStr.c_str());
        return false;
    }
    return true;
}


int openDevice(const std::string& path)
{
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0)
    {
        perror(path.c_str());
    }
    return fd;
}


/**
 * Open @p device and return the current rts setting in @p rts.  Return false
 * if there is no path for this device or there was an error trying to
 * retrieve rts.
 */
bool
read_rts(int fd, bool& rts)
{
    bool result = false;
    int status = 0;
    if (fd < 0)
    {
        return false;
    }
    if (ioctl(fd, TIOCMGET, &status) == -1)
    {
        perror("set_rts(): TIOCMGET");
    }
    else
    {
        rts = ((status & TIOCM_RTS) != 0);
        result = true;
    }
    return result;
}


void
print_status(HardwareDevice& device, int fd=-1)
{
    cout << left << setw(8) << device;
    if (auto oi = device.iOutput())
    {
        cout << left << setw(5) << oi->getState();
        if (auto iserial = device.iSerial())
        {
            PortType ptype;
            PortTermination term;
            iserial->getConfig(ptype, term);
            cout << left << setw(5) << ptype.toString(ptf_485);
            cout << left << setw(8) << term;
            bool rts;
            if (read_rts(fd, rts))
            {
                cout << left << setw(5) << (rts ? "rts1" : "rts0");
            }
        }
        if (auto ibutton = device.iButton())
        {
            cout << (ibutton->isDown() ? "down" : "up");
        }
    }
    else
        cout << "could-not-open";
    cout << endl;
}


void print_all()
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

    auto hwi = HardwareInterface::getHardwareInterface();
  
    if (Operation == "list")
    {
        // Dump a list of devies with descriptions.
        for (auto& device: hwi->devices())
        {
            cout << std::setw(10) << device << ":  "
                 << device.description() << endl;
        }
        return 0;
    }

    if (Operation == "status" && Device.empty())
    {
        print_all();
        return 0;
    }

    HardwareDevice device;
    device = hwi->lookupDevice(Device);
    if (device.isEmpty())
    {
        std::cerr << "unrecognized device: " << Device << endl;
        return 2;
    }
    int fd = -1;

    auto ioutput = device.iOutput();
    if(Operation == "wifi_test")
    {
        auto ibutton = device.iButton();
        auto buttonState=device.iOutput()->getState();
        do{
            
        }while(!ibutton->isDown());

       cout<<"test"<<endl;
       if(buttonState.toString()=="off")
       {
            ioutput->on();
       }
       else
       {
            ioutput->off();
       }
        cout<<buttonState<<endl;

    }
    if (Operation == "switch")
    {
        auto ibutton = device.iButton();
        if (!ibutton)
            return unsupported(device, "switch");
        timespec decayStart, decayStop, pressWaitStart, pressWaitNow;
        std::cout << "Waiting for " << device << " switch to be pressed..." << std::endl;
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
            std::cout << std::endl << "Detected " << device << " switch pressed..." << std::endl;
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

    std::string path = hwi->lookupPath(device);
    if (!path.empty())
    {
        fd = openDevice(path);
    }

    if (!Serial.empty() || !RTS.empty())
    {
        auto iserial = device.iSerial();
        if (!iserial)
            return unsupported(device, Operation);

        if (!Serial.empty())
            iserial->setConfig(PTYPE, PTERM);

        if (!RTS.empty())
            set_rts(fd, (RTS == "rts1"));
    }

    if (!Output.empty())
    {
        if (!ioutput)
            return unsupported(device, Operation);
        if (Output == "on")
            ioutput->on();
        else
            ioutput->off();
    }

    print_status(device, fd);
    if (fd >= 0)
        ::close(fd);

    // all good, return 0
    return 0;
}
