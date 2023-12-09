// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
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

#include "UDPiPMSensor.h"

#include <nidas/util/UTime.h>
#include <nidas/util/Exception.h>
#include <nidas/util/Logger.h>

#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, UDPiPMSensor);

const int UDPiPMSensor::MAX_CHANNELS = 8;


UDPiPMSensor::UDPiPMSensor() :
    _deviceAddr(), _statusPort(0), _measureRate(0), _recordPeriod(0),
    _baudRate(0), _numAddr(0), _addrInfo(8), _ctrl_pid(0)
{

}

UDPiPMSensor::~UDPiPMSensor()
{
    close();
}

void UDPiPMSensor::validate()
{
    UDPSocketSensor::validate();

    const Parameter *p;
    p = getParameter("device"); // device
    if (!p) throw n_u::InvalidParameterException(getName(),
          "device", "not found");
    _deviceAddr = p->getStringValue(0);
    ILOG(("device is ") << _deviceAddr);

    p = getParameter("measurerate"); // STATUS & MEASURE collection rate (hz)
    if (!p) throw n_u::InvalidParameterException(getName(),
          "measurerate", "not found");
    _measureRate = (unsigned int)p->getNumericValue(0);
    ILOG(("measurerate is ") << _measureRate);

    p = getParameter("recordperiod"); // Period of RECORD queries (minutes)
    if (!p) throw n_u::InvalidParameterException(getName(),
          "recordperiod", "not found");
    _recordPeriod = (unsigned int)p->getNumericValue(0);
    ILOG(("recordperiod is ") << _recordPeriod);

    p = getParameter("baudrate"); // Baud rate
    // Baud rate is now optional since hardcoded to 115200
    if (p) {
        _baudRate = (unsigned int)p->getNumericValue(0);
        ILOG(("baudrate is ") << _baudRate);
    }

    p = getParameter("num_addr"); // Number of addresses being used
    if (!p) throw n_u::InvalidParameterException(getName(),
          "num_addr", "not found");
    _numAddr = (unsigned int)p->getNumericValue(0);
    ILOG(("") << _numAddr << (" addresses in use"));

    for (int i=0; i < _numAddr; i++)
    {
        p = getParameter("dev" + std::to_string(i));  // details of address i
        if (!p) throw n_u::InvalidParameterException(getName(),
              "dev", "not found");
        // addrInfo string contains addr, procquery, port,
        // eg 0,5,30101
        _addrInfo[i] = p->getStringValue(0);
        ILOG(("addr, query, port info for address ") << i << (": ") <<
             _addrInfo[i]);
    }

}

void UDPiPMSensor::open(int flags)
{
    UDPSocketSensor::open(flags);

    // Construct command for child process
    char *args[40];
    char port[32];
    char m_rate[32];
    char r_period[32];
    char baud[32];
    char num_addr[32];
    std::vector<std::string> addrnum(8);
    int argc = 0;
    char cmd[256] = "";
    int cmd_len = 0;

    args[argc++] = (char *)"ipm_ctrl";
    if (_deviceAddr.length() > 0) {
        args[argc++] = (char *)"-p";
        args[argc++] = (char *)_deviceAddr.c_str();
    }

    if (_measureRate > 0) {
        sprintf(m_rate, "%u", _measureRate);
        args[argc++] = (char *)"-m";
        args[argc++] = (char *)m_rate;
    }

    if (_recordPeriod > 0) {
        sprintf(r_period, "%u", _recordPeriod);
        args[argc++] = (char *)"-r";
        args[argc++] = (char *)r_period;
    }

    if (_baudRate > 0) {
        sprintf(baud, "%u", _baudRate);
        args[argc++] = (char *)"-b";
        args[argc++] = (char *)baud;
    }

    if (_numAddr > 0) {
        sprintf(num_addr, "%u", _numAddr);
        args[argc++] = (char *)"-n";
        args[argc++] = (char *)num_addr;
    }

    for (int i=0; i < _numAddr; i++)
    {
        addrnum[i] = "-" + std::to_string(i);
        args[argc++] = (char *)addrnum[i].c_str();
        args[argc++] = (char *)_addrInfo[i].c_str();
    }

    args[argc] = (char *)0;

    // Create a string containing the entire command for logging
    // purposes.
    for (int i=0; i < argc; i++)
    {
        cmd_len += strlen(args[i]) + 1;
        if (cmd_len < 255)  // prevent memory overrun (unlikely)
        {
            strcat(cmd, args[i]);
            strcat(cmd, " ");
        }
    }


    // Fork child process
    ILOG(("UDPiPMSensor: forking command ") << cmd);
    _ctrl_pid = fork();

    if (_ctrl_pid == -1)
    {
        PLOG(("UDPiPMSensor: error forking errno = ") << errno);
    }
    else
    if (_ctrl_pid == 0)
    {
        // The exec function only returns if an error has occurred and the
        // return value is always -1. errno is set to indicate the error
        if (execvp(args[0], args) == -1)
        {
            ELOG(("UDPiPMSensor: error executing command: ") << args[0] <<
            ": error " << errno << ": " << n_u::Exception::errnoToString(errno));
            exit(1);
        }
    }
}

void UDPiPMSensor::close()
{
    UDPSocketSensor::close();

    if (_ctrl_pid > 0)
    {
        if (kill(_ctrl_pid, SIGTERM) == -1)
        {
            ELOG(("UDPiPMSensor: kill() error: ") << ": error " << errno
            << " : " << n_u::Exception::errnoToString(errno));
        }

        if (waitpid(_ctrl_pid, 0, 0) == -1)
        {
            ELOG(("UDPiPMSensor: waitpid() error: ") << ": error " << errno
            << " : " << n_u::Exception::errnoToString(errno));
        }
    }
    _ctrl_pid = 0;
}

/* -------------------------------------------------------------------- */
