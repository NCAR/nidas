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
#include <nidas/core/Project.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/NidasApp.h>

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


NidasAppArg Device("-d,--device", "i.e. /dev/ttyDSMx",
        "Serial device to use when running autoconfig on a sensor.\n"
        "Only specify this if -x/--xml is not used", "");

NidasAppArg Sensor("-s,--sensor", "i.e. isff.PTB210",
        "Sensor on which autoconfig will be performed.\n"
        "Only specify this if -x/--xml is not used", "");

static NidasApp app("dsm_auto_config");

int usage(const char* argv0)
{
    std::cerr << "\
Usage: " << argv0
         << " [options] [-h | -d | -s | -x | -l]" << std::endl << std::endl
         << app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.XmlHeaderFile | app.loggingArgs() |
                        app.Version | app.Help | Device | Sensor);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested() || argc < 2)
    {
        return usage(argv[0]);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (parseRunString(argc, argv))
        exit(1);

    // xml config file use case
    std::string xmlFileName = app.xmlHeaderFile();
    if (xmlFileName.length() != 0) {
        AutoProject ap;
    	struct stat statbuf;

        if (::stat(xmlFileName.c_str(),&statbuf) == 0) {
            NLOG(("Found XML file: ") << xmlFileName);

            ap().parseXMLConfigFile(xmlFileName);
            DSMConfigIterator di = ap().getDSMConfigIterator();
            while (di.hasNext()) {
            	DSMConfig* pDsm = const_cast<DSMConfig*>(di.next());
            	(*pDsm).validate();
            }

            NLOG(("Iterating through all the sensors specified in the XML file"));
            SensorIterator sensit = ap().getSensorIterator();
            while (sensit.hasNext() ) {
                SerialSensor* pSerialSensor = dynamic_cast<SerialSensor*>(sensit.next());
                if (!pSerialSensor) {
                    NLOG(("Can't auto config a non-serial sensor. Skipping..."));
                    continue;
                }
                pSerialSensor->init();
                pSerialSensor->open(O_RDWR);
                pSerialSensor->close();
            }
        }
    }

    else {
        std::string sensorClass = Sensor.getValue();
        if (sensorClass.length() != 0) {
            NLOG(("Using Sensor: ") << sensorClass);

            std::string deviceStr = Device.getValue();
            
            if (deviceStr.length() != 0) {
                NLOG(("Performing Auto Config on Device: ") << deviceStr);
            }
            else
            {
                std::cerr << "No device name specified. Cannot continue!!" << std::endl;
                return 100;
            }

            DOMObjectFactory sensorFactory;

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

            NLOG(("Printing sensor power State..."));
            pSerialSensor->updatePowerState();
            pSerialSensor->printPowerState();

            NLOG(("Setting sensor to support AutoConfig..."));
            pSerialSensor->setAutoConfigSupported();

            NLOG(("Opening serial sensor, where all the autoconfig magic happens!"));
            pSerialSensor->open(O_RDWR);

            NLOG(("All the fun there was to be had, has been had"));
            NLOG(("Close the device"));
            pSerialSensor->close();

            delete pSerialSensor;
        }

        else {
            std::cerr << "Must supply the sensor class name if not using DSM config file!!" << std::endl;
            return usage(argv[0]);
        }
    }

    // all good, return 0
    return 0;
}
