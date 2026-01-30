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

#include "CSAT3_Sonic.h"

#include <nidas/Config.h>
#include <nidas/util/EndianConverter.h>
#include <nidas/core/Sample.h>
#include <nidas/core/VariableIndex.h>

class TimetagAdjuster;

namespace nidas { namespace dynld { namespace isff {

/**
 * CSI_IRGA_Fields holds the fields reported by the CSI IRGA Sonic in their
 * native types.
 */
struct CSI_IRGA_Fields
{
    const float floatNAN = nidas::core::floatNAN;

    float u { floatNAN };
    float v { floatNAN };
    float w { floatNAN };
    float tc { floatNAN };
    u_int32_t diagbits { 0 };
    float h2o { floatNAN };
    float co2 { floatNAN };
    u_int32_t irgadiag { 0 };
    float Tirga { floatNAN };
    float Pirga { floatNAN };
    float SSco2 { floatNAN };
    float SSh2o { floatNAN };
    float dPirga { floatNAN };
    float Tsource { floatNAN };
    float Tdetector { floatNAN };

    /// Number of valid fields (when unpacked from a sonic message)
    int nvals { 0 };

    template <typename F>
    void visit(F& f);
};



/**
 * A class for making sense of data from a Campbell Scientific
 * IRGASON integrated Gas Analyzer and 3D sonic anemometer.
 */
class CSI_IRGA_Sonic: public CSAT3_Sonic
{
public:

    CSI_IRGA_Sonic();

    ~CSI_IRGA_Sonic();

    void open(int flags);

    void parseParameters();

    void checkSampleTags();

    bool process(const Sample* samp,std::list<const Sample*>& results);

    virtual void updateAttributes() override;

    /**
     * Unpack the binary buffer @p buf into @p fields, up until the end of
     * buffer @p eob.  Return the number of fields successfully unpacked. If
     * the CRC signature check fails or the buffer is too short, the return
     * value is zero.
     */
    int
    unpackBinary(const char* buf, const char* eob, CSI_IRGA_Fields& fields);

    /**
     * Pack @p fields into the binary buffer @p buf.  Return the number of
     * fields packed, which should be equal to the number of valid fields in
     * @p fields (nvals). The CRC signature and termination bytes are added at
     * the end of the buffer.
     */
    int
    packBinary(const CSI_IRGA_Fields& fields, std::vector<char>& buf);

    /**
     * Calculate the CRC signature of a data record. From EC150 manual.
     */
    static unsigned short signature(const unsigned char* buf, const unsigned char* eob);

private:

    typedef nidas::core::VariableIndex VariableIndex;

    bool reportBadCRC();

    /**
     * Requested number of output variables.
     */
    unsigned int _numOut;

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

    /// integer mask for IRGA diagnostic bits to ignore
    unsigned int _irgaDiagMask;

    VariableIndex _irgaDiag;
    VariableIndex _h2o;
    VariableIndex _co2;
    VariableIndex _Pirga;
    VariableIndex _Tirga;

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

    nidas::core::TimetagAdjuster* _ttadjust;

    /// No copying
    CSI_IRGA_Sonic(const CSI_IRGA_Sonic &);

    /// No assignment.
    CSI_IRGA_Sonic& operator = (const CSI_IRGA_Sonic &);

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
