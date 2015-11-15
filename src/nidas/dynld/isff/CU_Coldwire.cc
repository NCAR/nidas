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
    _nsamps(0),
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

    if (tags.empty())
        throw n_u::InvalidParameterException(getName() +
                " must be at least one sample");
    _nsamps = tags.size();
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

    // First mode, len=87 bytes,  0x7e + 86 more
    // Second mode, len=85 bytes,  0x7e + 84 more
    if (len < 85) return false;  // 
    const unsigned char* eptr = buf0 + len - 1;   // point to checksum

    unsigned char sval = *eptr;

    unsigned char cval = checksum(buf0+3,eptr);

    // cerr << "len=" << len << ", checksum=" << hex << (int)sval << ", calc'd=" << (int)cval << ", diff=" << (int)(unsigned char)(sval - cval) << dec << endl;

    if (cval != sval) return reportBadChecksum();

    list<SampleTag*>& tags = getSampleTags();
    list<SampleTag*>::const_iterator si = tags.begin();

    SampleTag* stag = *si;
    SampleTag* stag2 = 0;
    if (_nsamps > 1) {
        si++;
        stag2 = *si;
    }

    unsigned int nvars = stag->getVariables().size();

    // new sample
    SampleT<float>* psamp = getSample<float>(nvars);

    dsm_time_t timetag = samp->getTimeTag() - 100 * USECS_PER_MSEC;
    psamp->setTimeTag(timetag);
    psamp->setId(stag->getId());
    results.push_back(psamp);

    float* dout = psamp->getDataPtr();
    float* dend = dout + nvars;

    for (float* dtmp = dout; dtmp < dend; ) *dtmp++ = floatNAN;

    // For 87 byte samples, pressure is bytes 24-27, 4 byte unsigned int.
    // For 85 byte samples, pressure is bytes 22-25.
    unsigned int skip = 24;
    if (len == 85) skip = 22;

    const unsigned char* bptr = buf0 + skip;
    if (bptr + sizeof(int) > eptr) return true;
    // Divide by 1000 to get mbar
    *dout++ = (float)fromBig->uint32Value(bptr) / 1000.;
    bptr += sizeof(int);
    if (dout == dend) return true;

    const double vscale = 8.192 / 65536;    // scale from counts to voltage

    // 10 cold wire values, 16 bit signed ints, sampled at 100Hz
    for (int i = 0; i < 10; i++) {
        if (bptr + sizeof(short) > eptr) return true;

        if (stag2) {
            /* second sample, for 100Hz coldwire voltages.
             */
            float val = fromBig->int16Value(bptr) * vscale;
            SampleT<float>* cwsamp = getSample<float>(1);
            cwsamp->setTimeTag(timetag);
            timetag += 10 * USECS_PER_MSEC;
            cwsamp->setId(stag2->getId());
            float* cwout = cwsamp->getDataPtr();
            *cwout = val;
            results.push_back(cwsamp);
        }
        else if (i == 0) {
            /* if one sample is configured, then
             * the first voltage is the cold wire,
             * scaled by 10000.
             */
            float val = fromBig->int16Value(bptr) / 10000.0;
            *dout++ = val;
            if (dout == dend) return true;
        }
        else if (i == 1) {
            /* if one sample is configured, then
             * the second voltage is the hot wire (unsigned int)
             * scaled by 10000, and the rest of the wire
             * voltages are ignored.
             */
            float val = fromBig->uint16Value(bptr) / 10000.0;
            *dout++ = val;
            if (dout == dend) return true;
        }

        bptr += sizeof(short);
    }

    // humidity, 16 bit signed int, convert to volts
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
