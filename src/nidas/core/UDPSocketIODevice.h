/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/core/UDPSocketIODevice.h $

*/
#ifndef NIDAS_CORE_UDPSOCKETIODEVICE_H
#define NIDAS_CORE_UDPSOCKETIODEVICE_H

#include <nidas/util/Socket.h>
#include <nidas/core/SocketIODevice.h>

namespace nidas { namespace core {

/**
 * A sensor connected through a UDP socket.
 */
class UDPSocketIODevice : public SocketIODevice {

public:

    /**
     * Create a UDPSocketIODevice.  No IO operations to the sensor
     * are performed in the constructor (hence no IOExceptions).
     */
    UDPSocketIODevice();

    virtual ~UDPSocketIODevice();

    /**
     * The file descriptor used when reading from this SocketIODevice.
     */
    int getReadFd() const
    {
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


    /**
    * close the sensor (and any associated FIFOs).
    */
    void close() throw(nidas::util::IOException);


protected:

    /**
     * The datagramsocket.  This isn't in an auto_ptr because
     * one must close the socket prior to deleting it.
     * The nidas::util::Socket destructor does not close
     * the file descriptor.
     */
    nidas::util::DatagramSocket* socket;


};

}}	// namespace nidas namespace core

#endif
