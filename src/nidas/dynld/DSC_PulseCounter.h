// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_DYNLD_DSC_PULSECOUNTER_H
#define NIDAS_DYNLD_DSC_PULSECOUNTER_H

#include <nidas/core/DSMSensor.h>
#include <nidas/util/EndianConverter.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * Sensor support for a simple pulse counter device.
 * This implementation supports a device that can
 * configured with a reporting period value ( e.g. 1 sec, 
 * or 1/10 sec).  Samples from the device are little-endian
 * 4 byte unsigned integer counts at the reporting period.
 * This class currently has hard-coded ioctl commands to
 * the dmd_mmat device driver which supports a counter on a
 * Diamond DMMAT card.
 */
class DSC_PulseCounter : public DSMSensor {

public:

    DSC_PulseCounter();
    ~DSC_PulseCounter();

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
     * @throws nidas::util::InvalidParameterException
     **/
    void validate();

    /**
     * @throws nidas::util::InvalidParameterException)
     **/
    void init();
                                                                                
    /**
     * Close the device connected to the sensor.
     *
     * @throws nidas::util::IOException
     **/
    void close();

    /**
     * @throw()
     **/
    void printStatus(std::ostream& ostr);

    /**
     * Process a raw sample, which in this case means 
     * convert the input counts to a float.
     *
     * @throw()
     */
    bool process(const Sample*,std::list<const Sample*>& result);

private:

    dsm_sample_id_t _sampleId;

    int _msecPeriod;

    const nidas::util::EndianConverter* _cvtr;

    /** No copying. */
    DSC_PulseCounter(const DSC_PulseCounter&);

    /** No assignment. */
    DSC_PulseCounter& operator=(const DSC_PulseCounter&);

};

}}	// namespace nidas namespace dynld

#endif
