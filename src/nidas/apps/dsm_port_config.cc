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
#include "libusb.h"
#include "ftdi.hpp"
#include "../util/optionparser.h"

static const char* ARG_LOOPBACK = "LOOPBACK";
static const char* ARG_RS232 = "RS232";
static const char* ARG_RS422 = "RS422";
static const char* ARG_RS485_HALF = "RS485_HALF";
static const char* ARG_RS485_FULL = "RS485_FULL";

struct Arg: public option::Arg
{
    static void printError(const char* msg1, const option::Option& opt, const char* msg2)
    {
        std::cerr << "ERROR: " << msg1;
        std::string optName(opt.name, opt.namelen );
        std::cerr << optName;
        std::cerr << msg2;
    }
    static option::ArgStatus Unknown(const option::Option& option, bool msg)
    {
        if (msg) printError("Unknown option '", option, "'\n");
        return option::ARG_ILLEGAL;
    }
    static option::ArgStatus Required(const option::Option& option, bool msg)
    {
        if (option.arg != 0)
        return option::ARG_OK;
        if (msg) printError("Option '", option, "' requires an argument\n");
        return option::ARG_ILLEGAL;
    }
    static option::ArgStatus NonEmpty(const option::Option& option, bool msg)
    {
        if (option.arg != 0 && option.arg[0] != 0)
        return option::ARG_OK;
        if (msg) printError("Option '", option, "' requires a non-empty argument\n");
        return option::ARG_ILLEGAL;
    }
    static option::ArgStatus Numeric(const option::Option& option, bool msg)
    {
        if (option.arg != 0)
        {
            try {
                int arg = -1;
                std::istringstream(option.arg) >> arg;
                return option::ARG_OK;
            }
            catch (std::exception e)
            {
                if (msg) printError("Option '", option, "' requires a numeric argument\n");
                return option::ARG_ILLEGAL;
            }
        }

        return option::ARG_ILLEGAL;
    }
    static option::ArgStatus PortNum(const option::Option& option, bool msg)
    {
        option::ArgStatus argStatus = Numeric( option, msg);
        if ( argStatus == option::ARG_OK ) 
        {
            int port = -1;
            std::istringstream(option.arg) >> port;
            if (port < 0 || port > 8 ) // 0-7
            {
                argStatus = option::ARG_ILLEGAL;
                if (msg) printError("Option '", option, "' Port option arg must be in range 0-7.\n");
            }
        }

        return argStatus;
    }
    static option::ArgStatus PortType(const option::Option& option, bool msg)
    {
        option::ArgStatus argStatus = Required( option, msg);
        if ( argStatus == option::ARG_OK ) 
        {
            argStatus = NonEmpty(option, msg);
            if (argStatus == option::ARG_OK)
            {
                std::string argStr(option.arg);
                std::transform(argStr.begin(), argStr.end(), argStr.begin(), ::toupper);
                if ( argStr == ARG_LOOPBACK
                    || argStr == ARG_RS232 
                    || argStr == ARG_RS485_HALF 
                    || argStr == ARG_RS485_FULL 
                    || argStr == ARG_RS422 )
                {
                    return option::ARG_OK;
                }
                else
                {
                    argStatus = option::ARG_ILLEGAL; 
                    if (msg)
                    {
                        printError("Option '", option, "' does not specify a valid port type\n");
                    }
                }
            }
        }

        return argStatus;
    }
};

enum  optionIndex { UNKNOWN, HELP, PORT, TYPE };
const option::Descriptor usage[] =
{
    {UNKNOWN, 0, "", "", option::Arg::None, "USAGE: dsm_port_config [options]\n\n"
                                            "Options:" },
    {HELP, 0, "h", "help", option::Arg::None, "  --help  \tPrint usage and exit." },
    {PORT, 0, "p", "port", Arg::PortNum, "  --port, -p  \tSpecify port index 0-7." },
    {TYPE, 0, "t", "type", Arg::PortType,   "  --type, -t  \tSpecify port type:\n"
                                            "    LOOPBACK\n"
                                            "    RS232\n"
                                            "    RS485_HALF\n"
                                            "    RS485_FULL\n"
                                            "    RS422\n" },
    {UNKNOWN, 0, "", "",option::Arg::None, "\nExamples:\n"
                                "  dsm_port_config -p0 -tRS232\n"
                                "  dsm_port_config -p3 -tRS422\n"
                                "  dsm_port_config -p6 -tRS485_HALF\n"},
    {0,0,0,0,0,0}
};

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
    // adjust port to always be 0-3, assuming 2 bits per port type.
    PORT_DEFS adjPort = static_cast<PORT_DEFS>(port % PORT4);
    return bits << (adjPort * 2);
}

// Sets the port type for a particular DSM port.
// Currently assumes 2 bits per port def.
void setPortType(const PORT_DEFS port, const PORT_TYPE portType) 
{
    // first zero out the two bits in the correct port type slot...
    portTypeDefs &= ~adjustBitPosition(port, 0x03);
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
            port = 4;
            break;
        default: 
            break;
    }


    std::cout << "Port" << port << ": " << portTypeToStr(tmpTypeDefs & 0x03) << std::endl;
    port++; tmpTypeDefs >>= 2;
    std::cout << "Port" << port << ": " << portTypeToStr(tmpTypeDefs & 0x03) << std::endl;
    port++; tmpTypeDefs >>= 2;
    std::cout << "Port" << port << ": " << portTypeToStr(tmpTypeDefs & 0x03) << std::endl;
    port++; tmpTypeDefs >>= 2;
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
        case PORT2:
        case PORT3:
            iface = INTERFACE_A;
            break;

        case PORT4:
        case PORT5:
        case PORT6:
        case PORT7:
            iface = INTERFACE_B;
            break;

        default:  
            break;
    }

    return iface;
}

int main(int argc, char* argv[]) {
    // options handler setup
    if (argc > 0) // skip program name argv[0] if present
    {
        argc--;
        argv += 1;
    }

    option::Stats  stats(usage, argc, argv);
    option::Option options[stats.options_max];
    option::Option buffer[stats.buffer_max];
    option::Parser parse(usage, argc, argv, &options[0], &buffer[0]);
    if (parse.error())
    {
        std::cerr << "Failed to parse the command line options!" << std::endl;
        return 1;
    }

    if (options[HELP] || argc == 0) {
        option::printUsage(std::cout, usage);
        return 0;
    }

    int portOpt;

    PORT_DEFS port = (PORT_DEFS)-1;
    PORT_TYPE portType = (PORT_TYPE)-1;

    // check the options first to set up the port and port control
    if (options[PORT]) {
        std::istringstream popt(options[PORT].arg);
        popt >> portOpt;
        switch (portOpt) 
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
        option::printUsage(std::cout, usage);
        return 1;
    }

    if (options[TYPE]) 
    {
        std::string portStr(options[TYPE].arg);
        std::transform(portStr.begin(), portStr.end(), portStr.begin(), ::toupper);
        if (portStr == ARG_LOOPBACK) portType = LOOP_BACK;
        else if (portStr == ARG_RS232) portType = RS232;
        else if (portStr == ARG_RS422) portType = RS485_RS422_FULL;
        else if (portStr == ARG_RS485_HALF) portType = RS485_HALF;
        else if (portStr == ARG_RS485_FULL) portType = RS485_RS422_FULL;
        else
        {
            std::cerr << "Unknown/Illegal/Missing port type argumeent.\n" << std::endl;
            option::printUsage(std::cout, usage);
            return 1;
        }
    }

    else
    {
        std::cerr << "Must supply a port type option on the command line.\n" << std::endl;
        option::printUsage(std::cout, usage);
        return 1;
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

    std::cout << std::endl << "Opening device on USB bus: " << bus << " device id: " << device;
    if (!ftdi_usb_open_bus_addr(c_context, bus, device))
    {
        char manuf[80];
        char descript[80];
        char serialNo[80];
        ftdi_usb_get_strings2(c_context, libusb_get_device(c_context->usb_dev), manuf, 80, descript, 80, serialNo, 80);
        std::cout << ", " << manuf << ", "
            << descript << ", "
            << serialNo << std::endl; 

        // set the baud clock to push the data out
        //ftdi_set_baudrate(c_context, 9600);
        // Now initialize the chosen device for bit-bang mode, all outputs
        ftdi_set_bitmode(c_context, 0xFF, BITMODE_BITBANG);

        // get the current port definitions
        ftdi_read_pins(c_context, &portTypeDefs);
        std::cout << std::endl << "Initial Port Definitions" << std::endl << "========================" << std::endl;
        printPortDefs(iface);

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
