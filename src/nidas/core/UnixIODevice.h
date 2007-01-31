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

#include <sys/ioctl.h>

#ifdef DEBUG
#endif
#include <iostream>

namespace nidas { namespace core {

/**
 * A simple Unix implementation of a IODevice.
 */
class UnixIODevice : public IODevice {

public:

    /**
     * Constructor. Does not open any actual device.
     */
    UnixIODevice(): fd(-1) {}

    /**
     * Constructor, passing the name of the device. Does not open
     * the device.
     */
    UnixIODevice(const std::string& name):fd(-1)
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
    int getReadFd() const { return fd; }

    /**
     * The file descriptor used when writing to this device.
     */
    int getWriteFd() const { return fd; }

    /**
     * open the device.
     */
    void open(int flags) throw(nidas::util::IOException)
    {
        if ((fd = ::open(getName().c_str(),flags)) < 0)
		throw nidas::util::IOException(getName(),"open",errno);
    }

    /**
     * Read from the device.
     */
    size_t read(void *buf, size_t len) throw(nidas::util::IOException)
    {
	ssize_t result;
        if ((result = ::read(fd,buf,len)) < 0)
		throw nidas::util::IOException(getName(),"read",errno);
	if (result == 0) 
		throw nidas::util::EOFException(getName(),"read");
	return result;
    }

    /**
     * Write to the device.
     */
    size_t write(const void *buf, size_t len) throw(nidas::util::IOException)
    {
	ssize_t result;
        if ((result = ::write(fd,buf,len)) < 0)
		throw nidas::util::IOException(getName(),"write",errno);
	return result;
    }

    /*
     * Perform an ioctl on the device. request is an integer
     * value which must be supported by the device. Normally
     * this is a value from a header file for the device.
     */
    void ioctl(int request, void* buf, size_t len) throw(nidas::util::IOException)
    {
        if (::ioctl(fd,request,buf) < 0)
		throw nidas::util::IOException(getName(),"ioctl",errno);
    }

    /**
     * close the device
    */
    void close() throw(nidas::util::IOException)
    {
        if (::close(fd) < 0)
		throw nidas::util::IOException(getName(),"close",errno);
    }

protected:

    int fd;

};

}}	// namespace nidas namespace core

#endif
