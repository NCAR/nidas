// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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

#include <nidas/Config.h>

#ifdef HAVE_BLUETOOTH_RFCOMM_H

#ifndef NIDAS_CORE_BLUETOOTHRFCOMMSOCKETIODEVICE_H
#define NIDAS_CORE_BLUETOOTHRFCOMMSOCKETIODEVICE_H

#include <nidas/util/BluetoothRFCommSocket.h>
#include "SocketIODevice.h"

namespace nidas { namespace core {

/**
 * A BluetoothRFCommSocket implementation of an IODevice.
 */
class BluetoothRFCommSocketIODevice : public SocketIODevice {

public:

    /**
     * Create a BluetoothRFCommSocketIODevice.  No IO operations
     * are performed in the constructor, hence no IOExceptions.
     */
    BluetoothRFCommSocketIODevice();

    ~BluetoothRFCommSocketIODevice();

    /**
     * open the RFComm.
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

private:

    nidas::util::BluetoothRFCommSocket* _socket;

    /** No copy. */
    BluetoothRFCommSocketIODevice(const BluetoothRFCommSocketIODevice &);

    /** No assigmnent. */
    BluetoothRFCommSocketIODevice& operator=(const BluetoothRFCommSocketIODevice &);

};

}}	// namespace nidas namespace core

#endif
#endif
