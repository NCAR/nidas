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
/* trh_load_cal.cc

    User space code that uses isff/NCAR_TRH class to open a port, and 
    then read calibration coefficient commands out of a file, and 
    send them to a NCAR_TRH instrument over the opened port.

    Original Author: Paul Ourada

*/

#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <list>
#include <cstdio>


#include <nidas/util/auto_ptr.h>
#include <nidas/util/Logger.h>
#include <nidas/dynld/isff/NCAR_TRH.h>
#include <nidas/core/Project.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/NidasApp.h>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace nidas::util;

class AutoProject
{
public:
    AutoProject() { nidas::core::Project::getInstance(); }
    ~AutoProject() { nidas::core::Project::destroyInstance(); }
    nidas::core::Project& operator()() {return *nidas::core::Project::getInstance();}
};


NidasAppArg Device("-d,--device", "i.e. /dev/ttyDSMx",
                   "Serial device to use when loading calibration "
                   "coefficients onto an NCAR_TRH sensor.", "", true);

NidasAppArg Sensor("-s,--sensor", "i.e. isff.NCAR_TRH",
                   "Sensor on which autoconfig will be performed.\n", 
                   "isff.NCAR_TRH");

NidasAppArg TrhCalFile("-f,--file", "i.e. TRH_SN_123456_20200203.cal",
                       "File containing the calibration coefficients ",
                       "", true);

static NidasApp app("trh_load_cal");

int usage(const char* argv0)
{
    std::cerr << "\
Usage: " << argv0
         << " [options] [-h | -d | -s | -f | -l]" << std::endl << std::endl
         << app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Version | app.Help | 
                        Device | Sensor | TrhCalFile);

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

    // cal command file use case
    std::string calFileName = TrhCalFile.getValue();
    std::ifstream calFile;
    std::string line;

    if (calFileName.length() != 0) {
        AutoProject ap;
    	struct stat statbuf;

        if (::stat(calFileName.c_str(),&statbuf) == 0) {
            NLOG(("Found cal file: ") << calFileName);
            calFile = std::ifstream(calFileName);
            if (!calFile) {
                ELOG(("Failed to open ") << calFileName.c_str() << " on ifstream!!!");
                return 10;
            }
        }
        else {
            ELOG(("Failed to find cal file: ") << calFileName);
            return -20;
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

            if (pSerialSensor->supportsAutoConfig()) {
                NLOG(("Setting sensor to enable AutoConfig..."));
                pSerialSensor->setAutoConfigEnabled();
            }

            else {
                // set the message parameters just for the purposes of this test...
                pSerialSensor->setMessageParameters(1, std::string("\n"), true);
            }

            NLOG(("Opening serial sensor, where all the autoconfig magic happens!"));
            pSerialSensor->open(O_RDWR);

            NLOG(("Sending calibration coefficient commands..."));
            std::string coeffCmd;
            while (getline(calFile, line)) {
                size_t numWritten = pSerialSensor->write(line.c_str(), line.length());
                if (numWritten != line.length()) {
                    ELOG(("Failed to properly write cal coeff command: ") << line );
                }
            }

            NLOG(("All the fun there was to be had, has been had"));
            NLOG(("Close the device"));
            calFile.close();
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
