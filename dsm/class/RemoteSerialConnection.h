/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_REMOTESERIALCONNECTION_H
#define DSM_REMOTESERIALCONNECTION_H

#include <atdUtil/Socket.h>
#include <DSMSerialSensor.h>
#include <SampleClient.h>
#include <atdUtil/EOFException.h>

namespace dsm {

class RemoteSerialConnection : public SampleClient {
public:

    RemoteSerialConnection(atdUtil::Socket* sock, const std::string& d);

    virtual ~RemoteSerialConnection();

    int getFd() const { return socket->getFd(); }
    const std::string& getSensorName() const { return devname; }

    void setDSMSensor(DSMSensor* val) throw(atdUtil::IOException);

    DSMSensor* getDSMSensor() const { return sensor; }

    /**
     * Receive a sample from the DSMSensor, write data portion to socket.
     */
    bool receive(const Sample* s) throw();

    /**
     * Read data from socket, write to DSMSensor.
     */
    void read() throw(atdUtil::IOException);

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
    std::string doEscCmds(const std::string& inputstr) throw(atdUtil::IOException);
  
private:
    atdUtil::Socket* socket;

    std::string devname;

    DSMSerialSensor* sensor;

    /**
     * Left over input characters after previous parse for escape sequences.
     */
    std::string input;
};

}
#endif
