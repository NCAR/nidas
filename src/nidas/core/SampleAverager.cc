/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SampleAverager.h>
#include <nidas/core/SampleTag.h>
#include <nidas/core/Project.h>
#include <nidas/core/DSMTime.h>
#include <nidas/util/UTime.h>

#include <iomanip>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleAverager::SampleAverager():
        _source(false),
	_endTime(0),_ndataValues(0),_sums(0),_cnts(0)
{
    _outSample.setSampleId(Project::getInstance()->getUniqueSampleId(0));
    setAveragePeriodSecs(1.0);
    addSampleTag(&_outSample);
}

SampleAverager::SampleAverager(const vector<Variable*>& vars):
        _source(false),
	_endTime(0),_ndataValues(0),_sums(0),_cnts(0)
{
    _outSample.setSampleId(Project::getInstance()->getUniqueSampleId(0));
    setAveragePeriodSecs(1.0);
    vector<const Variable*> cvars(vars.begin(),vars.end());
    addVariables(cvars);
    addSampleTag(&_outSample);
}

SampleAverager::SampleAverager(const vector<const Variable*>& vars):
        _source(false),
	_endTime(0),_ndataValues(0),_sums(0),_cnts(0)
{
    _outSample.setSampleId(Project::getInstance()->getUniqueSampleId(0));
    setAveragePeriodSecs(1.0);
    addVariables(vars);
    addSampleTag(&_outSample);
}

SampleAverager::~SampleAverager()
{
    delete [] _sums;
    delete [] _cnts;
}

void SampleAverager::addVariables(const vector<const Variable*>& vars)
{
    int nvars = vars.size();
    for (int i = 0; i < nvars; i++) {
        const Variable* var = vars[i];
        addVariable(var);
    }
}

void SampleAverager::addVariable(const Variable *var)
{
    Variable* newvar = new Variable(*var);
    _outSample.addVariable(newvar);
    _outVarIndices[newvar] = _ndataValues;
    _ndataValues += var->getLength();
}


void SampleAverager::connect(SampleSource* source)
        throw(n_u::InvalidParameterException)
{
    // make a copy of source's SampleTags collection.
    list<const SampleTag*> intags = source->getSampleTags();
    list<const SampleTag*>::const_iterator inti = intags.begin();

    for ( ; inti != intags.end(); ++inti ) {
        const SampleTag* intag = *inti;
        dsm_sample_id_t sampid = intag->getId();

        // loop over variables in this input sample, checking
        // for a match against one of my variable names.
        VariableIterator vi = intag->getVariableIterator();
        bool varMatch = false;
        for (int iv = 0; vi.hasNext(); iv++) {
            const Variable* var = vi.next();
            int vindex = intag->getDataIndex(var);	// index of variable in its sample

            for (unsigned int iout = 0;
                iout < _outSample.getVariables().size(); iout++) {
                Variable& myvar = _outSample.getVariable(iout);
                if (*var == myvar) {
                    int vlen = var->getLength();

                    // index of the 0th value of this variable in the
                    // output array.
                    map<Variable*,int>::iterator vi = _outVarIndices.find(&myvar);
                    assert(vi != _outVarIndices.end());
                    int outIndex = vi->second;

                    map<dsm_sample_id_t,vector<int> >::iterator mi;
                    if ((mi = _inmap.find(sampid)) == _inmap.end()) {
                        vector<int> tmp;
                        tmp.push_back(vindex);
                        _inmap[sampid] = tmp;
                        assert((mi = _outmap.find(sampid)) == _outmap.end());
                        tmp.clear();
                        tmp.push_back(outIndex);
                        _outmap[sampid] = tmp;
                        tmp.clear();
                        tmp.push_back(vlen);
                        _lenmap[sampid] = tmp;
                    }
                    else {
                        mi->second.push_back(vindex);
                        assert((mi = _outmap.find(sampid)) != _outmap.end());
                        mi->second.push_back(outIndex);
                        assert((mi = _lenmap.find(sampid)) != _lenmap.end());
                        mi->second.push_back(vlen);
                    }
                    // copy attributes of variable
                    myvar = *var;
                    varMatch = true;
                }
            }
        }
        if (varMatch) source->addSampleClientForTag(this,intag);
    }
    init();
}

void SampleAverager::disconnect(SampleSource* source) throw()
{
    source->removeSampleClient(this);
}

void SampleAverager::init() throw()
{
    delete [] _sums;
    _sums = new double[_ndataValues];
    delete [] _cnts;
    _cnts = new int[_ndataValues];
    _endTime = 0;
}

/*
 * Send out last partial average.
 */
void SampleAverager::finish() throw ()
{
    int nok = 0;
    if (_endTime > 0) {
        SampleT<float>* osamp = getSample<float>(_ndataValues);
        osamp->setTimeTag(_endTime - _averagePeriodUsecs / 2);
        osamp->setId(_outSample.getId());
        float* fp = osamp->getDataPtr();
        for (int i = 0; i < _ndataValues; i++) {
            if (_cnts[i] > 0) {
                nok++;
                fp[i] = _sums[i] / _cnts[i];
            }
            else 
                fp[i] = floatNAN;
        }
        if (nok) _source.distribute(osamp);
        else osamp->freeReference();
        _endTime += _averagePeriodUsecs;
    }
    for (int i = 0; i < _ndataValues; i++) {
        _cnts[i] = 0;
        _sums[i] = 0.0;
    }
    _source.flush();
}

bool SampleAverager::receive(const Sample* samp) throw()
{
    if (samp->getType() != FLOAT_ST) return false;

    dsm_sample_id_t id = samp->getId();

    map<dsm_sample_id_t,vector<int> >::iterator mi;

    if ((mi = _inmap.find(id)) == _inmap.end()) return false;
    const vector<int>& invec = mi->second;

    assert((mi = _outmap.find(id)) != _outmap.end());
    const vector<int>& outvec = mi->second;

    assert((mi = _lenmap.find(id)) != _lenmap.end());
    const vector<int>& lenvec = mi->second;

    assert(invec.size() == outvec.size());
    assert(invec.size() == lenvec.size());

    dsm_time_t tt = samp->getTimeTag();

    if (tt >= _endTime) {
	if (_endTime > 0) {
	    SampleT<float>* osamp = getSample<float>(_ndataValues);
	    osamp->setTimeTag(_endTime - _averagePeriodUsecs / 2);
	    osamp->setId(_outSample.getId());
// #define DEBUG
#ifdef DEBUG
	    cerr << "SampleAverager: " << n_u::UTime(osamp->getTimeTag()).format("%c") <<
	    	" nvars=" << _ndataValues;
#endif
	    float* fp = osamp->getDataPtr();
	    for (int i = 0; i < _ndataValues; i++) {
		if (_cnts[i] > 0)
		    fp[i] = _sums[i] / _cnts[i];
		else 
		    fp[i] = floatNAN;
#ifdef DEBUG
		cerr << ' ' << setprecision(6) << setw(13) << fp[i];
#endif
	    }
#ifdef DEBUG
	    cerr << endl;
#endif
	    _source.distribute(osamp);
	    _endTime += _averagePeriodUsecs;
	    if (tt > _endTime) _endTime = timeCeiling(tt,_averagePeriodUsecs);
	}
	else {
            _endTime = timeCeiling(tt,_averagePeriodUsecs);
            if (!_cnts) init();
        }
	for (int i = 0; i < _ndataValues; i++) {
	    _cnts[i] = 0;
	    _sums[i] = 0.0;
	}
    }

    const SampleT<float>* fsamp = (const SampleT<float>*) samp;
    const float *fp = fsamp->getConstDataPtr();

    for (unsigned int i = 0; i < invec.size(); i++) {
	unsigned int ii = invec[i];
        int oi = outvec[i];
        for (int j = 0; j < lenvec[i]; j++) {
            if (ii >= fsamp->getDataLength()) continue;
            float v = fp[ii];
            assert(oi < _ndataValues);
            if (!isnan(v)) {
                _sums[oi] += v;
                _cnts[oi]++;
            }
            oi++;
            ii++;
        }
    }
    return true;
}
