/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/dynld/raf/SyncRecordSource.h>
#include <nidas/core/SampleInput.h>
#include <nidas/dynld/raf/Aircraft.h>
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>

#include <iomanip>

#include <cmath>

// #define DEBUG

using namespace nidas::core;
using namespace nidas::dynld::raf;
using namespace std;

namespace n_u = nidas::util;

SyncRecordSource::SyncRecordSource():
	syncRecord(0),badTimes(0),aircraft(0),
	initialized(false),unknownSampleType(0)
{
    syncRecordHeaderSampleTag.setDSMId(0);
    syncRecordHeaderSampleTag.setSensorId(0);
    syncRecordHeaderSampleTag.setSampleId(SYNC_RECORD_HEADER_ID);
    syncRecordHeaderSampleTag.setRate(0.0);
    sampleTags.insert(&syncRecordHeaderSampleTag);

    syncRecordDataSampleTag.setDSMId(0);
    syncRecordDataSampleTag.setSensorId(0);
    syncRecordDataSampleTag.setSampleId(SYNC_RECORD_ID);
    syncRecordDataSampleTag.setRate(1.0);
    sampleTags.insert(&syncRecordDataSampleTag);
}

SyncRecordSource::~SyncRecordSource()
{
    if (syncRecord) syncRecord->freeReference();

    map<dsm_sample_id_t,int*>::const_iterator vi;
    for (vi = varOffsets.begin(); vi != varOffsets.end(); ++vi)
	delete [] vi->second;

    map<dsm_sample_id_t,size_t*>::const_iterator vi2;
    for (vi2 = varLengths.begin(); vi2 != varLengths.end(); ++vi2)
	delete [] vi2->second;

}

void SyncRecordSource::connect(SampleInput* input) throw()
{


    // make a copy of input's SampleTags collection
    list<const SampleTag*> itags(input->getSampleTags().begin(),
	input->getSampleTags().end());
    list<const SampleTag*>::const_iterator si = itags.begin();

    for ( ; si != itags.end(); ++si) {
	const SampleTag* stag = *si;

	// nimbus needs to know the aircraft
	if (!aircraft) {
	    const DSMConfig* dsm =
		    Project::getInstance()->findDSM(stag->getDSMId());
	    const Site* site = dsm->getSite();
	    const Aircraft* acft = dynamic_cast<const Aircraft*>(site);
	    aircraft = acft;
	}

	DSMSensor* sensor =
		Project::getInstance()->findSensor(stag->getId());
	if (!sensor) {
	    n_u::Logger::getInstance()->log(LOG_WARNING,
               "sensor matching id=%d,%d, not found",
               GET_DSM_ID(stag->getId()),GET_SHORT_ID(stag->getId()));
	    continue;
	}
	addSensor(sensor);
	// This adds sample tags to input, hence the copy of the
	// set above.
	input->addProcessedSampleClient(this,sensor);
    }
    init();
}

void SyncRecordSource::disconnect(SampleInput* input) throw()
{
    set<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
        DSMSensor* sensor = *si;
	input->removeProcessedSampleClient(this,sensor);
    }
}

void SyncRecordSource::addSensor(DSMSensor* sensor) throw()
{
    sensors.insert(sensor);

    const set<const SampleTag*>& tags = sensor->getSampleTags();

    set<const SampleTag*>::const_iterator ti;
    for (ti = tags.begin(); ti != tags.end(); ++ti) {
	const SampleTag* tag = *ti;

	if (!tag->isProcessed()) continue;

	dsm_sample_id_t sampleId = tag->getId();
	float rate = tag->getRate();

	const vector<const Variable*>& vars = tag->getVariables();

	// skip samples with one non-continuous, non-counter variable
	if (vars.size() == 1) {
	    Variable::type_t vt = vars.front()->getType();
	    if (vt != Variable::CONTINUOUS && vt != Variable::COUNTER)
		    continue;
	}

	int groupId = varsOfRate.size();
	varsOfRate.push_back(list<const Variable*>());

	groupLengths.push_back(0);
	groupOffsets.push_back(0);

	rates.push_back(rate);
	usecsPerSample.push_back((int)rint(USECS_PER_SEC / rate));
	samplesPerSec.push_back((int)ceil(rate));

#ifdef DEBUG
	cerr << "SyncRecordSource, rate=" << rate <<
	    " groupId=" << groupId << endl;
#endif

	groupIds[sampleId] = groupId;

	int* varOffset = new int[vars.size()];
	varOffsets[sampleId] = varOffset;

	size_t* varLen = new size_t[vars.size()];
	varLengths[sampleId] = varLen;

	numVars[sampleId] = vars.size();

	vector<const Variable*>::const_iterator vi;
	int iv;
	for (vi = vars.begin(),iv=0; vi != vars.end(); ++vi,iv++) {
	    const Variable* var = *vi;
	    size_t vlen = var->getLength();
	    varLen[iv] = vlen;

	    Variable::type_t vt = var->getType();
	    varOffset[iv] = -1;
	    if (vt == Variable::CONTINUOUS || vt == Variable::COUNTER) {
		varOffset[iv] = groupLengths[groupId];
		groupLengths[groupId]+= vlen * samplesPerSec[groupId];
		varsOfRate[groupId].push_back(var);
		variables.push_back(var);
	    }
	    syncRecordHeaderSampleTag.addVariable(new Variable(*var));
	    syncRecordDataSampleTag.addVariable(new Variable(*var));
	}
    }
}

void SyncRecordSource::init() throw()
{

    if (initialized) return;
    initialized = true;

    int offset = 0;
    // iterate over the group ids
    for (unsigned int i = 0; i < varsOfRate.size(); i++) {
#ifdef DEBUG
        cerr << "i=" << i << ", rate=" << samplesPerSec[i] <<
		", groupLength=" << groupLengths[i] <<
		", offset=" << offset << endl;
#endif
	groupOffsets[i] = offset;
	offset += groupLengths[i] + 1;
    }
    recSize = offset;
#ifdef DEBUG
    cerr << "recSize=" << recSize << endl;
#endif

    map<dsm_sample_id_t,int>::const_iterator gi;
    for (gi = groupIds.begin(); gi != groupIds.end(); ++gi) {
        dsm_sample_id_t sampleId = gi->first;
        int groupId = gi->second;

	list<const Variable*>::const_iterator vi;
#ifdef DEBUG
        cerr << "sampleId=" << sampleId << ", groupId=" << groupId << endl;
#endif
	for (size_t i = 0; i < numVars[sampleId]; i++) {
	    if (varOffsets[sampleId][i] >= 0)
		varOffsets[sampleId][i] += groupOffsets[groupId];
#ifdef DEBUG
	    cerr << "varOffsets[" << sampleId << "][" << i << "]=" <<
	    	varOffsets[sampleId][i] << endl;
#endif
	}
    }
#ifdef DEBUG
    cerr << "SyncRecordSource, recSize=" << recSize << endl;
#endif

}

/* local utility function to replace one character in a string
 * with another.
 */
namespace {
void replace_util(string& str,char c1, char c2) {
    for (string::size_type bi; (bi = str.find(c1)) != string::npos;)
	str.replace(bi,1,1,c2);
}
}

void SyncRecordSource::createHeader(ostream& ost) throw()
{

    ost << "project  " << aircraft->getProject()->getName() << endl;
    ost << "aircraft " << aircraft->getTailNumber() << endl;
    // ost << "flight " << aircraft->getProject()->getFlightName() << endl;

    // write variable fields.

    // variable type abbreviations:
    //		n=normal, continuous
    //		c=counter
    //		t=clock
    //		o=other
    const char vtypes[] = { 'n','c','t','o' };

    ost << "variables {" << endl;
    list<const Variable*>::const_iterator vi;
    for (vi = variables.begin(); vi != variables.end(); ++vi) {
        const Variable* var = *vi;

	string varname = var->getName();
	if (varname.find(' ') != string::npos) {
	    n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"variable name \"%s\" has one or more embedded spaces, replacing with \'_\'",
		varname.c_str());
	    replace_util(varname,' ','_');
	}

	char vtypeabbr = 'o';
	unsigned int iv = (int)var->getType();
	if (iv < sizeof(vtypes)/sizeof(vtypes[0]))
		vtypeabbr = vtypes[iv];

	ost << varname << ' ' <<
		vtypeabbr << ' ' <<
		var->getLength() << ' ' <<
		"\"" << var->getUnits() << "\" " <<
		"\"" << var->getLongName() << "\" ";
	const VariableConverter* conv = var->getConverter();
	if (conv) {
	    const Linear* lconv = dynamic_cast<const Linear*>(conv);
	    if (lconv) {
	        ost << lconv->getIntercept() << ' ' <<
			lconv->getSlope() << " \"" << lconv->getUnits() << "\"";
	    }
	    else {
		const Polynomial* pconv = dynamic_cast<const Polynomial*>(conv);
		if (pconv) {
		    const std::vector<float>& coefs = pconv->getCoefficients();
		    for (unsigned int i = 0; i < coefs.size(); i++)
			ost << coefs[i] << ' ';
		    ost << " \"" << pconv->getUnits() << "\"";
		}
	    }
	}
	else ost << " \"" << var->getUnits() << "\"";
	ost << ';' << endl;
    }
    ost << "}" << endl;

    // write rate entries
    ost << "rates {" << endl;
    for (unsigned int groupId = 0; groupId < varsOfRate.size(); groupId++) {
        float rate = rates[groupId];
	ost << fixed << setprecision(2) << rate << ' ';
	list<const Variable*>::const_iterator vi;
	for (vi = varsOfRate[groupId].begin();
		vi != varsOfRate[groupId].end(); ++vi) {
	    const Variable* var = *vi;
	    string varname = var->getName();
	    replace_util(varname,' ','_');
	    ost << varname << ' ';
	}
	ost << ';' << endl;
    }
    ost << "}" << endl;

}

void SyncRecordSource::allocateRecord(dsm_time_t timetag)
{
    syncRecord = getSample<float>(recSize);
    syncRecord->setTimeTag(timetag);
    syncRecord->setId(SYNC_RECORD_ID);
    floatPtr = syncRecord->getDataPtr();
    for (int i = 0; i < recSize; i++) floatPtr[i] = floatNAN;
}

void SyncRecordSource::sendHeader(dsm_time_t thead) throw()
{
    headerStream.str("");	// initialize header to empty string
    createHeader(headerStream);
    string headstr = headerStream.str();

    SampleT<char>* headerRec = getSample<char>(headstr.length()+1);
    headerRec->setTimeTag(thead);

#ifdef DEBUG
    cerr << "SyncRecordSource::sendHeader timetag=" << headerRec->getTimeTag() << endl;
    cerr << "sync header=" << endl << headstr << endl;
#endif

    headerRec->setId(SYNC_RECORD_HEADER_ID);
    strcpy(headerRec->getDataPtr(),headstr.c_str());

    distribute(headerRec);

}

void SyncRecordSource::finish() throw()
{
    flush();
}

void SyncRecordSource::flush() throw()
{
    if (syncRecord) {
	distribute(syncRecord);
	syncRecord = 0;
	syncTime += USECS_PER_SEC;
    }
}

bool SyncRecordSource::receive(const Sample* samp) throw()
{
#ifdef DEBUG
    static int nsamps;
    if (!(nsamps++ % 100)) cerr <<
    	"SyncRecordSource, nsamps=" << nsamps << endl;
#endif
    dsm_time_t tt = samp->getTimeTag();
    dsm_sample_id_t sampid = samp->getId();

    if (!syncRecord) {
        syncTime = tt - (tt % USECS_PER_SEC);
	allocateRecord(syncTime);
    }
	
    // screen bad times
    if (tt < syncTime) {
        if (!(badTimes++ % 100))
	    n_u::Logger::getInstance()->log(LOG_WARNING,
		"SyncRecordSource: sample timetag < syncTime by %d usec, dsm=%d, id=%d\n",
		(int)(syncTime-tt),GET_DSM_ID(sampid),GET_SHORT_ID(sampid));
	return false;
    }
    if (tt >= syncTime + USECS_PER_SEC) {
#ifdef DEBUG
	cerr << "distribute syncRecord, tt=" <<
		tt << " syncTime=" << syncTime << endl;
#endif

	flush();
	if (tt >= syncTime + USECS_PER_SEC) {	// leap forward
	    badTimes++;
	    syncTime = tt - (tt % USECS_PER_SEC);
	}
	allocateRecord(syncTime);
    }

    map<dsm_sample_id_t, int>::const_iterator gi =  groupIds.find(sampid);
    if (gi == groupIds.end()) {
        unrecognizedSamples++;
#ifdef DEBUG
	cerr << "unrecognizedSample, id=" << sampid << endl;
#endif
	return false;
    }
        
    int groupId = gi->second;

    assert(groupId < (signed)usecsPerSample.size());
    int usecsPerSamp = usecsPerSample[groupId];

    int* varOffset = varOffsets[sampid];
    assert(varOffset);
    size_t* varLen = varLengths[sampid];
    assert(varLen);
    size_t numVar = numVars[sampid];
    assert(numVar);

    // rate	usec/sample
    //	1000	1000
    //	100	10000
    //	50	20000
    //  12.5	80000
    //  10	100000
    //  8       125000
    //	1	1000000
    //

    int timeIndex = (tt - syncTime) / usecsPerSamp;
    assert(timeIndex < samplesPerSec[groupId]);

    int offsetIndex = groupOffsets[groupId];
    if (::isnan(floatPtr[offsetIndex])) floatPtr[offsetIndex] =
	    tt - syncTime - (timeIndex * usecsPerSamp);

    switch (samp->getType()) {

    case FLOAT_ST:
	{
	    const float* fp = (const float*)samp->getConstVoidDataPtr();
	    const float* ep = fp + samp->getDataLength();

	    for (size_t i = 0; i < numVar && fp < ep; i++) {
	        size_t outlen = varLen[i];
	        size_t inlen = std::min((size_t)(ep-fp),outlen);

		if (varOffset[i] >= 0) {
		    float* dp = floatPtr + varOffset[i] + 1 +
		    	outlen * timeIndex;
		    assert(dp + outlen <= floatPtr + recSize);
		    memcpy(dp,fp,inlen*sizeof(float));
		}
		fp += inlen;
	    }
	}
	break;
    default:
	if (!(unknownSampleType++ % 1000)) 
	    n_u::Logger::getInstance()->log(LOG_WARNING,
	    	"sample id %d is not a float type",sampid);

        break;
    }
    return true;
}

