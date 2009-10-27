/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/core/SocketIODevice.h $

*/
#ifndef NIDAS_CORE_SERVERSOCKETIODEVICE_H
#define NIDAS_CORE_SERVERSOCKETIODEVICE_H

#include <nidas/core/IODevice.h>
#include <nidas/util/Socket.h>
#include <nidas/util/ParseException.h>

#include <memory> // auto_ptr<>

namespace nidas { namespace core {

/**
 * A sensor connected through a listening socket.
 */
class ServerSocketIODevice : public IODevice {

public:

    /**
     * Create a SocketIODevice.  No IO operations to the sensor
     * are performed in the constructor (hence no IOExceptions).
     */
    ServerSocketIODevice();

    virtual ~ServerSocketIODevice();

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
    * Read from the sensor with a timeout in milliseconds.
    */
    size_t read(void *buf, size_t len, int msecTimeout) throw(nidas::util::IOException)
    {
	size_t l = 0;
	try {
		socket->setTimeout(msecTimeout);
		l = socket->recv(buf,len,msecTimeout);
		socket->setTimeout(0);
	}
	catch(const nidas::util::IOException& e) {
		socket->setTimeout(0);
		throw e;
        }
	return l;
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
    void closeServerSocket() throw(nidas::util::IOException);

private:

    /**
     * The type of the destination address, AF_INET or AF_UNIX.
     */
    int addrtype;	

    /**
     * Path name of AF_UNIX socket.
     */
    std::string unixPath;

    /**
     * Port number that is parsed from sensor name.
     */
    int sockPort;

    /**
     * The destination socket address.
     */
    std::auto_ptr<nidas::util::SocketAddress> sockAddr;

    /**
     * The listen socket.  This isn't in an auto_ptr because
     * one must close the socket prior to deleting it.
     * The nidas::util::Socket destructor does not close
     * the file descriptor.
     */
    nidas::util::ServerSocket* serverSocket;

    nidas::util::Socket* socket;

    bool tcpNoDelay;

};

}}	// namespace nidas namespace core

#endif
