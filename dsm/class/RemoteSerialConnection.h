/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_REMOTESERIALCONNECTION_H
#define DSM_REMOTESERIALCONNECTION_H

#include <atdUtil/IOException.h>
#include <atdUtil/EOFException.h>
#include <SampleClient.h>
#include <DSMSensor.h>

namespace dsm {

class RemoteSerialConnection : public SampleClient {
public:
    RemoteSerialConnection(int f, const std::string& d) :
	fd(f),devname(d),port(0) {}
    virtual ~RemoteSerialConnection();

    int getFd() const { return fd; }
    const std::string& getSensorName() const { return devname; }

    void setPort(DSMSensor* val) {
	if (val) val->addSampleClient(this);
	else if (port) port->removeSampleClient(this);
	port = val;
    }

    DSMSensor* getPort() const { return port; }

    /**
     * Receive a sample from the DSMSensor, write data portion to fd.
     */
    bool receive(const Sample* s)
		throw(SampleParseException,atdUtil::IOException);

    /**
     * Read data from file descriptor, write to DSMSensor.
     */
    void read() throw(atdUtil::IOException);
  
private:
    int fd;
    std::string devname;
    DSMSensor* port;
};

}
#endif
