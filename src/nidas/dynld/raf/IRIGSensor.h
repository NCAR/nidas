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

    /**
     * @throws nidas::util::IOException
     **/
    IODevice* buildIODevice();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    SampleScanner* buildSampleScanner();

    /**
     * Open the device connected to the sensor.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void open(int flags);

    /**
     * Close the device connected to the sensor.
     *
     * @throws nidas::util::IOException
     **/
    void close();

    /**
     * Get the current time from the IRIG card.
     * This is not meant to be used for frequent use.
     *
     * @throws nidas::util::IOException
     **/
    dsm_time_t getIRIGTime();

    /**
     * Set the time on the IRIG card.
     *
     * @throws nidas::util::IOException
     **/
    void setIRIGTime(dsm_time_t val);

    static std::string statusString(unsigned char status,bool xml=false);

    static std::string shortStatusString(unsigned char status,bool xml=false);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Process a raw sample.
     */
    bool process(const Sample* samp,std::list<const Sample*>& result)
	throw();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement*);

    /**
     * Return the IRIG time in an IRIG sample.
     */
    static dsm_time_t getIRIGTime(const Sample* samp);

    /**
     * Return the UNIX time in an IRIG sample.
     * Early IRIG samples don't contain the UNIX time. For those
     * the returned value will be 0LL.
     */
    static dsm_time_t getUnixTime(const Sample* samp);

    /**
     * fetch the pointer to the clock status in an IRIG sample.
     */
    static const unsigned char* getStatusPtr(const Sample* samp);

    static float get100HzBacklog(const Sample* samp);

    static const nidas::util::EndianConverter* lecvtr;
private:

    /**
     * @throws nidas::util::IOException
     **/
    void checkClock();

    dsm_sample_id_t _sampleId;

    int _nvars;

    int _nStatusPrints;

    int _slews[IRIG_MAX_DT_DIFF - IRIG_MIN_DT_DIFF + 1];

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
