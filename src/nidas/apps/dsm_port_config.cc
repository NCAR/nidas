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

using namespace nidas::core;

static const char* ARG_LOOPBACK = "LOOPBACK";
static const char* ARG_RS232 = "RS232";
static const char* ARG_RS422 = "RS422";
static const char* ARG_RS485_HALF = "RS485_HALF";
static const char* ARG_RS485_FULL = "RS485_FULL";

// This enum specifies the bit pattern which should be written to the control FT4232H device.
typedef enum {LOOP_BACK, RS232, RS485_HALF, RS485_RS422_FULL} PORT_TYPE;
// This enum specifies the ports in the DSM. Since each FTDI chip suppports 4 ports, 
// the enumeration starts over at PORT4. This simplifies the code for populating the 
// ftdiX_portDefs variables.
typedef enum {PORT0=0, PORT1, PORT2, PORT3, PORT4, PORT5, PORT6, PORT7} PORT_DEFS;
typedef enum {TERM_96k_OHM, TERM_100_OHM} TERM;

unsigned char portTypeDefs = 0;

// shift the port type into the correct location for insertion
unsigned char adjustBitPosition(const PORT_DEFS port, const unsigned char bits ) 
{
    // adjust port to always be 0-3, assuming 4 bits per port type.
    PORT_DEFS adjPort = static_cast<PORT_DEFS>(port % PORT2);
    return bits << (adjPort * 4);
}

// Sets the port type for a particular DSM port.
// Currently assumes 2 bits per port def.
void setPortType(const PORT_DEFS port, const PORT_TYPE portType) 
{
    // first zero out the two bits in the correct port type slot...
    portTypeDefs &= ~adjustBitPosition(port, 0x0F);
    // insert the new port type in the correct slot.
    portTypeDefs |= adjustBitPosition(port, static_cast<unsigned char>(portType));
}

const std::string& portTypeToStr( unsigned char portDefs) 
{
    static std::string portDefStr;
    portDefStr = "";
    switch ( static_cast<PORT_DEFS>(portDefs) ) {
        case LOOP_BACK:
            portDefStr.append(ARG_LOOPBACK);
            break;
        case RS232:
            portDefStr.append(ARG_RS232);
            break;
        case RS485_HALF:
            portDefStr.append(ARG_RS485_HALF);
            break;
        case RS485_RS422_FULL:
            portDefStr.append(ARG_RS422);
            portDefStr.append("/");
            portDefStr.append(ARG_RS485_FULL);
            break;
        default:
            break;
    }

    return portDefStr;
}

void printPortDefs(const enum ftdi_interface iface) 
{
    unsigned int port = 1;
    unsigned char tmpTypeDefs = portTypeDefs;
    switch( iface ) {
        case INTERFACE_A:
            port = 0;
            break;
        case INTERFACE_B:
            port = 2;
            break;
        case INTERFACE_C:
            port = 4;
            break;
        case INTERFACE_D:
            port = 6;
            break;
        default: 
            break;
    }


    std::cout << "Port" << port << ": " << portTypeToStr(tmpTypeDefs & 0x03) << std::endl;
    port++; tmpTypeDefs >>= 4;
    std::cout << "Port" << port << ": " << portTypeToStr(tmpTypeDefs & 0x03) << std::endl;
}

// need to select the interface based on the specified port 
// at present, assume 2 bits per port definition
enum ftdi_interface port2iface(const unsigned int port)
{
    enum ftdi_interface iface = INTERFACE_ANY;
    switch ( port ) 
    {
        case PORT0:
        case PORT1:
            iface = INTERFACE_A;
            break;
        case PORT2:
        case PORT3:
            iface = INTERFACE_B;
            break;

        case PORT4:
        case PORT5:
            iface = INTERFACE_C;
            break;
        case PORT6:
        case PORT7:
            iface = INTERFACE_D;
            break;

        default:  
            break;
    }

    return iface;
}

NidasApp app("dsm_port_config");

NidasAppArg Port("-p,--port-id", "<0-7>",
        		 "DSM canonical serial port id.", "0");
NidasAppArg Mode("-m,--xcvr-mode", "<RS232>",
				 "Port transceiver modes supported by Exar SP339 chip. i.e. - \n"
				 "    LOOPBACK\n"
				 "    RS232\n"
				 "    RS422\n"
				 "    RS485_FULL\n"
				 "    RS485_HALF", "RS232");
NidasAppArg Display("-d,--display", "",
					 "Display current port setting and exit", "");
NidasAppArg LineTerm("-t,--term", "<NONE>",
					 "Port transceiver line termination supported by Exar SP339 chip. i.e. - \n"
					 "    NONE\n"
					 "    TERM_120", "NONE");

int usage(const char* argv0)
{
    std::cerr << "\
Usage: " << argv0 << "-p <port ID> -m <mode ID> [-t <termination> -l <log level>]" << std::endl
		 << argv0 << "-p <port ID> -d" << std::endl << std::endl
         << app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Version | app.Help
    		            | Port | Mode | Display);

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

    PORT_DEFS port = (PORT_DEFS)-1;
    PORT_TYPE portType = (PORT_TYPE)-1;

    // check the options first to set up the port and port control
    DLOG(("Port Option Flag/Value: ") << Port.getFlag() << ": " << Port.asInt());
    DLOG(("Port Option Flag Length: ") << Port.getFlag().length());
    if (Port.getFlag().length() != 0) {
        port = (PORT_DEFS)Port.asInt();
        switch (port)
        {
            case 0:
                port = PORT0;
                break;
            case 1:
                port = PORT1;
                break;
            case 2:
                port = PORT2;
                break;
            case 3:
                port = PORT3;
                break;
            case 4:
                port = PORT4;
                break;
            case 5:
                port = PORT5;
                break;
            case 6:
                port = PORT6;
                break;
            case 7:
                port = PORT7;
                break;
            default:
                std::cerr << "Something went wrong, as the port arg wasn't in the range 0-7" << std::endl;
                return 1;
        }
    }

    else 
    {
        std::cerr << "Must supply a port option on the command line.\n" << std::endl;
        usage(argv[0]);
        return 1;
    }

    if (Mode.getFlag().length() != 0)
    {
        std::string modeStr(Mode.getValue());
        DLOG(("Mode Option Flag/Value: ") << Mode.getFlag() << ": " << modeStr);
        std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::toupper);
        if (modeStr == ARG_LOOPBACK) portType = LOOP_BACK;
        else if (modeStr == ARG_RS232) portType = RS232;
        else if (modeStr == ARG_RS422) portType = RS485_RS422_FULL;
        else if (modeStr == ARG_RS485_HALF) portType = RS485_HALF;
        else if (modeStr == ARG_RS485_FULL) portType = RS485_RS422_FULL;
        else
        {
                std::cerr << "Unknown/Illegal/Missing port type argumeent.\n" << std::endl;
                usage(argv[0]);
                return 1;
        }
    }

    else
    {
        if (Display.getFlag().length() == 0) {
            std::cerr << "Must supply a port mode option on the command line.\n" << std::endl;
            usage(argv[0]);
            return 1;
        }
    }

    // vid and pid of FT4243H devices
    // NOT USED int vid = 0x0403, pid = 0x6011, 
    int bus = 1, device = 6;

    ftdi_context* c_context = ftdi_new();

    enum ftdi_interface iface = port2iface(port);

    if (ftdi_set_interface(c_context, iface)) {
        std::cerr << c_context->error_str << std::endl;
        return 2;
    }

    std::cout << std::endl << "Opening FT4232H device with description: FT4232H NCAR. Found device: ";
    if (!ftdi_usb_open_desc(c_context, 0x0403, 0x6011, "FT4232H NCAR", 0))
    {
        char manuf[80];
        char descript[80];
        char serialNo[80];
        ftdi_usb_get_strings2(c_context, libusb_get_device(c_context->usb_dev), manuf, 80, descript, 80, serialNo, 80);
        std::cout << ", " << manuf << ", "
            << descript << ", "
            << serialNo << std::endl; 

        // Now initialize the chosen device for bit-bang mode, all outputs
        ftdi_set_bitmode(c_context, 0xFF, BITMODE_BITBANG);

        // get the current port definitions
        ftdi_read_pins(c_context, &portTypeDefs);
        std::cout << std::endl << "Current Port Definitions" << std::endl << "========================" << std::endl;
        printPortDefs(iface);

        if(Display.getFlag().length()) {
            // don't do anything else
            return 0;
        }

        // Set the port type for the desired port 
        setPortType(port, portType);

        // Call FTDI API to set the desired port types
        ftdi_write_data(c_context, &portTypeDefs, 1);

        // print out the new port configurations
        std::cout << std::endl << "New Port Definitions" << std::endl << "====================" << std::endl;
        printPortDefs(iface);

        ftdi_usb_close(c_context);
        ftdi_free(c_context);
    }

    else
    {
        std::cout << std::endl << "Failed to open FTDI device at bus: " << bus << " device: " << device<< std::endl;
        return 3;
    }

    // all good, return 0
    return 0;
}
