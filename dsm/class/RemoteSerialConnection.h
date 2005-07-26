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
    RemoteSerialConnection(atdUtil::Socket* sock, const std::string& d) :
	socket(sock),devname(d),sensor(0) {}
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
  
private:
    atdUtil::Socket* socket;
    std::string devname;
    DSMSerialSensor* sensor;
};

}
#endif
