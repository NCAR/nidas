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
#include <fstream>
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

#include <json/json.h>

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

NidasAppArg Update("-U,--update", "<seconds>",
R"(Update status at the given interval if not zero.
Ignored if not a status operation.)", "0");

NidasAppArg JsonOutput("--json", "",
R"(Write status output in JSON format.)");

NidasAppArg OutputPath("--output", "<path>",
R"(Write status output to <path>.  Most useful with --update.)", "");

NidasAppArg Timestamp("--timestamp", "",
R"(Include a timestamp in text status output.)", "false");

// Borrowed from data_stats, eventually it should be added to UTime to
// replace both.
inline std::string
json_format(const UTime& ut)
{
    return ut.format(true, "%Y-%m-%dT%H:%M:%S.%3fZ");
}


void
usage()
{
    std::cout
        << R""""(
Usage: pio [options] [list|status|device [op ...]]

Query and control power relays, sensor power, LEDs, serial ports, and
pushbutton switches.  If no arguments, show the status of all devices.

  {list}:
    List the known devices, without opening the hardware.

  {status}:
    Open all the hardware devices and show the status of each.

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
        << R""""(

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


NidasAppException toomany(const std::string& msg)
{
    return NidasAppException(msg + ": too many arguments.  Use -h for help.");
}


void parseRunString(int argc, char* argv[])
{
    app.enableArguments(Update | JsonOutput | OutputPath | Timestamp |
                        app.loggingArgs() | app.Help);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        usage();
        exit(0);
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
                throw toomany(Operation);
            }
            break;
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
                throw toomany(Operation);
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
        throw NidasAppException("operation unknown: " + arg);
    }
    if (Update.asInt() != 0 && (Operation != "status" || !Device.empty()))
    {
        throw NidasAppException("update interval only valid when getting "
                                "status of all devices");
    }
}


int unsupported(const HardwareDevice& device, const std::string& op)
{
    cerr << "unsupported operation on device "
         << device << ": " << op << endl;
    return 1;
}


int open_device(HardwareDevice& device)
{
    auto hwi = HardwareInterface::getHardwareInterface();
    int fd = -1;
    std::string path = hwi->lookupPath(device);
    if (!path.empty())
    {
        fd = ::open(path.c_str(), O_RDONLY);
        if (fd < 0)
        {
            perror(path.c_str());
        }
    }
    return fd;
}


bool
set_rts(HardwareDevice& device, bool rts)
{
    int fd = open_device(device);
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
    ::close(fd);
    return true;
}


/**
 * Open @p device and return the current rts setting in @p rts.  Return false
 * if there is no path for this device or there was an error trying to
 * retrieve rts.
 */
bool
read_rts(HardwareDevice& device, bool& rts)
{
    bool result = false;
    int status = 0;
    int fd = open_device(device);
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
    ::close(fd);
    return result;
}


void
write_status(std::ofstream& out, HardwareDevice& device)
{
    out << left << setw(8) << device;
    if (auto oi = device.iOutput())
    {
        out << left << setw(5) << oi->getState();
        if (auto iserial = device.iSerial())
        {
            PortType ptype;
            PortTermination term;
            iserial->getConfig(ptype, term);
            out << left << setw(5) << ptype.toString(ptf_485);
            out << left << setw(8) << term;
            bool rts;
            if (read_rts(device, rts))
            {
                out << left << setw(5) << (rts ? "rts1" : "rts0");
            }
        }
        if (auto ibutton = device.iButton())
        {
            out << (ibutton->isDown() ? "down" : "up");
        }
    }
    else
        out << "could-not-open";
    out << endl;
}


const std::string STDOUT = "/dev/stdout";
std::ofstream output_stream;
std::string output_path;
std::string tmp_path;

std::ofstream& get_output()
{
    if (output_stream.is_open())
    {
        return output_stream;
    }
    output_path = OutputPath.getValue();
    if (output_path.empty() || output_path == "-")
    {
        output_path = STDOUT;
        tmp_path = "";
        output_stream.open(output_path);
    }
    else
    {
        tmp_path = output_path + ".tmp";
        output_stream.open(tmp_path);
    }
    if (!output_stream.is_open())
    {
        throw std::runtime_error("Could not open output file: " +
                                 output_path);
    }
    return output_stream;
}


void close_output()
{
    if (output_stream.is_open())
    {
        output_stream.close();
        if (!tmp_path.empty() && output_path != STDOUT)
        {
            // rename the temporary file to the final output path.
            if (::rename(tmp_path.c_str(), output_path.c_str()) != 0)
            {
                throw std::runtime_error("Could not rename " + tmp_path +
                                         " to " + output_path);
            }
        }
    }
}


/**
 * Write the status of the given device to the JSON object @p dev.
 */
void json_status(HardwareDevice& device, Json::Value& dev)
{
    std::string devName = device.id();
    std::string ptype;
    std::string term;
    if (auto oi = device.iOutput())
    {
        dev["output"] = oi->getState().toString();
        if (auto iserial = device.iSerial())
        {
            PortType p;
            PortTermination terminal;
            iserial->getConfig(p, terminal);
            ptype= p.toString(ptf_485);
            dev["porttype"] = ptype;
            term = terminal.toShortString();
            dev["termination"] = term;
            bool rts;
            if (read_rts(device, rts))
            {
                dev["rts"] = rts ? "rts1" : "rts0";
            }
        }
        if (auto ibutton = device.iButton())
        {
            dev["button"] = ibutton->isDown() ? "down" : "up";
        }
    }
    else
    {
        dev["error"] = "device not found: " + devName;
    }
}


void write_all_status(std::vector<std::string> ids = {})
{
    // Write the current state for all power outputs, either to stdout
    // or to a JSON file.
    bool json_output = JsonOutput.asBool();

    auto hwi = HardwareInterface::getHardwareInterface();
    Json::Value root;
    std::ofstream& out = get_output();
    std::string timestamp = json_format(UTime());
    root["timestamp"] = timestamp;
    Json::Value& devices = root["devices"];
    if (!json_output && Timestamp.asBool())
    {
        out << "timestamp: " << timestamp << std::endl;
    }
    for (auto& device: hwi->devices())
    {
        if (!ids.empty() && std::find(ids.begin(), ids.end(),
                                      device.id()) == ids.end())
        {
            continue; // skip devices not in the list
        }
        if (json_output)
        {
            json_status(device, devices[device.id()]);
        }
        else
        {
            write_status(out, device);
        }
    }
    if (json_output)
    {
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "  "; // pretty print
        out << Json::writeString(writer, root) << std::endl;
    }
    close_output();
}


int main(int argc, char* argv[])
{
    try {
        parseRunString(argc, argv);
    }
    catch (NidasAppException& e)
    {
        std::cerr << e.what() << std::endl;
        exit(1);
    }

    // status is first because if it loops, then we need to make sure a
    // HardwareInterface is not held open between iterations.
    if (Operation == "status" && Device.empty())
    {
        int update = Update.asInt();
        write_all_status();
        while (update > 0)
        {
            sleep(update);
            write_all_status();
        }
        return 0;
    }

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

    HardwareDevice device;
    device = hwi->lookupDevice(Device);
    if (device.isEmpty())
    {
        std::cerr << "unrecognized device: " << Device << endl;
        return 2;
    }

    auto ioutput = device.iOutput();
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

    if (!Serial.empty() || !RTS.empty())
    {
        auto iserial = device.iSerial();
        if (!iserial)
            return unsupported(device, Operation);

        if (!Serial.empty())
            iserial->setConfig(PTYPE, PTERM);

        if (!RTS.empty())
            set_rts(device, (RTS == "rts1"));
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

    // Write the status for a single device.
    write_all_status({device.id()});

    // all good, return 0
    return 0;
}
