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
/* eio.cc

    User space code that uses libusb and libftdi to program flash in the FT4243 devices used for GPIO 
    in the DSM in order to make them easier to find on the USB bus. 

    The mechanism for this will be to change the product string in the flash to "FT4232H-GPIOx" where 
    x is 0, 1, 2, etc.

    This should only need to be performed once, preferrably prior to installing the UART board in the 
    DSM. However, if it is not possible to power the UART board outside the DSM, then it must be done 
    when the DSM is being manufactured, or after the UART board has been replaced.

    Procedure: It is assumed that the FT4232H devices will be hard wired into a USB hub on the UART
    board. Thus, they will always be enumerated in the same order, and will never be unplugged. This 
    utility will be run on the DSM once when manufactured and once when the UART board is replaced.

    Original Author: Paul Ourada

*/

#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>
#include "libusb.h"
#include "ftdi.h"
#include "../util/optionparser.h"

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
    static option::ArgStatus Index(const option::Option& option, bool msg)
    {
        option::ArgStatus argStatus = Required( option, msg);
        if ( argStatus == option::ARG_OK ) 
        {
            argStatus = Numeric(option, msg);
            if (argStatus == option::ARG_OK)
            {
                    return option::ARG_OK;
            }
            else
            {
                argStatus = option::ARG_ILLEGAL; 
                if (msg)
                {
                    printError("Option '", option, "' numeric argument, usually a single digit.\n");
                }
            }
        }

        return argStatus;
    }
    static option::ArgStatus Description(const option::Option& option, bool msg)
    {
        option::ArgStatus argStatus = Required( option, msg);
        if ( argStatus == option::ARG_OK ) 
        {
            argStatus = NonEmpty(option, msg);
            if (argStatus == option::ARG_OK)
            {
                    return option::ARG_OK;
            }
            else
            {
                argStatus = option::ARG_ILLEGAL; 
                if (msg)
                {
                    printError("Option '", option, "' requires a non-empty string\n");
                }
            }
        }

        return argStatus;
    }
};

enum  optionIndex { UNKNOWN, HELP, DESCR, INDEX };
const option::Descriptor usage[] =
{
    {UNKNOWN, 0, "", "", option::Arg::None, "USAGE: eio -d<string> -i<number>\n\n"
                                            "Options:" },
    {HELP, 0, "h", "help", option::Arg::None, "  --help  \tPrint usage and exit." },
    {DESCR, 0, "d", "description", Arg::Description, "  --description, -d  \tSpecify description string." },
    {INDEX, 0, "i", "index", Arg::Index, "  --index, -i  \tUnique identifier: 0, 1, 2, etc." },
    {RESET, 0, "r", "reset", option::Arg::None, "  --reset, -i  \tReset the device product string" },
    {UNKNOWN, 0, "", "",option::Arg::None, "\nExamples:\n"
                                "  eio -dPortConfig -i0\n"
                                "  eio --description GPIO --index 1\n" },
    {0,0,0,0,0,0}
};


int main(int argc, char* argv[]) 
{
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
        option::printUsage(std::cout, usage);
        return 1;
    }

    if (options[HELP] || argc == 0) {
        option::printUsage(std::cout, usage);
        return 0;
    }

    // check the options first to verify they both exist
    if (options[DESCR])
    {
        std::string descr(options[DESCR].arg);
        if (options[INDEX]) 
        {
            descr.append(options[INDEX].arg);
        }
        else
        {
            std::cout << "No index option, so assuming 0" << std::endl;
            descr.append("0");
        }

        std::cout << "Product string to be written to the flash: " << descr << std::endl;

        int vid = 0x0403, pid = 0x6011;
        int bus = 1, device = 6;
        
        ftdi_context* c_context = ftdi_new();

        std::cout << std::endl << "Opening device at USB bus addr: bus[" << bus << "]/device[" 
                << device << "]: ";
        if (!ftdi_usb_open_bus_addr(c_context, bus, device))
        {
            char manuf[80];
            char descript[80];
            char serialNo[80];
            ftdi_usb_get_strings2(c_context, libusb_get_device(c_context->usb_dev), manuf, 80, descript, 80, serialNo, 80);
            std::cout << manuf << ", "
                << descript << ", "
                << serialNo << std::endl;

            std::string descriptStr(descript);
            descriptStr.append("-" + descr);

            // get the eeprom size and create a buffer for it.
            // TODO: at this point, we know that the mini module has a 256 byte eeprom
            // change this if the production eeprom is different
            const int ebuf_size = 256; 
            unsigned char ebuf[ebuf_size];
            ftdi_set_eeprom_buf(c_context, ebuf, ebuf_size);

            // attempt to read the existing eeprom
            if (!ftdi_read_eeprom(c_context))
            {
                if (!ftdi_eeprom_decode(c_context, true))
                {
                    int chipSize = 0;
                    ftdi_get_eeprom_value(c_context, CHIP_SIZE, &chipSize);
                    if (chipSize == -1)
                    {
                        // eeprom is blank, so start from scratch
                        // initialize the defaults with the data read from the device and the new string to be appended
                        ftdi_eeprom_initdefaults(c_context, manuf, const_cast<char*>(descriptStr.c_str()), 
                                                serialNo);
                    }

                    else
                    {
                        // just update the description string
                        ftdi_eeprom_set_strings(c_context, 0, const_cast<char*>(descriptStr.c_str()), 0);
                    }
                }
            }

            // erase the device in order to get the chip type
            if (!ftdi_erase_eeprom(c_context))
            {
                // build the binary for burning to the 93c56 eeprom
                if (ftdi_eeprom_build(c_context))
                {
                    if (!ftdi_write_eeprom(c_context))
                    {
                        // read it back
                        if (!ftdi_read_eeprom(c_context))
                        {
                            // check the results
                            ftdi_eeprom_decode(c_context, true);
                        }
                    }
                }
            }
            ftdi_usb_close(c_context);
        }

        else
        {
            std::cout << std::endl << "Cannot open device at bus addr: bus[" << bus << "]/device[" 
                << device << "]" << std::endl;
        }
        ftdi_free(c_context);
    }
    else
    {
        std::cout << std::endl << "eio utility the description command line switch.\n" << std::endl;
        printUsage(std::cout, usage);
        return 3;
    }

    // all good, return 0
    return 0;
}
