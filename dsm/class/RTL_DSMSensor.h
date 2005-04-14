/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/
#ifndef RTL_DSMSENSOR_H
#define RTL_DSMSENSOR_H

#include <DSMSensor.h>
#include <RTL_DevIoctl.h>
#include <Sample.h>
// #include <SampleSource.h>

namespace dsm {

/**
 * A RealTime Linux implementation of a DSMSensor.  We try to provide
 * a simple interface to the user, hiding the details of the
 * half-duplex RT-Linux FIFOs that are used to read/write and
 * perform ioctls with the device.
 */
class RTL_DSMSensor : public DSMSensor {

public:

    /**
     * Create a sensor.  No IO (open/read/write/ioctl)
     * operations to the sensor are performed in the constructor.
     */
    RTL_DSMSensor();

    virtual ~RTL_DSMSensor();

    /**
     * Constructor for a sensor.
     * @param val devname The device name.
     * Since we have to generate 4 FIFO names
     * from this devname, the devname should follow this convention:
     * <ul>
     * <li>"/dev/"
     * <li>followed by a name prefix consisting of a series of
     * characters (A-Z, a-z, _ -, 0-9), terminated by
     * non-digit,
     * <li>followed by one or more digits.
     * </ul>
     * The trailing digits specify the port number.
     * Examples:   /dev/xxxx0, /dev/xxxx_0, /dev/acme99_4,  /dev/xxx09
     */
    virtual void setDeviceName(const std::string& val);

    /**
     * The file descriptor used when reading from this sensor.
     */
    int getReadFd() const { return infifofd; }

    /**
     * The file descriptor used when writing to this sensor.
     */
    int getWriteFd() const { return outfifofd; }

    /**
    * open the sensor. This opens the associated RT-Linux FIFOs.
    */
    void open(int flags) throw(atdUtil::IOException);

    /**
    * Read from the sensor.
    */
    size_t read(void *buf, size_t len) throw(atdUtil::IOException);	

    /**
    * Write to the sensor.
    */
    size_t write(const void *buf, size_t len) throw(atdUtil::IOException);

    /*
    * Perform an ioctl on the device. request is an integer
    * value which must be supported by the device. Normally
    * this is a value from a header file for the device.
    */
    void ioctl(int request, void* buf, size_t len) throw(atdUtil::IOException);
    void ioctl(int request, const void* buf, size_t len) throw(atdUtil::IOException);

    /**
    * close the sensor (and any associated FIFOs).
    */
    virtual void close() throw(atdUtil::IOException);

    virtual const std::string& getInFifoName() const { return inFifoName; }
    virtual const std::string& getOutFifoName() const { return outFifoName; }

protected:

    /**
     * return an iterator pointing to one-past end of prefix
     */
    std::string::const_iterator getPrefixEnd(const std::string& name);

    /**
     * Prefix created from the name of the device, which is used
     * to generate the associated FIFO names.
     */
    std::string prefix;

    /**
     * Port number that is parsed from sensor name.
     */
    int portNum;

    /**
    * Actual name of RT-Linux FIFO that is used to read data from the
    * device.
    */
    std::string inFifoName;

    /**
    * Actual name of RT-Linux FIFO that is used to write data to the
    * device.
    */
    std::string outFifoName;

    /**
    * The RTLIoctlFifo used by this device.
    */
    RTL_DevIoctl* devIoctl;

    int infifofd;

    int outfifofd;

};

}
#endif
