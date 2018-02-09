// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
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

#ifndef _nidas_dynld_raf_vcsel_serial_h_
#define _nidas_dynld_raf_vcsel_serial_h_

#include <nidas/core/SerialSensor.h>
#include <nidas/core/DerivedDataClient.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * VCSEL Serial Sensor.  This would be able to use the generic SerialSensor class
 * except for the need to send Ambient Temperature up to the instrument.
 */
class VCSEL_Serial : public SerialSensor, public DerivedDataClient
{

public:
    VCSEL_Serial();
    ~VCSEL_Serial();

    /**
     * open the sensor and perform any intialization to the driver.
     */
    void open(int flags) throw(nidas::util::IOException);

    void close() throw(nidas::util::IOException);

    int getATXRate() const { return _atxRate; }

    void setATXRate(int val) { _atxRate = val; }
    
    virtual void
    derivedDataNotify(const nidas::core:: DerivedDataReader * s)
        throw();

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();


protected:
    /**
     * Send the ambient temperature up to the VCSEL.
     * @param atx is the ambient temperature to send.
     */
    virtual void sendAmbientTemperature(float atx) throw(nidas::util::IOException);

    /**
     * How often to send ATX to the VCSEL.
     */
    int _atxRate;

    /**
     * 25Hz sample index counter.  We manufacture time for this instrument since we
     * know that the data is actually exactly spaced by 40 milliseconds.
     */
    int _hz_counter;
};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
