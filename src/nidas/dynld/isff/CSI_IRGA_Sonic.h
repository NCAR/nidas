// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2012, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_ISFF_CSI_IRGA_SONIC_H
#define NIDAS_DYNLD_ISFF_CSI_IRGA_SONIC_H

#include "SonicAnemometer.h"

#include <nidas/util/EndianConverter.h>

namespace nidas { namespace dynld { namespace isff {

/**
 * A class for making sense of data from a Campbell Scientific
 * IRGASON integrated Gas Analyzer and 3D sonic anemometer.
 */
class CSI_IRGA_Sonic: public SonicAnemometer
{
public:

    CSI_IRGA_Sonic();

    ~CSI_IRGA_Sonic();

    void validate()
            throw(nidas::util::InvalidParameterException);

    void validateSscanfs() throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    /**
     * Calculate the CRC signature of a data record. From EC150 manual.
     */
    static unsigned short signature(const unsigned char* buf, const unsigned char* eob);

private:

    bool reportBadCRC();

    /**
     * Requested number of output variables.
     */
    unsigned int _numOut;

    /**
     * Index in output sample of ldiag value.
     */
    unsigned int _ldiagIndex;

    /**
     * If user requests wind speed, variable name "spd", its index in the output sample.
     */
    unsigned int _spdIndex;

    /**
     * If user requests wind direction, variable name "dir", its index in the output sample.
     */
    unsigned int _dirIndex;

    /**
     * Output sample id
     */
    dsm_sample_id_t _sampleId;

    /**
     * Index transform vector for wind components.
     * Used for unusual sonic orientations, as when the sonic
     * is hanging down, when the usual sonic w axis becomes the
     * new u axis, u becomes w and v becomes -v.
     */
    int _tx[3];

    /**
     * Wind component sign conversion. Also used for unusual sonic
     * orientations, as when the sonic is hanging down, and the sign
     * of v is flipped.
     */
    int _sx[3];

    /**
     * Filter time delay, depends on the selected bandwidth.
     * From the Campbell Scientific manual: "EC150 CO2 and H2O Open-Path
     * Gas Analyzer and EC100 Electronics with Optional CSAT3A 3D
     * Sonic Anemometer".
     * bandwidth(Hz)    delay(ms)
     *    5                800
     *    10               400
     *    12.5             320
     *    20               200
     *    25               160
     */
    int _timeDelay;

    /**
     * Counter of the number of records with incorrect CRC signatures.
     */
    unsigned int _badCRCs;

    unsigned int _irgaDiagIndex;

    unsigned int _h2oIndex;

    unsigned int _co2Index;

    /**
     * Campbell has provided custom firmware on the EC100 logger box so that
     * it can generate binary values (IEEE floats and 4-byte integers)
     * instead of ASCII.
     */
    bool _binary;

    /**
     * Endian-ness of binary values.
     */
    nidas::util::EndianConverter::endianness _endian;

    /**
     * Converter for binary values.
     */
    const nidas::util::EndianConverter* _converter;

    /// No copying
    CSI_IRGA_Sonic(const CSI_IRGA_Sonic &);

    /// No assignment.
    CSI_IRGA_Sonic& operator = (const CSI_IRGA_Sonic &);

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
