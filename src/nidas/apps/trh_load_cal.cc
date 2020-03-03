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

NidasAppArg TrhCalFile("-f,--file", "i.e. TRH_SN_123456_20200203.cal",
                       "File containing the calibration coefficients ",
                       "which should be in command form."
                       "", true);

static NidasApp app("trh_load_cal");

int usage(const char* argv0)
{
    std::cerr << "\
Usage: " << argv0
         << " [options] [-h | -d | -f | -l]" << std::endl << std::endl
         << app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Version | app.Help | 
                        Device | TrhCalFile);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested() || argc < 2)
    {
        return usage(argv[0]);
    }
    return 0;
}

std::ifstream calFile;
std::ofstream calFileLog;
NCAR_TRH*  pTRHSensor = 0;

int shutdown(int code)
{
    NLOG(("All the fun there was to be had, has been had"));
    NLOG(("Close the device"));
    calFile.close();
    calFileLog.flush();
    calFileLog.close();

    if (pTRHSensor) {
        pTRHSensor->pwrOff();
        pTRHSensor->close();
        delete pTRHSensor;
    }

    exit(code);
}

int main(int argc, char* argv[]) {
    if (parseRunString(argc, argv))
        exit(1);

    // cal command file use case
    std::string calFileName = TrhCalFile.getValue();

    if (calFileName.length() != 0) {
        AutoProject ap;
    	struct stat statbuf;

        if (::stat(calFileName.c_str(), &statbuf) == 0) {
            std::string calFileFound = 
                "Found cal file: " + calFileName;

            calFileLog = std::ofstream(calFileName + ".log", std::ofstream::trunc|std::ofstream::out);
            if (!calFileLog.is_open()) {
                std::string calFileLogOpenFail = 
                    "Failed to open " + calFileName + ".log on ofstream!!!";
                ELOG(("") << calFileLogOpenFail);
                shutdown(20);
            }

            std::string loggingToStr = "Logging to: " + calFileName + ".log";
            NLOG(("") << loggingToStr);
            calFileLog << loggingToStr << std::endl;
            NLOG(("") << calFileFound);
            calFileLog << calFileFound << std::endl;

            calFile = std::ifstream(calFileName);
            if (!calFile.is_open()) {
                std::string calFileOpenFail = 
                    "Failed to open " + calFileName + " on ifstream!!!";
                ELOG(("") << calFileOpenFail);
                calFileLog << calFileOpenFail << std::endl;
                shutdown(10);
            }

            std::string calFileOpenStr = "Successfully opened: " + calFileName;
            NLOG(("") << calFileOpenStr);
            calFileLog << calFileOpenStr << std::endl;
        }
        else {
            std::string calFileNotFound = 
                "Failed to find cal file: " + calFileName;
            ELOG(("") << calFileNotFound);
            std::cerr << calFileNotFound << std::endl;
            shutdown(30);
        }
    }

    std::string sensorClass = "isff.NCAR_TRH";
    std::string usingSensor = 
        "Using Sensor: " + sensorClass;
    NLOG(("") << usingSensor);
    calFileLog << usingSensor << std::endl;

    std::string deviceStr = Device.getValue();
    
    if (deviceStr.length() != 0) {
        std::string usingDevice = 
            "Performing Auto Config and TRH Cal Load on Device: " + deviceStr;
        NLOG(("") << usingDevice);
        calFileLog << usingDevice << std::endl;
    }
    else
    {
        std::string noDeviceFail = 
            "No device name specified. Cannot continue!!";
        ELOG(("") << noDeviceFail);
        calFileLog << noDeviceFail << std::endl;
        shutdown(100);
    }

    DOMObjectFactory sensorFactory;

    DOMable* domSensor = sensorFactory.createObject(sensorClass);
    if (!domSensor) {
        std::string sensorFactoryFail = 
            "Sensor creator object not found: " + sensorClass;
        ELOG(("") << sensorFactoryFail);
        calFileLog << sensorFactoryFail << std::endl;
        shutdown(200);
    }

    pTRHSensor = dynamic_cast<NCAR_TRH*>(domSensor);
    if (!pTRHSensor) {
        std::string trhSensorCastFail = 
            "This utility only works with SerialSensor subclasses, "
            "particularly those which have an autoconfig capability";
        ELOG(("") << trhSensorCastFail);
        calFileLog << trhSensorCastFail << std::endl;
        shutdown(300);
    }

    // if (pTRHSensor->supportsAutoConfig()) {
    //     pTRHSensor->setAutoConfigEnabled();
    // }

    std::string settingDeviceName = 
        "Setting Device Name: " + deviceStr;
    NLOG(("") << settingDeviceName);
    calFileLog << settingDeviceName << std::endl;
    pTRHSensor->setDeviceName(deviceStr);
    std::string setDeviceName = 
        "Set Device Name: " + pTRHSensor->getDeviceName();
    NLOG(("") << setDeviceName);
    calFileLog << setDeviceName << std::endl;

    pTRHSensor->pwrOff();

    std::string openingSensor = 
        "Opening TRH sensor where port configuration occurs and power is turned on...";
    NLOG(("") << openingSensor);
    calFileLog << openingSensor << std::endl;
    pTRHSensor->open(O_RDWR);

    pTRHSensor->drainResponse();

    std::string enteringEEPROMMenu = 
        "Putting TRH into EEPROM Menu";
    NLOG(("") << enteringEEPROMMenu);
    calFileLog << enteringEEPROMMenu << std::endl;
    if (pTRHSensor->sendAndCheckSensorCmd(ENTER_EEPROM_MENU_CMD)) {
        NLOG(("Sending calibration coefficient commands..."));
        std::string coeffCmdStr;
        std::getline(calFile, coeffCmdStr);
        std::string firstString = 
            "Got first string: " +  coeffCmdStr + "\n";
        NLOG(("") << firstString);
        calFileLog << firstString;

        while (!coeffCmdStr.empty()) {
            coeffCmdStr.append("\n");
            std::string writingCoeff = 
                "Writing coeff command: " +  coeffCmdStr;
            NLOG(("") << writingCoeff);
            calFileLog << writingCoeff;
            size_t numWritten = pTRHSensor->writePause(coeffCmdStr.c_str(), coeffCmdStr.length(), 100);
            if (numWritten != coeffCmdStr.length()) {
                std::string writeCoeffFail = 
                    "Failed to properly write cal coeff command: " + coeffCmdStr;
                ELOG(("") << writeCoeffFail);
            }
            pTRHSensor->flush();

            char respbuf[256];
            std::memset(respbuf, 0, 256);
            pTRHSensor->readEntireResponse(respbuf,255, 500);
            std::string respStr("Command Response: ");
            respStr.append(respbuf);
            NLOG(("") << respStr);
            calFileLog << respStr << std::endl;
            
            std::getline(calFile, coeffCmdStr);
        }
    }
    else {
        std::string eepromMenuFail("Failed to put the device into EEPROM Menu!!");
        ELOG(("") << eepromMenuFail);
        calFileLog << eepromMenuFail << std::endl;
        shutdown(400);
    }

    // all good, return 0
    shutdown(0);
}
