/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

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

    static std::string statusString(unsigned char status,bool xml=false);

    void printStatus(std::ostream& ostr) throw();

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
    /**
     * compute the dsm_time_t from an IRIG sample.
     */
    dsm_time_t getTime(const Sample* samp) const {
	const dsm_clock_data* dp = (dsm_clock_data*)samp->getConstVoidDataPtr();
	return (dsm_time_t)(dp->tval.tv_sec) * 1000 + dp->tval.tv_usec / 1000;
    }

    /**
     * fetch the clock status from an IRIG sample.
     */
    unsigned char getStatus(const Sample* samp) const {
	const dsm_clock_data* dp = (dsm_clock_data*)samp->getConstVoidDataPtr();
	return dp->status;
    }

    void checkClock() throw(atdUtil::IOException);

    dsm_sample_id_t sampleId;

};

}

#endif
