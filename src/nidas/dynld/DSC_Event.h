// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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
#ifndef NIDAS_DYNLD_DSC_EVENT_H
#define NIDAS_DYNLD_DSC_EVENT_H

#include <nidas/core/DSMSensor.h>
#include <nidas/util/EndianConverter.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * Sensor support for a simple event detector.
 * This implementation supports a device that can
 * configured with a reporting period value ( e.g. 1 sec, 
 * or 1/10 sec).  Samples from the device are little-endian
 * 4 byte unsigned integer accumulated counts.
 * This class currently has hard-coded ioctl commands to
 * the gpio_mm device driver which supports a counter on a
 * Diamond GPIO card.
 */
class DSC_Event : public DSMSensor {

public:

    DSC_Event();

    ~DSC_Event();

    IODevice* buildIODevice() throw(nidas::util::IOException);

    SampleScanner* buildSampleScanner()
        throw(nidas::util::InvalidParameterException);

    /**
     * Open the device connected to the sensor.
     */
    void open(int flags) throw(nidas::util::IOException,
        nidas::util::InvalidParameterException);

    void init() throw(nidas::util::InvalidParameterException);
                                                                                
    /*
     * Close the device connected to the sensor.
     */
    void close() throw(nidas::util::IOException);

    void printStatus(std::ostream& ostr) throw();

    /**
     * Process a raw sample, which in this case means 
     * convert the input counts to a float.
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

private:

    dsm_sample_id_t _sampleId;

    const nidas::util::EndianConverter* _cvtr;

    /** No copying. */
    DSC_Event(const DSC_Event&);

    /** No assignment. */
    DSC_Event& operator=(const DSC_Event&);

};

}}	// namespace nidas namespace dynld

#endif
