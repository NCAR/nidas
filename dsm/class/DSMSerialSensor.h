/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef DSMSERIALSENSOR_H
#define DSMSERIALSENSOR_H

#include <dsm_serial.h>

#include <CharacterSensor.h>
#include <atdTermio/Termios.h>

namespace dsm {
/**
 * A sensor connected to a serial port.
 */
class DSMSerialSensor : public CharacterSensor, public atdTermio::Termios {

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
    void open(int flags) throw(atdUtil::IOException,atdUtil::InvalidParameterException);

    /*
     * Close the device connected to the sensor.
     */
    void close() throw(atdUtil::IOException);

    void printStatus(std::ostream& ostr) throw();


  /**
     * Is prompting active, i.e. isPrompted() is true, and startPrompting
     * has been called?
     */
    bool isPrompting() const { return prompting; }

    void startPrompting() throw(atdUtil::IOException);

    void stopPrompting() throw(atdUtil::IOException);

    void fromDOMElement(const xercesc::DOMElement* node)
    	throw(atdUtil::InvalidParameterException);

protected:

    void rtlDevInit(int flags)
    	throw(atdUtil::IOException,atdUtil::InvalidParameterException);

    void unixDevInit(int flags)
    	throw(atdUtil::IOException,atdUtil::InvalidParameterException);

private:

    bool prompting;

};

}

#endif
