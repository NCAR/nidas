// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#include <nidas/Config.h>

#ifdef HAVE_BLUETOOTH_RFCOMM_H

#ifndef NIDAS_CORE_BLUETOOTHRFCOMMSOCKETIODEVICE_H
#define NIDAS_CORE_BLUETOOTHRFCOMMSOCKETIODEVICE_H

#include <nidas/util/BluetoothRFCommSocket.h>
#include <nidas/core/SocketIODevice.h>

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
