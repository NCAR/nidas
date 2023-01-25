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

#ifndef _nidas_dynld_raf_ipm_udp_h_
#define _nidas_dynld_raf_ipm_udp_h_

#include <nidas/dynld/UDPSocketSensor.h>
#include <nidas/core/Parameter.h>


namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * iPM over UDP, data received from the NAI iPM device.
 */
class UDPiPMSensor : public UDPSocketSensor
{

public:
    UDPiPMSensor();
    virtual ~UDPiPMSensor();

    virtual void validate();

    virtual void open(int flags);

    virtual void close();

protected:
    /// devicename of the NAI iPM device.
    std::string     _deviceAddr;

    /// Status port number for the alta_ctrl program.
    unsigned int    _statusPort;

    static const int MAX_CHANNELS;

private:
    //  PID for process that intializes and controls ENET unit.
    pid_t _ctrl_pid;

};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
