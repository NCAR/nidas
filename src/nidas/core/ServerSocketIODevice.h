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
#ifndef NIDAS_CORE_SERVERSOCKETIODEVICE_H
#define NIDAS_CORE_SERVERSOCKETIODEVICE_H

#include <nidas/core/IODevice.h>
#include <nidas/util/Socket.h>
#include <nidas/util/ParseException.h>

#include <memory> // auto_ptr<>

namespace nidas { namespace core {

/**
 * An IODevice supporting a TCP or UNIX server socket.
 * This class has a critical limitation and isn't currently used anywhere
 * in NIDAS. The IODevice::open() method should not block, and
 * this class violates that because open() does a
 * nidas::util::ServerSocket::accept() which can block forever.
 * To really support this class we need to spawn a ServerSocket
 * listening thread.
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
	if (_socket) return _socket->getFd();
	return -1;
    }

    /**
     * The file descriptor used when writing to this sensor.
     */
    int getWriteFd() const {
	if (_socket) return _socket->getFd();
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
        return _socket->recv(buf,len);
    }

    /**
    * Read from the sensor with a timeout in milliseconds.
    */
    size_t read(void *buf, size_t len, int msecTimeout) throw(nidas::util::IOException)
    {
	size_t l = 0;
	try {
		_socket->setTimeout(msecTimeout);
		l = _socket->recv(buf,len,msecTimeout);
		_socket->setTimeout(0);
	}
	catch(const nidas::util::IOException& e) {
		_socket->setTimeout(0);
		throw e;
        }
	return l;
    }

    /**
    * Write to the sensor.
    */
    size_t write(const void *buf, size_t len) throw(nidas::util::IOException) 
    {
        return _socket->send(buf,len);
    }

    /*
    * Perform an ioctl on the device. Not necessary for a socket,
    * and will throw an IOException.
    */
    void ioctl(int, void*, size_t) throw(nidas::util::IOException)
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
        _tcpNoDelay = val;
    }

    bool getTcpNoDelay() throw(nidas::util::IOException)
    {
	return _tcpNoDelay;
    }

protected:

    void closeServerSocket() throw(nidas::util::IOException);

private:

    /**
     * The type of the destination address, AF_INET or AF_UNIX.
     */
    int _addrtype;	

    /**
     * Path name of AF_UNIX socket.
     */
    std::string _unixPath;

    /**
     * Port number that is parsed from sensor name.
     */
    int _sockPort;

    /**
     * The destination socket address.
     */
    std::auto_ptr<nidas::util::SocketAddress> _sockAddr;

    /**
     * The listen socket.  This isn't in an auto_ptr because
     * one must close the socket prior to deleting it.
     * The nidas::util::Socket destructor does not close
     * the file descriptor.
     */
    nidas::util::ServerSocket* _serverSocket;

    nidas::util::Socket* _socket;

    bool _tcpNoDelay;

    /** No copying. */
    ServerSocketIODevice(const ServerSocketIODevice&);

    /** No assignment. */
    ServerSocketIODevice& operator=(const ServerSocketIODevice&);

};

}}	// namespace nidas namespace core

#endif
