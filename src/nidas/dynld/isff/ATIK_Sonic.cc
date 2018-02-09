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

#include "ATIK_Sonic.h"

#include <nidas/core/Variable.h>
#include <nidas/core/PhysConstants.h>
#include <nidas/core/Parameter.h>

#include <byteswap.h>

using namespace nidas::dynld::isff;
using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,ATIK_Sonic)

const float ATIK_Sonic::GAMMA_R = 403.242;

ATIK_Sonic::ATIK_Sonic():
    Wind3D(),
    _numOut(0),
    _spikeIndex(-1),
    _cntsIndex(-1),
    _expectedCounts(0),
    _diagThreshold(0.1),
    _maxShadowAngle(70.0 * M_PI / 180.0)
{
}

ATIK_Sonic::~ATIK_Sonic()
{
}

void ATIK_Sonic::parseParameters()
    throw(n_u::InvalidParameterException)
{
    Wind3D::parseParameters();

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    for ( ; pi != params.end(); ++pi) {
        const Parameter* parameter = *pi;

        if (parameter->getName() == "maxShadowAngle") {
            if (parameter->getType() != Parameter::FLOAT_PARAM ||
                    parameter->getLength() != 1)
                    throw n_u::InvalidParameterException(getName(),
                            "maxShadowAngle","must be one float");
            _maxShadowAngle = parameter->getNumericValue(0) * M_PI / 180.0;
        }
        else if (parameter->getName() == "expectedCounts") {
            if (parameter->getType() != Parameter::INT_PARAM ||
                    parameter->getLength() != 1)
                    throw n_u::InvalidParameterException(getName(),
                            "expectedCounts","must be one integer");
            _expectedCounts = (int) parameter->getNumericValue(0);
        }
        else if (parameter->getName() == "maxMissingFraction") {
            if (parameter->getType() != Parameter::FLOAT_PARAM ||
                    parameter->getLength() != 1)
                    throw n_u::InvalidParameterException(getName(),
                            "maxMissingFraction","must be one float");
            _diagThreshold = parameter->getNumericValue(0);
        }
        else if (parameter->getName() == "shadowFactor");
        else if (parameter->getName() == "orientation");
        else if (parameter->getName() == "despike");
        else if (parameter->getName() == "outlierProbability");
        else if (parameter->getName() == "discLevelMultiplier");
        else throw n_u::InvalidParameterException(getName(),
                        "unknown parameter", parameter->getName());
    }
}

void ATIK_Sonic::checkSampleTags()
    throw(n_u::InvalidParameterException)
{

    Wind3D::checkSampleTags();

    list<SampleTag*>& tags= getSampleTags();

    if (tags.size() != 1)
        throw n_u::InvalidParameterException(getName() +
                " must have one sample");

    const SampleTag* stag = tags.front();
    size_t nvars = stag->getVariables().size();
    /*
     * nvars
     * 7	u,v,w,tc,diag,spd,dir
     * 10	u,v,w,tc,diag,spd,dir,counts*3
     * 11	u,v,w,tc,diag,spd,dir,uflag,vflag,wflag,tcflag
     */

    if (_expectedCounts == 0 && stag->getRate() > 0.0)
        _expectedCounts = (int)rint(200.0 / stag->getRate());

    _diagIndex = 4;

    switch(nvars) {
    case 7:
        break;
    case 10:
        _cntsIndex = 7;
        break;
    case 11:
        _spikeIndex = 7;
        break;
    default:
        throw n_u::InvalidParameterException(getName() +
                " unsupported number of variables. Must be: u,v,w,tc,diag,spd,dir,[3 x counts or 4 x flags]]");
    }

    if (_spdIndex < 0 || _dirIndex < 0)
        throw n_u::InvalidParameterException(getName() +
                " ATIK cannot find speed or direction variables");

    _numOut = nvars;
    _numParsed =  7;    // u,v,w,tc,ucount,vcount,wcount

}
void ATIK_Sonic::transducerShadowCorrection(dsm_time_t, float* uvw) throw()
{
    if (_shadowFactor == 0.0) return;

    float nuvw[3];

    double spd = sqrt(uvw[0] * uvw[0] + uvw[1] * uvw[1] + uvw[2] * uvw[2]);

    /* If one component is missing, do we mark all as missing?
     * This should not be a common occurance, but since this data
     * is commonly averaged, it wouldn't be obvious in the averages
     * whether some values were not being shadow corrected. So we'll
     * let one NAN "spoil the barrel".
     */
    if (isnan(spd)) {
        for (int i = 0; i < 3; i++) uvw[i] = floatNAN;
        return;
    }

    for (int i = 0; i < 3; i++) {
        double x = uvw[i];
        double theta = acos(fabs(x)/spd);
        nuvw[i] = (theta > _maxShadowAngle ? x : x / (1.0 - _shadowFactor + _shadowFactor * theta / _maxShadowAngle));
    }

    memcpy(uvw,nuvw,3*sizeof(float));
}

void ATIK_Sonic::removeShadowCorrection(dsm_time_t, float* ) throw()
{
    // TODO. The shadow correction is somewhat difficult to invert,
    // though I think Tom has an approximation in his notes somewhere...
    return;
}

bool ATIK_Sonic::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{

    float uvwt[4];
    float counts[3] = {0.0,0.0,0.0};
    float diag = 0.0;

    if (getScanfers().size() > 0) {
        std::list<const Sample*> parseResults;
        SerialSensor::process(samp,parseResults);
        if (parseResults.empty()) return false;

        // result from base class parsing of ASCII
        const Sample* psamp = parseResults.front();

        unsigned int nvals = psamp->getDataLength();
        const float* pdata = (const float*) psamp->getConstVoidDataPtr();
        const float* pend = pdata + nvals;

        int i;
        for (i = 0; i < 3; i++) {
            int ix = _tx[i];
            if (ix < (signed) nvals) {
                float f = pdata[ix];
                if ( f < -9998.0 || f > 9998.0) uvwt[i] = floatNAN;
                else uvwt[i] = _sx[i] * f / 100.0;
            }
            else uvwt[i] = floatNAN;
        }
        // Sonic temperature, i=3
        if (i < (signed) nvals) {
            float f = pdata[i];
            if ( f < -9998.0 || f > 9998.0) uvwt[i] = floatNAN;
            else uvwt[i] = f / 100.0;
        }
        else uvwt[i] = floatNAN;

        pdata += 4;
        int miss_sum = 0;
        for (i = 0; i < 3 && pdata < pend; i++) {
            float f = counts[i] = *pdata++;
            int c = 0;
            if (!isnan(f)) c = (int) f;
            miss_sum += std::min(_expectedCounts - c,0);
            // cerr << "c=" << c << " expected=" << _expectedCounts << ", sum=" << miss_sum << endl;
        }
        for (; i < 3; i++) {
            counts[i] = floatNAN;
            miss_sum += _expectedCounts;
        }
        diag = (float) miss_sum / _expectedCounts * 3;

        if (diag > _diagThreshold) {
            for (i = 0; i < 4; i++) uvwt[i] = floatNAN;
        }
        psamp->freeReference();
    }
    else {
        // binary output into a char sample.
        unsigned int nvals = samp->getDataLength() / sizeof(short);
        const short* pdata = (const short*) samp->getConstVoidDataPtr();

        int i;
        for (i = 0; i < 3; i++) {
            int ix = _tx[i];
            if (ix < (signed) nvals) {
#if __BYTE_ORDER == __BIG_ENDIAN
                short f = bswap_16(pdata[ix]);
#else
                short f = pdata[ix];
#endif
                if ( f < -9998 || f > 9998) uvwt[i] = floatNAN;
                else uvwt[i] = _sx[i] * (float) f / 100.0;
            }
            else uvwt[i] = floatNAN;
        }
        // Sonic temperature, i=3
        if (i < (signed) nvals) {
#if __BYTE_ORDER == __BIG_ENDIAN
            short f = bswap_16(pdata[i]);
#else
            short f = pdata[i];
#endif
            if ( f < -9998 || f > 9998) uvwt[i] = floatNAN;
            else uvwt[i] = (float) f / 100.0;
        }
        else uvwt[i] = floatNAN;
        pdata += 4;
    }

    // compute speed of sound squared from temperature, removing
    // path curvature correction that was applied by the sonic,
    // because u and v may be corrected below.
    double uv2 = uvwt[0] * uvwt[0] + uvwt[1] * uvwt[1];
    double c2 = GAMMA_R * (uvwt[3] + KELVIN_AT_0C);
    c2 -= uv2;

    removeShadowCorrection(samp->getTimeTag(), uvwt);

    /*
     * Three situations:
     * 1. no spike detection
     *      getDespike() == false, _spikeIndex < 0
     * 2. spike detection, output spike indicators, but don't replace
     *      spike value with forecasted value
     *      getDespike() == false, _spikeIndex >= 0
     * 3. spike detection, output spike indicators, replace spikes
     *      with forecasted value
     *      getDespike() == true, _spikeIndex >= 0
     */


    bool spikes[4] = {false,false,false,false};
    if (getDespike() || _spikeIndex >= 0) {
        despike(samp->getTimeTag(),uvwt,4,spikes);
    }

    transducerShadowCorrection(samp->getTimeTag(), uvwt);

    /*
     * Recompute Tc with speed of sound corrected
     * for new path curvature (before rotating from
     * instrument coordinates).
     */
    uv2 = uvwt[0] * uvwt[0] + uvwt[1] * uvwt[1];
    c2 += uv2;
    uvwt[3] = c2 / GAMMA_R - KELVIN_AT_0C;

    offsetsTiltAndRotate(samp->getTimeTag(),uvwt);

    // new sample
    SampleT<float>* wsamp = getSample<float>(_numOut);
    wsamp->setTimeTag(samp->getTimeTag());
    wsamp->setId(_sampleId);

    float* dout = wsamp->getDataPtr();

    memcpy(dout,uvwt,4*sizeof(float));

    if (_spdIndex >= 0 && _spdIndex < (signed)_numOut)
        dout[_spdIndex] = sqrt(dout[0] * dout[0] + dout[1] * dout[1]);

    if (_dirIndex >= 0 && _dirIndex < (signed)_numOut) {
        float dr = atan2f(-dout[0],-dout[1]) * 180.0 / M_PI;
        if (dr < 0.0) dr += 360.;
        dout[_dirIndex] = dr;
    }

    if (_diagIndex >= 0) dout[_diagIndex] = diag;
    if (_cntsIndex >= 0) memcpy(dout + _cntsIndex,counts,3 * sizeof(float));
    if (_spikeIndex >= 0) {
        for (int i = 0; i < 4; i++)
            dout[i + _spikeIndex] = (float) spikes[i];
    }
    results.push_back(wsamp);
    return true;
}
