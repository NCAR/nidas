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
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NearestResampler::NearestResampler(const vector<const Variable*>& vars)
{
    ctorCommon(vars);
}

NearestResampler::NearestResampler(const vector<Variable*>& vars)
{
    vector<const Variable*> newvars;
    for (unsigned int i = 0; i < vars.size(); i++)
    	newvars.push_back(vars[i]);
    ctorCommon(newvars);
}

NearestResampler::~NearestResampler()
{
    delete [] prevTT;
    delete [] nearTT;
    delete [] prevData;
    delete [] nearData;
    delete [] samplesSinceMaster;

    map<dsm_sample_id_t,vector<int*> >::iterator vmi;
    for (vmi = sampleMap.begin(); vmi != sampleMap.end(); ++vmi) {
	vector<int*>& vindices = vmi->second;
	for (unsigned int iv = 0; iv < vindices.size(); iv++)
	    delete [] vindices[iv];
    }
}

void NearestResampler::ctorCommon(const vector<const Variable*>& vars)
{
    nvars = vars.size();
    outlen = nvars + 1;
    master = 0;
    nmaster = 0;
    prevTT = new dsm_time_t[nvars];
    nearTT = new dsm_time_t[nvars];
    prevData = new float[nvars];
    nearData = new float[nvars];
    samplesSinceMaster = new int[nvars];

    for (int i = 0; i < nvars; i++) {
	prevTT[i] = 0;
	nearTT[i] = 0;
	prevData[i] = floatNAN;
	nearData[i] = floatNAN;
	samplesSinceMaster[i] = 0;

	Variable* v = new Variable(*vars[i]);
	outSample.addVariable(v);
    }

    // Number of non-NAs in the output sample.
    Variable* v = new Variable();
    v->setName("nonNANs");
    v->setType(Variable::WEIGHT);
    v->setUnits("");
    outSample.addVariable(v);
}
void NearestResampler::connect(SampleInput* input)
	throw(n_u::IOException)
{
    if (sampleTags.size() > 0)
    	throw n_u::IOException(input->getName(),"NearestResampler",
		"cannot have more than one input");

    long dsmid = -1;
    bool oneDSM = true;

    // make a copy of input's SampleTags collection.
    list<const SampleTag*> intags(input->getSampleTags().begin(),
	input->getSampleTags().end());

    // cerr << "NearestResamples, intags.size()=" << intags.size() << endl;
    list<const SampleTag*>::const_iterator inti = intags.begin();
    for ( ; inti != intags.end(); ++inti ) {
	const SampleTag* intag = *inti;
	dsm_sample_id_t id = intag->getId();
	dsm_sample_id_t sensorId = id - intag->getSampleId();

	map<dsm_sample_id_t,vector<int*> >::iterator vmi =
	    sampleMap.find(id);

	vector<int*> indices;
	vector<int*>* vptr = &indices;
	if (vmi != sampleMap.end()) vptr = &vmi->second;

	// if it is a raw sample from a sensor, then
	// this should be non-NULL.
	DSMSensor* sensor = Project::getInstance()->findSensor(sensorId);
        // cerr << "NearestResampler, sensor=" << (sensor ? sensor->getName() : "not found") << endl;

	// loop over variables in this input sample, checking
	// for a match against one of my variable names.
	VariableIterator vi = intag->getVariableIterator();
	for (int iv = 0; vi.hasNext(); iv++) {
	    const Variable* var = vi.next();

	    for (unsigned int iout = 0;
	    	iout < outSample.getVariables().size(); iout++) {

		Variable& myvar = outSample.getVariable(iout);

#ifdef DEBUG
		// variable match in name and station.
		cerr << "checking " << var->getName() << "(" << var->getStation() << ") against " <<
			myvar.getName() << "(" << myvar.getStation() <<
			"), result=" << (*var == myvar) << endl;
#endif
		if (*var == myvar) {
		    // paranoid check that this variable hasn't been added
		    unsigned int j;
		    for (j = 0; j < vptr->size(); j++)
			if ((unsigned)(*vptr)[j][1] == iout) break;
		    if (j == vptr->size()) {
			int* idxs = new int[2];
			idxs[0] = iv;	// input index
			idxs[1] = iout;	// output index
			vptr->push_back(idxs);
			if (dsmid < 0) dsmid = intag->getDSMId();
			else if (dsmid != (signed) intag->getDSMId())
				oneDSM = false;
		    }
		    // copy attributes of variable
		    myvar = *var;
		    if (sensor)
			input->addProcessedSampleClient(this,sensor);
		    else
			input->addSampleClient(this);
		}
	    }
	}
	if (vmi == sampleMap.end() && indices.size() > 0)
		sampleMap[id] = indices;
    }

    if (!oneDSM) dsmid = 0;

    outSample.setDSMId(dsmid);
    outSample.setSensorId(0);
    dsm_sample_id_t id;
    id  = Project::getInstance()->getUniqueSampleId(dsmid);
    outSample.setSampleId(id);
    sampleTags.insert(&outSample);
}

void NearestResampler::disconnect(SampleInput* input)
	throw(n_u::IOException)
{
    input->removeProcessedSampleClient(this);
    input->removeSampleClient(this);
}

bool NearestResampler::receive(const Sample* s) throw()
{
    dsm_sample_id_t id = s->getId();

    map<dsm_sample_id_t,vector<int*> >::const_iterator vmi =
    	sampleMap.find(id);
    if (vmi == sampleMap.end()) return false;	// unrecognized sample

    const vector<int*>& vindices = vmi->second;

    dsm_time_t tt = s->getTimeTag();
    const float* inData = (const float*) s->getConstVoidDataPtr();

    for (unsigned int iv = 0; iv < vindices.size(); iv++) {
	int invar = vindices[iv][0];
	int outvar = vindices[iv][1];
	float val = inData[invar];
	if (outvar == master) {
	    /*
	     * received a new master variable. Output values that were
	     * nearest to previous master.
	     */
	    dsm_time_t maxTT;		// time tags must be < maxTT
	    dsm_time_t minTT;		// time tags must be > minTT
	    if (nmaster < 2) {
		if (nmaster++ == 0) {
		    nearTT[master] = prevTT[master];
		    prevTT[master] = tt;
		    prevData[master] = val;
		    continue;
		}
		// maxTT is current time minus 0.1 of deltat
		maxTT = tt - (tt - prevTT[master]) / 10;
		// minTT is previous time minus 0.9 of deltat
		minTT = prevTT[master] - ((tt - prevTT[master]) * 9) / 10;
	    }
	    else {
		// maxTT is current time minus 0.1 of deltat
		maxTT = tt - (tt - prevTT[master]) / 10;
		// minTT is two times back plus 0.1 of deltat
		minTT = nearTT[master] +
		    (prevTT[master] - nearTT[master]) / 10;
	    }

	    SampleT<float>* osamp = getSample<float>(outlen);
	    float* outData = osamp->getDataPtr();
	    int nonNANs = 0;
	    for (int k = 0; k < nvars; k++) {
		if (k == master) {
		  // master variable
		  if (!isnan(outData[k] = prevData[k])) nonNANs++;
		  continue;
		}
		switch (samplesSinceMaster[k]) {
		case 0:
		    // If there was no sample for this variable since
		    // the previous master then use prevData
		    if (prevTT[k] > maxTT || prevTT[k] < minTT)
			    outData[k] = floatNAN;
		    else if (!isnan(outData[k] = prevData[k])) nonNANs++;
		    break;
		default:        // 1 or more
		    if (nearTT[k] > maxTT || nearTT[k] < minTT)
			    outData[k] = floatNAN;
		    else if (!isnan(outData[k] = nearData[k])) nonNANs++;
		    break;
		}
		samplesSinceMaster[k] = 0;
	    }
	    osamp->setTimeTag(prevTT[master]);
	    osamp->setId(outSample.getId());
	    outData[nvars] = (float) nonNANs;
	    distribute(osamp);

	    nearTT[master] = prevTT[master];
	    prevTT[master] = tt;
	    prevData[master] = val;
	}
	else {
	    switch (samplesSinceMaster[outvar]) {
	    case 0:
	      // this is the first sample of this variable since the last master
	      // Assumes input samples are sorted in time!!
	      // Determine which of previous and current sample is the nearest
	      // to prevMasterTT.
	      if (prevTT[master] > (tt + prevTT[outvar]) / 2) {
		  nearData[outvar] = val;
		  nearTT[outvar] = tt;
	      }
	      else {
		  nearData[outvar] = prevData[outvar];
		  nearTT[outvar] = prevTT[outvar];
	      }
	      samplesSinceMaster[outvar]++;
	      break;
	    default:
		// this is at least the second sample since the last master sample
		// since samples are in time sequence, this one can't
		// be the nearest one to the previous master.
		break;
	    }
	    prevData[outvar] = val;
	    prevTT[outvar] = tt;
	}
    }
    return true;
}

/*
 * Send out whatever we have.
 */
void NearestResampler::finish() throw()
{
    if (nmaster < 2) return;
    dsm_time_t maxTT;			// times must be < maxTT
    dsm_time_t minTT;			// times must be > minTT

    maxTT = prevTT[master] + (prevTT[master] - nearTT[master]);
    minTT = nearTT[master];

    SampleT<float>* osamp = getSample<float>(outlen);
    float* outData = osamp->getDataPtr();
    int nonNANs = 0;
    for (int k = 0; k < nvars; k++) {
	if (k == master) {
	    // master variable
	    if (!isnan(outData[k] = prevData[k])) nonNANs++;
	    prevData[k] = floatNAN;
	    continue;
	}
	switch (samplesSinceMaster[k]) {
	case 0:
	    // If there was no sample for this variable since
	    // the previous master then use prevData
	    if (prevTT[k] > maxTT || prevTT[k] < minTT)
		    outData[k] = floatNAN;
	    else if (!isnan(outData[k] = prevData[k])) nonNANs++;
	    break;
	default:        // 1 or more
	    if (nearTT[k] > maxTT || nearTT[k] < minTT)
		    outData[k] = floatNAN;
	    else if (!isnan(outData[k] = nearData[k])) nonNANs++;
	    break;
	}
	samplesSinceMaster[k] = 0;
	prevData[k] = floatNAN;
    }
    osamp->setTimeTag(prevTT[master]);
    osamp->setId(outSample.getId());
    outData[nvars] = (float) nonNANs;
    distribute(osamp);

    nmaster = 0;	// reset
}
