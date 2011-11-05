/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_DSMSERIALSENSOR_H
#define NIDAS_DYNLD_DSMSERIALSENSOR_H

#include <nidas/core/CharacterSensor.h>
#include <nidas/core/LooperClient.h>
#include <nidas/util/Termios.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * A sensor connected to a serial port.
 */
class DSMSerialSensor : public CharacterSensor, public nidas::util::Termios 
{

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    DSMSerialSensor();

    ~DSMSerialSensor();

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    /*
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

    /**
     * Set message separator and message length parameters
     * on the hardware device. When a device driver supports
     * these parameters, and the device is open, a call to this
     * method will notify the driver, via an ioctl(), of
     * the current values for message length and the
     * message separator.
     */
    void setMessageParameters(unsigned int len,const std::string& sep, bool eom)
        throw(nidas::util::InvalidParameterException,nidas::util::IOException);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Is prompting active, i.e. isPrompted() is true, and startPrompting
     * has been called?
     */
    bool isPrompting() const { return _prompting; }

    /**
     * Start the prompters. They can be started and stopped multiple
     * times once a DSMSerialSensor is opened.
     */
    void startPrompting() throw(nidas::util::IOException);

    void stopPrompting() throw(nidas::util::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
    	throw(nidas::util::InvalidParameterException);

    /**
     * Calculate the transmission time of each byte from this
     * sensor. For RS232/485/422 serial sensors is it simply
     * calculated as
     *  (databits + stopbits + 1) / baudrate
     * converted to microseconds.  This value is used by
     * MessageStreamScanners to guess at the time that the
     * first byte of a buffer was sent, knowing
     * the arrival time of that buffer of characters.
     * In the absence of further information, this corrected
     * transmission time is then used as the sample time.
     *
     * This method checks if the underlying IODevice is a tty,
     * returning a value of 0 usecs if it is not a tty, such as for
     * a socket.  Therefore the IODevice should exist and be open
     * for this method to work correctly.
     * In DSMSensor::open(), the IODevice is built, then opened,
     * and then the SampleScanner is created, which is when
     * this method should be called.
     *
     * It will return 0 for socket devices.
     */
    int getUsecsPerByte() const;

protected:

    /**
     * Set appropriate parameters on the I/O device to support the
     * requested message parameters.
     */
    void applyMessageParameters() throw(nidas::util::IOException);

    /**
     * Perform whatever is necessary to initialize prompting right
     * after the device is opened.
     */
    void initPrompting() throw(nidas::util::IOException);

    /**
     * Shutdown prompting, typically done when a device is closed.
     */
    void shutdownPrompting() throw(nidas::util::IOException);

    void unixDevInit(int flags)
    	throw(nidas::util::IOException);

private:

    class Prompter: public nidas::core::LooperClient
    {
    public:
        Prompter(DSMSerialSensor* sensor): _sensor(sensor),
		_prompt(0),_promptLen(0), _promptPeriodMsec(0) {}

        ~Prompter();

        void setPrompt(const std::string& val);
        const std::string getPrompt() const { return _prompt; }

        void setPromptPeriodMsec(const int);
        const int getPromptPeriodMsec() const { return _promptPeriodMsec; }

        /**
         * Method called by Looper in order to send a prompt.
         */
        void looperNotify() throw();
    private:
        DSMSerialSensor* _sensor;
        char* _prompt;
	int _promptLen;
        int _promptPeriodMsec;
    };

    std::list<Prompter*> _prompters;

    bool _prompting;

};

}}	// namespace nidas namespace dynld

#endif
