/*
   Copyright by the National Center for Atmospheric Research
*/

#ifndef RTL_DEVIOCTL_H
#define RTL_DEVIOCTL_H

#include <atdUtil/IOException.h>

/**
 * A class providing ioctl capabilities to/from an RTL_DSMDevice.
 */
class RTL_DevIoctl {
public:

    /**
     * Constructor.
     * @param prefix The device prefix, containing the leading "/dev/".
     * @see RTL_DSMSensor::RTL_DSMSensor()
     */
    RTL_DevIoctl(const std::string& prefix, int boardNum, int firstPort);

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
     * Return the first port number of the board.
     */
    int getFirstPortNum() const { return firstPortNum; }

    /**
     * How many ports are on this board?  This is fetched
     * via the GET_NUM_PORTS ioctl to the board module.
     */
    int getNumPorts() throw(atdUtil::IOException);

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

    int firstPortNum;

    int numPorts;
     
    std::string inputFifoName;

    std::string outputFifoName;

    int infifofd;

    int outfifofd;

    bool opened;

};

#endif
