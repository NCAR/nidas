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

#include <nidas/dynld/isff/CU_Coldwire.h>
#include <nidas/util/EndianConverter.h>

#include <nidas/core/Variable.h>

using namespace nidas::dynld::isff;
using namespace nidas::core;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,CU_Coldwire)

static const n_u::EndianConverter* fromBig =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::EC_BIG_ENDIAN);


CU_Coldwire::CU_Coldwire():
    _numOut(0),
    _sampleId(0),
    _badChecksums(0)
{
}

CU_Coldwire::~CU_Coldwire()
{
}

void CU_Coldwire::validate()
    throw(n_u::InvalidParameterException)
{

    list<SampleTag*>& tags= getSampleTags();

    if (tags.size() != 1)
        throw n_u::InvalidParameterException(getName() +
                " must have one sample");

    const SampleTag* stag = tags.front();
    _numOut = stag->getVariables().size();
    _sampleId = stag->getId();

}

unsigned char CU_Coldwire::checksum(const unsigned char* buf, const unsigned char* eob)
{
    unsigned char sum = 0;
    for ( ; buf < eob; buf++) {
        sum += *buf;
    }
    return ~sum;
}

bool CU_Coldwire::reportBadChecksum()
{
    if (!(_badChecksums++ % 1000))
            WLOG(("%s (CU_Coldwire): %d checksum errors so far",
                        getName().c_str(),_badChecksums));
    return false;
}

bool CU_Coldwire::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{
    const unsigned char* buf0 = (const unsigned char*) samp->getConstVoidDataPtr();
    unsigned int len = samp->getDataByteLength();

    if (len < 87) return false;  // 
    const unsigned char* eptr = buf0 + 86;   // point to checksum

    unsigned char sval = *eptr;

    unsigned char cval = checksum(buf0+3,eptr);

    // cerr << "len=" << len << ", checksum=" << hex << (int)sval << ", calc'd=" << (int)cval << ", diff=" << (int)(unsigned char)(sval - cval) << dec << endl;

    if (cval != sval) return reportBadChecksum();

    // new sample
    SampleT<float>* psamp = getSample<float>(_numOut);

    psamp->setTimeTag(samp->getTimeTag());
    psamp->setId(_sampleId);

    float* dout = psamp->getDataPtr();
    float* dend = dout + _numOut;

    if (dout == dend) return true;

    results.push_back(psamp);
    for (float* dtmp = dout; dtmp < dend; ) *dtmp++ = floatNAN;

    const unsigned char* bptr = buf0 + 24;

    // Pressure,  4 byte unsigned int, divide by 1000 to get mbar
    if (bptr + sizeof(int) > eptr) return true;
    *dout++ = (float)fromBig->uint32Value(bptr) / 1000.;
    bptr += sizeof(int);
    if (dout == dend) return true;

    // 10 cold wire values, sampled at 100Hz, with 10 values in a 10 Hz sample
    double cwsum = 0;
    const double vscale = 8.192 / 65536;    // scale from counts to voltage
    for (int i = 0; i < 10; i++) {
        if (bptr + sizeof(short) > eptr) return true;

        float val = fromBig->int16Value(bptr) * vscale;
        *dout++ = val;
        bptr += sizeof(short);
        cwsum += val;
        if (dout == dend) return true;
    }
    // average of cold wire values
    *dout++ = cwsum / 10;
    if (dout == dend) return true;

    // humidity, convert to volts
    if (bptr + sizeof(short) > eptr) return true;
    float humv = fromBig->int16Value(bptr) * vscale;
    *dout++ = humv;
    bptr += sizeof(short);
    if (dout == dend) return true;

    // temperature, calibrated, degC/128 counts, 16 bit integer
    if (bptr + sizeof(short) > eptr) return true;
    float temp = fromBig->int16Value(bptr) / 128.0;
    *dout++ = temp;
    bptr += sizeof(short);
    if (dout == dend) return true;

    // Convert humidity voltage to cal'd value
    *dout++ = ((humv / 2.0 + 1.555) / 3.300 - 0.1515) / 0.00636 / (1.0546 - 0.00216 * temp);

    return true;
}
