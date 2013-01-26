// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
   Copyright 2005 UCAR, NCAR, All Rights Reserved

   $LastChangedDate: 2011-11-16 15:03:17 -0700 (Wed, 16 Nov 2011) $

   $LastChangedRevision: 6326 $

   $LastChangedBy: maclean $

   $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/dynld/isff/CSI_IRGA_Sonic.cc $

*/

#include <nidas/dynld/isff/CSI_IRGA_Sonic.h>

#include <nidas/core/Variable.h>
#include <nidas/core/Sample.h>
#include <nidas/core/AsciiSscanf.h>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(isff,CSI_IRGA_Sonic)

CSI_IRGA_Sonic::CSI_IRGA_Sonic():
    _numOut(0),
    _ldiagIndex(-1),
    _spdIndex(-1),
    _dirIndex(-1),
    _sampleId(0),
    _tx(),_sx(),
    _timeDelay(0)
{
    /* index and sign transform for usual sonic orientation.
     * Normal orientation, no component change: 0 to 0, 1 to 1 and 2 to 2,
     * with no sign change. */
    for (int i = 0; i < 3; i++) {
        _tx[i] = i;
        _sx[i] = 1;
    }
}

CSI_IRGA_Sonic::~CSI_IRGA_Sonic()
{
}

void CSI_IRGA_Sonic::validate()
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
        else if (parameter->getName() == "bandwidth") {
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
        else if (parameter->getName() == "despike");
        else if (parameter->getName() == "outlierProbability");
        else if (parameter->getName() == "discLevelMultiplier");
        else throw n_u::InvalidParameterException(getName(),
                        "unknown parameter", parameter->getName());
        if (_timeDelay == 0.0)
            WLOG(("%s: IRGASON/EC150 bandwidth not specified. Time delay will be set to 0 ms",
                        getName().c_str()));
    }

    std::list<const SampleTag*> tags= getSampleTags();

    if (tags.size() != 1)
        throw n_u::InvalidParameterException(getName() +
                " must have one sample");

    const SampleTag* stag = tags.front();
    size_t nvars = stag->getVariables().size();
    /*
     * variable sequence
     * u,v,w,tc,diag,other irga variables,ldiag,spd,dir
     * ldiag, spd and dir can be in any order, as long
     * as they are last. The code checks for ldiag,spd and dir
     * variable names beginning with exactly those strings,
     * which isn't a great idea.
     */

    _sampleId = stag->getId();

    VariableIterator vi = stag->getVariableIterator();
    for (int i = 0; vi.hasNext(); i++) {
        const Variable* var = vi.next();
        const string& vname = var->getName();
        if (vname.length() > 2 && vname.substr(0,3) == "spd")
            _spdIndex = i;
        else if (vname.length() > 2 && vname.substr(0,3) == "dir")
            _dirIndex = i;
        else if (vname.length() > 4 && vname.substr(0,5) == "ldiag")
            _ldiagIndex = i;
    }
    if (_spdIndex < 0 || _dirIndex < 0 || _ldiagIndex < 0)
        throw n_u::InvalidParameterException(getName() +
                " CSI_IRGA_Sonic cannot find speed, direction or ldiag variables");

    if (nvars - _spdIndex > 3 || nvars -_dirIndex > 3 || nvars - _ldiagIndex > 3)
        throw n_u::InvalidParameterException(getName() +
                " CSI_IRGA_Sonic speed, direction and ldiag variables should be at the end of the list");

    _numOut = nvars;

}

void CSI_IRGA_Sonic::validateSscanfs() throw(n_u::InvalidParameterException)
{
    const std::list<AsciiSscanf*>& sscanfers = getScanfers();
    std::list<AsciiSscanf*>::const_iterator si = sscanfers.begin();

    for ( ; si != sscanfers.end(); ++si) {
        AsciiSscanf* sscanf = *si;
        const SampleTag* tag = sscanf->getSampleTag();

        unsigned int nexpected = tag->getVariables().size();
        if (_spdIndex >= 0) nexpected--; // derived, not scanned
        if (_dirIndex >= 0) nexpected--; // derived, not scanned
        if (_ldiagIndex >= 0) nexpected--;   // derived, not scanned

        unsigned int nf = sscanf->getNumberOfFields();

        if (nf != nexpected) {
            ostringstream ost;
            ost << "number of scanf fields (" << nf <<
                ") is less than the number expected (" << nexpected;
            throw n_u::InvalidParameterException(getName(),"scanfFormat",ost.str());
        }
    }
}

bool CSI_IRGA_Sonic::process(const Sample* samp,
	std::list<const Sample*>& results) throw()
{

    std::list<const Sample*> parseResults;

    DSMSerialSensor::process(samp,parseResults);

    if (parseResults.empty()) return false;

    // result from base class parsing of ASCII
    const Sample* psamp = parseResults.front();

    unsigned int nvals = psamp->getDataLength();
    const float* pdata = (const float*) psamp->getConstVoidDataPtr();
    const float* pend = pdata + nvals;

    float uvwtd[5];
    // u,v,w,tc,diag
    for (unsigned int i = 0; i < sizeof(uvwtd)/sizeof(uvwtd[0]); i++) {
        int ix = i;
        if (i < 3) ix = _tx[i];
        if (ix < (signed) nvals) {
            float f = pdata[ix];
            // Need to check documentation to see if there is a special
            // encoding of a missing value, like 9999.0
            if (i < 3) uvwtd[i] = _sx[i] * f;
            else uvwtd[i] = f;
        }
        else uvwtd[i] = floatNAN;
    }
    pdata += sizeof(uvwtd)/sizeof(uvwtd[0]);

    if (getDespike()) {
        bool spikes[4] = {false,false,false,false};
        despike(samp->getTimeTag(),uvwtd,4,spikes);
    }

    offsetsAndRotate(samp->getTimeTag(),uvwtd);

    // new sample
    SampleT<float>* wsamp = getSample<float>(_numOut);

    wsamp->setTimeTag(samp->getTimeTag() - _timeDelay);
    wsamp->setId(_sampleId);

    float* dout = wsamp->getDataPtr();
    float* dend = dout + _numOut;
    float *dptr = dout;

    memcpy(dptr,uvwtd,sizeof(uvwtd));
    dptr += sizeof(uvwtd) / sizeof(uvwtd[0]);

    for ( ; pdata < pend && dptr < dend; ) *dptr++ = *pdata++;
    for ( ; dptr < dend; ) *dptr++ = floatNAN;

    if (_ldiagIndex >= 0) dout[_ldiagIndex] = uvwtd[4] != 0;

    if (_spdIndex >= 0) {
        dout[_spdIndex] = sqrt(uvwtd[0] * uvwtd[0] + uvwtd[1] * uvwtd[1]);
    }
    if (_dirIndex >= 0) {
        float dr = atan2f(-uvwtd[0],-uvwtd[1]) * 180.0 / M_PI;
        if (dr < 0.0) dr += 360.;
        dout[_dirIndex] = dr;
    }

    psamp->freeReference();

    results.push_back(wsamp);
    return true;
}
