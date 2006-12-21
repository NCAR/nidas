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

#include <nidas/rtlinux/dsm_serial.h>

#include <nidas/core/CharacterSensor.h>
#include <nidas/core/LooperClient.h>
#include <nidas/util/Termios.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * A sensor connected to a serial port.
 */
class DSMSerialSensor : public CharacterSensor, public nidas::util::Termios,
	public LooperClient {

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    DSMSerialSensor();

    ~DSMSerialSensor();

    SampleScanner* buildSampleScanner();

    /**
     * Override DSMSensor::getDefaultMode to allow writing.
     * @return One of O_RDONLY, O_WRONLY or O_RDWR.
     */
    int getDefaultMode() const { return O_RDWR; }

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException,nidas::util::InvalidParameterException);

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
    void setMessageParameters() throw(nidas::util::IOException);

    void printStatus(std::ostream& ostr) throw();


    /**
     * Is prompting active, i.e. isPrompted() is true, and startPrompting
     * has been called?
     */
    bool isPrompting() const { return prompting; }

    void startPrompting() throw(nidas::util::IOException);

    void stopPrompting() throw(nidas::util::IOException);

    /**
     * Method called by Looper in order to send a prompt.
     */
    void looperNotify() throw();

    void fromDOMElement(const xercesc::DOMElement* node)
    	throw(nidas::util::InvalidParameterException);

protected:

    void rtlDevInit(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    void unixDevInit(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

private:

    bool prompting;

    char* cPromptString;

    int cPromptStringLen;

    int promptPeriodMsec;

};

}}	// namespace nidas namespace dynld

#endif
