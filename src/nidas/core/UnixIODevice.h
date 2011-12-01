// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/
#ifndef NIDAS_CORE_UNIXIODEVICE_H
#define NIDAS_CORE_UNIXIODEVICE_H

#include <nidas/core/IODevice.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/IOTimeoutException.h>
#include <nidas/util/time_constants.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

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
        if ((result = ::read(_fd,buf,len)) < 0)
		throw nidas::util::IOException(getName(),"read",errno);
	if (result == 0) 
		throw nidas::util::EOFException(getName(),"read");
	return result;
    }

    /**
    * Read from the device with a timeout in milliseconds.
    */
    size_t read(void *buf, size_t len, int msecTimeout) throw(nidas::util::IOException)
    {
	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(_fd, &fdset);
	struct timeval tmpto = { 0, msecTimeout * USECS_PER_MSEC };
        int res;
	if ((res = ::select(_fd+1,&fdset,0,0,&tmpto)) < 0) {
	    throw nidas::util::IOException(getName(),"read",errno);
	}
	if (res == 0)
	    throw nidas::util::IOTimeoutException(getName(),"read");
        return read(buf,len);
    }

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
