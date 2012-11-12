// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/NearestResampler.h>
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/Variable.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NearestResampler::NearestResampler(const vector<const Variable*>& vars,bool nansVariable):
    _source(false),
    _outSample(),
    _reqVars(),_outVarIndices(),
    _inmap(),_lenmap(), _outmap(),
    _ndataValues(0),_outlen(0),_master(0),_nmaster(0),
    _prevTT(0),_nearTT(0),_prevData(0),_nearData(0),_samplesSinceMaster(0),
    _ttOutOfOrder(),
    _debug(false)
{
    ctorCommon(vars,nansVariable);
}

NearestResampler::NearestResampler(const vector<Variable*>& vars, bool nansVariable):
    _source(false),
    _outSample(),
    _reqVars(),_outVarIndices(),
    _inmap(),_lenmap(), _outmap(),
    _ndataValues(0),_outlen(0),_master(0),_nmaster(0),
    _prevTT(0),_nearTT(0),_prevData(0),_nearData(0),_samplesSinceMaster(0),
    _ttOutOfOrder(),
    _debug(false)
{
    vector<const Variable*> newvars;
    for (unsigned int i = 0; i < vars.size(); i++)
    	newvars.push_back(vars[i]);
    ctorCommon(newvars,nansVariable);
}

NearestResampler::~NearestResampler()
{
    delete [] _prevTT;
    delete [] _nearTT;
    delete [] _prevData;
    delete [] _nearData;
    delete [] _samplesSinceMaster;

    vector<Variable*>::iterator vi = _reqVars.begin();
    for ( ; vi != _reqVars.end(); ++vi) delete *vi;
}

void NearestResampler::ctorCommon(const vector<const Variable*>& vars,bool nansVariable)
{
    _ndataValues = 0;
    int dsmId = -1;

    for (unsigned int i = 0; i < vars.size(); i++) {
	const Variable* vin = vars[i];
#ifdef DEBUG
        if (i == 0) _debug = vin->getName().substr(0,3) == "h2o"; 
#endif
        Variable * reqVar = new Variable(*vin);

        dsm_sample_id_t id = 0;

        const SampleTag * vtag;
        if ((vtag = vin->getSampleTag())) id = vtag->getId();

        int did = GET_DSM_ID(id);
        if (dsmId == -1) dsmId = did;
        else if (dsmId != did) dsmId = -2;

        _reqVars.push_back(reqVar);
        _outVarIndices[reqVar] = _ndataValues;

	Variable* v = new Variable(*reqVar);
#ifdef DEBUG
        if (_debug) cerr << "NearestResampler, v=" << v->getName() << ", site=" <<
            (v->getSite() ? v->getSite()->getName() : "unk") << '(' <<
                v->getStation() << ')' << endl;

#endif
	_outSample.addVariable(v);

#ifdef DEBUG
        if (_debug) cerr << "NearestResampler, v=" << v->getName() << ", site=" <<
            (v->getSite() ? v->getSite()->getName() : "unk") << '(' <<
                v->getStation() << ')' << endl;
#endif

        _ndataValues += v->getLength();
    }

    _outlen = _ndataValues;

    if (nansVariable) {
        // Number of non-NAs in the output sample.
        Variable* v = new Variable();
        v->setName("nonNANs");
        v->setType(Variable::WEIGHT);
        v->setUnits("");
        _outSample.addVariable(v);
        _outlen++;
    }

    _master = 0;
    _nmaster = 0;
    _prevTT = new dsm_time_t[_ndataValues];
    _nearTT = new dsm_time_t[_ndataValues];
    _prevData = new float[_ndataValues];
    _nearData = new float[_ndataValues];
    _samplesSinceMaster = new int[_ndataValues];

    for (unsigned int i = 0; i < _ndataValues; i++) {
	_prevTT[i] = 0;
	_nearTT[i] = 0;
	_prevData[i] = floatNAN;
	_nearData[i] = floatNAN;
	_samplesSinceMaster[i] = 0;
    }

    dsm_sample_id_t uid = Project::getInstance()->getUniqueSampleId(dsmId);
    _outSample.setDSMId(GET_DSM_ID(uid));
    _outSample.setSampleId(GET_SPS_ID(uid));

    addSampleTag(&_outSample);
}
void NearestResampler::connect(SampleSource* source)
	throw(n_u::InvalidParameterException)
{

    vector<bool> matched(_reqVars.size());

    // make a copy of input's SampleTags collection.
    list<const SampleTag*> intags = source->getSampleTags();

    // cerr << "NearestResamples, intags.size()=" << intags.size() << endl;
    list<const SampleTag*>::const_iterator inti = intags.begin();
    for ( ; inti != intags.end(); ++inti ) {
	const SampleTag* intag = *inti;
	dsm_sample_id_t sampid = intag->getId();

	// loop over variables in this input sample, checking
	// for a match against one of my variable names.
	VariableIterator vi = intag->getVariableIterator();
        bool varMatch = false;
	for ( ; vi.hasNext(); ) {
	    const Variable* var = vi.next();

            // index of 0th value of variable in its sample data array.
            unsigned int vindex = intag->getDataIndex(var);

            for (unsigned int rvi = 0; rvi < _reqVars.size(); rvi++) {
                Variable* myvar = _reqVars[rvi];

		if (*var == *myvar) {

#ifdef DEBUG
                    cerr << "sample var=" << var->getName() <<
                        "(" << var->getStation() << "), " <<
                        var->getNameWithoutSite() <<
                        ", myvar=" << myvar->getName() <<
                        "(" << myvar->getStation() << "), " <<
                        myvar->getNameWithoutSite() <<
                        ", match=" << (*var == *myvar) << endl;
#endif
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
    if (nmatches < _reqVars.size()) WLOG(("NearestResampler: no match for these variables: ") << notFound);
}

void NearestResampler::disconnect(SampleSource* source) throw()
{
    source->removeSampleClient(this);
}

bool NearestResampler::receive(const Sample* samp) throw()
{
    if (samp->getType() != FLOAT_ST && samp->getType() != DOUBLE_ST) return false;

    dsm_sample_id_t sampid = samp->getId();

#ifdef DEBUG
    cerr << "NR in: " << n_u::UTime(samp->getTimeTag()).format(true,"%Y %m %d %H%M%S.%6f ") <<
        GET_DSM_ID(sampid) << ',' << GET_SPS_ID(sampid) << ", len=" << samp->getDataLength() << endl;
#endif

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

    for (unsigned int iv = 0; iv < invec.size(); iv++) {
	unsigned int ii = invec[iv];
	unsigned int oi = outvec[iv];
        for (unsigned int iv2 = 0; iv2 < lenvec[iv] && ii < samp->getDataLength();
            iv2++,ii++,oi++) {
            float val = samp->getDataValue(ii);
            if (oi == _master) {
                /*
                 * received a new master variable. Output values that were
                 * nearest to previous master.
                 *
                 * For the master variable, _nearTT[_master] is actually
                 * the time tag previous to the previous one.
                 * For all other variables _nearTT[oi] is the time
                 * of the sample nearest the master time tag.
                 */
                dsm_time_t maxTT;		// time tags must be < maxTT
                dsm_time_t minTT;		// time tags must be > minTT
                if (_nmaster < 2) {
                    if (_nmaster++ == 0) {
                        _nearTT[_master] = _prevTT[_master];
                        _prevTT[_master] = tt;
                        _prevData[_master] = val;
                        continue;
                    }
                    // maxTT is current time minus 0.1 of deltat
                    maxTT = tt - (tt - _prevTT[_master]) / 10;
                    // minTT is previous time minus 0.9 of deltat
                    minTT = _prevTT[_master] - ((tt - _prevTT[_master]) * 9) / 10;
                }
                else {
                    // maxTT is current time minus 0.1 of deltat
                    maxTT = tt - (tt - _prevTT[_master]) / 10;
                    // minTT is two times back plus 0.1 of deltat
                    minTT = _nearTT[_master] +
                        (_prevTT[_master] - _nearTT[_master]) / 10;
                }

                // times out of order. Not good. Try to recover.
                // Can't ignore this sample, perhaps the previous one had a
                // off-into-the-future bad time tag.
                if (tt < _prevTT[_master]) {
                    if (!(_ttOutOfOrder[sampid]++ % 100)) {
                        WLOG(("NearestResampler: sample id ") << 
                            GET_DSM_ID(sampid) << ',' << GET_SPS_ID(sampid) << " backwards by " <<
                            (double(_prevTT[_master] - tt) / USECS_PER_MSEC) << " sec at " <<
                            n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%6f"));
                    }
                    _nmaster = 1;
                    for (unsigned int k = 0; k < _ndataValues; k++) {
                        if (k != _master) {
                            if (::llabs(tt - _nearTT[k]) > ::llabs(tt - _prevTT[k])) {
                                _nearTT[k] = _prevTT[k];
                                _nearData[k] = _prevData[k];
                            }
                            if (_prevTT[k] > tt) _samplesSinceMaster[k] = 1;
                            else _samplesSinceMaster[k] = 0;
                        }
                        else {
                            _nearTT[k] = 0;
                            _prevTT[k] = tt;
                            _prevData[k] = val;
                        }
                    }
                    continue;
                }

                SampleT<float>* osamp = getSample<float>(_outlen);
                float* outData = osamp->getDataPtr();
                int nonNANs = 0;
                for (unsigned int k = 0; k < _ndataValues; k++) {
                    if (k == _master) {
                      // master variable
                      if (!isnan(outData[k] = _prevData[k])) nonNANs++;
                      continue;
                    }
                    switch (_samplesSinceMaster[k]) {
                    case 0:
                        // If there was no sample for this variable since
                        // the previous master then use prevData
                        if (_prevTT[k] > maxTT || _prevTT[k] < minTT)
                                outData[k] = floatNAN;
                        else if (!isnan(outData[k] = _prevData[k])) nonNANs++;
                        break;
                    default:        // 1 or more
                        if (_nearTT[k] > maxTT || _nearTT[k] < minTT)
                                outData[k] = floatNAN;
                        else if (!isnan(outData[k] = _nearData[k])) nonNANs++;
                        break;
                    }
                    _samplesSinceMaster[k] = 0;
                }
                osamp->setTimeTag(_prevTT[_master]);
                osamp->setId(_outSample.getId());
                if (_ndataValues < _outlen) outData[_ndataValues] = (float) nonNANs;
#ifdef DEBUG
                cerr << "NR out: " << n_u::UTime(osamp->getTimeTag()).format(true,"%Y %m %d %H:%M:%S.%6f ") <<
                    GET_DSM_ID(_outSample.getId()) << ',' << GET_SPS_ID(_outSample.getId()) << ", len=" << osamp->getDataLength() << endl;
#endif
                _source.distribute(osamp);

                _nearTT[_master] = _prevTT[_master];
                _prevTT[_master] = tt;
                _prevData[_master] = val;
            }
            else {
                // backwards time, do the best we can
                if (tt < _prevTT[oi]) {
                    if (iv == 0 && !(_ttOutOfOrder[sampid]++ % 100)) {
                        WLOG(("NearestResampler: sample id ") << 
                            GET_DSM_ID(sampid) << ',' << GET_SPS_ID(sampid) << " backwards by " <<
                            (double(_prevTT[oi] - tt) / USECS_PER_MSEC) << " sec at " <<
                            n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%6f"));
                    }
                    switch (_samplesSinceMaster[oi]) {
                    case 0:
                        // previous sample was before master. It must be closer than this one.
                        // discard this one
                        break;
                    default:
                        // previous sample was after master. Check which is closer, the previous closest or this one
                        if (::llabs(_prevTT[_master] - tt) < ::llabs(_prevTT[_master] - _nearTT[oi])) {
                            _nearTT[oi] = tt;
                            _nearData[oi] = val;
                        }
                        _prevData[oi] = val;
                        _prevTT[oi] = tt;
                        break;
                    }
                }
                else {
                    if (tt < _prevTT[_master]) {
                        if (iv == 0 && !(_ttOutOfOrder[sampid]++ % 100)) {
                            WLOG(("NearestResampler: sample id ") << 
                                GET_DSM_ID(sampid) << ',' << GET_SPS_ID(sampid) << " backwards by " <<
                                (double(_prevTT[_master] - tt) / USECS_PER_MSEC) << " sec at " <<
                                n_u::UTime(tt).format(true,"%Y %m %d %H:%M:%S.%6f"));
                        }
                        _prevData[oi] = val;
                        _prevTT[oi] = tt;
                        _samplesSinceMaster[oi] = 0;
                    }
                    else {
                        switch (_samplesSinceMaster[oi]) {
                        case 0:
                            // this is the first sample of this variable since the last master
                            // Assumes input samples are sorted in time!!
                            // Determine which of previous and current sample is the nearest
                            // to prevMasterTT.
                            if (_prevTT[_master] > (tt + _prevTT[oi]) / 2) {
                                _nearData[oi] = val;
                                _nearTT[oi] = tt;
                            }
                            else {
                                _nearData[oi] = _prevData[oi];
                                _nearTT[oi] = _prevTT[oi];
                            }
                            _samplesSinceMaster[oi]++;
                            break;
                        default:
                            // this is at least the second sample since the last master sample
                            // since samples are in time sequence, this one can't
                            // be the nearest one to the previous master.
                            break;
                        }
                        _prevData[oi] = val;
                        _prevTT[oi] = tt;
                    }
                }
            }
        }
    }
    return true;
}

/*
 * Send out whatever we have.
 */
void NearestResampler::finish() throw()
{
    if (_nmaster < 2) return;
    dsm_time_t maxTT;			// times must be < maxTT
    dsm_time_t minTT;			// times must be > minTT

    maxTT = _prevTT[_master] + (_prevTT[_master] - _nearTT[_master]);
    minTT = _nearTT[_master];

    SampleT<float>* osamp = getSample<float>(_outlen);
    float* outData = osamp->getDataPtr();
    int nonNANs = 0;
    for (unsigned int k = 0; k < _ndataValues; k++) {
	if (k == _master) {
	    // master variable
	    if (!isnan(outData[k] = _prevData[k])) nonNANs++;
	    _prevData[k] = floatNAN;
	    continue;
	}
	switch (_samplesSinceMaster[k]) {
	case 0:
	    // If there was no sample for this variable since
	    // the previous master then use prevData
	    if (_prevTT[k] > maxTT || _prevTT[k] < minTT)
		    outData[k] = floatNAN;
	    else if (!isnan(outData[k] = _prevData[k])) nonNANs++;
	    break;
	default:        // 1 or more
	    if (_nearTT[k] > maxTT || _nearTT[k] < minTT)
		    outData[k] = floatNAN;
	    else if (!isnan(outData[k] = _nearData[k])) nonNANs++;
	    break;
	}
	_samplesSinceMaster[k] = 0;
	_prevData[k] = floatNAN;
    }
    osamp->setTimeTag(_prevTT[_master]);
    osamp->setId(_outSample.getId());
    if (_ndataValues < _outlen) outData[_ndataValues] = (float) nonNANs;
    _source.distribute(osamp);

    _nmaster = 0;	// reset
    _source.flush();
}
