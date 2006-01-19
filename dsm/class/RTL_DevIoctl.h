/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved
*/

#ifndef RTL_DEVIOCTL_H
#define RTL_DEVIOCTL_H

#include <atdUtil/IOException.h>
#include <atdUtil/ThreadSupport.h>

namespace dsm {
/**
 * A class providing ioctl capabilities to/from an RTL DSM device.
 * The ioctl is implemented with two RTL FIFOs, one read-only,
 * the other write-only, which communicate with the RTL driver module.
 *
 * One RTL driver module may support more than one device.
 * For example the dsm_serial modules can support multiple boards,
 * and each board supports 8 serial ports.
 * The module can create as many fifo pairs as it wants to, as
 * long as the first pair is called
 *     /dev/prefix_ictl_0 and /dev/prefix_octl_0.
 * where "prefix" is the device prefix of the module, e.g. "dsmser".
 *
 * Scenario;
 * A user wants to open and send an ioctl to device /dev/prefix_14.
 * This code first sends a GET_NUM_PORTS request over /dev/prefix_[io]ctl_0.
 *
 * In this example, the module responds with a value of 8 ports,
 * meaning it supports devices /dev/prefix_0 - /dev/prefix_7
 * over ioctl fifos /dev/prefix_[io]ctl_0.
 *
 * Device 14 is out of this range,  so the code increments the name
 * to /dev/prefix_[io]ctl_1 (the "board" number is now 1) and
 * sends a GET_NUM_PORTS request over that fifo.
 * If the module again responds
 * with 8 ports, then /dev/prefix_[io]ctl_1 is the correct fifo
 * pair to use for that device.  The ioctls for this device
 * will be addressed to board 1 (the second board), port 6,
 * because device 14 is the 6th port on the second board.
 * 
 * In this way we can conserve fifos - we don't need a pair of ioctl
 * fifos for every device - just one (or more) for each driver module.
 * 
 */
class RTL_DevIoctl {
public:

    /**
     * Constructor.
     * @param prefix The device prefix, containing the leading "/dev/".
     * @boardNum: board number.
     * @firstDev: first device number on the board. 
     * @see RTL_DSMSensor::RTL_DSMSensor()
     */
    RTL_DevIoctl(const std::string& prefix, int boardNum, int firstDev);

    ~RTL_DevIoctl();

    /**
     * Return the prefix which was provided in the constructor.
     */
    const std::string& getPrefix() { return prefix; }


    /**
     * Return the board number provided in the constructor.
     */
    int getBoardNum() const { return boardNum; }

    /**
     * Return the first device number of the board.
     */
    int getFirstDevNum() const { return firstDevNum; }

    /**
     * How many devices are on this board?  This is fetched
     * via the GET_NUM_PORTS ioctl to the board module.
     */
    int getNumDevs() throw(atdUtil::IOException);

    /**
     * Open this RTL_DevIoctl.
     */
    void open() throw(atdUtil::IOException);

    /**
     * Open this RTL_DevIoctl.
     */
    void close();

    /**
     * Send an ioctl command to the RT-Linux module associated with
     * the prefix that was passed to the constructor.
     */
    void ioctl(int cmd, int port, void* buf, size_t len)
    	throw(atdUtil::IOException);

    /**
     * Overloaded ioctl method, which can only be used when doing an
     * ioctl set, since buf is a pointer to a constant.
     */
    void ioctl(int cmd, int port, const void* buf, size_t len)
    	throw(atdUtil::IOException);

    static std::string makeInputFifoName(const std::string& prefix,
    	int boardNum);

    static std::string makeOutputFifoName(const std::string& prefix,
    	int boardNum);

    /**
     * Return name of a DSMSensor for the given port.
     */
    std::string getDSMSensorName(int port) const;

protected:

    std::string prefix;

    int boardNum;

    int firstDevNum;

    int numDevs;
     
    std::string inputFifoName;

    std::string outputFifoName;

    int infifofd;

    int outfifofd;

    int usageCount;

    atdUtil::Mutex ioctlMutex;

};

}

#endif
