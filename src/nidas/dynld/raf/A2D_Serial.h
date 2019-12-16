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

#ifndef _nidas_dynld_raf_a2d_serial_h_
#define _nidas_dynld_raf_a2d_serial_h_

#include <nidas/core/SerialSensor.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * A2D Serial Sensor.  This would be able to use the generic SerialSensor class
 * except for the need to manfacture time-stamps.  Data is sampled in the A2D
 * at exact intervals, but serial time-stamping is mediocre.  We want no time-lagging
 * downstream that might affect spectral characteristics.
 */
class A2D_Serial : public SerialSensor
{

public:
    A2D_Serial();
    ~A2D_Serial();

    /**
     * open the sensor and perform any intialization to the driver.
     */
    void open(int flags) throw(nidas::util::IOException);

    /**
     * Setup whatever is necessary for process method to work.
     */
    void init() throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();


protected:
    bool checkCkSum(const Sample * samp);

    /**
     * Is device receiving PPS.  We read it from header packet.
     */
    size_t _havePPS;

    size_t _sampleRate;
    size_t _deltaT;

    size_t _shortPacketCnt;
    size_t _badCkSumCnt;
    size_t _largeTimeStampOffset;
};

}}}                     // namespace nidas namespace dynld namespace raf

#endif
