// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include "NearestResamplerAtRate.h"
#include "Project.h"
#include "Site.h"
#include "Variable.h"
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

#include <iomanip>

using namespace nidas::core;
using namespace std;
using nidas::util::LogContext;
using nidas::util::UTime;


NearestResamplerAtRate::
NearestResamplerAtRate(const std::vector<const Variable*>& vars,
                       bool nansVariable):
    _source(false),_outSample(),
    _reqVars(), _outVarIndices(),
    _inmap(),_lenmap(),_outmap(),_ndataValues(0),_outlen(0),_rate(0.0),
    _deltatUsec(0),_deltatUsecD10(0),_deltatUsecD2(0),
    _exactDeltatUsec(true),_middleTimeTags(true),
    _outputTT(LONG_LONG_MIN),_nextOutputTT(LONG_LONG_MIN),
    _prevTT(0),_nearTT(0),_prevData(0),_nearData(0),_samplesSinceOutput(0),
    _osamp(0),_fillGaps(false),
    _ttOutOfOrder()
{
    _ndataValues = 0;
    int dsmId = -1;

    for (unsigned int i = 0; i < vars.size(); i++) {
        const Variable* vin = vars[i];
        Variable * reqVar = new Variable(*vin);

        VLOG(("") << "vin=" << vin->getName() << '('
                  << vin->getSampleTag()->getDSMId() << ','
                  << vin->getSampleTag()->getSpSId() << ')');
        dsm_sample_id_t id = 0;

        const SampleTag * vtag;
        if ((vtag = vin->getSampleTag())) id = vtag->getId();

        int did = GET_DSM_ID(id);
        if (dsmId == -1) dsmId = did;
        else if (dsmId != did) dsmId = -2;

        _reqVars.push_back(reqVar);
        _outVarIndices[reqVar] = _ndataValues;

        Variable* v = new Variable(*vin);
        _outSample.addVariable(v);
        _ndataValues += v->getLength();
    }

    _outlen = _ndataValues;

    if (nansVariable) {
        // Variable containing the number of non-NAs in the output sample.
        Variable* v = new Variable();
        v->setName("nonNANs");
        v->setType(Variable::WEIGHT);
        v->setUnits("");
        _outSample.addVariable(v);
        _outlen++;
    }

    _prevTT = new dsm_time_t[_ndataValues];
    _nearTT = new dsm_time_t[_ndataValues];
    _prevData = new float[_ndataValues];
    _nearData = new float[_ndataValues];
    _samplesSinceOutput = new int[_ndataValues];

    for (int i = 0; i < _ndataValues; i++) {
	_prevTT[i] = 0;
	_nearTT[i] = 0;
	_prevData[i] = floatNAN;
	_nearData[i] = floatNAN;
	_samplesSinceOutput[i] = 0;
    }

    dsm_sample_id_t uid = Project::getInstance()->getUniqueSampleId(dsmId);
    _outSample.setDSMId(GET_DSM_ID(uid));
    _outSample.setSampleId(GET_SPS_ID(uid));

    addSampleTag(&_outSample);

    setRate(10.);       // pick a default
    _outputTT = _nextOutputTT = 0;
}

NearestResamplerAtRate::~NearestResamplerAtRate()
{
    delete [] _prevTT;
    delete [] _nearTT;
    delete [] _prevData;
    delete [] _nearData;
    delete [] _samplesSinceOutput;
    if (_osamp) _osamp->freeReference();

    vector<Variable*>::iterator ti = _reqVars.begin();
    for ( ; ti != _reqVars.end(); ++ti) delete *ti;
}

void NearestResamplerAtRate::setRate(double val)
{
    _rate = val;

    double dtusec = (double) USECS_PER_SEC / _rate;
    _deltatUsec = (int)rint(dtusec);

    // _exactDeltatUsec is true if rate <= 1 or if
    // deltatUsec is pretty close to an integer.
    _exactDeltatUsec = _rate <= 1.0 || fabs(dtusec - rint(dtusec)) < 1.e-2;

    VLOG(("rate=") << setprecision(7) << _rate
         << ",fabs(dtusec - rint(dtusec))=" << fabs(dtusec - rint(dtusec))
         << ",exact=" << _exactDeltatUsec);

    _deltatUsecD10 = _deltatUsec / 10;
    _deltatUsecD2 = _deltatUsec / 2;
}

void NearestResamplerAtRate::connect(SampleSource* source)
{
    vector<bool> matched(_reqVars.size());

    list<const SampleTag*> intags = source->getSampleTags();

    list<const SampleTag*>::const_iterator inti = intags.begin();
    for ( ; inti != intags.end(); ++inti )
    {
        const SampleTag* intag = *inti;
        dsm_sample_id_t sampid = intag->getId();

        // loop over variables in this input sample, checking
        // for a match against one of my variable names.
        bool varMatch = false;
        for (auto& var: intag->getVariables())
        {
            // index of 0th value of variable in its sample data array.
            unsigned int vindex = intag->getDataIndex(var);

            for (unsigned int rvi = 0; rvi < _reqVars.size(); rvi++)
            {
                Variable* myvar = _reqVars[rvi];

                VLOG(("var=") << var->getName() << "(" << GET_DSM_ID(sampid)
                     << "," << GET_SPS_ID(sampid) << "); "
                     << "myvar=" << myvar->getName()
                     << "(" << myvar->getSampleTag()->getDSMId() << ","
                     << myvar->getSampleTag()->getSpSId() << ")"
                     << ", match=" << (*var == *myvar));

                if (*var == *myvar) {
                    unsigned int vlen = var->getLength();
                    // index of the 0th value of this variable in the
                    // output array.
                    map<Variable*,unsigned int>::iterator vi = _outVarIndices.find(myvar);
                    assert(vi != _outVarIndices.end());
                    unsigned int outIndex = vi->second;

                    map<dsm_sample_id_t,vector<unsigned int> >::iterator mi;
                    if ((mi = _inmap.find(sampid)) == _inmap.end()) {
                        vector<unsigned int> tmp;
                        tmp.push_back(vindex);
                        _inmap[sampid] = tmp;

                        mi = _outmap.find(sampid);
                        assert(mi == _outmap.end());
                        tmp.clear();
                        tmp.push_back(outIndex);
                        _outmap[sampid] = tmp;

                        tmp.clear();
                        tmp.push_back(vlen);
                        _lenmap[sampid] = tmp;
                    }
                    else {
                        mi->second.push_back(vindex);

                        mi = _outmap.find(sampid);
                        assert(mi != _outmap.end());
                        mi->second.push_back(outIndex);

                        mi = _lenmap.find(sampid);
                        assert(mi != _lenmap.end());
                        mi->second.push_back(vlen);
                    }

                    varMatch = true;
                    matched[rvi] = true;
                }
            }
        }
        if (varMatch) source->addSampleClientForTag(this,intag);
    }

    string notFound;
    unsigned int nmatches = 0;
    for (unsigned int i = 0; i < _reqVars.size(); i++) {
        if (!matched[i]) {
            if (notFound.size() > 0) notFound += ',';
            notFound += _reqVars[i]->getName();
        }
        else nmatches++;
    }
    if (nmatches < _reqVars.size()) WLOG(("NearestResamplerAtRate: no match for these variables: ") << notFound);
}

void NearestResamplerAtRate::disconnect(SampleSource* source) throw()
{
    source->removeSampleClient(this);
}

bool NearestResamplerAtRate::receive(const Sample* samp) throw()
{

    if (samp->getType() != FLOAT_ST && samp->getType() != DOUBLE_ST) return false;

    dsm_sample_id_t sampid = samp->getId();

    map<dsm_sample_id_t,vector<unsigned int> >::iterator mi;

    if ((mi = _inmap.find(sampid)) == _inmap.end()) return false;
    const vector<unsigned int>& invec = mi->second;

    mi = _outmap.find(sampid);
    assert(mi != _outmap.end());
    const vector<unsigned int>& outvec = mi->second;

    mi = _lenmap.find(sampid);
    assert(mi != _lenmap.end());
    const vector<unsigned int>& lenvec = mi->second;

    assert(invec.size() == outvec.size());
    assert(invec.size() == lenvec.size());

    dsm_time_t tt = samp->getTimeTag();

    static LogContext lp(LOG_VERBOSE);
    if (lp.active())
    {
        static dsm_time_t lastTT{0};
        static dsm_sample_id_t lastId{0};
        if (tt < lastTT) {
            lp.log() << "tt=" << UTime(tt).format(true, "%c") << " id="
                     << GET_DSM_ID(sampid) << "," << GET_SHORT_ID(sampid);
            lp.log() << "lasttt=" << UTime(lastTT).format(true, "%c")
                     << " id="
                     << GET_DSM_ID(lastId) << "," << GET_SHORT_ID(lastId);
        }
        lastTT = tt;
        lastId = sampid;
    }

    if (tt > _nextOutputTT) sendSample(tt);

    for (unsigned int iv = 0; iv < invec.size(); iv++) {
        unsigned int ii = invec[iv];
        unsigned int oi = outvec[iv];

        for (unsigned int iv2 = 0; iv2 < lenvec[iv] && ii < samp->getDataLength();
            iv2++,ii++,oi++) {
            // note we are casting down to float if the samples are double
            float val = (float) samp->getDataValue(ii);
            if (isnan(val)) continue;        // doesn't exist

            if (tt < _prevTT[oi]) {
                if (iv == 0 && !(_ttOutOfOrder[sampid]++ % 100)) {
                    WLOG(("NearestResamplerAtRate: sample id ") <<
                        GET_DSM_ID(sampid) << ',' << GET_SPS_ID(sampid) << " backwards by " <<
                        (double(_prevTT[oi] - tt) / USECS_PER_SEC) << " sec at " <<
                        UTime(tt).format(true,"%Y %m %d %H:%M:%S.%6f"));
                }
                switch (_samplesSinceOutput[oi]) {
                case 0:
                    // previous sample was before _outputTT. It must be closer than this one.
                    // discard this one
                    break;
                default:
                    // previous sample was after _outputTT. Check which is closer, the previous closest or this one
                    if (::llabs(_outputTT - tt) < ::llabs(_outputTT - _nearTT[oi])) {
                        _nearTT[oi] = tt;
                        _nearData[oi] = val;
                    }
                    _prevData[oi] = val;
                    _prevTT[oi] = tt;
                    break;
                }
            }
            else {
                switch (_samplesSinceOutput[oi]) {
                case 0:
                    // this is the first sample of this variable since outputTT
                    // Assumes input samples are sorted in time!!
                    // Determine which of previous and current sample is the nearest
                    // to outputTT
                    if (tt >= _outputTT) {
                        if (_outputTT > (tt + _prevTT[oi]) / 2) {
                            _nearData[oi] = val;
                            _nearTT[oi] = tt;
                        }
                        else {
                            _nearData[oi] = _prevData[oi];
                            _nearTT[oi] = _prevTT[oi];
                        }
                        _samplesSinceOutput[oi]++;
                    }
                    break;
                default:
                    // this is at least the second sample since outputTT
                    // since samples are in time sequence, this one can't
                    // be the nearest one to outputTT.
                    break;
                }
                _prevData[oi] = val;
                _prevTT[oi] = tt;
            }
        }
    }
    return true;
}

void NearestResamplerAtRate::sendSample(dsm_time_t tt) throw()
{
    if (!_osamp) {
        if (_exactDeltatUsec) {
            if (_middleTimeTags) {
                dsm_time_t ttx = tt + _deltatUsecD2;
                _outputTT = ttx - ttx % _deltatUsec - _deltatUsecD2;
            }
            else _outputTT = tt - tt  % _deltatUsec;
            _nextOutputTT = _outputTT + _deltatUsec;
        }
        else {
            // If the deltat is not an exact number of microseconds,
            // and the rate is > 1, do modulus math of _deltatUsec with
            // the number of usecs since the start of the second,
            // avoiding the accumulated error of doing a modulus
            // against the time offset since Jan 1970.
            if (_middleTimeTags) {
                dsm_time_t ttx = tt + _deltatUsecD2;
                unsigned int tmod = ttx % USECS_PER_SEC;
                _outputTT = ttx - tmod % _deltatUsec - _deltatUsecD2;
            }
            else {
                unsigned int tmod = tt % USECS_PER_SEC;
                _outputTT = tt - tmod % _deltatUsec;
            }
            _nextOutputTT = _outputTT + _deltatUsec;
        }
        _osamp = getSample<float>(_outlen);
        _osamp->setId(_outSample.getId());
    }
    while (tt > _nextOutputTT) {
        dsm_time_t maxTT = _nextOutputTT - _deltatUsecD10;
        dsm_time_t minTT = _outputTT - _deltatUsec + _deltatUsecD10;
        int nonNANs = 0;
        float* outData = _osamp->getDataPtr();
        for (int i = 0; i < _ndataValues; i++) {
            switch (_samplesSinceOutput[i]) {
            case 0:
                // If there was no sample for this variable since outputTT
                // then match prevData with the outputTT.
                if (_prevTT[i] < minTT) outData[i] = floatNAN;
                else if (!isnan(outData[i] = _prevData[i])) nonNANs++;
                break;
            default:
                if (_nearTT[i] > maxTT || _nearTT[i] < minTT)
                    outData[i] = floatNAN;
                else if (!isnan(outData[i] = _nearData[i])) nonNANs++;
                break;
            }
            _samplesSinceOutput[i] = 0;
        }
        if (_outlen > _ndataValues) outData[_ndataValues] = (float) nonNANs;
        if (nonNANs > 0 || _fillGaps)  {
            _osamp->setTimeTag(_outputTT);
            _source.distribute(_osamp);
            _osamp = getSample<float>(_outlen);
            _osamp->setId(_outSample.getId());
            if (_exactDeltatUsec) {
                _outputTT += _deltatUsec;
            }
            else {
                if (_middleTimeTags) {
                    unsigned int tmod = _nextOutputTT % USECS_PER_SEC;
                    int n = tmod / _deltatUsec;
                    _outputTT = _nextOutputTT - tmod + n * _deltatUsec + _deltatUsecD2;
                }
                else {
                    // avoid round off errors
                    _nextOutputTT += _deltatUsecD2;
                    unsigned int tmod = _nextOutputTT % USECS_PER_SEC;
                    int n = tmod / _deltatUsec;
                    _outputTT = _nextOutputTT - tmod + n * _deltatUsec;
                }
            }
        }
        else {
            // jump ahead
            if (_exactDeltatUsec) {
                if (_middleTimeTags) {
                    dsm_time_t ttx = tt + _deltatUsecD2;
                    _outputTT = ttx - ttx % _deltatUsec - _deltatUsecD2;
                }
                else _outputTT = tt - tt % _deltatUsec;
            }
            else {
                if (_middleTimeTags) {
                    dsm_time_t ttx = tt + _deltatUsecD2;
                    unsigned int tmod = ttx % USECS_PER_SEC;
                    _outputTT = ttx - tmod % _deltatUsec - _deltatUsecD2;
                }
                else {
                    unsigned int tmod = tt % USECS_PER_SEC;
                    _outputTT = tt - tmod % _deltatUsec;
                }
            }
        }
        _nextOutputTT = _outputTT + _deltatUsec;
    }
}

/*
 * Implementation of SampleSource::flush().
 */
void NearestResamplerAtRate::flush() throw()
{
    sendSample(_nextOutputTT + 1);
}
