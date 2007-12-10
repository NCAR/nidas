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
	nvariables(0),sums(0),cnts(0)
{
    sampleTag.setSampleId(outSampleId);
    _tags.push_back(&sampleTag);
    setAveragePeriod(USECS_PER_SEC * 1);

}


SampleAverager::SampleAverager(const SampleAverager& x):
	averagePeriod(x.averagePeriod),outSampleId(x.outSampleId),
	endTime(0),nvariables(0), sums(0),cnts(0),sampleTag(x.sampleTag),
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
    sums = new double[nvariables];
    delete [] cnts;
    cnts = new int[nvariables];
    endTime = 0;
}

void SampleAverager::addVariable(const Variable *var)
{

    const SampleTag* stag = var->getSampleTag();
    int vindex = stag->getIndex(var);	// index of variable in its sample
    assert(vindex >= 0);
    dsm_sample_id_t sampid = stag->getId();

    std::map<dsm_sample_id_t,vector<int> >::iterator mi;

    Variable* newvar = new Variable(*var);
    sampleTag.addVariable(newvar);

    if ((mi = inmap.find(sampid)) == inmap.end()) {
	vector<int> tmp;
	tmp.push_back(vindex);
	inmap[sampid] = tmp;
	assert((mi = outmap.find(sampid)) == outmap.end());
	tmp.clear();
	tmp.push_back(nvariables++);
	outmap[sampid] = tmp;
    }
    else {
        mi->second.push_back(vindex);
	assert((mi = outmap.find(sampid)) != outmap.end());
        mi->second.push_back(nvariables++);
    }
#ifdef DEBUG
    cerr << "SampleAverager: addVariable: var=" << var->getName() <<
        " id=" << GET_DSM_ID(sampid) << ',' << GET_SHORT_ID(sampid) <<
        " vindex=" << vindex << " nvariables=" << nvariables << endl;
#endif
}

/*
 * Send out last partial average.
 */
void SampleAverager::finish() throw ()
{
    int nok = 0;
    if (endTime > 0) {
        SampleT<float>* osamp = getSample<float>(nvariables);
        osamp->setTimeTag(endTime - averagePeriod / 2);
        osamp->setId(outSampleId);
        float* fp = osamp->getDataPtr();
        for (int i = 0; i < nvariables; i++) {
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
    for (int i = 0; i < nvariables; i++) {
        cnts[i] = 0;
        sums[i] = 0.0;
    }
}

bool SampleAverager::receive(const Sample* samp) throw()
{
    if (samp->getType() != FLOAT_ST) return false;

    dsm_sample_id_t id = samp->getId();

    std::map<dsm_sample_id_t,vector<int> >::iterator mi;

    if ((mi = inmap.find(id)) == inmap.end()) return false;
    const vector<int>& invec = mi->second;

    assert((mi = outmap.find(id)) != outmap.end());
    const vector<int>& outvec = mi->second;

    assert(invec.size() == outvec.size());

    dsm_time_t tt = samp->getTimeTag();

    if (tt >= endTime) {
	if (endTime > 0) {
	    SampleT<float>* osamp = getSample<float>(nvariables);
	    osamp->setTimeTag(endTime - averagePeriod / 2);
	    osamp->setId(outSampleId);
// #define DEBUG
#ifdef DEBUG
	    cerr << "SampleAverager: " << n_u::UTime(osamp->getTimeTag()).format("%c") <<
	    	" nvars=" << nvariables;
#endif
	    float* fp = osamp->getDataPtr();
	    for (int i = 0; i < nvariables; i++) {
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
	for (int i = 0; i < nvariables; i++) {
	    cnts[i] = 0;
	    sums[i] = 0.0;
	}
    }

    const SampleT<float>* fsamp = (const SampleT<float>*) samp;
    const float *fp = fsamp->getConstDataPtr();

    for (unsigned int i = 0; i < invec.size(); i++) {
	unsigned int ii = invec[i];
	if (ii >= fsamp->getDataLength()) continue;
        float v = fp[ii];
	int oi = outvec[i];
	assert(oi < nvariables);
        if (!isnan(v)) {
            sums[oi] += v;
            cnts[oi]++;
        }
    }
    return true;
}
