// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_ISS_TILTSENSOR_H
#define NIDAS_DYNLD_ISS_TILTSENSOR_H

#include <nidas/dynld/DSMSerialSensor.h>

namespace nidas { namespace dynld { namespace iss {

using namespace nidas::core;

using nidas::dynld::DSMSerialSensor;
using nidas::util::IOException;
using nidas::util::InvalidParameterException;

/**
 * CXTILT02 2-axis tilt sensor from Crossbow.
 */
class TiltSensor: public DSMSerialSensor
{

public:

    TiltSensor();

    ~TiltSensor();

    void
    addSampleTag(SampleTag* stag) throw(InvalidParameterException);

    bool
    process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    void
    fromDOMElement(const xercesc::DOMElement* node)
	throw(InvalidParameterException);

protected:

    int checksumFailures;

    dsm_sample_id_t sampleId;

};

}}}	// namespace nidas namespace dynld namespace iss

#endif // NIDAS_DYNLD_ISS_TILTSENSOR_H
