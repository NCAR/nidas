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

#include <boost/filesystem.hpp>
using namespace boost::filesystem;

#include <nidas/util/Logger.h>
#include <nidas/dynld/isff/NCAR_TRH.h>
#include <nidas/core/Project.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/NidasApp.h>
#include <nidas/core/Metadata.h>

#include <sstream>

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

NidasAppArg MetadataReport("-m,--meta", "", "Report the sensor metadata and return", "");

static NidasApp app("trh_load_cal");

int usage()
{
    std::cerr << R""""(
Usage: trh_load_cal [options] {-h} | {-d <device> {-m|-c <calfile>}}

Load TRH calibration coefficients to a specific TRH ID attached
to a serial port or all TRHs attached to a DSM.
Accomplished by either specifying a calibration project XML file,
or by specifying a device and a coefficients file.

)"""";
    std::cerr << app.usage();
    std::cerr << R""""(
Examples:

Read then print sensor metadata:
  trh_load_cal -d /dev/ttyDSM[0-7] -m

Update a single device specified by serial port and calibration file:

  trh_load_cal -d /dev/ttyDSM[0-7] -c <path/to/cal_file>
)"""";

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(Device | CoeffFile | MetadataReport |
                        app.loggingArgs() | app.Version | app.Help);

    try {
        ArgVector args = app.parseArgs(argc, argv);
    }
    catch(NidasAppException& e) {
        std::cerr << e.what() << std::endl;
        return usage();
    }

    if (app.helpRequested() || argc < 1)
    {
        return usage();
    }
    return 0;
}


using mstream = std::ostringstream;


/**
 * Log the steps and results when updating calibrations.
 * 
 * Ultimately this could be an event stream, with different methods to create
 * and record different events, with timestamps, status codes, error messages,
 * and metadata.
 */
class CalibrationLog
{
public:
    CalibrationLog(const std::string& filename=""):
        _filename(filename),
        _logfile()
    {
    }

    bool
    open(const std::string& calFileName)
    {
        string logFile = calFileName + ".log";
        _logfile.open(logFile, std::ofstream::trunc|std::ofstream::out);
        if (!_logfile.is_open())
        {
            ELOG(("") << "Failed to open: " << logFile);
            return false;
        }
        ok(mstream() << "Opened calibration log for calfile "
                     << calFileName);
        return true;
    }

    void
    ok(const std::ostringstream& buf)
    {
        ok(buf.str());
    }

    void
    ok(const std::string& message)
    {
        _logfile << "OK: " << message << std::endl;
        NLOG(("") << message);
    }

    void
    fail(const std::ostringstream& buf)
    {
        fail(buf.str());
    }

    void
    fail(const std::string& message)
    {
        _logfile << "FAILED: " << message << std::endl;
        ELOG(("") << message);
    }

    void
    close()
    {
        _logfile.flush();
        _logfile.close();
    }

private:
    std::string _filename;
    std::ofstream _logfile;
};


std::ifstream calFile;
std::ofstream snsrMetaData;
NCAR_TRH*  pTRHSensor = 0;

CalibrationLog calFileLog;


int shutdown(int code)
{
    NLOG(("All the fun there was to be had, has been had"));
    NLOG(("Close everything..."));
    calFile.close();
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
    calFileLog.ok("Putting TRH into EEPROM Menu");
    if (pTRHSensor->enterMenuMode()) {
        retval = true;
    }
    else {
        calFileLog.fail("Failed to put the device into EEPROM Menu!!");
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

    calFileLog.ok(mstream() << "Got first string: " << coeffCmdStr);

    while (!coeffCmdStr.empty()) {

        calFileLog.ok(mstream() << "Writing coeff command: " << coeffCmdStr);
        coeffCmdStr.append("\n");

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
        calFileLog.ok(mstream() << "Command Response: " << std::string(respbuf));
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

        if (!calFileLog.open(calFileName))
        {
            return retval;
        }
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

#ifdef notdef
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
                UTime when(0l);
                when.from_iso(dateStr, true);
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
#endif

void outputMetaData(const string& when)
{
    SensorMetadata md;

    md.timestamp = UTime();
    md.set("applied", when);

    pTRHSensor->getMetadata(md);
    std::cout << md.to_buffer(2) << std::endl;
    // may as well include a copy in the log.
    calFileLog.ok(mstream() << md.to_buffer(0));
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
        // None of this currently works and has not been used in the lab, so
        // defer porting it until (if) it's needed.  Really if the other case
        // works, where no xml is needed, the program just probes all the
        // ports looking for TRH devices, then that's the simpler and more
        // robust approach anyway.
#ifdef notdef
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
                        Metadata& md = *pTRHSensor->getMetadata();
                        if (md.manufacturer == "NCAR" && md.model == "TRH")
                        {
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
                                    if (calFileName.length() != 0 && openCalFile(calFileName))
                                    {
                                        std::ostringstream buf;
                                        buf << "Found cal file: " << calFileName << " for TRH-" << md.serial_number;
                                        NLOG(("") << buf.str());
                                        calFileLog << buf.str();
                                        outputMetaData("before");
                                        sendCmds();
                                        updateMetaData();
                                        outputMetaData("after");

                                    }
                                    else {
                                        NLOG(("Couldn't find a calibration file for TRH ID: ") 
                                            << md.serial_number << " - skipping...");
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
                                << md.manufacturer 
                                << "\n\tmodel: " << md.model);
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
            return usage();
        }
#endif
    }
    else {
        // cal command file use case
        if (Device.specified()) {
            std::string deviceStr = Device.getValue();
            
            if (deviceStr.length() != 0) {
                calFileLog.ok(mstream() << "Performing Auto Config and TRH Cal Load on Device: " << deviceStr);
            }
            else
            {
                calFileLog.fail(mstream() << "No device name specified. Cannot continue!!");
                shutdown(100);
            }

            DOMObjectFactory sensorFactory;
            std::string sensorClass = "isff.NCAR_TRH";
            calFileLog.ok(mstream() << "Using Sensor: " << sensorClass);
            DOMable* domSensor = sensorFactory.createObject(sensorClass);
            if (!domSensor) {
                calFileLog.fail(mstream() << "Sensor creator object not found: " << sensorClass);
                shutdown(200);
            }

            pTRHSensor = dynamic_cast<NCAR_TRH*>(domSensor);
            if (!pTRHSensor) {
                calFileLog.fail(mstream() << 
                    "This utility only works with SerialSensor subclasses, "
                    "particularly those which have an autoconfig capability."
                    "This sensor is not of the NCAR_TRH sensor class.");
                shutdown(300);
            }

            calFileLog.ok(mstream() << "Setting Device Name: " << deviceStr);
            pTRHSensor->setDeviceName(deviceStr);
            calFileLog.ok(mstream() << "Set Device Name: " << pTRHSensor->getDeviceName());
            pTRHSensor->setAutoConfigEnabled();

            calFileLog.ok(mstream()
                << "Opening TRH sensor where port configuration occurs and power is turned on...");
            pTRHSensor->open(O_RDWR);

            if (MetadataReport.specified()) {
                outputMetaData("no");
                calFileLog.ok(mstream() << "Completed retrieving sensor metadata.");
                shutdown(0);
            }
            else if (CoeffFile.specified()) {
                std::string calFileName = CoeffFile.getValue();

                if (openCalFile(calFileName)) {
                    outputMetaData("before");
                    sendCmds();
                    updateMetaData();
                    outputMetaData("after");
                    calFileLog.ok(mstream() << "Completed updating sensor coefficients.");
                    shutdown(0);
                }
            }
        }
    }

    // all good, return 0
    shutdown(0);
}
