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
#ifndef NIDAS_CORE_UDPSOCKETIODEVICE_H
#define NIDAS_CORE_UDPSOCKETIODEVICE_H

#include <nidas/util/Socket.h>
#include <nidas/core/SocketIODevice.h>

#include <iostream>

namespace nidas { namespace core {

/**
 * An IODevice consisting of an UDP socket.
 */
class UDPSocketIODevice : public SocketIODevice {

public:

    /**
     * Create a UDPSocketIODevice.  No IO operations
     * are performed in the constructor, hence no IOExceptions.
     */
    UDPSocketIODevice();

    ~UDPSocketIODevice();

    /**
     * Open the socket, which does a socket bind to the remote
     * address which is parsed from the contents of getName().
     * See SocketIODevice::open() and SocketIODevice::parseAddress().
     */
    void open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    /**
     * The file descriptor used when reading from this SocketIODevice.
     */
    int getReadFd() const
    {
	if (_socket) return _socket->getFd();
	return -1;
    }

    /**
     * The file descriptor used when writing to this device.
     */
    int getWriteFd() const {
        if (_socket) return _socket->getFd();
        return -1;
    }
    
    /**
     * Read from the device.
     */
    size_t read(void *buf, size_t len) throw(nidas::util::IOException)
    {
	return _socket->recv(buf,len);
    }

    /**
     * Read from the device with a timeout in milliseconds.
     */
    size_t read(void *buf, size_t len, int msecTimeout)
        throw(nidas::util::IOException);

    /**
     * Write to the device.
     */
    size_t write(const void *buf, size_t len) throw(nidas::util::IOException) 
    {
        return _socket->send(buf,len);
    }

    /**
     * close the device.
     */
    void close() throw(nidas::util::IOException);

protected:

    /**
     * The datagramsocket.  This isn't in an auto_ptr because
     * one must close the socket prior to deleting it.
     * The nidas::util::Socket destructor does not close
     * the file descriptor.
     */
    nidas::util::DatagramSocket* _socket;

    /**
     * No copy.
     */
    UDPSocketIODevice(const UDPSocketIODevice&);

    /**
     * No assignment.
     */
    UDPSocketIODevice& operator=(const UDPSocketIODevice&);

};

}}	// namespace nidas namespace core

#endif
