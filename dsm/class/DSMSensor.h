/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision: 671 $

    $LastChangedBy: maclean $

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/
#ifndef DSMSENSOR_H
#define DSMSENSOR_H

#include <atdUtil/IOException.h>

#include <string>
#include <fcntl.h>

/**
 * An interface for a DSM Sensor.
 */
class DSMSensor {

public:

    /**
    * Create a sensor, giving its name.  No IO (open/read/write/ioctl)
    * operatations to the sensor are performed in the constructor.
    */
    DSMSensor(const std::string& n) : name(n) {}

    virtual ~DSMSensor() {}

    /**
    * Open the device. flags are a combination of O_RDONLY, O_WRONLY.
    */
    virtual void open(int flags) throw(atdUtil::IOException) = 0;

    /**
    * Read from the device (duh). Behaves like read(2) system call,
    * without a file descriptor argument, and with an IOException.
    */
    virtual ssize_t read(void *buf, size_t len) throw(atdUtil::IOException) = 0;	

    /**
    * Write to the device (duh). Behaves like write(2) system call,
    * without a file descriptor argument, and with an IOException.
    */
    virtual ssize_t write(void *buf, size_t len) throw(atdUtil::IOException) = 0;

    /**
    * Perform an ioctl on the device. request is an integer
    * value which must be supported by the device. Normally
    * this is a value from a header file for the device.
    */
    virtual void ioctl(int request, void* buf, size_t len) throw(atdUtil::IOException) = 0;

    /**
     * Overloaded ioctl method, used when doing an ioctl set from
     * a pointer to constant user data.
     */
    virtual void ioctl(int request, const void* buf, size_t len) throw(atdUtil::IOException) = 0;

    /**
    * close
    */
    virtual void close() throw(atdUtil::IOException) = 0;

    virtual const std::string& getName() const { return name; }

protected:

    std::string name;
};

#endif
