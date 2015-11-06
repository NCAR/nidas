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

#ifndef NIDIS_DYNLD_TSI_CPC3772_H
#define NIDIS_DYNLD_TSI_CPC3772_H

#include <nidas/dynld/DSMSerialSensor.h>

namespace nidas { namespace dynld {

/**
 * Support for a TSI CPC3772 particle counter.
 */
class TSI_CPC3772: public DSMSerialSensor
{
public:

    TSI_CPC3772():
        DSMSerialSensor(),_deltaTusecs(USECS_PER_SEC/10),_rate(0)
    {
    }

    void addSampleTag(SampleTag* stag)
            throw(nidas::util::InvalidParameterException);

    /**
     * The CPC3772 puts out a 1 Hz sample of 10 values, which
     * are the individual 10Hz samples in a second. We override
     * the process method in order to break the sample
     * into 10 individual 10 Hz samples.
     */
    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

protected:

    unsigned int _deltaTusecs;

    int _rate;
};

}}	// namespace nidas namespace dynld

#endif
