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

#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "libftdi1/ftdi.h"
#include "../util/optionparser-1.7/src/optionparser.h"

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
        char* endptr = 0;
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
                if ( argStr == "LOOPBACK"
                    || argStr == "RS232" 
                    || argStr == "RS485_HALF" 
                    || argStr == "RS485_FULL" 
                    || argStr == "RS422" )
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

typedef enum {ASYNC, SYNC} BIT_BANG_MODE;

unsigned char ftdi0_portTypeDefs = 0;
unsigned char ftdi1_portTypeDefs = 0;

void setFtdiBitBangMode(const BIT_BANG_MODE mode = ASYNC) {
    // Call libFtdi API here
}

// shift the port type into the correct location for insertion
unsigned char adjustBitPosition(const PORT_DEFS port, const unsigned char bits ) {
    return bits << (port * 2);
}

// Sets the port type for a particular DSM port.
//

void setPortType(const PORT_DEFS port, const PORT_TYPE portType) {
    // get the correct state variable to act upon...
    unsigned char& portTypeDefs = port < PORT4 ? ftdi0_portTypeDefs : ftdi1_portTypeDefs;

    // first zero out the two bits in the correct port type slot...
    unsigned char zeroBits = ~adjustBitPosition(port, 0x03);
    portTypeDefs &= adjustBitPosition(port, zeroBits);
    // insert the new port type in the correct slot.
    portTypeDefs |= adjustBitPosition(port, static_cast<unsigned char>(portType));
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

        if (portStr == "LOOPBACK") portType = LOOP_BACK;
        else if (portStr == "RS232") portType = RS232;
        else if (portStr == "RS422") portType = RS485_RS422_FULL;
        else if (portStr == "RS485_HALF") portType = RS485_HALF;
        else if (portStr == "RS485_FULL") portType = RS485_RS422_FULL;
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

    // Initialize the FT4243H
    struct ftdi_context *ftdi;

    if ((ftdi = ftdi_new()) == 0)
    {
        std::cerr << "ftdi_new failed" << std::endl;
        return 2;
    }

    int f = ftdi_usb_open(ftdi, 0x0403, 0x6011);

    if (f < 0 && f != -5)
    {
        std::cerr << "unable to open ftdi device: " << f << ", " << ftdi_get_error_string(ftdi) << std::endl;
        return 3;
    }

    std::cout << "ftdi open succeeded: " << f << std::endl;

    // Put the control FT4232 into bit bang mode
    // setFtdiBitBangMode();
    // Set the port type for the desired port 
    // Maybe the user can set multiple ports at once?
    // setPortType(port, portType);

    // Call FTDI API to set the desired port types

    // print out the new port configurations

    return -1;
}


    

//     printf("enabling bitbang mode\n");
//     ftdi_set_bitmode(ftdi, 0xFF, BITMODE_BITBANG);

//     sleep(3);

//     buf[0] = 0x0;
//     printf("turning everything on\n");
//     f = ftdi_write_data(ftdi, buf, 1);
//     if (f < 0)
//     {
//         fprintf(stderr,"write failed for 0x%x, error %d (%s)\n",buf[0],f, ftdi_get_error_string(ftdi));
//     }

//     sleep(3);

//     buf[0] = 0xFF;
//     printf("turning everything off\n");
//     f = ftdi_write_data(ftdi, buf, 1);
//     if (f < 0)
//     {
//         fprintf(stderr,"write failed for 0x%x, error %d (%s)\n",buf[0],f, ftdi_get_error_string(ftdi));
//     }

//     sleep(3);

//     for (i = 0; i < 32; i++)
//     {
//         buf[0] =  0 | (0xFF ^ 1 << (i % 8));
//         if ( i > 0 && (i % 8) == 0)
//         {
//             printf("\n");
//         }
//         printf("%02hhx ",buf[0]);
//         fflush(stdout);
//         f = ftdi_write_data(ftdi, buf, 1);
//         if (f < 0)
//         {
//             fprintf(stderr,"write failed for 0x%x, error %d (%s)\n",buf[0],f, ftdi_get_error_string(ftdi));
//         }
//         sleep(1);
//     }

//     printf("\n");

//     printf("disabling bitbang mode\n");
//     ftdi_disable_bitbang(ftdi);

//     ftdi_usb_close(ftdi);
// done:
//     ftdi_free(ftdi);

//     return retval;
// }