// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_REMOTESERIALCONNECTION_H
#define NIDAS_CORE_REMOTESERIALCONNECTION_H

#include <nidas/util/Socket.h>
#include "SerialSensor.h"
#include "SampleClient.h"
#include <nidas/util/EOFException.h>

#include "Polled.h"

namespace nidas { namespace core {

class SensorHandler;

class RemoteSerialConnection : public SampleClient, public Polled
{
public:

    RemoteSerialConnection(nidas::util::Socket* sock,SensorHandler* handler);

    ~RemoteSerialConnection();

    /**
     * Implementation of SampleClient::flush().
     */
    void flush() throw() {}

    const std::string& getName() const { return _name; }

    void setName(const std::string& val) { _name = val; }

    void close() throw(nidas::util::IOException);

    void readSensorName() throw(nidas::util::IOException);

    int getFd() const { return _socket->getFd(); }

    const std::string& getSensorName() const { return _devname; }

    void setSensor(CharacterSensor* val) throw(nidas::util::IOException);

    DSMSensor* getDSMSensor() const { return _sensor; }

    /** 
     * Notify this RemoteSerialConnection that a sensor
     * matching getSensorName() was not found.
     */
    void sensorNotFound() throw(nidas::util::IOException);

    /**
     * Receive a sample from the DSMSensor, write data portion to socket.
     */
    bool receive(const Sample* s) throw();

    /**
     * An epoll event occurred, most likely it is time to read data
     * from socket, write to DSMSensor.
     * @return: read consumed all available data from the socket.
     */
    bool handlePollEvents(uint32_t events) throw();

    /**
     * little utility for translating newlines to
     * carriage-return + newlines in a string.
     * @param input   string of characters, altered in place.
     */
    void nlTocrnl(std::string& input);

    /**
     * Parse and execute ESC commands in user input. Return
     * buffer with escape sequences removed.
     * ESC-p	toggle prompting
     * ESC-bxxxx set baud rate to xxxx (non-digit after xxxx)
     * ESC-peven set parity to even
     * ESC-podd set parity to odd
     * ESC-ESC send ESC to sensor
     * @param inputstr input string
     * @return output string, with recognized escape sequences removed.
     */
    std::string doEscCmds(const std::string& inputstr) throw(nidas::util::IOException);
  
private:

    std::string _name;

    nidas::util::Socket* _socket;

    std::string _devname;

    CharacterSensor* _sensor;

    SerialSensor * _serSensor;

    /**
     * Left over input characters after previous parse for escape sequences.
     */
    std::string _input;

    bool _nullTerminated;

    SensorHandler* _handler;

    /**
     * Has the user been sent a "sensor is closed" warning.
     */
    bool _closedWarning;

    /**
     * Sensor timeout when the RemoteSerialConnection was established.
     */
    int _timeoutMsecs;

    /** Copy not needed */
    RemoteSerialConnection(const RemoteSerialConnection&);

    /** Assignment not needed */
    RemoteSerialConnection& operator=(const RemoteSerialConnection&);

};

}}	// namespace nidas namespace core

#endif
