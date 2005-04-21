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

#include <atdUtil/IOException.h>
#include <atdUtil/EOFException.h>
#include <atdUtil/Socket.h>
#include <SampleClient.h>
#include <DSMSensor.h>

namespace dsm {

class RemoteSerialConnection : public SampleClient {
public:
    RemoteSerialConnection(atdUtil::Socket* sock, const std::string& d) :
	socket(sock),devname(d),sensor(0) {}
    virtual ~RemoteSerialConnection();

    int getFd() const { return socket->getFd(); }
    const std::string& getSensorName() const { return devname; }

    void setSensor(DSMSensor* val) {
	if (val) val->addSampleClient(this);
	else if (sensor) sensor->removeSampleClient(this);
	sensor = val;
    }

    DSMSensor* getPort() const { return sensor; }

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
    DSMSensor* sensor;
};

}
#endif
