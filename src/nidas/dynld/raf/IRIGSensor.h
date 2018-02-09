// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
#ifndef NIDAS_DYNLD_RAF_IRIGSENSOR_H
#define NIDAS_DYNLD_RAF_IRIGSENSOR_H

#include <nidas/linux/irigclock.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/util/InvalidParameterException.h>
#include <nidas/util/EndianConverter.h>

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

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    /**
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

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

    static std::string shortStatusString(unsigned char status,bool xml=false);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Process a raw sample.
     */
    bool process(const Sample* samp,std::list<const Sample*>& result)
    	throw();

    void fromDOMElement(const xercesc::DOMElement*)
    	throw(nidas::util::InvalidParameterException);

    /**
     * compute the dsm_time_t from an IRIG sample.
     * Values from device are little-endian.
     */
    dsm_time_t getIRIGTime(const Sample* samp) const;

    /**
     * compute the dsm_time_t from the Unix struct timeval32 in an IRIG sample.
     * Values from device are little-endian.
     */
    dsm_time_t getUnixTime(const Sample* samp) const;

    /**
     * fetch the clock status from an IRIG sample.
     */
    unsigned char getStatus(const Sample* samp) const;

    float get100HzBacklog(const Sample* samp) const;

    static const nidas::util::EndianConverter* lecvtr;
private:

    void checkClock() throw(nidas::util::IOException);

    dsm_sample_id_t _sampleId;

    int _nvars;

    int _nStatusPrints;

    int _slews[IRIG_MAX_DT_DIFF - IRIG_MIN_DT_DIFF + 1];

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
