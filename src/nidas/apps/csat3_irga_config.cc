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
/* csat3_irga_config.cc

    The purpose of this program is to set the CSAT3 xxxx option to a value
    that is much less likely to result in dropped data.

    ASSUMPTIONS
    -----------
    1. Unprompted data is not present on EC100 USB port
    2.

    User supplies linux device to operate on. Program assumes that this is
    a USB virtual serial port, e.g. FTDI.

    Program opens serial port, adjusts the baud rate to 115200. Sends several
    <carriage return> characters. Looks for the EC100> prompt.

    Assuming it finds the prompt, the program then sends the TERM command and
    looks for the CSAT3 prompt. If it finds the CSAT3 prompt, then is sends
    the query command (??), and examines the output for the AA setting.

    If the AA setting is less than 50, then the program calculates the number
    of times to send the increment command (AA+) to reach the value of 50.
    It then sends the increment command the calculated number of times.

    Program checks the query command output to verify that the AA setting is
    at 50.
    
    Original Author: Paul Ourada

*/

#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>
#include "nidas/core/NidasApp.h"
#include "nidas/util/SerialPort.h"
#include "nidas/util/Termios.h"

using namespace nidas::core;
using namespace nidas::util;

NidasApp app("csat_irga_config");

NidasAppArg Device("-d,--device", "</dev/ttyUSBx>",
                   "Linux device path.", "");
NidasAppArg Info("-i,--info", "",
                   "Respond with the output of the EC100 \'D\' command and exit.", "");
NidasAppArg Bandwidth("-b,--bandwidth", "[5|10|12.5|20|25|Open]",
                      "Set the measurement bandwidth in hertz.", "10");
NidasAppArg Rate("-r,--rate", "[10|20|50|Open]",
                      "Set the sample rate in hertz. Should be 2X the bandwidth.", "20");

std::string explanatoryText = "  This utility configures the EC100 and CSAT3 sensors.\n"
                              "  The computer running this utility must be connected to the EC100 USB port.\n"
                              "  Please ensure that this connection is made before proceeding.";

nidas::util::SerialPort serPort;

typedef std::map<std::string, std::string> MapType;
MapType bwMap;
MapType rateMap;

std::map<std::string, std::string> createBwMap()
{
    MapType m;
    m.insert(MapType::value_type("5", "0"));
    m.insert(MapType::value_type("10", "1"));
    m.insert(MapType::value_type("12.5", "2"));
    m.insert(MapType::value_type("20", "3"));
    m.insert(MapType::value_type("25", "4"));
    m.insert(MapType::value_type("Open", "5"));

    return m;
}

std::map<std::string, std::string> createRateMap()
{
    MapType m;
    m.insert(MapType::value_type("10", "0"));
    m.insert(MapType::value_type("20", "1"));
    m.insert(MapType::value_type("25", "2"));
    m.insert(MapType::value_type("50", "3"));
    m.insert(MapType::value_type("Open", "4"));

    return m;
}

int usage(const char* argv0)
{
    std::cerr << "\
Usage: " << argv0 << "-d /dev/ttyUSB[0-n]" << std::endl << std::endl
         << explanatoryText << std::endl << std::endl
         << app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Version | app.Help
    		            | Device | Info | Bandwidth | Rate);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested() || !Device.specified())
    {
        return usage(argv[0]);
    }
    return 0;
}

size_t readAll(char* buf, const size_t bufSize, int msTimeout)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(serPort.getFd(), &readfds);
    int nfds = serPort.getFd() + 1;

    struct timeval timeout;
    timeout.tv_sec = msTimeout/1000;
    timeout.tv_usec = (msTimeout%1000)*1000;

    fd_set fds = readfds;

    // wait for data
    int nfd = select(nfds,&fds,0,0,(msTimeout > 0 ? &timeout : 0));

    if (nfd < 0) {
        if (errno == EINTR) return -1;
//        throw n_u::IOException(serPort.getName(),"select",errno);
    }

    if (nfd == 0) {
        ILOG((serPort.getName().c_str()) << ": timeout " << msTimeout << " mS");
        return -1;
    }

    int bufRemaining = bufSize;
    size_t charsRead = serPort.read(buf, bufRemaining);
    buf += charsRead;
    bufRemaining -= charsRead;
    while (charsRead > 0 && bufRemaining ) {
        nfd = select(nfds,&fds,0,0,(msTimeout > 0 ? &timeout : 0));
        if (nfd > 0) {
            charsRead = serPort.read(buf, bufRemaining);
            buf += charsRead;
            bufRemaining -= charsRead;
        }
        else
            break;
    }

    return bufSize-bufRemaining;
}

bool promptFound(const char* prompt, int numTries)
{
    static std::string response;
    static const int BUF_SIZE = 256;
    static char buf[BUF_SIZE];

    bool retval = false;
    int tries = 1;
    ILOG(("Looking for ") << prompt);

    do {
        DLOG(("Try: ") << tries++);
        // Send wakeup command several times...
        serPort.write("\r\n", 1);

        memset(buf, 0, BUF_SIZE);
        response = "";
        std::size_t numChars = readAll(buf, BUF_SIZE, 1000);
        if (numChars > 0) {
            response.append(buf);

            DLOG(("Response for this try: ") << response);

            size_t promptIdx = response.find(prompt);
            if (promptIdx != std::string::npos) {
                retval = true;
                break;
            }
        }
    } while (--numTries);

    return retval;
}

void exitCSATTerm()
{
    // Get the heck out...
    serPort.write("quit", 4);
    if (!promptFound("EC100>", 5)) {
        ILOG(("Couldn't find the EC100> prompt."));
        exit(-8);
    }
    ILOG(("Found the EC100> prompt after exiting CSAT3 terminal mode."));
}

int main(int argc, char* argv[]) {

    if (parseRunString(argc, argv))
        exit(1);

    std::cerr  << std::endl << explanatoryText << std::endl << std::endl << "Type any character to continue...";
    (void)getchar();
    std::cerr << std::endl << std::endl;

    bwMap = createBwMap();
    rateMap = createRateMap();

    std::string devName;
    if (Device.specified()) {
        devName = Device.getValue();

        // Open device for rw
        serPort.setName(devName);
        serPort.open(O_RDWR);

        // Set baud rate of serial device
        nidas::util::Termios serTermios = serPort.getTermios();
        if (!serTermios.setBaudRate(115200)) {
            DLOG(("Couldn't set the serial port baud rate to 115200, the normal EC100 baud rate."));
            usage(argv[0]);
            return 3;
        }
    }

    else 
    {
        ILOG(("Must supply a device option on the command line."));
        usage(argv[0]);
        return 1;
    }

    // Always have to find the EC100 prompt first, before anything else.
    if (promptFound("EC100>", 5)) {
        ILOG(("Found the EC100> prompt."));
    }
    else {
        ILOG(("EC100> prompt not found!"));
        return 4;
    }

    const int SETTINGS_BUF_SIZE = 512;
    char settingsBuf[SETTINGS_BUF_SIZE];
    memset(settingsBuf, 0, SETTINGS_BUF_SIZE);
    std::string response = "";

    // Check if the user only wants the sensor information to be dumped
    if (Info.specified()) {
        serPort.write("D\r", 2);
        if (readAll(settingsBuf, SETTINGS_BUF_SIZE, 100)) {
            std::cerr << std::endl << settingsBuf << std::endl;
            return 0;
        }
    }

    // Put code to update the EC100 settings here...
    if (Bandwidth.specified()) {
        ILOG(("Modifying bandwidth to: ") << Bandwidth.getValue());

        std::string cmd = "N\r";
        serPort.write(cmd.c_str(), cmd.length());

        char bwBuf[256];
        memset(bwBuf, 0, 256);
        readAll(bwBuf, 256, 100);
        DLOG(("Response to bandwidth menu request, N<cr>:\n") << bwBuf);
        cmd = "1\r";
        serPort.write(cmd.c_str(), cmd.length());

        memset(bwBuf, 0, 256);
        readAll(bwBuf, 256, 100);
        DLOG(("Response to bandwidth menu request, 1<cr>:\n") << bwBuf);

        MapType::iterator mIter = bwMap.find(Bandwidth.getValue());
        if (mIter == bwMap.end()) {
            CLOG(("Illegal bandwidth value: ") << Bandwidth.getValue());
            return 5;
        }

        std::string newBW = mIter->second;
        newBW.append("\r");
        DLOG(("Bandwidth command line argument parameter: ") << newBW);
        serPort.write(newBW.c_str(), newBW.length());
        memset(bwBuf, 0, 256);
        readAll(bwBuf, 256, 100);
        DLOG(("Response to bandwidth modification:\n") << bwBuf);

        // now save it...
        serPort.write("Y\r", 2);
        memset(bwBuf, 0, 256);
        readAll(bwBuf, 256, 100);
        DLOG(("Response to rate save operation:\n") << bwBuf);
    }

    // Get the EC100> prompt back...
    if (promptFound("EC100>", 5)) {
        ILOG(("Found the EC100> prompt."));
    }
    else {
        ILOG(("EC100> prompt not found!"));
        return 4;
    }

    if (Rate.specified()) {
        ILOG(("Modifying unattended update rate to: ") << Rate.getValue());

        std::string cmd = "N\r";
        serPort.write(cmd.c_str(), cmd.length());

        char rateBuf[256];
        memset(rateBuf, 0, 256);
        readAll(rateBuf, 256, 100);
        DLOG(("Response to rate menu request, N<cr>:\n") << rateBuf);
        cmd = "3\r";
        serPort.write(cmd.c_str(), cmd.length());

        memset(rateBuf, 0, 256);
        readAll(rateBuf, 256, 100);
        DLOG(("Response to rate menu request, 3<cr>:\n") << rateBuf);

        MapType::iterator mIter = rateMap.find(Rate.getValue());
        if (mIter == rateMap.end()) {
            CLOG(("Illegal rate value: ") << Rate.getValue());
            return 6;
        }

        std::string newRate = mIter->second;
        newRate.append("\r");
        DLOG(("Rate command line argument parameter: ") << newRate);
        serPort.write(newRate.c_str(), newRate.length());
        memset(rateBuf, 0, 256);
        readAll(rateBuf, 256, 100);
        DLOG(("Response to rate modification:\n") << rateBuf);

        // now save it...
        serPort.write("Y\r", 2);
        memset(rateBuf, 0, 256);
        readAll(rateBuf, 256, 100);
        DLOG(("Response to rate save operation:\n") << rateBuf);
    }

    // Get the EC100> prompt back...
    if (promptFound("EC100>", 5)) {
        ILOG(("Found the EC100> prompt."));
    }
    else {
        ILOG(("EC100> prompt not found!"));
        return 4;
    }

    ILOG(("Always check the CSAT AA setting..."));
    DLOG(("Send TERM command to access CSAT3"));
    serPort.write("TERM\r", 5);
    if (promptFound("CSAT>", 3)) {
        ILOG(("Found the CSAT> prompt."));
    }
    else {
        ILOG(("CSAT> prompt not found!"));
        return 4;
    }

    // Send CSAT query command
    serPort.write("??\r", 3);

    // Check whether AA setting needs to be modified
    memset(settingsBuf, 0, SETTINGS_BUF_SIZE);
    response = "";
    readAll(settingsBuf, SETTINGS_BUF_SIZE, 100);
    response.append(settingsBuf);
    DLOG(("Response to CSAT query: ") << settingsBuf);

    size_t matchIdx = response.find("AA=");
    if (matchIdx == std::string::npos) {
        ILOG(("Couldn't find the AA setting."));
        exitCSATTerm();
        return 6;
    }

    char* aaStart = settingsBuf+matchIdx+3;
    const int AA_BUF_SIZE = 5;
    char aaBuf[AA_BUF_SIZE];
    memset(aaBuf, 0, AA_BUF_SIZE);

    for (int i=0; isdigit(aaStart[i]) && i<3; ++i) {
        aaBuf[i] = aaStart[i];
    }

    int aa = atoi(aaStart);

    ILOG(("Found the AA setting: ") << aa);

    // Modify AA setting
    int numAdj = (50 - aa)/5;       // adjustments in increments of 5.
    std::string aaAdjStr = "AA";
    if (numAdj > 0) {
        aaAdjStr.append("+\r");
    }
    else if (numAdj < 0) {
        aaAdjStr.append("-\r");
    }
    else {
        ILOG(("No adustment needed."));
        exitCSATTerm();
        return 0;
    }

    DLOG(("Command to adjust AA setting: ") << aaAdjStr);

    for (int i = abs(numAdj); i > 0; --i) {
        serPort.write(aaAdjStr.c_str(), aaAdjStr.length());
        memset(settingsBuf, 0, SETTINGS_BUF_SIZE);
        readAll(settingsBuf, SETTINGS_BUF_SIZE, 100);
        DLOG((settingsBuf));
    }

    // Check new AA setting by sending the query command again
    serPort.write("??\r", 3);

    // Check whether AA setting needs to be modified
    memset(settingsBuf, 0, SETTINGS_BUF_SIZE);
    response = "";
    readAll(settingsBuf, SETTINGS_BUF_SIZE, 100);
    response.append(settingsBuf);
    matchIdx = response.find("AA=");
    if (!matchIdx) {
        ILOG(("Couldn't find the AA setting for adjustment check."));
        exitCSATTerm();
        return 6;
    }

    aaStart = settingsBuf+matchIdx+3;
    memset(aaBuf, 0, AA_BUF_SIZE);

    for (int i=0; isdigit(aaStart[i]) && i<3; ++i) {
        aaBuf[i] = aaStart[i];
    }

    aa = atoi(aaStart);

    if (aa == 50) {
        ILOG(("Success: AA setting: ") << aa);
    }
    else {
        ILOG(("Failure: AA setting != 50: ") << aa);
        exitCSATTerm();
        return 7;
    }

    // Get the heck out...
    exitCSATTerm();
    // all good, return 0
    return 0;
}
