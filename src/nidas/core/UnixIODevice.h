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
#ifndef NIDAS_CORE_UNIXIODEVICE_H
#define NIDAS_CORE_UNIXIODEVICE_H

#include "IODevice.h"
#include <nidas/util/EOFException.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/time_constants.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef DEBUG
#include <iostream>
#endif

namespace nidas { namespace core {

/**
 * A basic Unix I/O device, such as a named pipe, or a watched file.
 */
class UnixIODevice : public IODevice {

public:

    /**
     * Constructor. Does not open any actual device.
     */
    UnixIODevice(): _fd(-1) {}

    /**
     * Constructor, passing the name of the device. Does not open
     * the device.
     */
    UnixIODevice(const std::string& name):_fd(-1)
    {
        setName(name);
    }

    /**
     * Destructor. Does not close the device.
     */
    ~UnixIODevice() {}

    /**
     * The file descriptor used when reading from this device.
     */
    int getReadFd() const { return _fd; }

    /**
     * The file descriptor used when writing to this device.
     */
    int getWriteFd() const { return _fd; }

    /**
     * open the device.
     */
    void open(int flags) throw(nidas::util::IOException)
    {
        if ((_fd = ::open(getName().c_str(),flags)) < 0)
		throw nidas::util::IOException(getName(),"open",errno);
    }

    /**
     * Read from the device.
     */
    size_t read(void *buf, size_t len) throw(nidas::util::IOException)
    {
	ssize_t result;
        if ((result = ::read(_fd,buf,len)) < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
		throw nidas::util::IOException(getName(),"read",errno);
        }
	if (result == 0) 
		throw nidas::util::EOFException(getName(),"read");
	return result;
    }

    /**
     * Read from the device with a timeout in milliseconds.
     */
    size_t read(void *buf, size_t len, int msecTimeout) throw(nidas::util::IOException);

    /**
     * Write to the device.
     */
    size_t write(const void *buf, size_t len) throw(nidas::util::IOException)
    {
	ssize_t result;
        if ((result = ::write(_fd,buf,len)) < 0)
		throw nidas::util::IOException(getName(),"write",errno);
	return result;
    }

    /*
     * Perform an ioctl on the device. request is an integer
     * value which must be supported by the device. Normally
     * this is a value from a header file for the device.
     * size_t len parameter is not used.
     */
    void ioctl(int request, void* buf, size_t) throw(nidas::util::IOException)
    {
        if (::ioctl(_fd,request,buf) < 0)
		throw nidas::util::IOException(getName(),"ioctl",errno);
    }

    /**
     * close the device
    */
    void close() throw(nidas::util::IOException)
    {
        int fd = _fd;
        _fd = -1;
        if (fd >= 0 && ::close(fd) < 0)
		throw nidas::util::IOException(getName(),"close",errno);
    }

protected:

    int _fd;

};

}}	// namespace nidas namespace core

#endif
