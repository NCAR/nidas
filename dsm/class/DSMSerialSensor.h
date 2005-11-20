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

#include <RTL_DSMSensor.h>
#include <MessageStreamSensor.h>
#include <atdUtil/InvalidParameterException.h>
#include <atdTermio/Termios.h>
#include <AsciiScanner.h>

namespace dsm {
/**
 * A sensor connected to a serial port.
 */
class DSMSerialSensor : public RTL_DSMSensor, public MessageStreamSensor, public atdTermio::Termios {

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    DSMSerialSensor();

    ~DSMSerialSensor();

    /**
     * Override DSMSensor::getDefaultMode to allow writing.
     * @return One of O_RDONLY, O_WRONLY or O_RDWR.
     */
    int getDefaultMode() const { return O_RDWR; }

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(atdUtil::IOException,atdUtil::InvalidParameterException);

    void init() throw(atdUtil::InvalidParameterException);

    void addSampleTag(SampleTag* var)
    	throw(atdUtil::InvalidParameterException);
    /*
     * Close the device connected to the sensor.
     */
    void close() throw(atdUtil::IOException);

    /**
     * Is prompting active, i.e. isPrompted() is true, and startPrompting
     * has been called?
     */
    bool isPrompting() const { return prompting; }

    void startPrompting() throw(atdUtil::IOException);

    void stopPrompting() throw(atdUtil::IOException);

    unsigned long getSampleId() const { return sampleId; }

    void printStatus(std::ostream& ostr) throw();

    /**
     * Process a raw sample, which in this case means do
     * a sscanf on the character string contents, creating
     * a processed sample of binary floating point data.
     */
    virtual bool process(const Sample*,std::list<const Sample*>& result)
    	throw();

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:

    dsm_sample_id_t sampleId;

    bool prompting;

};

}

#endif
