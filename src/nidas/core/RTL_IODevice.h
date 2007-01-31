/*
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/
#ifndef NIDAS_CORE_RTL_IODEVICE_H
#define NIDAS_CORE_RTL_IODEVICE_H

#include <nidas/core/IODevice.h>
#include <nidas/core/RTL_DevIoctl.h>

namespace nidas { namespace core {

/**
 * A RealTime Linux implementation of a IODevice.  We try to provide
 * a simple interface to the user, hiding the details of the
 * half-duplex RT-Linux FIFOs that are used to read/write and
 * perform ioctls with the device.
 */
class RTL_IODevice : public IODevice {

public:

    /**
     * Constructor.
     */
    RTL_IODevice();

    RTL_IODevice(const std::string& name);

    ~RTL_IODevice();

    /**
     * Set the name of this device.
     * Since we have to generate 4 FIFO names
     * from this name, the name should follow this convention:
     * <ul>
     * <li>"/dev/"
     * <li>followed by a name prefix consisting of a series of
     * characters (A-Z, a-z, _ -, 0-9), terminated by
     * non-digit,
     * <li>followed by one or more digits.
     * </ul>
     * The trailing digits specify the device number.
     * Examples:   /dev/xxxx0, /dev/xxxx_0, /dev/acme99_4,  /dev/xxx09
     */
    void setName(const std::string& val);

    /**
     * The file descriptor used when reading from this device.
     */
    int getReadFd() const { return infifofd; }

    /**
     * The file descriptor used when writing to this device.
     */
    int getWriteFd() const { return outfifofd; }

    /**
     * open the device. This opens the associated RT-Linux FIFOs.
     */
    void open(int flags) throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    /**
     * Read from the device.
     */
    size_t read(void *buf, size_t len) throw(nidas::util::IOException);	

    /**
     * Write to the device.
     */
    size_t write(const void *buf, size_t len) throw(nidas::util::IOException);

    /*
     * Perform an ioctl on the device. request is an integer
     * value which must be supported by the device. Normally
     * this is a value from a header file for the device.
     */
    void ioctl(int request, void* buf, size_t len) throw(nidas::util::IOException);

    /**
     * close the device (and any associated FIFOs).
     */
    void close() throw(nidas::util::IOException);

    /**
     * Whether to reopen this device on an IOException.
     * The read/write errors seen on RTL Fifos to this point
     * have not been fixed by a re-open.  If an RTL Fifo runs out of
     * memory, a re-open causes a segmentation fault.
     */
    // bool reopenOnIOException() const { return false; }

    const std::string& getInFifoName() const { return inFifoName; }
    const std::string& getOutFifoName() const { return outFifoName; }

protected:

    /**
     * Prefix created from the name of the device, which is used
     * to generate the associated FIFO names.
     */
    std::string prefix;

    /**
     * Device number that is parsed from device name.
     */
    int devNum;

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

}}	// namespace nidas namespace core

#endif
