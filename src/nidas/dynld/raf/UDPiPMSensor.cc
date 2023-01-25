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
    _deviceAddr(), _statusPort(0), _ctrl_pid(0)
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
    ILOG(("device is %s", _deviceAddr.c_str()));

    p = getParameter("status_port"); // Port for IPM
    if (!p) throw n_u::InvalidParameterException(getName(),
          "status_port", "not found");
    _statusPort = (unsigned int)p->getNumericValue(0);
    ILOG(("status_port is %u", _statusPort));
}

void UDPiPMSensor::open(int flags)
{
    ILOG(("*******In UDPiPMSensor::open *****************************"));
    UDPSocketSensor::open(flags);

    _ctrl_pid = fork();

    if (_ctrl_pid == -1)
    {
        PLOG(("UDPiPMSensor: error forking errno = %d", errno));
    }
    else
    if (_ctrl_pid == 0)
    {
        char *args[20], port[32];
        int argc = 0;

        args[argc++] = (char *)"ipm_ctrl";
        if (_deviceAddr.length() > 0) {
            args[argc++] = (char *)"--device";
            args[argc++] = (char *)_deviceAddr.c_str();
        }

        /*if (_statusPort > 0) {
            sprintf(port, "%u", _statusPort);
            args[argc++] = (char *)"--port";
            args[argc++] = (char *)port;
        } */

        args[argc] = (char *)0;
        ILOG(("forking command %s", args[0], args));
        execvp(args[0], args);
    }
}

void UDPiPMSensor::close()
{
    UDPSocketSensor::close();

    if (_ctrl_pid > 0)
    {
        int rc = kill(_ctrl_pid, SIGTERM);
        wait(&rc);
    }
    _ctrl_pid = 0;
}

/* -------------------------------------------------------------------- */
