/*
   Copyright by the National Center for Atmospheric Research
*/
#ifndef RTLDEVICE_H
#define RTLDEVICE_H

#include <string>
#include <atdUtil/IOException.h>

/**
 * A RealTime linux sensor.  We try to provide a simple interface
 * to the user, hiding the details of the half-duplex RTL FIFOs that
 * are used to read/write and  perform ioctls with the device.
 */
class RTL_DSMSensor : public DSMSensor {

public:

    /**
     * Constructor.
    * @param name Since we have to generate 4 FIFO names
    * from this name, the name should follow this convention:
    * a series of characters (A-Z, a-z, _ -, 0-9), terminated by
    * non-digit, then followed by one or more digits.
    * Examples:   xxxx0, xxxx_0, acme99_4,  xxx09
    */
    RTL_DSMSensor(const std::string& name);

    ~RTL_DSMSensor();

    /**
    * open the fifos that connect to the device on the RTLCore side.
    */
    int open(int flags) throw(atdUtil::IOException);

    /**
    * Read from the read fifo.
    */
    ssize_t read(void *buf, size_t len) throw(atdUtil::IOException);	

    /**
    * Write to the write fifo.
    */
    ssize_t write(void *buf, size_t len) throw(atdUtil::IOException);

    /*
    * Perform an ioctl on the device. request is an integer
    * value which must be supported by the device. Normally
    * this is a value from a header file for the device.
    */
    int ioctlSend(int request, size_t len, void* buf) throw(atdUtil::IOException);
    int ioctlRecv(int request, size_t len, void* buf) throw(atdUtil::IOException);

    /**
    * close associated fifos
    */
    int close() throw(atdUtil::IOException);

    virtual const std::string& getInFifoName() const { return inFifoname; }
    virtual const std::string& getOutFifoName() const { return outFifoname; }

protected:

    std::string::const_iterator RTL_DSMSensor::getPrefixEnd(const string& name);

    /**
     * Prefix created from the name of the device, which is used
     * to generate the associated FIFO names.
     */
    std::string prefix;

    /*
     * Port number that is parsed from sensor name.
     */
    int portNum;

    /**
    * Actual name of RTL FIFO that is used to read data from the
    * device.
    */
    std::string inFifoName;

    /**
    * Actual name of RTL FIFO that is used to write data to the
    * device.
    */
    std::string outFifoName;

    /**
    * The RTLIoctlFifo used by this device.
    */
    RTLIoctlFifo* ioctlFifo;

    int infifofd;

    int outfifofd;

};

#endif
