// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef NIDAS_DYNLD_UDPSOCKETSENSOR_H_
#define NIDAS_DYNLD_UDPSOCKETSENSOR_H

#include <nidas/core/CharacterSensor.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * Sensor class using a UDPSocketIODevice for reading from a UDP socket.
 * The device name should have a format like "inet::5555", where 5555 is the port 
 * number. See SocketIODevice::open() for questions about the format of
 * the device name. The host address portion of the device name usually empty,
 * so that the socket will be bound to INADDR_ANY, i.e. all local interfaces.
 * The socket is enabled to accept broadcast packets.
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
