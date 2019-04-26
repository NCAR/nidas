// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_CORE_IODEVICE_H
#define NIDAS_CORE_IODEVICE_H

#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/IOException.h>

#include <sys/ioctl.h>

namespace nidas { namespace core {

/**
 * An interface to an IO device. The main user of this interface is
 * DSMSensor, which contains an IODevice, using it to do I/O with
 * the underlying device.
 */
class IODevice
{
public:

    IODevice(): _devname() {}

    virtual ~IODevice() {}

    /**
     * Set the device name to be opened for this sensor.
     */
    virtual void setName(const std::string& val) 
    {
        _devname = val;
    }

    virtual const std::string& getName() const
    {
        return _devname;
    }

    /**
     * Open the device. This operation should not block - it should
     * either fail or succeed in a reasonable amount of time, typically
     * less than 1 or 2 seconds.
     */
    virtual void open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException) = 0;

    /**
     * The file descriptor used when reading from this sensor.
     */
    virtual int getReadFd() const = 0;

    /**
     * The file descriptor used when writing to this sensor.
     */
    virtual int getWriteFd() const = 0;

    /**
     * Read from the sensor.
     */
    virtual size_t read(void *buf, size_t len) throw(nidas::util::IOException) = 0;	

    /**
     * Read from the sensor with a millisecond timeout.
     */
    virtual size_t read(void *buf, size_t len,int msecTimeout) throw(nidas::util::IOException) = 0;	

    /**
     * Return how many bytes are available to read on this IODevice.
     * This method is only useful when ioctl FIONREAD is supported
     * on this this IODevice, as for example with a UDP socket.
     * It is not available, and not necessary, on most other devices,
     * like serial ports, TCP sockets, or devices with nidas driver
     * module support, in which case it will return an
     * nidas::util::IOException.
     * It is an optimization for use with UDP sockets, where after
     * select determines that data is available on the socket file
     * descriptor, a read will only read one datagram, even if there are 
     * more than one packet available, which would not be optimal if a sensor
     * generated many small packets. Rather than returning back to select,
     * we check if there are more datagrams to read. This is not
     * necessary on other types of IODevices, where it is just a matter
     * of using a big enough buffer to get all (or most) available data
     * after a select.
     *
     * According to man udp(7), FIONREAD will return 0 if the size of the
     * next packet is 0. So, if select/poll indicate there is data
     * available, one read at least should be done, just in case a
     * sensor sends a datagram of length 0, otherwise it will never
     * be consumed.
     */
    virtual size_t getBytesAvailable() const throw(nidas::util::IOException)
    {
        int nbytes;
        int err = ::ioctl(getReadFd(),FIONREAD,&nbytes);
        if (err < 0)
            throw nidas::util::IOException(getName(),"ioctl FIONREAD",errno);
        return nbytes;
    }

    /**
     * Write to the sensor.
     */
    virtual size_t write(const void *buf, size_t len) throw(nidas::util::IOException) = 0;

    /*
     * Perform an ioctl on the device. request is an integer
     * value which must be supported by the device. Normally
     * this is a value from a header file for the device.
     */
    virtual void ioctl(int request, void* buf, size_t len)
    	throw(nidas::util::IOException) = 0;

    /**
     * Close the device.
     */
    virtual void close() throw(nidas::util::IOException) = 0;

    /**
     * Whether to reopen this sensor on an IOException.
     */
    // bool reopenOnIOException() const { return false; }

private:

    std::string _devname;

    /**
     * Derived classes don't implement copy so we won't here.
     */
    IODevice(const IODevice&);

    /**
     * Derived classes don't implement assignment so we won't here.
     */
    IODevice& operator=(const IODevice&);


};

}}	// namespace nidas namespace core

#endif
