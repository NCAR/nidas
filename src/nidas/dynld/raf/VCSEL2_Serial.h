// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2013, Copyright University Corporation for Atmospheric Research
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

#ifndef _nidas_dynld_raf_vcsel2_serial_h_
#define _nidas_dynld_raf_vcsel2_serial_h_

#include <nidas/core/SerialSensor.h>
#include <nidas/core/DerivedDataClient.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * VCSEL2 Serial Sensor.  A one off test used for the IDEAS-4-GV test project.
 */
class VCSEL2_Serial : public SerialSensor, public DerivedDataClient
{

public:
    VCSEL2_Serial();
    ~VCSEL2_Serial();

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


protected:
    /**
     * Send the ambient temperature up to the VCSEL.
     * @param atx is the ambient temperature to send.
     * @param psx is the static pressure to send.
     */
    virtual void sendTemperaturePressure(float atx, float psx) throw(nidas::util::IOException);

    /**
     * How often to send ATX to the VCSEL.
     */
    int _atxRate;

};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
