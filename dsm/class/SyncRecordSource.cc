/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <SyncRecordSource.h>
#include <DSMSerialSensor.h>
#include <DSMArincSensor.h>
#include <DSMAnalogSensor.h>
#include <Aircraft.h>

#include <iomanip>

#include <math.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

SyncRecordSource::SyncRecordSource():
	syncRecord(0),doHeader(false),badTimes(0),aircraft(0)
{
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

void SyncRecordSource::init(const list<const DSMConfig*>& dsms) throw()
{

    headerStream.str("");	// initialize header to empty string

    list<const DSMConfig*>::const_iterator di;

    list<DSMSensor*> analogSensors;
    list<DSMSensor*> serialSensors;
    list<DSMSensor*> arincSensors;
    list<DSMSensor*> irigSensors;
    list<DSMSensor*> otherSensors;

    aircraft = 0;
    
    for (di = dsms.begin(); di != dsms.end(); ++di) {
        const DSMConfig* dsm = *di;

// #define DEBUG
#ifdef DEBUG
	cerr << "SyncRecordSource, dsm=" << dsm->getName() << endl;
#endif
	const Site* site = dsm->getSite();
	const Aircraft* acft = dynamic_cast<const Aircraft*>(site);
	assert(acft);

	if (!aircraft) aircraft = acft;
	else {
	    if (acft != aircraft)
	    	cerr << "multiple aircraft: " << acft->getName() <<
			" and " << aircraft->getName();
	}
	const list<DSMSensor*>& sensors = dsm->getSensors();
	list<DSMSensor*>::const_iterator si;

	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    DSMSensor* sensor = *si;
#ifdef DEBUG
	    cerr << "SyncRecordSource, sensor=" << sensor->getName() << endl;
#endif

	    if (dynamic_cast<DSMAnalogSensor*>(sensor))
		analogSensors.push_back(sensor);
	    else if (dynamic_cast<DSMSerialSensor*>(sensor))
		serialSensors.push_back(sensor);
	    else if (dynamic_cast<DSMArincSensor*>(sensor))
		arincSensors.push_back(sensor);
	    else otherSensors.push_back(sensor);
	}
    }

    scanSensors(analogSensors);

#ifdef DEBUG
    cerr << "SyncRecordSource, # of serial sensors=" <<
    	serialSensors.size() << endl;
#endif
    scanSensors(serialSensors);

#ifdef DEBUG
    cerr << "SyncRecordSource, # of arinc sensors=" <<
    	arincSensors.size() << endl;
#endif
    scanSensors(arincSensors);

#ifdef DEBUG
    cerr << "SyncRecordSource, # of other sensors=" <<
    	otherSensors.size() << endl;
#endif
    scanSensors(otherSensors);

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

    createHeader(headerStream);
}

void SyncRecordSource::createHeader(ostream& ost) throw()
{

    ost << "project  " << aircraft->getProject()->getName() << endl;
    ost << "aircraft " << aircraft->getTailNumber() << endl;

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

	char vtypeabbr = 'o';
	unsigned int iv = (int)var->getType();
	if (iv < sizeof(vtypes)/sizeof(vtypes[0]))
		vtypeabbr = vtypes[iv];

	ost << var->getName() << ' ' <<
		vtypeabbr << ' ' <<
		var->getLength() << ' ' <<
		"\"" << var->getUnits() << "\" " <<
		"\"" << var->getLongName() << "\" ";
	const VariableConverter* conv = var->getConverter();
	if (conv) {
	    const Linear* lconv = dynamic_cast<const Linear*>(conv);
	    if (lconv) {
	        ost << lconv->getIntercept() << ' ' <<
			lconv->getSlope();
	    }
	    else {
		const Polynomial* pconv = dynamic_cast<const Polynomial*>(conv);
		if (pconv) {
		    const std::vector<float>& coefs = pconv->getCoefficients();
		    for (unsigned int i = 0; i < coefs.size(); i++)
			ost << coefs[i] << ' ';
		}
	    }
	}
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
	    ost << var->getName() << ' ';
	}
	ost << ';' << endl;
    }
    ost << "}" << endl;

}

void SyncRecordSource::scanSensors(const list<DSMSensor*>& sensors)
{
    // for a given rate, the group id
    map<float,int> groupsByRate;
    map<float,int>::const_iterator mi;

    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
        DSMSensor* sensor = *si;

	const vector<const SampleTag*>& tags = sensor->getSampleTags();

	vector<const SampleTag*>::const_iterator ti;
	for (ti = tags.begin(); ti != tags.end(); ++ti) {
	    const SampleTag* tag = *ti;

	    dsm_sample_id_t sampleId = tag->getId();
	    float rate = tag->getRate();

	    const vector<const Variable*>& vars = tag->getVariables();
	    // skip samples with one non-continuous, non-counter variable
	    if (vars.size() == 1) {
	        Variable::type_t vt = vars.front()->getType();
		if (vt != Variable::CONTINUOUS && vt != Variable::COUNTER)
			continue;
	    }

	    int groupId;

	    mi = groupsByRate.find(rate);
	    if (mi == groupsByRate.end()) {
		// new rate for this sensor type
		groupId = varsOfRate.size();
		varsOfRate.push_back(list<const Variable*>());
		groupsByRate[rate] = groupId;

		groupLengths.push_back(0);
		groupOffsets.push_back(0);

		rates.push_back(rate);
		usecsPerSample.push_back((int)rint(USECS_PER_SEC / rate));
		samplesPerSec.push_back((int)ceil(rate));
	    }
	    else groupId = mi->second;
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
	    }
	}
    }
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
    headerTime = thead;
    doHeader = true;
}

void SyncRecordSource::sendHeader() throw()
{
    string headstr = headerStream.str();

    SampleT<char>* headerRec = getSample<char>(headstr.length()+1);
    headerRec->setTimeTag(headerTime);

#ifdef DEBUG
    cerr << "SyncRecordSource::sendHeader timetag=" << headerRec->getTimeTag() << endl;
#endif

    cerr << "sync header=" << endl << headstr << endl;

    headerRec->setId(SYNC_RECORD_HEADER_ID);
    strcpy(headerRec->getDataPtr(),headstr.c_str());

    distribute(headerRec);
    headerRec->freeReference();
    doHeader = false;

}

void SyncRecordSource::flush() throw()
{
    if (syncRecord) {
	distribute(syncRecord);
	syncRecord->freeReference();
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
	
    // need to screen bad times
    if (tt < syncTime) {
        badTimes++;
#ifdef DEBUG
	cerr << "SyncRecordSource badTime" << endl;
#endif
	return false;
    }
    if (tt >= syncTime + USECS_PER_SEC) {
#ifdef DEBUG
	cerr << "distribute syncRecord, tt=" <<
		tt << " syncTime=" << syncTime << endl;
#endif

	if (doHeader) sendHeader();

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
    //	1	1000000
    //

    int timeIndex = (int)rint((tt - syncTime) / usecsPerSamp);
    assert(timeIndex < samplesPerSec[groupId]);

    switch (samp->getType()) {

    case FLOAT_ST:
	{
	    const float* fp = (const float*)samp->getConstVoidDataPtr();
	    const float* ep = fp + samp->getDataLength();

	    if ((unsigned)varOffset[0] == groupOffsets[groupId] && timeIndex == 0) {
#ifdef DEBUG
	        cerr << "groupId=" << groupId << " lag=" << tt - syncTime << endl;
#endif
		floatPtr[groupOffsets[groupId]] = tt - syncTime;	// lag
	    }

	    for (size_t i = 0; i < numVar; i++) {
	        size_t vlen = varLen[i];
		assert(fp + vlen <= ep);

		if (varOffset[i] >= 0) {
		    float* dp = floatPtr + varOffset[i] + 1 + vlen * timeIndex;
		    assert(dp + vlen <= floatPtr + recSize);
		    memcpy(dp,fp,vlen*sizeof(float));
		}
		fp += vlen;
	    }
	}
	break;
    default:
        break;
    }
    return true;
}

