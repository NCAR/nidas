/*
   Copyright by the National Center for Atmospheric Research
*/

#ifndef RTL_DEVIOCTL_H
#define RTL_DEVIOCTL_H

/**
 * A class providing ioctl capabilities to/from an RTL_DSMDevice.
 */
class RTL_DevIoctl {
public:

    /**
     * Constructor.
     */
    RTL_DevIoctl(const std::string& prefix, int boardNum, int firstPort);
    ~RTL_DevIoctl();

    const std::string& getPrefix() return { prefix; }
    int getBoardNum() const { return boardNum; }
    int getFirstPortNum() const { return firstPortNum; }

    int getNumPorts() throw(atdUtil::IOException);

    void open() throw(atdUtil::IOException);

    int ioctlSend(int cmd, int port, size_t len, void* buf)
    	throw(atdUtil::IOException);
    int ioctlRecv(int cmd, int port, size_t len, void* buf)
    	throw(atdUtil::IOException);

    static std::string makeInputFifoName(const string& prefix, int firstPort)ZZ

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
