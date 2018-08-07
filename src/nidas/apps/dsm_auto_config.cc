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
#include <list>


#include <nidas/util/auto_ptr.h>
#include <nidas/util/Logger.h>
#include <nidas/dynld/isff/PTB210.h>
#include <nidas/util/optionparser.h>
#include <nidas/core/Project.h>
#include <nidas/core/NidsIterators.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/XMLParser.h>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace nidas::util;

class AutoProject
{
public:
    AutoProject() { n_c::Project::getInstance(); }
    ~AutoProject() { n_c::Project::destroyInstance(); }
    n_c::Project& operator()() {return *n_c::Project::getInstance();}
};

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
                std::istringstream("abcd") >> arg;
                std::cout << "Numeric arg test gave: " << arg << std::endl;
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
        }

        return argStatus;
    }

    static option::ArgStatus Device(const option::Option& option, bool msg)
    {
        option::ArgStatus argStatus = Required(option, msg);
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

    static option::ArgStatus LogLevel(const option::Option& option, bool msg)
    {
        option::ArgStatus argStatus = Optional(option, msg);

        if (argStatus == option::ARG_OK) {
            argStatus = Numeric(option, msg);
            if ( argStatus == option::ARG_OK ) {
                try {
                    // this shouldn't fail since Numeric() succeeded...
                    int arg = -1;
                    std::istringstream(option.arg) >> arg;
                    if (1 <= arg && arg <=4) {
                        return option::ARG_OK;
                    }
                    else {
                        if (msg) printError("Option '", option, "' must be in the range of 1..4\n");
                        return option::ARG_ILLEGAL;
                    }
                }
                catch (std::exception e)
                {
                    if (msg) printError("Option '", option, "' requires a pure numeric argument, if one is supplied\n");
                    return option::ARG_ILLEGAL;
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
    {LOG, 0, "l", "log", Arg::LogLevel, "  --log, -l[1..4]  \tSet the log level to debug. If there is an argument, it must be attached: -l2." },
    {XML, 0, "x", "xml", Arg::Xml, "  --xml, -x  \tSpecify the xml file containing the sensor settings." },
    {SENSOR, 0, "s", "sensor", Arg::Sensor,   "  --sensor, -s  \tSpecify the sensor to be configured:\n" },
    {DEVICE, 0, "d", "dev", Arg::Device,   "  --device, -d  \tSpecify the device on which to perform auto" 
                                           " config activities. Must be the device to which the sensor is attached.\n" },
    {UNKNOWN, 0, "", "",option::Arg::None, "\nExamples:\n"
                                "  dsm_auto_config -d /dev/ttyUSB0\n"
                                "  dsm_auto_config -l -d /dev/ttyUSB0\n"
                                "  dsm_auto_config -l2 -d /dev/ttyUSB0\n" },
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

    std::string levelStr("level=");

    if (options[LOG]) {
        int arg = -1;

        try {
            std::istringstream(options[LOG].arg) >> arg;
        }
        catch (std::exception e) {}

        switch (arg) {
            case 1: 
                levelStr.append("notice");
                break;

            case 2: 
                levelStr.append("info");
                break;

            case 3: 
                levelStr.append("debug");
                break;

            case 4: 
                levelStr.append("verbose");
                break;

            default:
                levelStr.append("warning");
                break;
        }

    }

    if (levelStr == "level=") {
        levelStr.append("warning");
    }
    std::cout << "Log level string: " << levelStr << std::endl;

    logger = Logger::getInstance();
    scheme = logger->getScheme("autoconfig_default");
    LogConfig lc(levelStr.c_str());
    scheme.addConfig(lc);
    logger->setScheme(scheme);

    // xml config file use case
    if (options[XML]) {
        typedef std::list<SerialSensor*> SerialSensorList;

        AutoProject ap;
    	struct stat statbuf;
        std::string xmlFileName = options[XML].arg;
        SerialSensorList allSensors;

        if (::stat(xmlFileName.c_str(),&statbuf) == 0) {
            NLOG(("Found XML file: ") << xmlFileName);

            // auto_ptr<xercesc::DOMDocument> doc(ap().parseXMLConfigFile(xmlFileName));
            ap().parseXMLConfigFile(xmlFileName);

            NLOG(("Iterating through all the sensors specified in the XML file"));
            SensorIterator sensit = ap().getSensorIterator();
            while (sensit.hasNext() ) {
                SerialSensor* pSerialSensor = dynamic_cast<SerialSensor*>(sensit.next());
                if (!pSerialSensor) {
                    NLOG(("Can't auto config a non-serial sensor. Skipping..."));
                    continue;
                }
                pSerialSensor->open(O_RDWR);
                pSerialSensor->close();
            }
        }
    }

    else {
        if (options[SENSOR]) {
            std::string deviceStr;
            
            if (options[DEVICE]) {
                deviceStr = options[DEVICE].arg;
                NLOG(("Performing Auto Config on Device: ") << deviceStr);
            }
            else
            {
                std::cerr << "No device name specified. Cannot continue!!" << std::endl;
                return 100;
            }

            DOMObjectFactory sensorFactory;
            std::string sensorClass = options[SENSOR].arg;
            NLOG(("Using Sensor: ") << sensorClass);

            DOMable* domSensor = sensorFactory.createObject(sensorClass);
            if (!domSensor) {
                std::cerr << "Sensor creator object not found: " << sensorClass << std::endl;
                return 200;
            }

            SerialSensor*  pSerialSensor = dynamic_cast<SerialSensor*>(domSensor);
            if (!pSerialSensor) {
                std::cerr << "This utility only works with serial sensors, "
                             "particularly those which have an autoconfig capability" << std::endl;
                return 300;
            }

            NLOG(("Setting Device Name: ") << deviceStr);
            pSerialSensor->setDeviceName(deviceStr);
            NLOG(("Set device name: ") << pSerialSensor->getDeviceName());

            NLOG(("Opening serial sensor, where all the autoconfig magic happens!"));
            pSerialSensor->open(O_RDWR);

            NLOG(("All the fun there was to be had, has been had"));
            NLOG(("Close the device"));
            pSerialSensor->close();

            delete pSerialSensor;
        }
    }


        // There's only PTB210 for now, so let's just instantiate it, and see how it goes.
        // PTB210 ptb210;
        // ptb210.setDeviceName("/dev/ttyUSB0");
        // ptb210.open(O_RDWR);
        // ptb210.close();

    // all good, return 0
    return 0;
}
