/*
   Copyright by the National Center for Atmospheric Research
*/
#ifndef DSMSENSOR_H
#define DSMSENSOR_H

#include <string>
#include <atdUtil/IOException.h>

/**
 * A DSM Sensor interface
 */
class DSMSensor {

public:

    /**
    * Create a sensor, giving its name.  No IO (open, ioctl) operatations
    * are performed at this point since it throws no Exceptions.
    */
    DSMSensor(const std::string& name);

    virtual ~DSMSensor() = 0;

    /**
    * Open the device. flags are a combination of O_RDONLY, O_WRONLY.
    */
    virtual int open(int flags) throw(atdUtil::IOException) = 0;

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

    /*
    * Perform an ioctl on the device. request is an integer
    * value which must be supported by the device. Normally
    * this is a value from a header file for the device.
    */
    virtual int ioctlSend(int request, size_t len, void* buf) throw(atdUtil::IOException) = 0;
    virtual int ioctlRecv(int request, size_t len, void* buf) throw(atdUtil::IOException) = 0;

    /**
    * close
    */
    virtual int close() throw(atdUtil::IOException) = 0;

    virtual const std::string& getName() const { return name; }

protected:

    std::string name;
};

#endif
