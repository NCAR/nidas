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
#include <nidas/core/DSMTime.h>
#include <nidas/util/UTime.h>

#include <iomanip>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

SampleAverager::SampleAverager():
	outSampleId(50000),endTime(0),
	_ndataValues(0),sums(0),cnts(0)
{
    sampleTag.setSampleId(outSampleId);
    _tags.push_back(&sampleTag);
    setAveragePeriod(USECS_PER_SEC * 1);

}


SampleAverager::SampleAverager(const SampleAverager& x):
	averagePeriod(x.averagePeriod),outSampleId(x.outSampleId),
	endTime(0),_ndataValues(0), sums(0),cnts(0),sampleTag(x.sampleTag),
        _tags(x._tags)
{
}
SampleAverager::~SampleAverager()
{
    delete [] sums;
    delete [] cnts;
}

void SampleAverager::init() throw()
{
    delete [] sums;
    sums = new double[_ndataValues];
    delete [] cnts;
    cnts = new int[_ndataValues];
    endTime = 0;
}

void SampleAverager::addVariable(const Variable *var)
{

    const SampleTag* stag = var->getSampleTag();
    int vindex = stag->getDataIndex(var);	// index of variable in its sample
    assert(vindex >= 0);

    int vlen = var->getLength();
    dsm_sample_id_t sampid = stag->getId();

    std::map<dsm_sample_id_t,vector<int> >::iterator mi;

    Variable* newvar = new Variable(*var);
    sampleTag.addVariable(newvar);

    if ((mi = _inmap.find(sampid)) == _inmap.end()) {
	vector<int> tmp;
	tmp.push_back(vindex);
	_inmap[sampid] = tmp;
	assert((mi = _outmap.find(sampid)) == _outmap.end());
	tmp.clear();
	tmp.push_back(_ndataValues);
	_outmap[sampid] = tmp;
	tmp.clear();
	tmp.push_back(vlen);
	_lenmap[sampid] = tmp;
    }
    else {
        mi->second.push_back(vindex);
	assert((mi = _outmap.find(sampid)) != _outmap.end());
        mi->second.push_back(_ndataValues);
	assert((mi = _lenmap.find(sampid)) != _lenmap.end());
        mi->second.push_back(vlen);
    }
    _ndataValues += vlen;
#ifdef DEBUG
    cerr << "SampleAverager: addVariable: var=" << var->getName() <<
        " id=" << GET_DSM_ID(sampid) << ',' << GET_SHORT_ID(sampid) <<
        " vindex=" << vindex << " _ndataValues=" << _ndataValues << endl;
#endif
}

/*
 * Send out last partial average.
 */
void SampleAverager::finish() throw ()
{
    int nok = 0;
    if (endTime > 0) {
        SampleT<float>* osamp = getSample<float>(_ndataValues);
        osamp->setTimeTag(endTime - averagePeriod / 2);
        osamp->setId(outSampleId);
        float* fp = osamp->getDataPtr();
        for (int i = 0; i < _ndataValues; i++) {
            if (cnts[i] > 0) {
                nok++;
                fp[i] = sums[i] / cnts[i];
            }
            else 
                fp[i] = floatNAN;
        }
        if (nok) distribute(osamp);
        else osamp->freeReference();
        endTime += averagePeriod;
    }
    for (int i = 0; i < _ndataValues; i++) {
        cnts[i] = 0;
        sums[i] = 0.0;
    }
}

bool SampleAverager::receive(const Sample* samp) throw()
{
    if (samp->getType() != FLOAT_ST) return false;

    dsm_sample_id_t id = samp->getId();

    std::map<dsm_sample_id_t,vector<int> >::iterator mi;

    if ((mi = _inmap.find(id)) == _inmap.end()) return false;
    const vector<int>& invec = mi->second;

    assert((mi = _outmap.find(id)) != _outmap.end());
    const vector<int>& outvec = mi->second;

    assert((mi = _lenmap.find(id)) != _lenmap.end());
    const vector<int>& lenvec = mi->second;

    assert(invec.size() == outvec.size());
    assert(invec.size() == lenvec.size());

    dsm_time_t tt = samp->getTimeTag();

    if (tt >= endTime) {
	if (endTime > 0) {
	    SampleT<float>* osamp = getSample<float>(_ndataValues);
	    osamp->setTimeTag(endTime - averagePeriod / 2);
	    osamp->setId(outSampleId);
// #define DEBUG
#ifdef DEBUG
	    cerr << "SampleAverager: " << n_u::UTime(osamp->getTimeTag()).format("%c") <<
	    	" nvars=" << _ndataValues;
#endif
	    float* fp = osamp->getDataPtr();
	    for (int i = 0; i < _ndataValues; i++) {
		if (cnts[i] > 0)
		    fp[i] = sums[i] / cnts[i];
		else 
		    fp[i] = floatNAN;
#ifdef DEBUG
		cerr << ' ' << setprecision(6) << setw(13) << fp[i];
#endif
	    }
#ifdef DEBUG
	    cerr << endl;
#endif
	    distribute(osamp);
	    endTime += averagePeriod;
	    if (tt > endTime) endTime = timeCeiling(tt,averagePeriod);
	}
	else {
            endTime = timeCeiling(tt,averagePeriod);
            if (!cnts) init();
        }
	for (int i = 0; i < _ndataValues; i++) {
	    cnts[i] = 0;
	    sums[i] = 0.0;
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
                sums[oi] += v;
                cnts[oi]++;
            }
            oi++;
            ii++;
        }
    }
    return true;
}
