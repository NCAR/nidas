/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/NearestResamplerAtRate.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

NearestResamplerAtRate::NearestResamplerAtRate(const vector<const Variable*>& vars): osamp(0),fillGaps(false)
{
    ctorCommon(vars);
}

NearestResamplerAtRate::NearestResamplerAtRate(const vector<Variable*>& vars):
    osamp(0),fillGaps(false)
{
    vector<const Variable*> newvars;
    for (unsigned int i = 0; i < vars.size(); i++)
    	newvars.push_back(vars[i]);
    ctorCommon(newvars);
}

NearestResamplerAtRate::~NearestResamplerAtRate()
{
    delete [] prevTT;
    delete [] nearTT;
    delete [] prevData;
    delete [] nearData;
    delete [] samplesSinceOutput;

    map<dsm_sample_id_t,vector<int*> >::iterator vmi;
    for (vmi = sampleMap.begin(); vmi != sampleMap.end(); ++vmi) {
	vector<int*>& vindices = vmi->second;
	for (unsigned int iv = 0; iv < vindices.size(); iv++)
	    delete [] vindices[iv];
    }
    if (osamp) osamp->freeReference();
}

void NearestResamplerAtRate::ctorCommon(const vector<const Variable*>& vars)
{
    nvars = vars.size();
    outlen = nvars + 1;
    prevTT = new dsm_time_t[nvars];
    nearTT = new dsm_time_t[nvars];
    prevData = new float[nvars];
    nearData = new float[nvars];
    samplesSinceOutput = new int[nvars];

    for (int i = 0; i < nvars; i++) {
	prevTT[i] = 0;
	nearTT[i] = 0;
	prevData[i] = floatNAN;
	nearData[i] = floatNAN;
	samplesSinceOutput[i] = 0;

	Variable* v = new Variable(*vars[i]);
	outSampleTag.addVariable(v);
    }

    // Variable containing the number of non-NAs in the output sample.
    Variable* v = new Variable();
    v->setName("nonNANs");
    v->setType(Variable::WEIGHT);
    v->setUnits("");
    outSampleTag.addVariable(v);
    setRate(10.);       // pick a default
    outputTT = nextOutputTT = 0;
}

void NearestResamplerAtRate::connect(SampleInput* input)
	throw(n_u::IOException)
{
    if (sampleTags.size() > 0)
    	throw n_u::IOException(input->getName(),"NearestResamplerAtRate",
		"cannot have more than one input");

    long dsmid = -1;

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
        // cerr << "NearestResamplerAtRate, sensor=" << (sensor ? sensor->getName() : "not found") << endl;

	// loop over variables in this input sample, checking
	// for a match against one of my variable names.
	VariableIterator vi = intag->getVariableIterator();
	for (int iv = 0; vi.hasNext(); iv++) {
	    const Variable* var = vi.next();

	    for (unsigned int iout = 0;
	    	iout < outSampleTag.getVariables().size(); iout++) {

		Variable& myvar = outSampleTag.getVariable(iout);

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
				dsmid = 0;
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

    outSampleTag.setDSMId(dsmid);
    outSampleTag.setSensorId(0);
    dsm_sample_id_t id;
    id  = Project::getInstance()->getUniqueSampleId(dsmid);
    outSampleTag.setSampleId(id);
    sampleTags.insert(&outSampleTag);
}

void NearestResamplerAtRate::disconnect(SampleInput* input)
	throw(n_u::IOException)
{
    input->removeProcessedSampleClient(this);
    input->removeSampleClient(this);
}

bool NearestResamplerAtRate::receive(const Sample* s) throw()
{

    dsm_sample_id_t id = s->getId();

    map<dsm_sample_id_t,vector<int*> >::const_iterator vmi =
    	sampleMap.find(id);
    if (vmi == sampleMap.end()) return false;	// unrecognized sample

    const vector<int*>& vindices = vmi->second;

    dsm_time_t tt = s->getTimeTag();
    const float* inData = (const float*) s->getConstVoidDataPtr();

#ifdef DEBUG
    static dsm_time_t lastTT;
    static dsm_sample_id_t lastId;
    if (tt < lastTT) {
        cerr << "tt=" << n_u::UTime(tt).format(true,"%c") <<
            " id=" << GET_DSM_ID(id) << "," << GET_SHORT_ID(id) << endl;
        cerr << "lasttt=" << n_u::UTime(lastTT).format(true,"%c") <<
            " id=" << GET_DSM_ID(lastId) << "," << GET_SHORT_ID(lastId) << endl;
    }
    lastTT = tt;
    lastId = id;
#endif

    if (tt > nextOutputTT) sendSample(tt);

    for (unsigned int iv = 0; iv < vindices.size(); iv++) {
	int invar = vindices[iv][0];
	float val = inData[invar];
        if (isnan(val)) continue;        // doesn't exist
	int outvar = vindices[iv][1];

        switch (samplesSinceOutput[outvar]) {
        case 0:
            // this is the first sample of this variable since outputTT
	    // Assumes input samples are sorted in time!!
	    // Determine which of previous and current sample is the nearest
	    // to outputTT
            if (tt >= outputTT) {
                if (outputTT > (tt + prevTT[outvar]) / 2) {
                    nearData[outvar] = val;
                    nearTT[outvar] = tt;
                }
                else {
                    nearData[outvar] = prevData[outvar];
                    nearTT[outvar] = prevTT[outvar];
                }
                samplesSinceOutput[outvar]++;
            }
            break;
        default:
            // this is at least the second sample since outputTT
            // since samples are in time sequence, this one can't
            // be the nearest one to outputTT.
            break;
        }
        prevData[outvar] = val;
        prevTT[outvar] = tt;
    }
    return true;
}


void NearestResamplerAtRate::sendSample(dsm_time_t tt) throw()
{
    if (!osamp) {
        outputTT = tt - (tt % deltatUsec);
        nextOutputTT = outputTT + deltatUsec;
        osamp = getSample<float>(outlen);
        osamp->setId(outSampleTag.getId());
    }
    while (tt > nextOutputTT) {
        dsm_time_t maxTT = nextOutputTT - deltatUsec10;
        dsm_time_t minTT = outputTT - deltatUsec + deltatUsec10;
        int nonNANs = 0;
        float* outData = osamp->getDataPtr();
        for (int i = 0; i < nvars; i++) {
            switch (samplesSinceOutput[i]) {
            case 0:
                // If there was no sample for this variable since outputTT
                // then match prevData with the outputTT.
                if (prevTT[i] > maxTT || prevTT[i] < minTT)
                    outData[i] = floatNAN;
                else if (!isnan(outData[i] = prevData[i])) nonNANs++;
                break;
            default:
                if (nearTT[i] > maxTT || nearTT[i] < minTT)
                    outData[i] = floatNAN;
                else if (!isnan(outData[i] = nearData[i])) nonNANs++;
                break;
            }
            samplesSinceOutput[i] = 0;
        }
        outData[nvars] = (float) nonNANs;
        if (nonNANs > 0 || fillGaps)  {
            osamp->setTimeTag(outputTT);
            distribute(osamp);
            osamp = getSample<float>(outlen);
            osamp->setId(outSampleTag.getId());
            outputTT += deltatUsec;
            nextOutputTT += deltatUsec;
        }
        else {
            // jump ahead
            outputTT = tt - (tt % deltatUsec);
            nextOutputTT = outputTT + deltatUsec;
        }
    }
}

/*
 * Send out whatever we have.
 */
void NearestResamplerAtRate::finish() throw()
{
    sendSample(nextOutputTT + 1);
}
