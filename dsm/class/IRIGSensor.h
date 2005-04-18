/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-11-22 14:41:27 -0700 (Mon, 22 Nov 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/DSMSerialSensor.h $

 ******************************************************************
*/
#ifndef IRIGSENSOR_H
#define IRIGSENSOR_H

#include <dsm_serial.h>

#include <RTL_DSMSensor.h>
#include <atdUtil/InvalidParameterException.h>

namespace dsm {
/**
 * Sensor class for controlling and recieving data from an IRIG clock.
 */
class IRIGSensor : public RTL_DSMSensor
{

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    IRIGSensor();

    ~IRIGSensor();

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(atdUtil::IOException);

    /*
     * Close the device connected to the sensor.
     */
    void close() throw(atdUtil::IOException);

    /**
     * Process a raw sample.
     */
    bool process(const Sample* samp,std::list<const Sample*>& result)
    	throw();

    virtual SampleDater::status_t setSampleTime(SampleDater*,Sample* samp);

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

protected:
    void checkClock() throw(atdUtil::IOException);

    dsm_sample_id_t sampleId;

};

}

#endif
