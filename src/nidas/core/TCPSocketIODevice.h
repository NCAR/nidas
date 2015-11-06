// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
#ifndef NIDAS_CORE_TCPSOCKETIODEVICE_H
#define NIDAS_CORE_TCPSOCKETIODEVICE_H

#include <nidas/core/SocketIODevice.h>

namespace nidas { namespace core {

/**
 * An IODevice consisting of a TCP socket.
 */
class TCPSocketIODevice : public SocketIODevice {

public:

    /**
     * Create a TCPSocketIODevice.  No IO operations
     * are performed in the constructor, hence no IOExceptions.
     */
    TCPSocketIODevice();

    ~TCPSocketIODevice();

    /**
     * Open the socket, which does a socket connect to the remote address
     * which is parsed from the contents of getName().
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

    void setTcpNoDelay(bool val) throw(nidas::util::IOException)
    {
        _tcpNoDelay = val;
    }

    bool getTcpNoDelay() throw(nidas::util::IOException)
    {
	return _tcpNoDelay;
    }

    void setKeepAliveIdleSecs(int val) throw(nidas::util::IOException)
    {
	_keepAliveIdleSecs = val;
    }

    int getKeepAliveIdleSecs() const throw(nidas::util::IOException)
    {
	return _keepAliveIdleSecs;
    }

protected:

    /**
     * The socket. This isn't in an auto_ptr because
     * one must close the socket prior to deleting it.
     * The nidas::util::Socket destructor does not close
     * the file descriptor.
     */
    nidas::util::Socket* _socket;

    bool _tcpNoDelay;

    int _keepAliveIdleSecs;

    /**
     * No copy.
     */
    TCPSocketIODevice(const TCPSocketIODevice&);

    /**
     * No assignment.
     */
    TCPSocketIODevice& operator=(const TCPSocketIODevice&);

};

}}	// namespace nidas namespace core

#endif
