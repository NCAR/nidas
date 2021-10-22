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
#include <regex>
#include <chrono>
#include <cstdlib>
#include <json/json.h>

#if __cplusplus >= 201703L
    #include <filesystem>
    using namespace std::filesystem
#else
    #include <boost/filesystem.hpp>
    using namespace boost::filesystem;
#endif

#include <nidas/util/auto_ptr.h>
#include <nidas/util/Logger.h>
#include <nidas/dynld/isff/NCAR_TRH.h>
#include <nidas/core/Project.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/NidasApp.h>

using namespace nidas::core;
using namespace nidas::dynld::isff;
using namespace nidas::util;

using std::regex;
using std::cmatch;
using std::cout;
using std::string;


static const char* CAL_DATE_REGEX_SPEC = 
    // "-(([[digit:]]{4}(-[[:digit:]]{2}){2})T[[:digit:]]{2}(_[[:digit:]]{2}){2}(\\.[[:digit:]]{1,6}){0,1})-";
    // "-(([0-9]{4}(-[0-9]{2}){2})T[0-9]{2}(_[0-9]{2}){2}(\\.[0-9]]{1,6}){0,1})-";
    "-(([0-9]{4}(-[0-9]{2}){2})T([0-9]{2}(_[0-9]{2}){2}).*)-";
static regex iso_date_regex(CAL_DATE_REGEX_SPEC,  std::regex_constants::extended);

NidasAppArg Device("-d,--device", "i.e. /dev/ttyDSMx",
                   "Serial device to use when loading calibration "
                   "coefficients onto an NCAR_TRH sensor.", "", true);

NidasAppArg CoeffFile("-c,--coeffs", "i.e. TRH_SN_123456_20200203.cal",
                      "File containing the calibration coefficients ",
                      "which should be in command form."
                      "", true);

NidasAppArg Metadata("-m,--meta", "", "Report the sensor metadata and return", "");

static NidasApp app("trh_load_cal");

int usage(const char* argv0)
{
    std::cerr << "\
Usage: " << argv0
         << " [options -h  | -l | [-d -m] | [-d -c] | [-x -H]]" << std::endl 
         << std::endl
         << " Loads TRH calibration coefficients to a specific TRH ID attached " << std::endl
         << " to a serial port or all TRHs attached to a DSM. " << std::endl
         << " Accomplished by either specifying a calibration project XML file, " << std::endl
         << " or by specifying a device and a coefficients file." << std::endl 
         << std::endl
         << " Print help/usage:" << std::endl
         << "     $trh_load_cal" << std::endl
         << "     $trh_load_cal -h" << std::endl 
         << std::endl
         << " Output metadata and exit:" << std::endl
         << "     $trh_load_cal -d /dev/ttyDSM[0-7] -m" << std::endl
         << std::endl
         << " Update all devices attached to DSM specified by XML project file" << std::endl
         << " and a calibration type specification - e.g. trh, ptb210, etc." << std::endl
         << " Calibration files are found in $HOME/projects/isfs/cal_files/<sensor type>." << std::endl
         << "     $trh_load_cal -f <xml file> -c <cal type>" << std::endl
         << std::endl
         << " Update a single device specified by serial port and calibration file:" << std::endl
         << "     $trh_load_cal -d /dev/ttyDSM[0-7] -c <path/to/cal_file>" << std::endl
         << std::endl
         << app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.XmlHeaderFile | app.loggingArgs() | app.Version 
                        | app.Hostname | app.Help | Device | CoeffFile | Metadata);

    try {
        ArgVector args = app.parseArgs(argc, argv);
    }
    catch(NidasAppException& e) {
        cout << std::endl;
        ELOG(("") << e.what());
        cout << std::endl;
        return usage(argv[0]);
    }

    if (app.helpRequested() || argc < 1)
    {
        return usage(argv[0]);
    }
    return 0;
}

std::ifstream calFile;
std::ofstream calFileLog;
std::ofstream snsrMetaData;
NCAR_TRH*  pTRHSensor = 0;

int shutdown(int code)
{
    NLOG(("All the fun there was to be had, has been had"));
    NLOG(("Close everything..."));
    calFile.close();
    calFileLog.flush();
    calFileLog.close();

    if (pTRHSensor) {
        // pTRHSensor->pwrOff();
        pTRHSensor->close();
        delete pTRHSensor;
    }

    exit(code);
}

bool enterMenu()
{
    pTRHSensor->drainResponse();

    bool retval = false;
    std::string enteringEEPROMMenu = 
        "Putting TRH into EEPROM Menu";
    NLOG(("") << enteringEEPROMMenu);
    calFileLog << enteringEEPROMMenu << std::endl;
    if (pTRHSensor->enterMenuMode()) {
        retval = true;
    }
    else {
        std::string eepromMenuFail("Failed to put the device into EEPROM Menu!!");
        ELOG(("") << eepromMenuFail);
        calFileLog << eepromMenuFail << std::endl;
    }

    return retval;
}

void sendCmds()
{
    // try to break into the TRH EEPROM menu...
    for (int i=0; i<3; ++i) {
        if (enterMenu()) {
            break;
        }
    }

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

bool openCalFile(const string& calFileName)
{
    bool retval = false;
    struct stat statbuf;
    std::string calFileFound;

    if (::stat(calFileName.c_str(), &statbuf) == 0) {
        calFileFound = "Found cal file: " + calFileName;
        DLOG(("") << calFileFound);
        calFile = std::ifstream(calFileName);
        if (!calFile.is_open()) {
            std::string calFileOpenFail = 
                "Failed to open " + calFileName + " on ifstream!!!";
            ELOG(("") << calFileOpenFail);
            return retval;
        }

        std::string calFileOpenStr = "Successfully opened: " + calFileName;
        DLOG(("") << calFileOpenStr);

        string logFile = calFileName + ".log";
        calFileLog = std::ofstream(logFile, std::ofstream::trunc|std::ofstream::out);
        if (!calFileLog.is_open()) {
            std::string calFileLogOpenFail = 
                "Failed to open " + logFile + " on ofstream!!!";
            ELOG(("") << calFileLogOpenFail);
            return retval;
        }

        calFileLog << calFileFound << std::endl;
        calFileLog << calFileOpenStr << std::endl;
        DLOG(("Cal log file: ") << logFile);
        calFileLog << "Cal log file: " << logFile << std::endl;
        retval = true;
    }
    else {
        std::string calFileNotFound = 
            "Failed to find cal file: " + calFileName;
        ELOG(("") << calFileNotFound);
        std::cerr << calFileNotFound << std::endl;
        return retval;
    }

    return retval;
}

std::time_t parseISO8601(const std::string& input)
{
    constexpr const size_t expectedLength = sizeof("1234-12-12T12:12:12Z") - 1;
    static_assert(expectedLength == 20, "Unexpected ISO 8601 date/time length");

    if (input.length() < expectedLength)
    {
        return 0;
    }

    std::tm time;
    time.tm_year = std::strtol(&input[0], nullptr, 10) - 1900;
    time.tm_mon =  std::strtol(&input[5], nullptr, 10) - 1;
    time.tm_mday = std::strtol(&input[8], nullptr, 10);
    time.tm_hour = std::strtol(&input[11], nullptr, 10);
    time.tm_min =  std::strtol(&input[14], nullptr, 10);
    time.tm_sec =  std::strtol(&input[17], nullptr, 10);
    time.tm_isdst = 0;
    const int millis = input.length() > 20 ? std::strtol(&input[20], nullptr, 10) : 0;
    return timegm(&time) * 1000 + millis;
}

const string findLatestCal(const string& dirPath, const string& searchStr)
{
    string latestCalFileName;
    int64_t timeToLatestCalFile = 0;
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

    for (const auto & entry : directory_iterator(dirPath)) {
        string fileName(entry.path().generic_string());
        DLOG(("Checking file name: ") << fileName);
        if (fileName.find(searchStr) != string::npos) {
            DLOG(("File name matches: ") << searchStr << " - checking date...");
            // find date of cal from timestamp in name
            cmatch results;
            bool regexFound = std::regex_search(fileName.c_str(), 
                                                results, iso_date_regex);
            if (regexFound && results[1].matched) {
                DLOG(("Found date match in file name: ") << fileName << " - convert '_' to ':'");
                string dateStr = results[1].str();
                DLOG(("Found date string: ") << dateStr);
                size_t n = 0;
                while((n = dateStr.find_first_of('_')) != string::npos) {
                    dateStr.replace(n, 1, ":");
                }
                DLOG(("Converted date string: ") << dateStr);
                // convert to date object and compare time to now.
                std::chrono::system_clock::time_point caldate
                    = std::chrono::system_clock::from_time_t(parseISO8601(dateStr));
                
                // minimum time wins.
                if (timeToLatestCalFile == 0) {
                    DLOG(("First cal file found, so it wins unless another file is closer to now."));
                    timeToLatestCalFile 
                        = std::chrono::duration_cast<std::chrono::milliseconds>(caldate.time_since_epoch()).count();
                    latestCalFileName = entry.path().generic_string();
                }

                else {
                    DLOG(("Another cal file found, checking if it's newer..."));
                    auto calDateDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - caldate);
                    if (calDateDiff.count() < timeToLatestCalFile) {
                        DLOG(("Cal file is newer - saving off for next check."));
                        timeToLatestCalFile = calDateDiff.count();
                        latestCalFileName = entry.path().generic_string();
                    }
                    else {
                        DLOG(("Cal file is not newer."));
                    }
                }
            }
            else {
                if (regexFound) {
                    DLOG(("Found overall regex match"));
                    if (!results[1].matched) {
                        DLOG(("Couldn't find date string match"));
                    }
                }
                else {
                    DLOG(("Didn't find overall regex match"));
                }
            }
        }
    }

    if (latestCalFileName.length() == 0 || timeToLatestCalFile == 0) {
        ELOG(("Failed to find a calibration file matching ") << searchStr);
    }

    return latestCalFileName;
}

void outputMetaData(const string& when)
{
    Json::Value json_root;
    SensorManufacturerMetaData manufMeta = pTRHSensor->getSensorManufMetaData();
    SensorConfigMetaData configMeta = pTRHSensor->getSensorConfigMetaData();

    json_root["applied"] = when;
    json_root["manufacturer"] = manufMeta.manufacturer;
    json_root["model"] = manufMeta.model;
    json_root["serialNum"] = manufMeta.serialNum;
    json_root["hwVersion"] = manufMeta.hwVersion;
    json_root["manufactureDate"] = manufMeta.manufactureDate;
    json_root["fwVersion"] = manufMeta.fwVersion;
    json_root["fwBuild"] = manufMeta.fwBuild;
    json_root["calDate"] = manufMeta.calDate;

    for (unsigned int i=0; i<configMeta.customMetaData.size(); ++i ) {
        MetaDataItem rItem = configMeta.customMetaData[i];
        json_root[rItem.first] = rItem.second;
    }

    std::cout << json_root << std::endl;
}

void updateMetaData()
{
    if (pTRHSensor) {
        pTRHSensor->updateMetaData();
    }
    else {
        NLOG(("") << "Can't update metadata. No sensor is currently open.");
    }
}

int main(int argc, char* argv[]) 
{
    if (parseRunString(argc, argv))
        exit(1);

    // xml config file use case
    if (app.XmlHeaderFile.specified()) {
        if (app.Hostname.specified()) {
            std::string xmlFileName = app.XmlHeaderFile.getValue();
            std::string hostName = app.Hostname.getValue();
            NLOG(("Specififed calibration host: ") << hostName);
            if (xmlFileName.length() != 0) {

                // force host name to trh, if improperly specified
                // not strictly needed here, but a reminder for checking 
                // in a generic sensor cal loader
                transform(hostName.begin(), hostName.end(), hostName.begin(), ::tolower);
                if (hostName.length() == 0 || strncmp(hostName.c_str(), "trh", 3)) {
                    NLOG(("Found host name garbage: ") << hostName << " - Forcing it to \"trh\"");
                    hostName = "trh";
                }

                if (exists(path(xmlFileName))) {
                    NLOG(("Found XML file: ") << xmlFileName);

                    // Gotta set these environment vars...
                    setenv("WIND3D_HORIZ_ROTATION", "0", 0);
                    setenv("WIND3D_TILT_CORRECTION", "0", 0);

                    app.getProject()->parseXMLConfigFile(xmlFileName);
                    DSMConfig* pDSMConfig = app.getProject()->findDSMFromHostname(hostName);
                    if (!pDSMConfig) {
                        ELOG(("Couldn't find DSM: ") << hostName << " in project XML file: " << hostName);
                        shutdown(600);
                    }
                    pDSMConfig->validate();

                    NLOG(("Iterating through all the sensors specified in the XML file"));
                    SensorIterator sensor_iter = pDSMConfig->getSensorIterator();
                    while (sensor_iter.hasNext() ) {
                        pTRHSensor = dynamic_cast<NCAR_TRH*>(sensor_iter.next());
                        if (!pTRHSensor) {
                            NLOG(("Only care about TRH sensors. Skipping..."));
                            continue;
                        }
                        pTRHSensor->init();
                        // TODO - Remove this for production, but leave the open() call!!!
                        if (pTRHSensor->getDeviceName() == "/dev/ttyDSM0") {
                            pTRHSensor->open(O_RDWR); // port should still be open after this...
                        }
                        else{
                            NLOG(("Skipping ports that I know don't have anything in them."));
                            continue;
                        }
                        // TODO - Remove the above for production!!!

                        // get sensor ID here
                        nidas::core::SensorManufacturerMetaData meta = pTRHSensor->getSensorManufMetaData();
                        if (meta.manufacturer.compare("NCAR") == 0 
                            && meta.model.compare("TRH") == 0) {
                            // find cal file
                            std::string cal_files_dir = getenv("HOME") + string("/isfs/cal_files/trh");
                            if (!exists(cal_files_dir)) {
                                DLOG(("Creating cal files direcotry: ") << cal_files_dir);
                                create_directories(cal_files_dir);
                                NLOG(("Had to create the cal files directory, so there are no cal files to search for. Exiting..."));
                                shutdown(700);
                            }
                            else
                            {
                                if (is_directory(cal_files_dir)) {
                                    DLOG(("Cal file directory already exists: ") << cal_files_dir);
                            
                                    string searchTRHId = "UCAR_TRH-" + string("120"); //meta.serialNum;
                                    string calFileName = findLatestCal(cal_files_dir, searchTRHId);
                                    if (calFileName.length() != 0 && openCalFile(calFileName)) {
                                        string foundCalFileMsg = "Found cal file: " + calFileName + " for TRH-" + meta.serialNum;
                                        NLOG(("") << foundCalFileMsg);
                                        calFileLog << foundCalFileMsg;
                                        outputMetaData("before");
                                        sendCmds();
                                        updateMetaData();
                                        outputMetaData("after");

                                    }
                                    else {
                                        NLOG(("Couldn't find a calibration file for TRH ID: ") 
                                            << meta.serialNum << " - skipping...");
                                    }
                                }
                                else {
                                    string cal_file_dir_error = "Couldn't find the isfs/cal_files directory!!";
                                    ELOG(("") << cal_file_dir_error);
                                    shutdown(400);
                                }
                            }
                        }

                        else {
                            NLOG(("Was looking for TRH sensor meta data, but found: \n\tManuf: ") 
                                << meta.manufacturer 
                                << "\n\tmodel: " << meta.model);
                        }

                        pTRHSensor->close();
                    }

                    NLOG(("") << "Completed updating sensor coefficients");
                    shutdown(0);
                }
                else {
                    ELOG(("") << "Could not find project file: " << xmlFileName);
                    shutdown(500);
                }
            }
        }
        else {
            string badCmdLineArgs = "If specifying an XML file, you must "
                                    "also specify the calibration host.";
            cout << std::endl;
            ELOG(("") << badCmdLineArgs);
            cout << std::endl;
            return usage(argv[0]);
        }
    }
    else {
        // cal command file use case
        if (Device.specified()) {
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
            std::string sensorClass = "isff.NCAR_TRH";
            std::string usingSensor = 
                "Using Sensor: " + sensorClass;
            NLOG(("") << usingSensor);
            calFileLog << usingSensor << std::endl;
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
                    "particularly those which have an autoconfig capability."
                    "This sensor is not of the NCAR_TRH sensor class.";
                ELOG(("") << trhSensorCastFail);
                calFileLog << trhSensorCastFail << std::endl;
                shutdown(300);
            }

            std::string settingDeviceName = 
                "Setting Device Name: " + deviceStr;
            DLOG(("") << settingDeviceName);
            calFileLog << settingDeviceName << std::endl;
            pTRHSensor->setDeviceName(deviceStr);
            std::string setDeviceName = 
                "Set Device Name: " + pTRHSensor->getDeviceName();
            DLOG(("") << setDeviceName);
            calFileLog << setDeviceName << std::endl;

            // There should not be any need to turn off the sensor before
            // turning it back on when it is opened.
            //
            // pTRHSensor->pwrOff();
            pTRHSensor->setAutoConfigEnabled();

            std::string openingSensor = 
                "Opening TRH sensor where port configuration occurs and power is turned on...";
            DLOG(("") << openingSensor);
            calFileLog << openingSensor << std::endl;
            pTRHSensor->open(O_RDWR);

            if (Metadata.specified()) {
                outputMetaData("no");
                NLOG(("") << "Completed retrieving sensor metadata.");
                shutdown(0);
            }
            else {
                if (CoeffFile.specified()) {
                    std::string calFileName = CoeffFile.getValue();

                    if (openCalFile(calFileName)) {
                        outputMetaData("before");
                        sendCmds();
                        updateMetaData();
                        outputMetaData("after");
                        NLOG(("") << "Completed updating sensor coefficients");
                        shutdown(0);
                    }
                }
                else {

                }
            }
        }
        else {

        }
    }

    // all good, return 0
    shutdown(0);
}
