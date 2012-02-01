// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate: 2011-11-16 15:03:17 -0700 (Wed, 16 Nov 2011) $

   $LastChangedRevision: 6326 $

   $LastChangedBy: maclean $

   $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/isff/ATIK_Sonic.cc $

*/

#include <nidas/dynld/isff/ATIK_Sonic.h>

#include <nidas/core/Variable.h>
#include <nidas/core/PhysConstants.h>

#include <byteswap.h>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,ATIK_Sonic)

ATIK_Sonic::ATIK_Sonic():
    _windNumOut(0),
    _ldiagIndex(-1),
    _spdIndex(-1),
    _dirIndex(-1),
    _spikeIndex(-1),
    _cntsIndex(-1),
    _sampleId(0),
    _tx(),_sx(),
    _expectedCounts(0),
    _diagThreshold(0.1),
    _shadowFactor(0.16),_maxShadowAngle(70.0 * M_PI / 180.0)
{
    /* index and sign transform for usual sonic orientation.
     * Normal orientation, no component change: 0 to 0, 1 to 1 and 2 to 2,
     * with no sign change. */
    for (int i = 0; i < 3; i++) {
        _tx[i] = i;
        _sx[i] = 1;
    }
}

ATIK_Sonic::~ATIK_Sonic()
{
}

void ATIK_Sonic::validate()
    throw(n_u::InvalidParameterException)
{
    SonicAnemometer::validate();

    const list<const Parameter*>& params = getParameters();
    list<const Parameter*>::const_iterator pi = params.begin();

    for ( ; pi != params.end(); ++pi) {
        const Parameter* parameter = *pi;

        if (parameter->getName() == "orientation") {
            bool pok = parameter->getType() == Parameter::STRING_PARAM &&
                parameter->getLength() == 1;
            if (pok && parameter->getStringValue(0) == "normal") {
                _tx[0] = 0;
                _tx[1] = 1;
                _tx[2] = 2;
                _sx[0] = 1;
                _sx[1] = 1;
                _sx[2] = 1;
            }
            else if (pok && parameter->getStringValue(0) == "down") {
                /* When the sonic is hanging down, the usual sonic w axis
                 * becomes the new u axis, u becomes w, and v becomes -v. */
                _tx[0] = 2;     // new u is normal w
                _tx[1] = 1;     // v is -v
                _tx[2] = 0;     // new w is normal u
                _sx[0] = 1;
                _sx[1] = -1;    // v is -v
                _sx[2] = 1;
            }
            else if (pok && parameter->getStringValue(0) == "flipped") {
                /* Sonic flipped over, w becomes -w, v becomes -v. */
                _tx[0] = 0;
                _tx[1] = 1;
                _tx[2] = 2;
                _sx[0] = 1;
                _sx[1] = -1;
                _sx[2] = -1;
            }
            else
                throw n_u::InvalidParameterException(getName(),
                        "orientation parameter",
                        "must be one string: \"normal\" (default), \"down\" or \"flipped\"");
        }
        else if (parameter->getName() == "shadowFactor") {
            if (parameter->getType() != Parameter::FLOAT_PARAM ||
                    parameter->getLength() != 1)
                    throw n_u::InvalidParameterException(getName(),
                            "shadowFactor","must be one float");
            _shadowFactor = parameter->getNumericValue(0);
        }
        else if (parameter->getName() == "maxShadowAngle") {
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
        else if (parameter->getName() == "despike");
        else if (parameter->getName() == "outlierProbability");
        else if (parameter->getName() == "discLevelMultiplier");
        else throw n_u::InvalidParameterException(getName(),
                        "unknown parameter", parameter->getName());
    }

    std::list<const SampleTag*> tags= getSampleTags();

    if (tags.size() != 1)
        throw n_u::InvalidParameterException(getName() +
                " must have one sample");

    const SampleTag* stag = tags.front();
    size_t nvars = stag->getVariables().size();
    /*
     * nvars
     * 7	u,v,w,tc,ldiag,spd,dir
     * 10	u,v,w,tc,ldiag,spd,dir,counts*3
     * 11	u,v,w,tc,ldiag,spd,dir,uflag,vflag,wflag,tcflag
     */

    _sampleId = stag->getId();
    if (_expectedCounts == 0 && stag->getRate() > 0.0)
        _expectedCounts = (int)rint(200.0 / stag->getRate());

    _ldiagIndex = 4;

    VariableIterator vi = stag->getVariableIterator();
    for (int i = 0; vi.hasNext(); i++) {
        const Variable* var = vi.next();
        const string& vname = var->getName();
        if (vname.length() > 2 && vname.substr(0,3) == "spd")
            _spdIndex = i;
        else if (vname.length() > 2 && vname.substr(0,3) == "dir")
            _dirIndex = i;
    }
    if (_spdIndex < 0 || _dirIndex < 0)
        throw n_u::InvalidParameterException(getName() +
                " ATIK cannot find speed or direction variables");

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
                " unsupported number of variables. Must be: u,v,w,tc,ldiag,spd,dir,[3 x counts or 4 x flags]]");
    }
    _windNumOut = nvars;

}
void ATIK_Sonic::pathShadowCorrection(float* uvwt)
{
    if (_shadowFactor == 0.0) return;
    if (isnan(uvwt[0]) || isnan(uvwt[1]) || isnan(uvwt[2])) return;

    float nuvw[3];

    double spd = sqrt(uvwt[0] * uvwt[0] + uvwt[1] * uvwt[1] + uvwt[2] * uvwt[2]);

    for (int i = 0; i < 3; i++) {
        double x = uvwt[i];
        double theta = acos(fabs(x)/spd);
        nuvw[i] = (theta > _maxShadowAngle ? x : x / (1.0 - _shadowFactor + _shadowFactor * theta / _maxShadowAngle));
    }

    memcpy(uvwt,nuvw,3*sizeof(float));
}

void removeShadowCorrection(float* )
{
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
        DSMSerialSensor::process(samp,parseResults);
        if (parseResults.empty()) return false;

        // result from base class parsing of ASCII
        const Sample* psamp = parseResults.front();

        unsigned int nvals = psamp->getDataLength();
        const float* pdata = (const float*) psamp->getConstVoidDataPtr();
        const float* pend = pdata + nvals;

        int i;
        for (i = 0; i < 4; i++) {
            int ix = i;
            if (i < 3) ix = _tx[i];
            if (ix < (signed) nvals) {
                float f = pdata[ix];
                if ( f < -9998.0 || f > 9998.0) uvwt[i] = floatNAN;
                else if (i < 3) uvwt[i] = _sx[i] * f / 100.0;
                else uvwt[i] = f / 100.0;
            }
            else uvwt[i] = floatNAN;
        }
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
        for (i = 0; i < 4; i++) {
            int ix = i;
            if (i < 3) ix = _tx[i];
            if (ix < (signed) nvals) {
#if __BYTE_ORDER == __BIG_ENDIAN
                short f = bswap_16(pdata[ix]);
#else
                short f = pdata[ix];
#endif
                if ( f < -9998 || f > 9998) uvwt[i] = floatNAN;
                else if (i < 3) uvwt[i] = _sx[i] * (float) f / 100.0;
                else uvwt[i] = (float) f / 100.0;
            }
            else uvwt[i] = floatNAN;
        }
        pdata += 4;
    }

    // compute speed of sound squared from temperature, removing
    // path curvature correction that was applied by the sonic,
    // because u and v may be corrected below.
    double uv2 = uvwt[0] * uvwt[0] + uvwt[1] * uvwt[1];
    double c2 = GAMMA_R * (uvwt[3] + KELVIN_AT_0C);
    c2 -= uv2;

    removeShadowCorrection(uvwt);

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

    pathShadowCorrection(uvwt);

    /*
     * Recompute Tc with speed of sound corrected
     * for new path curvature (before rotating from
     * instrument coordinates).
     */
    uv2 = uvwt[0] * uvwt[0] + uvwt[1] * uvwt[1];
    c2 += uv2;
    uvwt[3] = c2 / GAMMA_R - KELVIN_AT_0C;

    offsetsAndRotate(samp->getTimeTag(),uvwt);

    // new sample
    SampleT<float>* wsamp = getSample<float>(_windNumOut);
    wsamp->setTimeTag(samp->getTimeTag());
    wsamp->setId(_sampleId);

    float* dout = wsamp->getDataPtr();

    memcpy(dout,uvwt,4*sizeof(float));

    if (_spdIndex >= 0)
        dout[_spdIndex] = sqrt(dout[0] * dout[0] + dout[1] * dout[1]);
    if (_dirIndex >= 0) {
        float dr = atan2f(-dout[0],-dout[1]) * 180.0 / M_PI;
        if (dr < 0.0) dr += 360.;
        dout[_dirIndex] = dr;
    }

    if (_ldiagIndex >= 0) dout[_ldiagIndex] = diag;
    if (_cntsIndex >= 0) memcpy(dout + _cntsIndex,counts,3 * sizeof(float));
    if (_spikeIndex >= 0) {
        for (int i = 0; i < 4; i++)
            dout[i + _spikeIndex] = (float) spikes[i];
    }
    results.push_back(wsamp);
    return true;
}
