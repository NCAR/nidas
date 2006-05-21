/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <SampleAverager.h>
#include <SampleTag.h>
#include <DSMTime.h>

#include <iomanip>

using namespace dsm;
using namespace std;

SampleAverager::SampleAverager():
	outSampleId(50000),endTime(0),
	nvariables(0),sums(0),cnts(0)
{
    sampleTag.setSampleId(outSampleId);
    setAveragePeriod(USECS_PER_SEC * 1);
}


SampleAverager::SampleAverager(const SampleAverager& x):
	averagePeriod(x.averagePeriod),outSampleId(x.outSampleId),
	endTime(0),nvariables(0), sums(0),cnts(0),sampleTag(x.sampleTag)
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
    // kludge until we support a variable type attribute
    if (!var->getName().compare("Clock")) return;

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
}

/*
 * Todo: implement finish.
 */
void SampleAverager::finish() throw () {}

bool SampleAverager::receive(const Sample* samp) throw()
{
    // processed clock samples are long long
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
	    cerr << "SampleAverager: " << osamp->getTimeTag() <<
	    	" nvars=" << nvariables;
	    float* fp = osamp->getDataPtr();
	    for (int i = 0; i < nvariables; i++) {
		if (cnts[i] > 0)
		    fp[i] = sums[i] / cnts[i];
		else 
		    fp[i] = floatNAN;
		cerr << ' ' << setprecision(6) << setw(13) << fp[i];
	    }
	    cerr << endl;
	    distribute(osamp);
	    endTime += averagePeriod;
	    if (tt > endTime) endTime = timeCeiling(tt,averagePeriod);
	}
	else endTime = timeCeiling(tt,averagePeriod);
	for (int i = 0; i < nvariables; i++) {
	    cnts[i] = 0;
	    sums[i] = 0.0;
	}
    }

    const SampleT<float>* fsamp = (const SampleT<float>*) samp;

    for (unsigned int i = 0; i < invec.size(); i++) {
	unsigned int ii = invec[i];
	assert(ii < fsamp->getDataLength());
        float v = fsamp->getConstDataPtr()[ii];
	int oi = outvec[i];
	assert(oi < nvariables);
	sums[oi] += v;
	cnts[oi]++;
    }
    return true;
}
