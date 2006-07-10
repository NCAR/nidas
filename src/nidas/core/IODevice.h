/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-01-19 16:57:44 -0700 (Thu, 19 Jan 2006) $

    $LastChangedRevision: 3235 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/RTL_DSMSensor.h $

*/
#ifndef NIDAS_CORE_IODEVICE_H
#define NIDAS_CORE_IODEVICE_H

#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/IOException.h>

namespace nidas { namespace core {

/**
 * An interface to an IO device.
 */
class IODevice
{
public:

    virtual ~IODevice() {}

    /**
     * Set the device name to be opened for this sensor.
     */
    virtual void setName(const std::string& val) 
    {
        devname = val;
    }

    virtual const std::string& getName() const
    {
        return devname;
    }

    /**
     * The file descriptor used when reading from this sensor.
     */
    virtual int getReadFd() const = 0;

    /**
     * The file descriptor used when writing to this sensor.
     */
    virtual int getWriteFd() const = 0;

    /**
    * open the sensor. This opens the associated RT-Linux FIFOs.
    */
    virtual void open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException) = 0;

    /**
    * Read from the sensor.
    */
    virtual size_t read(void *buf, size_t len) throw(nidas::util::IOException) = 0;	

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
     * close the sensor (and any associated FIFOs).
     */
    virtual void close() throw(nidas::util::IOException) = 0;

    /**
     * Whether to reopen this sensor on an IOException.
     */
    // bool reopenOnIOException() const { return false; }

private:

    std::string devname;

};

}}	// namespace nidas namespace core

#endif
