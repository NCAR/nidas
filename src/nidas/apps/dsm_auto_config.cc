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
/* dsm_auto_config.cc

    User space code that uses dedicated sensor classes and the FT4243H GPIO device in 
    the DSM to programmatically configure the UART attached a specified port. In particular
    dsm_auto_config attempts to use the default configuration + any configuration items 
    specified in the sensor XML configuration. If it is unable to communicate using those 
    parameters, it then sweeps the serial port parameters in an attempt to find what parameters
    actually work. Once found, dsm_auto_config then attempts to modify the sensor to match 
    the desired configuration. After that, it configures the sensor's science parameters, 
    again, according to the default + XML overrides, if any. It then examines the output of 
    the sensor to determine whether it is behaving as expected.

    When all efforts are exhausted, or successful, dsm_auto_config displays the success/fail 
    state. In the case of failure, it displays the parameter combinations it tried. In the 
    case of success, it displays the parameters which succeeded.

    Original Author: Paul Ourada

*/

#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>

#include <nidas/util/Logger.h>
#include <nidas/dynld/isff/PTB210.h>
#include <nidas/util/optionparser.h>

namespace n_d_s = nidas::dynld::isff;
namespace n_u = nidas::util;

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
    static option::ArgStatus Xml(const option::Option& option, bool msg)
    {
        option::ArgStatus argStatus = NonEmpty( option, msg);
        if ( argStatus == option::ARG_OK ) 
        {
            struct stat fileMeta;
            if (stat(option.arg, &fileMeta) < 0)
            {
                argStatus = option::ARG_ILLEGAL;
                if (msg) printError("Option '", option, "' Could not stat XML file.\n");
            }
        }

        return argStatus;
    }
    static option::ArgStatus Sensor(const option::Option& option, bool msg)
    {
        option::ArgStatus argStatus = Required( option, msg);
        if ( argStatus == option::ARG_OK ) 
        {
            argStatus = NonEmpty( option, msg);
            if ( argStatus == option::ARG_OK ) 
            {
                std::string sensorStr(option.arg);
                std::transform(sensorStr.begin(), sensorStr.end(), sensorStr.begin(), ::toupper);
                if (sensorStr != "PTB210") 
                {
                    argStatus = option::ARG_ILLEGAL;
                    if (msg) printError("Option '", option, "' Device not supported.\n");
                }
            }
        }

        return argStatus;
    }
    static option::ArgStatus Device(const option::Option& option, bool msg)
    {
        option::ArgStatus argStatus = Required( option, msg);
        if ( argStatus == option::ARG_OK ) 
        {
            argStatus = NonEmpty(option, msg);
            if (argStatus == option::ARG_OK)
            {
                struct stat fileMeta;
                if (stat(option.arg, &fileMeta) < 0)
                {
                    argStatus = option::ARG_ILLEGAL;
                    if (msg) printError("Option '", option, "' Could not stat device file.\n");
                }
            }
        }

        return argStatus;
    }
};

enum  optionIndex { UNKNOWN, HELP, LOG, XML, SENSOR, DEVICE };
const option::Descriptor usage[] =
{
    {UNKNOWN, 0, "", "", option::Arg::None, "USAGE: dsm_auto_config [options]\n\n"
                                            "Options:" },
    {HELP, 0, "h", "help", option::Arg::None, "  --help  \tPrint usage and exit." },
    {LOG, 0, "l", "log", Arg::None, "  --log, -l  \tSet the log level to debug" },
    {XML, 0, "x", "xml", Arg::Xml, "  --xml, -x  \tSpecify the xml file containing the sensor settings." },
    {SENSOR, 0, "s", "sensor", Arg::Sensor,   "  --sensor, -s  \tSpecify the sensor to be configured:\n" },
    {DEVICE, 0, "d", "dev", Arg::Device,   "  --device, -d  \tSpecify the device on which to perform auto" 
                                           " config activities. Must be the device to which the sensor is attached.\n" },
    {UNKNOWN, 0, "", "",option::Arg::None, "\nExamples:\n"
                                "  dsm_auto_config -p0 -tRS232\n"
                                "  dsm_auto_config -p3 -tRS422\n"
                                "  dsm_auto_config -p6 -tRS485_HALF\n"},
    {0,0,0,0,0,0}
};


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

    n_u::Logger* logger = 0;
    n_u::LogScheme scheme;

    // Set a default for notifications and up.
    logger = Logger::getInstance();
    scheme = logger->getScheme("sing_default");
    scheme.addConfig(LogConfig("level=info"));
    logger->setScheme(scheme);

    // There's only PTB210 for now, so let's just instantiate it, and see how it goes.
    n_d_s::PTB210 ptb210;

    if (options[DEVICE]) {
        std::string deviceStr(options[DEVICE].arg);
        std::cout << "Device: " << deviceStr << std::endl;
        ptb210.setDeviceName(deviceStr);
        std::cout << "Set device name: " << ptb210.getDeviceName() << std::endl;
    }

    if (ptb210.getName().empty())
    {
        std::cout << "No device name specified. Cannot continue!!" << std::endl;
        exit(100);
    }

    ptb210.open(O_RDWR);

    // all good, return 0
    return 0;
}
