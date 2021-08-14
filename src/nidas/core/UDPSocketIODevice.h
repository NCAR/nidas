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
#ifndef NIDAS_CORE_UDPSOCKETIODEVICE_H
#define NIDAS_CORE_UDPSOCKETIODEVICE_H

#include <nidas/util/Socket.h>
#include "SocketIODevice.h"

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
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     */
    void open(int flags);

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
     *
     * @throws nidas::util::IOException
     */
    size_t read(void *buf, size_t len)
    {
        return _socket->recv(buf,len);
    }

    /**
     * Read from the device with a timeout in milliseconds.
     *
     * @throws nidas::util::IOException
     */
    size_t read(void *buf, size_t len, int msecTimeout);

    /**
     * Write to the device.
     *
     * @throws nidas::util::IOException
     */
    size_t write(const void *buf, size_t len)
    {
        return _socket->send(buf,len);
    }

    /**
     * close the device.
     *
     * @throws nidas::util::IOException
     */
    void close();

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
