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

#ifndef _nidas_dynld_raf_alicatsdi_h
#define _nidas_dynld_raf_alicatsdi_h

#include <nidas/core/SerialSensor.h>
#include <nidas/core/DerivedDataClient.h>
#include <nidas/core/Parameter.h>

#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Alicat Flow Controller, used for the SDI.
 */
class AlicatSDI : public SerialSensor, public DerivedDataClient
{

public:
    AlicatSDI();
    ~AlicatSDI();

    /**
     * open the sensor and perform any intialization to the instrument.
     */
    void open(int flags) throw(nidas::util::IOException);

    /**
     * read XML for specific parameters.
     */
    void validate() throw(nidas::util::InvalidParameterException);

    void close() throw(nidas::util::IOException);

    void derivedDataNotify(const nidas::core:: DerivedDataReader * s) throw();


protected:
    /**
     * Send the ambient temperature up to the VCSEL.
     * @param atx is the ambient temperature to send.
     * @param psx is the static pressure to send.
     */
    float computeFlow();
    void sendFlow(float flow) throw(nidas::util::IOException);

    /**
     * maintain a running average of tas.  nTASav comes from XML.
     */
    int _nTASav;

    /**
     * Use a vector as a circular buffer.
     */
    std::vector<float> _tas;
    int _tasIdx;
    float *_tasWeight;

    int _Qmin, _Qmax;

    float _Qfac;

private:

    /** No copying. */
    AlicatSDI(const AlicatSDI&);

    /** No copying. */
    AlicatSDI& operator=(const AlicatSDI&);

};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
