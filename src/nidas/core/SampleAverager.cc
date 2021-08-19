// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#include "SampleAverager.h"
#include "SampleTag.h"
#include "Variable.h"
#include "Project.h"
#include <nidas/util/UTime.h>

#include <iomanip>
#include <cmath>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleAverager::SampleAverager():
        _source(false),_outSample(),_averagePeriodUsecs(0),
	_endTime(0),_outVarIndices(),_inmap(),_lenmap(),_outmap(),
        _ndataValues(0),_sums(0),_cnts(0)
{
    _outSample.setSampleId(Project::getInstance()->getUniqueSampleId(0));
    setAveragePeriodSecs(1.0);
    addSampleTag(&_outSample);
}

SampleAverager::SampleAverager(const std::vector<Variable*>& vars):
        _source(false),_outSample(),_averagePeriodUsecs(0),
	_endTime(0),_outVarIndices(),_inmap(),_lenmap(),_outmap(),
        _ndataValues(0),_sums(0),_cnts(0)
{
    _outSample.setSampleId(Project::getInstance()->getUniqueSampleId(0));
    setAveragePeriodSecs(1.0);
    vector<const Variable*> cvars(vars.begin(),vars.end());
    addVariables(cvars);
    addSampleTag(&_outSample);
}

SampleAverager::SampleAverager(const std::vector<const Variable*>& vars):
        _source(false),_outSample(),_averagePeriodUsecs(0),
	_endTime(0),_outVarIndices(),_inmap(),_lenmap(),_outmap(),
        _ndataValues(0),_sums(0),_cnts(0)
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
{
    // make a copy of source's SampleTags collection.
    list<const SampleTag*> intags = source->getSampleTags();

    vector<bool> varMatched(_outSample.getVariables().size());

    unsigned int nMatch = 0;

    list<const SampleTag*>::const_iterator inti = intags.begin();
    for ( ; inti != intags.end(); ++inti ) {
        const SampleTag* intag = *inti;
        dsm_sample_id_t sampid = intag->getId();

        // loop over variables in this input sample, checking
        // for a match against one of my variable names.
        VariableIterator vi = intag->getVariableIterator();
        bool matchInSample = false;
        for ( ; vi.hasNext(); ) {
            const Variable* var = vi.next();

            // index of 0th value of variable in its sample data array.
            unsigned int vindex = intag->getDataIndex(var);

            for (unsigned int iout = 0;
                iout < _outSample.getVariables().size(); iout++) {

                Variable& myvar = _outSample.getVariable(iout);

#ifdef DEBUG
                if (myvar.getName().substr(0,4) == "CNT1") {
		    cerr << "SampleAverager::connect, var=" << var->getName() <<
			"(" << (var->getSite() ? var->getSite()->getName() : "null") << ")" <<
			"(" << var->getStation() << ")" <<
			", myvar=" << myvar.getName() <<
			"(" << (myvar.getSite() ? myvar.getSite()->getName() : "null") << ")" <<
			"(" << myvar.getStation() << ")" <<
			", match=" << (*var == myvar) << endl;
		}
#endif

                if (*var == myvar) {
                    varMatched[iout] = true;
                    nMatch++;
                    matchInSample = true;

                    unsigned int vlen = var->getLength();

                    // index of the 0th value of this variable in the
                    // output array.
                    map<Variable*,unsigned int>::iterator vi = _outVarIndices.find(&myvar);
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
                    // copy attributes of variable
                    myvar = *var;
                }
            }
        }
        if (matchInSample) source->addSampleClientForTag(this,intag);
    }
    if (nMatch < _outSample.getVariables().size()) {
        string vars;
        for (unsigned int i = 0; i < varMatched.size(); i++) {
            if (!varMatched[i]) {
                const Variable& var = _outSample.getVariable(i);
                vars += ' ';
                vars += var.getName();
                // When the variables are matched against variables
                // from a SampleSource, the Site is checked, so
                // notity the user is a site is not set for a requested
                // variable.
                if (!var.getSite()) vars += "(null site)";
            }
        }
        WLOG(("SampleAverager: variables not found in source:") << vars);
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
void SampleAverager::flush() throw ()
{
    int nok = 0;
    if (_endTime > 0) {
        SampleT<float>* osamp = getSample<float>(_ndataValues);
        osamp->setTimeTag(_endTime - _averagePeriodUsecs / 2);
        osamp->setId(_outSample.getId());
        float* fp = osamp->getDataPtr();
        for (unsigned int i = 0; i < _ndataValues; i++) {
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
    for (unsigned int i = 0; i < _ndataValues; i++) {
        _cnts[i] = 0;
        _sums[i] = 0.0;
    }
}

bool SampleAverager::receive(const Sample* samp) throw()
{
    if (samp->getType() != FLOAT_ST && samp->getType() != DOUBLE_ST) return false;

    dsm_sample_id_t id = samp->getId();

    map<dsm_sample_id_t,vector<unsigned int> >::iterator mi;

    if ((mi = _inmap.find(id)) == _inmap.end()) return false;
    const vector<unsigned int>& invec = mi->second;

    mi = _outmap.find(id);
    assert(mi != _outmap.end());
    const vector<unsigned int>& outvec = mi->second;

    mi = _lenmap.find(id);
    assert(mi != _lenmap.end());
    const vector<unsigned int>& lenvec = mi->second;

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
	    for (unsigned int i = 0; i < _ndataValues; i++) {
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
	    if (tt > _endTime) _endTime = n_u::timeCeiling(tt,_averagePeriodUsecs);
	}
	else {
            _endTime = n_u::timeCeiling(tt,_averagePeriodUsecs);
            if (!_cnts) init();
        }
	for (unsigned int i = 0; i < _ndataValues; i++) {
	    _cnts[i] = 0;
	    _sums[i] = 0.0;
	}
    }

    for (unsigned int iv = 0; iv < invec.size(); iv++) {
	unsigned int ii = invec[iv];
        unsigned int oi = outvec[iv];
        for (unsigned int j = 0;  j < lenvec[iv] && ii < samp->getDataLength(); j++) {
            double v = samp->getDataValue(ii);
            assert(oi < _ndataValues);
            if (!std::isnan(v)) {
                _sums[oi] += v;
                _cnts[oi]++;
            }
            oi++;
            ii++;
        }
    }
    return true;
}
