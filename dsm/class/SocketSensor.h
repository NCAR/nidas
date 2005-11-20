/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/
#ifndef SOCKETSENSOR_H
#define SOCKETSENSOR_H

#include <DSMSensor.h>
#include <MessageStreamSensor.h>
#include <Sample.h>

#include <atdUtil/Socket.h>
#include <atdUtil/ParseException.h>

namespace dsm {

/**
 * A sensor connected through a socket.
 */
class SocketSensor : public DSMSensor, public MessageStreamSensor {

public:

    /**
     * Create a SocketSensor.  No IO operations to the sensor
     * are performed in the constructor (hence no IOExceptions).
     */
    SocketSensor();

    virtual ~SocketSensor();

    /**
     * The file descriptor used when reading from this SocketSensor.
     */
    int getReadFd() const
    {
	if (socket.get()) return socket->getFd();
	return -1;
    }

    /**
     * The file descriptor used when writing to this sensor.
     */
    int getWriteFd() const {
	if (socket.get()) return socket->getFd();
    	return -1;
    }

    /**
    * open the socket.
    */
    void open(int flags)
    	throw(atdUtil::IOException,atdUtil::InvalidParameterException);

    void init() throw(atdUtil::InvalidParameterException);

    /**
    * Read from the sensor.
    */
    size_t read(void *buf, size_t len) throw(atdUtil::IOException)
    {
        return socket->recv(buf,len);
    }

    /**
    * Write to the sensor.
    */
    size_t write(const void *buf, size_t len) throw(atdUtil::IOException) 
    {
        return socket->send(buf,len);
    }

    /*
    * Perform an ioctl on the device. Not necessary for a socket,
    * and will throw an IOException.
    */
    void ioctl(int request, void* buf, size_t len) throw(atdUtil::IOException)
    {
        throw atdUtil::IOException(getDeviceName(),
		"ioctl","not supported on SocketSensor");
    }

    void ioctl(int request, const void* buf, size_t len)
    	throw(atdUtil::IOException)
    {
        throw atdUtil::IOException(getDeviceName(),
		"ioctl","not supported on SocketSensor");
    }

    /**
    * close the sensor (and any associated FIFOs).
    */
    void close() throw(atdUtil::IOException);

    dsm_time_t readSamples(SampleDater* dater)
    	throw(atdUtil::IOException);

    void parseAddress(const std::string& name) throw(atdUtil::ParseException);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    /**
     * The type of the destination address, AF_INET or AF_UNIX.
     */
    int addrtype;	

    std::string desthost;

    int destport;

    /**
     * Port number that is parsed from sensor name.
     */
    std::auto_ptr<atdUtil::SocketAddress> sockAddr;

    std::auto_ptr<atdUtil::Socket> socket;

};

}
#endif
