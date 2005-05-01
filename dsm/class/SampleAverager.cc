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

using namespace dsm;
using namespace std;

SampleAverager::SampleAverager(): outSampleId(0),endTime(0),
	nvariables(0),sums(0),cnts(0)
{
}

SampleAverager::~SampleAverager()
{
    delete [] sums;
    delete [] cnts;
}

void SampleAverager::init() throw()
{
    sums = new double[nvariables];
    cnts = new int[nvariables];
}

void SampleAverager::addVariable(const Variable *var)
{
    const SampleTag* stag = var->getSampleTag();
    int vindex = stag->getIndex(var);	// index of variable in its sample
    assert(vindex > 0);
    dsm_sample_id_t sampid = stag->getId();

    std::map<dsm_sample_id_t,vector<int> >::iterator mi;

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
bool SampleAverager::receive(const Sample* samp) throw()
{
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
	    for (int i = 0; i < nvariables; i++) {
		if (cnts[i] > 0)
		    osamp->getDataPtr()[i] = sums[i] / cnts[i];
		else 
		    osamp->getDataPtr()[i] = floatNAN;
	    }
	    distribute(osamp);
	    endTime += averagePeriod;
	    if (tt > endTime) endTime = timeCeiling(tt,averagePeriod);
	}
	else endTime = timeCeiling(tt,averagePeriod);
    }
	    	
    assert(samp->getType() == FLOAT_ST);

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
