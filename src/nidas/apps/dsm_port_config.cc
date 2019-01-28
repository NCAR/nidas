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
/* dsm_port_config.cc

    User space code that uses libusb and libftdi to bit bang the third FT4243 device in 
    the DSM to programmatically configure the UARTS attached to the other two FT4232H devices 
    for port type (RS23, RS422).

    NOTE: The FTDI used for the bit banging cannot be bound to the libftdi_sio kernel mode 
          driver. See the manpage for bind/unbind and lsusb.

    The DSM uses two FTDI FT4232H USB-Serial devices. Each of these devices has four TTL 
    level serial outputs. Each of the FT4232H devices' outputs are sent to one of four Exar SP339
    DUART devices which results in a total of 8 serial devices.

    The Exar SP339 devices are capable of operating in one of four distinct modes: 

    Loopback
    RS232
    RS485 half duplex
    RS485/RS422 full duplex

    The Exar SP339 uses two mode pin inputs M0/M1 to determine whether the device is in Loopback, 
    RS232, RS485 half/full duplex or RS422 full duplex modes. It has a third pin, TERM, which defines whether it
    is configured for 100 ohm or 96 kohm line termination schemes. The DSM currently uses dual 
    header connectors and jumpers to configure the Exar SP339 device ports.

    The future DSM uses a third FT4232H device configured for bit banging mode to provide 
    general I/O capability to control the Exar SP339 port configuration via its M0/1, TERM and DIR input pins.    
    
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
#include "nidas/core/SerialXcvrCtrl.h"
#include "nidas/util/SensorPowerCtrl.h"

using namespace nidas::core;
using namespace nidas::util;

static const char* ARG_LOOPBACK = "LOOPBACK";
static const char* ARG_RS232 = "RS232";
static const char* ARG_RS422 = "RS422";
static const char* ARG_RS485_HALF = "RS485_HALF";
static const char* ARG_RS485_FULL = "RS485_FULL";

NidasApp app("dsm_port_config");

NidasAppArg Port("-p,--port-id", "<0-7>",
        		 "DSM canonical serial port id.", "0");
NidasAppArg Mode("-m,--xcvr-mode", "<RS232>",
				 "Serial line transceiver modes supported by Exar SP339 chip. i.e. - \n"
				 "    LOOPBACK\n"
				 "    RS232\n"
				 "    RS422\n"
				 "    RS485_FULL\n"
				 "    RS485_HALF", "RS232");
NidasAppArg Display("-d,--display", "",
					 "Display current port configuration and exit", "");
NidasAppArg LineTerm("-t,--term", "<NO_TERM>",
                     "Port transceiver line termination supported by Exar SP339 chip. i.e. - \n"
                     "    NO_TERM\n"
                     "    TERM_120_OHM", "NO_TERM");
NidasAppArg Energy("-e,--energy", "<POWER_ON>",
                  "Controls whether the DSM sends power to the sensor via the bulgin port - \n"
                  "    POWER_OFF\n"
                  "    POWER_ON", "POWER_ON");

int usage(const char* argv0)
{
    std::cerr << "\
Usage: " << argv0 << "-p <port ID> -m <mode ID> [-t <termination> -e <power state> -l <log level>]" << std::endl << "       "
		 << argv0 << "-p <port ID> -d" << std::endl << std::endl
         << app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Version | app.Help
    		            | Port | Mode | Display | LineTerm | Energy);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        return usage(argv[0]);
    }
    return 0;
}

int main(int argc, char* argv[]) {

    if (parseRunString(argc, argv))
        exit(1);

    PORT_TYPES portType = (PORT_TYPES)-1;

    XcvrConfig xcvrConfig;

    // check the options first to set up the port and port control
    DLOG(("Port Option Flag/Value: ") << Port.getFlag() << ": " << Port.asInt());
    DLOG(("Port Option Flag Length: ") << Port.getFlag().length());
    if (Port.specified()) {
        xcvrConfig.port = (n_u::PORT_DEFS)Port.asInt();
        if (!(0 <= xcvrConfig.port && xcvrConfig.port <= 7)) {
            std::cerr << "Something went wrong, as the port arg wasn't in the range 0-7" << std::endl;
            return 2;
        }
    }

    else 
    {
        std::cerr << "Must supply a port option on the command line.\n" << std::endl;
        usage(argv[0]);
        return 3;
    }

    // print out the existing port configurations
    std::cout << std::endl << "Current Port Definitions" << std::endl << "========================" << std::endl;
    n_u::SerialGPIO sgpio(xcvrConfig.port);
    unsigned char rawConfig = sgpio.readInterface();
    DLOG(("rawConfig: %x", rawConfig));
    xcvrConfig.portType = SerialXcvrCtrl::bits2PortType(rawConfig);
    DLOG(("port type: ") << xcvrConfig.portType << " - "
                         << SerialXcvrCtrl::portTypeToStr(xcvrConfig.portType));
    xcvrConfig.termination = rawConfig & n_u::BITS_TERM ? TERM_120_OHM : NO_TERM;
    xcvrConfig.print();

    SensorPowerCtrl sensrPwrCtrl((n_u::PORT_DEFS)Port.asInt());
    sensrPwrCtrl.print();

    if (Display.specified()) {
        return 0;
    }

    if (Mode.specified())
    {
        std::string modeStr(Mode.getValue());
        DLOG(("Mode Option Flag/Value: ") << Mode.getFlag() << ": " << modeStr);
        std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::toupper);
        if (modeStr == ARG_LOOPBACK) portType = LOOPBACK;
        else if (modeStr == ARG_RS232) portType = RS232;
        else if (modeStr == ARG_RS422) portType = RS422;
        else if (modeStr == ARG_RS485_HALF) portType = RS485_HALF;
        else if (modeStr == ARG_RS485_FULL) portType = RS485_FULL;
        else
        {
            std::cerr << "Unknown/Illegal/Missing port type argument: " << Mode.getValue() << std::endl;
            usage(argv[0]);
            return 4;
        }

        xcvrConfig.portType = portType;
    }

    if (LineTerm.specified()) {
        std::string termStr(LineTerm.getValue());
        DLOG(("LineTerm Option Flag/Value: ") << LineTerm.getFlag() << ": " << termStr);
        std::transform(termStr.begin(), termStr.end(), termStr.begin(), ::toupper);
        TERM lineTerm = SerialXcvrCtrl::strToTerm(LineTerm.getValue());
        if (lineTerm != -1) {
            xcvrConfig.termination = lineTerm;
        }
        else
        {
            std::cerr << "Unknown/Illegal/Missing line termination argument: " << LineTerm.getValue() << std::endl;
            usage(argv[0]);
            return 5;
        }
    }

    if (Energy.specified()) {
        std::string pwrStr(Energy.getValue());
        DLOG(("Energy Option Flag/Value: ") << Energy.getFlag() << ": " << pwrStr);
        std::transform(pwrStr.begin(), pwrStr.end(), pwrStr.begin(), ::toupper);
        POWER_STATE power = PowerCtrlAbs::strToPowerState(pwrStr);
        if (power != ILLEGAL_POWER) {
            sensrPwrCtrl.enablePwrCtrl(true);
            power == POWER_ON ? sensrPwrCtrl.pwrOn() : sensrPwrCtrl.pwrOff();
        }
        else
        {
            std::cerr << "Unknown/Illegal/Missing energy argument: " << Energy.getValue() << std::endl;
            usage(argv[0]);
            return 5;
        }
    }

    // print out the new port configurations
    std::cout << std::endl << "New Port Definitions" << std::endl << "====================" << std::endl;
    SerialXcvrCtrl xcvrCtrl(xcvrConfig);
    xcvrCtrl.printXcvrConfig();
    sensrPwrCtrl.print();

    // all good, return 0
    return 0;
}
