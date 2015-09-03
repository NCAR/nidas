// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_UDPSOCKETSENSOR_H_
#define NIDAS_DYNLD_UDPSOCKETSENSOR_H

#include <nidas/core/CharacterSensor.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * Sensor class using a UDPSocketIODevice for reading from a UDP socket.
 * The device name should have a format like "usock::5555", where 5555 is the port 
 * number on the DSM that the instrumenation is sending packets to.
 * See CharacterSensor::buildIODevice() for questions about the format of
 * the device name. The host address portion of the device name usually empty,
 * so that the socket will be bound to INADDR_ANY, i.e. all local interfaces.
 * The socket is enabled to accept broadcast packets.
 *
 * This class is actually unnecessary, one can use DSMSerialSensor instead.
 * The only difference between the two is that with UDPSocketSensor, an
 * UDPSocketIODevice and DatagramSampleScanner are created whether the
 * device name prefix is "inet:", "sock:" or "usock:", whereas with
 * DSMSerialSensor the device name prefix must be "usock:" to create a
 * UDPSocketIODevice and DatagramSampleScanner, since "inet:" or "sock:"
 * result in a TCPSocketIODevice and MessageStreamScanner.
 *
 * The samples are scanned with DatagramSampleScanner. Since datagrams
 * are packetized by the networking layer, no message separators
 * are required in the data.
 *
 * Otherwise, this is a CharacterSensor, with support for sscanf-ing
 * of the datagram contents.
 */
class UDPSocketSensor: public CharacterSensor
{

public:

    UDPSocketSensor();

    virtual ~UDPSocketSensor() { }

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

private:

};

}}	// namespace nidas namespace dynld

#endif
