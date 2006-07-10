/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-01-20 09:58:25 -0700 (Fri, 20 Jan 2006) $

    $LastChangedRevision: 3245 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/SocketIODevice.h $

*/
#ifndef NIDAS_CORE_SOCKETIODEVICE_H
#define NIDAS_CORE_SOCKETIODEVICE_H

#include <nidas/core/IODevice.h>
#include <nidas/util/Socket.h>
#include <nidas/util/ParseException.h>

namespace nidas { namespace core {

/**
 * A sensor connected through a socket.
 */
class SocketIODevice : public IODevice {

public:

    /**
     * Create a SocketIODevice.  No IO operations to the sensor
     * are performed in the constructor (hence no IOExceptions).
     */
    SocketIODevice();

    virtual ~SocketIODevice();

    /**
     * The file descriptor used when reading from this SocketIODevice.
     */
    int getReadFd() const
    {
	if (socket) return socket->getFd();
	return -1;
    }

    /**
     * The file descriptor used when writing to this sensor.
     */
    int getWriteFd() const {
	if (socket) return socket->getFd();
    	return -1;
    }

    /**
    * open the socket.
    */
    void open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    /**
    * Read from the sensor.
    */
    size_t read(void *buf, size_t len) throw(nidas::util::IOException)
    {
        return socket->recv(buf,len);
    }

    /**
    * Write to the sensor.
    */
    size_t write(const void *buf, size_t len) throw(nidas::util::IOException) 
    {
        return socket->send(buf,len);
    }

    /*
    * Perform an ioctl on the device. Not necessary for a socket,
    * and will throw an IOException.
    */
    void ioctl(int request, void* buf, size_t len) throw(nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),
		"ioctl","not supported on SocketIODevice");
    }

    /**
    * close the sensor (and any associated FIFOs).
    */
    void close() throw(nidas::util::IOException);

    void setTcpNoDelay(bool val) throw(nidas::util::IOException)
    {
        tcpNoDelay = val;
    }

    bool getTcpNoDelay() throw(nidas::util::IOException)
    {
	return tcpNoDelay;
    }

    void parseAddress(const std::string& name) throw(nidas::util::ParseException);

protected:

    /**
     * The type of the destination address, AF_INET or AF_UNIX.
     */
    int addrtype;	

    /**
     * Destination host name from sensor name.
     */
    std::string desthost;

    /**
     * Port number that is parsed from sensor name.
     */
    int destport;

    /**
     * The destination socket address.
     */
    std::auto_ptr<nidas::util::SocketAddress> sockAddr;

    /**
     * The socket.  This isn't in an auto_ptr because
     * one must close the socket prior to deleting it.
     * The nidas::util::Socket destructor does not close
     * the file descriptor.
     */
    nidas::util::Socket* socket;

    bool tcpNoDelay;

};

}}	// namespace nidas namespace core

#endif
