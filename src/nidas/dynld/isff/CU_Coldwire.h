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

#ifndef NIDAS_DYNLD_ISFF_CU_COLDWIRE_H
#define NIDAS_DYNLD_ISFF_CU_COLDWIRE_H

#include <nidas/core/SerialSensor.h>
#include <nidas/core/Sample.h>

namespace nidas { namespace dynld { namespace isff {

using nidas::core::Sample;
using nidas::core::dsm_sample_id_t;

/**
 * A class for parsing the binary output of Dale Lawrence's cold-wire
 * sensor package.
 */
class CU_Coldwire: public nidas::core::SerialSensor
{
public:

    CU_Coldwire();

    ~CU_Coldwire();

    void validate()
            throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const nidas::core::Sample*>& results)
    	throw();

    /**
     * Calculate the checksum of a data record.
     */
    static unsigned char checksum(const unsigned char* buf, const unsigned char* eob);

private:

    bool reportBadChecksum();

    /**
     * Requested number of output variables.
     */
    int _numOut;

    /**
     * Output sample id of P,RHraw,T, RH sample
     */
    dsm_sample_id_t _sampleId;

    /**
     * Output sample id for 100Hz CW values
     */
    dsm_sample_id_t _sampleIdCW;

    /**
     * Counter of the number of records with incorrect checksums
     */
    unsigned int _badChecksums;

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
