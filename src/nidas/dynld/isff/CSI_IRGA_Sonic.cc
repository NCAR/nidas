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

#include "CSI_IRGA_Sonic.h"

#include <nidas/core/Variable.h>
#include <nidas/core/Sample.h>
#include <nidas/core/AsciiSscanf.h>

#include <limits>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,CSI_IRGA_Sonic)

CSI_IRGA_Sonic::CSI_IRGA_Sonic():
    CSAT3_Sonic(),
    _numOut(0),
    _timeDelay(0),
    _badCRCs(0),
    _irgaDiagIndex(-1),
    _h2oIndex(-1),
    _co2Index(-1),
    _binary(false),
    _endian(nidas::util::EndianConverter::EC_LITTLE_ENDIAN),
    _converter(0)
{
}

CSI_IRGA_Sonic::~CSI_IRGA_Sonic()
{
}

void CSI_IRGA_Sonic::open(int flags)
    throw(n_u::IOException,n_u::InvalidParameterException)
{
    SerialSensor::open(flags);
}

void CSI_IRGA_Sonic::parseParameters() throw(n_u::InvalidParameterException)
{
    CSAT3_Sonic::parseParameters();

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    for ( ; pi != params.end(); ++pi) {
        const Parameter* parameter = *pi;

        if (parameter->getName() == "bandwidth") {
            if (parameter->getType() != Parameter::FLOAT_PARAM ||
                parameter->getLength() != 1)
                throw n_u::InvalidParameterException(getName(),
                        "bandwidth parameter","must be one float value, in Hz");
            float bandwidth = parameter->getNumericValue(0);
            if (bandwidth <= 0.0)
                throw n_u::InvalidParameterException(getName(),
                        "bandwidth parameter","must be positive value in Hz");
            _timeDelay = (int)(rintf(25.0 / bandwidth * 160.0) * USECS_PER_MSEC);
        }
    }
    if (_timeDelay == 0.0)
        WLOG(("%s: IRGASON/EC150 bandwidth not specified. Time delay will be set to 0 ms",
                    getName().c_str()));
}

void CSI_IRGA_Sonic::checkSampleTags() throw(n_u::InvalidParameterException)
{

    // Don't call CSAT3_Sonic::checkSampleTags, as it makes different
    // assumptions about the expected number variables.
    Wind3D::checkSampleTags();

    list<SampleTag*>& tags= getSampleTags();

    if (tags.size() != 1)
        throw n_u::InvalidParameterException(getName() +
                " must have one sample");

    const SampleTag* stag = tags.front();
    _numOut = stag->getVariables().size();

    _numParsed = _numOut;
    /*
     * variable sequence
     * u,v,w,tc,diag,other irga variables,ldiag,spd,dir
     * ldiag, spd and dir can be in any order, as long
     * as they are last. The code checks for ldiag,spd and dir
     * variable names beginning with exactly those strings,
     * which isn't a great idea.
     */

    if (_spdIndex >= 0) _numParsed--; // derived, not parsed
    if (_dirIndex >= 0) _numParsed--; // derived, not parsed
    if (_ldiagIndex >= 0) _numParsed--; // derived, not parsed

    VariableIterator vi = stag->getVariableIterator();
    for (int i = 0; vi.hasNext(); i++) {
        const Variable* var = vi.next();
        const string& vname = var->getName();
        if (vname.length() > 7 && vname.substr(0,8) == "irgadiag")
            _irgaDiagIndex = i;
        else if (vname.length() > 2 && vname.substr(0,3) == "h2o")
            _h2oIndex = i;
        else if (vname.length() > 2 && vname.substr(0,3) == "co2")
            _co2Index = i;
    }

    if (_numParsed  < 5)
        throw n_u::InvalidParameterException(getName() +
                ": expect at least 5 variables in sample: u,v,w,tc,diag");

    bool ok = true;
    /* Make sure derived quantities are last. */
    if (_spdIndex >= 0 && _numOut - _spdIndex > 3) ok = false;
    if (_dirIndex >= 0 && _numOut - _dirIndex > 3) ok = false;
    if (_ldiagIndex >= 0 && _numOut - _ldiagIndex > 3) ok = false;

    if (!ok)
        throw n_u::InvalidParameterException(getName() +
                " CSI_IRGA_Sonic speed, direction and ldiag variables should be at the end of the list");

     const std::list<AsciiSscanf*>& sscanfers = getScanfers();
     if (sscanfers.empty()) {
         _binary = true;
         _converter = n_u::EndianConverter::getConverter(_endian,
            n_u::EndianConverter::getHostEndianness());
     }
}

unsigned short CSI_IRGA_Sonic::signature(const unsigned char* buf, const unsigned char* eob)
{
    /* The last field of the EC150 output is a CRC. From the EC150 manual,
     * here is how it is calculated.
     */

    unsigned char msb, lsb;
    unsigned char b;
    unsigned short seed = 0xaaaa;
    msb = seed >> 8;
    lsb = seed;
    for(; buf < eob; ) {
        b = (lsb << 1) + msb + *buf++;
        if( lsb & 0x80 ) b++;
        msb = lsb;
        lsb = b;
    }
    return (unsigned short)((msb << 8) + lsb);
}

bool CSI_IRGA_Sonic::reportBadCRC()
{
    if (!(_badCRCs++ % 1000))
            WLOG(("%s (CSI_IRGA_Sonic): %d CRC signature errors so far",
                        getName().c_str(),_badCRCs));
    return false;
}

bool CSI_IRGA_Sonic::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{

    const char* buf = (const char*) samp->getConstVoidDataPtr();
    unsigned int len = samp->getDataByteLength();
    const char* eob = buf + len;
    const char* bptr = eob;
    dsm_time_t wsamptime = samp->getTimeTag() - _timeDelay;

    // Check that the calculated CRC signature agrees with the value in the data record.
    unsigned short sigval;  // signature value in data buffer
    if (_binary) {
        bptr -= sizeof(short);
        if (bptr < buf) return reportBadCRC();
        if (::memcmp(bptr,"\x55\xaa",2)) return reportBadCRC();    // 55AA not found

        bptr -= sizeof(short);  // 2 byte signature
        if (bptr < buf) return reportBadCRC();
        sigval = _converter->uint16Value(bptr);
        eob = bptr;
    }
    else {
        bptr--;
        if (*bptr == '\0') bptr--;
        if (bptr >= buf && ::isspace(*bptr)) bptr--;   // carriage return, linefeed
        if (bptr >= buf && ::isspace(*bptr)) bptr--;
        if (bptr - 4 < buf) return false;

        bptr -= 4;
        if (*bptr != ',') return reportBadCRC();

        char* resptr;
        sigval = (unsigned short) ::strtol(bptr+1,&resptr,16);
        if (resptr != bptr + 5) return reportBadCRC();
    }

    // calculated signature from buffer contents
    unsigned short calcsig = signature((const unsigned char*)buf,(const unsigned char*)bptr);

    if (calcsig != sigval) return reportBadCRC();

    const Sample* psamp = 0;
    unsigned int nvals;
    const float* pdata;

    const unsigned int nbinvals = _numOut - 3;  // requested variables, except final 3 derived

    vector<float> pvector(nbinvals);

    if (_binary) {
        bptr = buf;
        for (nvals = 0; bptr + sizeof(float) <= eob && nvals < 4; ) {
            pvector[nvals++] = _converter->floatValue(bptr);  // u,v,w,tc
            bptr += sizeof(float);
        }
        if (bptr + sizeof(uint32_t) <= eob) {
            pvector[nvals++] = _converter->uint32Value(bptr);   // diagnostic
            bptr += sizeof(int);
        }
        for (int i = 0; bptr + sizeof(float) <= eob && i < 2 && nvals < nbinvals; i++) {
            pvector[nvals++] = _converter->floatValue(bptr);      // co2, h2o
            bptr += sizeof(float);
        }
        if (bptr + sizeof(uint32_t) <= eob && nvals < nbinvals) {
            pvector[nvals++] = _converter->uint32Value(bptr);   // IRGA diagnostic
            bptr += sizeof(int);
        }
        for ( ; bptr + sizeof(float) <= eob && nvals < nbinvals; ) {
            // cell temp and pressure, co2 sig, h2o sig, diff press, source temp, detector temp
            pvector[nvals++] = _converter->floatValue(bptr);
            bptr += sizeof(float);
        }
#ifdef UNPACK_COUNTER
        if (bptr + sizeof(uint32_t) <= eob) {
            unsigned int counter = _converter->uint32Value(bptr);   // counter
            bptr += sizeof(int);
            ILOG(("%s: counter=%u",getName().c_str(),counter));
        }
#endif

        for ( ; nvals < nbinvals; nvals++) pvector[nvals] = floatNAN;
        pdata = &pvector[0];

        // Also apply any conversions or calibrations, same as is done by
        // the base class process() for ascii sensors.
        if (getApplyVariableConversions()) {
            list<SampleTag*>& tags= getSampleTags();
            SampleTag* stag = tags.front();
            for (unsigned int iv = 0; iv < nbinvals; ++iv)
            {
                Variable* var = stag->getVariables()[iv];
                VariableConverter* conv = var->getConverter();
                if (conv)
                {
                    pvector[iv] = conv->convert(wsamptime, pvector[iv]);
                }
            }
        }
    }
    else {
        std::list<const Sample*> parseResults;

        SerialSensor::process(samp,parseResults);

        if (parseResults.empty()) return false;

        // result from base class parsing of ASCII
        psamp = parseResults.front();

        nvals = psamp->getDataLength();
        pdata = (const float*) psamp->getConstVoidDataPtr();
    }

    const float* pend = pdata + nvals;

    // u,v,w,tc,diag
    float uvwtd[5];

    // sonic diagnostic value
    bool diagOK = false;
    if (nvals > 4) {
        uvwtd[4] = pdata[4];
        if (uvwtd[4] == 0.0) diagOK = true;
    }
    else uvwtd[4] = floatNAN;

    for (unsigned int i = 0; i < 3; i++) {
        if (diagOK && i < nvals) {
            // Sonic puts out "NAN" for missing values, which 
            // should have been parsed above into a float nan.
            uvwtd[i] = pdata[i];
        }
        else uvwtd[i] = floatNAN;
    }

    // tc
    if (nvals > 3 && diagOK) uvwtd[3] = pdata[3];
    else uvwtd[3] = floatNAN;

    pdata += sizeof(uvwtd)/sizeof(uvwtd[0]);

    if (getDespike()) {
        bool spikes[4] = {false,false,false,false};
        despike(samp->getTimeTag(),uvwtd,4,spikes);
    }

#ifdef HAVE_LIBGSL
    // apply shadow correction before correcting for unusual orientation
    transducerShadowCorrection(samp->getTimeTag(),uvwtd);
#endif

    applyOrientation(samp->getTimeTag(), uvwtd);

    offsetsTiltAndRotate(samp->getTimeTag(), uvwtd);

    // new sample
    SampleT<float>* wsamp = getSample<float>(_numOut);

    wsamp->setTimeTag(wsamptime);
    wsamp->setId(_sampleId);

    float* dout = wsamp->getDataPtr();
    float* dend = dout + _numOut;
    float *dptr = dout;

    memcpy(dptr,uvwtd,sizeof(uvwtd));
    dptr += sizeof(uvwtd) / sizeof(uvwtd[0]);

    for ( ; pdata < pend && dptr < dend; ) *dptr++ = *pdata++;
    for ( ; dptr < dend; ) *dptr++ = floatNAN;

    // logical diagnostic value: 0=OK,1=bad
    if (_ldiagIndex >= 0) dout[_ldiagIndex] = (float) !diagOK;

    if (_spdIndex >= 0 && _spdIndex < (signed)_numOut) {
        dout[_spdIndex] = sqrt(uvwtd[0] * uvwtd[0] + uvwtd[1] * uvwtd[1]);
    }
    if (_dirIndex >= 0 && _dirIndex < (signed)_numOut) {
        float dr = atan2f(-uvwtd[0],-uvwtd[1]) * 180.0 / M_PI;
        if (dr < 0.0) dr += 360.;
        dout[_dirIndex] = dr;
    }

    // screen h2o and co2 values when the IRGA diagnostic value is non-zero.
    // If _irgaDiagIndex is -1, then we're not checking against it.
    bool irgaOK = (_irgaDiagIndex < 0);
    if (_irgaDiagIndex >= 0) irgaOK = (dout[_irgaDiagIndex] == 0.0);
    if (!irgaOK) {
        if (_h2oIndex >= 0) dout[_h2oIndex] = floatNAN;
        if (_co2Index >= 0) dout[_co2Index] = floatNAN;
    }

    if (psamp) psamp->freeReference();

    results.push_back(wsamp);
    return true;
}
