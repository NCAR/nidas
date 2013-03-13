// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_REMOTESERIALCONNECTION_H
#define NIDAS_CORE_REMOTESERIALCONNECTION_H

#include <nidas/util/Socket.h>
#include <nidas/core/SerialSensor.h>
#include <nidas/core/SampleClient.h>
#include <nidas/util/EOFException.h>

namespace nidas { namespace core {

class RemoteSerialConnection : public SampleClient {
public:

    RemoteSerialConnection(nidas::util::Socket* sock);

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

    void setDSMSensor(DSMSensor* val) throw(nidas::util::IOException);

    DSMSensor* getDSMSensor() const { return _charSensor; }

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
     * Read data from socket, write to DSMSensor.
     */
    void read() throw(nidas::util::IOException);

    /**
     * little utility for translating newlines to
     * carriage-return + newlines in a string.
     * @input   string of characters, altered in place.
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
     * @buf   input string
     * @return output string, with recognized escape sequences removed.
     */
    std::string doEscCmds(const std::string& inputstr) throw(nidas::util::IOException);
  
private:

    std::string _name;

    nidas::util::Socket* _socket;

    std::string _devname;

    CharacterSensor* _charSensor;

    SerialSensor * _serSensor;

    /**
     * Left over input characters after previous parse for escape sequences.
     */
    std::string _input;

    bool _nullTerminated;

    /** Copy not needed */
    RemoteSerialConnection(const RemoteSerialConnection&);

    /** Assignment not needed */
    RemoteSerialConnection& operator=(const RemoteSerialConnection&);

};

}}	// namespace nidas namespace core

#endif
