/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#ifndef NIDAS_DYNLD_RAF_IRIGSENSOR_H
#define NIDAS_DYNLD_RAF_IRIGSENSOR_H

#include <nidas/rtlinux/dsm_serial.h>

#include <nidas/core/DSMSensor.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Sensor class for controlling and recieving data from an IRIG clock.
 */
class IRIGSensor : public DSMSensor
{

public:

    /**
     * No arg constructor.  Typically the device name and other
     * attributes must be set before the sensor device is opened.
     */
    IRIGSensor();

    ~IRIGSensor();

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner();

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException);

    /**
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

    /**
     * Over-ride nextSample() method. After an IRIGSensor sample
     * is read, we set the system clock.
     */
    Sample* nextSample();

    /**
     * Get the current time from the IRIG card.
     * This is not meant to be used for frequent use.
     */
    dsm_time_t getIRIGTime() throw(nidas::util::IOException);

    /**
     * Set the time on the IRIG card.
     */
    void setIRIGTime(dsm_time_t val) throw(nidas::util::IOException);

    static std::string statusString(unsigned char status,bool xml=false);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Process a raw sample.
     */
    bool process(const Sample* samp,std::list<const Sample*>& result)
    	throw();

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

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
	return (dsm_time_t)(dp->tval.tv_sec) * USECS_PER_SEC +
		dp->tval.tv_usec;
    }

    /**
     * fetch the clock status from an IRIG sample.
     */
    unsigned char getStatus(const Sample* samp) const {
	const dsm_clock_data* dp = (dsm_clock_data*)samp->getConstVoidDataPtr();
	return dp->status;
    }

    void checkClock() throw(nidas::util::IOException);

    dsm_sample_id_t sampleId;

    dsm_time_t lastTime;

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
