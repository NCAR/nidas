/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#include <SyncRecordGenerator.h>
#include <DSMSerialSensor.h>
#include <DSMArincSensor.h>
#include <Aircraft.h>

#include <iomanip>

#include <math.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

SyncRecordGenerator::SyncRecordGenerator():
	syncRecord(0),floatNAN(nanf("")),doHeader(false)
{
}

SyncRecordGenerator::~SyncRecordGenerator()
{
    if (syncRecord) syncRecord->freeReference();
}


void SyncRecordGenerator::init(const list<DSMConfig*>& dsms) throw()
{

    headerStream.str("");	// initialize header to empty string

    list<DSMConfig*>::const_iterator di;

    list<DSMSensor*> serialSensors;
    list<DSMSensor*> arincSensors;
    list<DSMSensor*> irigSensors;
    list<DSMSensor*> otherSensors;
    
    for (di = dsms.begin(); di != dsms.end(); ++di) {
        const DSMConfig* dsm = *di;

#ifdef DEBUG
	cerr << "SyncRecordGenerator, dsm=" << dsm->getName() << endl;
#endif
	const list<DSMSensor*>& sensors = dsm->getSensors();
	list<DSMSensor*>::const_iterator si;


	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    DSMSensor* sensor = *si;
#ifdef DEBUG
	    cerr << "SyncRecordGenerator, sensor=" << sensor->getName() << endl;
#endif

	    if (dynamic_cast<DSMSerialSensor*>(sensor))
		serialSensors.push_back(sensor);
#ifdef HAVE_ARINC
	    else if (dynamic_cast<DSMArincSensor*>(sensor))
		arincSensors.push_back(sensor);
#endif
	    else otherSensors.push_back(sensor);
	}
    }

#ifdef DEBUG
    cerr << "SyncRecordGenerator, # of serial sensors=" <<
    	serialSensors.size() << endl;
#endif
    scanSensors(serialSensors);

#ifdef DEBUG
    cerr << "SyncRecordGenerator, # of arinc sensors=" <<
    	arincSensors.size() << endl;
#endif
    scanSensors(arincSensors);

#ifdef DEBUG
    cerr << "SyncRecordGenerator, # of other sensors=" <<
    	otherSensors.size() << endl;
#endif
    scanSensors(otherSensors);

    int offset = 1;	// first float is ndays
    // iterate over the group ids
    for (int i = 0; i < (signed)numVarsInRateGroup.size(); i++) {
	groupOffsets[i] = offset;
	offset += numVarsInRateGroup[i] * (1000 / msecsPerSample[i]) + 1;
    }
    recSize = offset;
#ifdef DEBUG
    cerr << "SyncRecordGenerator, recSize=" << recSize << endl;
#endif
}

void SyncRecordGenerator::scanSensors(const list<DSMSensor*>& sensors)
{
    /* for a given rate, the group id */
    map<float,int> groupsByRate;

    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
        DSMSensor* sensor = *si;

	const vector<const SampleTag*>& tags = sensor->getSampleTags();

	vector<const SampleTag*>::const_iterator ti;
	for (ti = tags.begin(); ti != tags.end(); ++ti) {
	    const SampleTag* tag = *ti;

	    unsigned long sampleId = tag->getId();
	    float rate = tag->getRate();

	    int groupId;

	    map<float,int>::const_iterator mi = groupsByRate.find(rate);
	    if (mi == groupsByRate.end()) {
		// new rate for this sensor type
		groupId = numVarsInRateGroup.size();
		groupsByRate[rate] = groupId;

		headerStream << endl << fixed << setprecision(2) << rate << ' ';

		numVarsInRateGroup.push_back(0);
		msecsPerSample.push_back(0);
		groupOffsets.push_back(0);

		variableNames.push_back(vector<string>());
	    }
	    else groupId = mi->second;
#ifdef DEBUG
	    cerr << "SyncRecordGenerator, rate=" << rate <<
	    	" groupId=" << groupId << endl;
#endif

	    groupIds[sampleId] = groupId;
	    sampleOffsets[sampleId] = numVarsInRateGroup[groupId] + 1;
	    msecsPerSample[groupId] = (int)(1000 / rate);

	    vector<string>& varNames = variableNames[groupId];

	    const vector<const Variable*>& vars = tag->getVariables();
	    vector<const Variable*>::const_iterator vi;
	    for (vi = vars.begin(); vi != vars.end(); ++vi) {
		const Variable* var = *vi;
		numVarsInRateGroup[groupId]++;
		varNames.push_back(var->getName());
		headerStream << var->getName() << ' ';
	    }
	}
    }
}


void SyncRecordGenerator::allocateRecord(int ndays,dsm_sample_time_t timetag)
{
    syncRecord = getSample<float>(recSize);
    syncRecord->setTimeTag(timetag);
    syncRecord->setId(SYNC_RECORD_ID);
    floatPtr = syncRecord->getDataPtr();
    for (int i = 0; i < recSize; i++) floatPtr[i] = floatNAN;
    floatPtr[0] = ndays;
}


void SyncRecordGenerator::sendHeader(dsm_sys_time_t thead) throw()
{
    headerTime = thead;
    doHeader = true;
}

void SyncRecordGenerator::sendHeader() throw()
{
    string headstr = headerStream.str();

    SampleT<char>* headerRec = getSample<char>(headstr.length()+1);
    headerRec->setTimeTag((dsm_sample_time_t)(headerTime % MSECS_PER_DAY));

#ifdef DEBUG
    cerr << "SyncRecordGenerator::sendHeader timetag=" << headerRec->getTimeTag() << endl;
#endif

    cerr << "sync header=" << endl << headstr << endl;

    headerRec->setId(SYNC_RECORD_HEADER_ID);
    strcpy(headerRec->getDataPtr(),headstr.c_str());

    distribute(headerRec);
    headerRec->freeReference();
    doHeader = false;

}

bool SyncRecordGenerator::receive(const Sample* samp) throw()
{
#ifdef DEBUG
    static int nsamps;
    if (!(nsamps++ % 100)) cerr <<
    	"SyncRecordGenerator, nsamps=" << nsamps << endl;
#endif
    dsm_sample_time_t tt = samp->getTimeTag();
    unsigned long id = samp->getId();
    unsigned short shortid = samp->getShortId();

    if (!syncRecord) {
	if (shortid != CLOCK_SAMPLE_ID ||
		samp->getType() != LONG_LONG_ST) return false;

	// clock samples go in the sync record stream too
	distribute(samp);

        syncTime = tt - (tt % 1000);
#ifdef DEBUG
	cerr << "syncTime=" << syncTime << endl;
#endif
	long long* timep = (long long*) samp->getConstVoidDataPtr();
	ndays = (*timep / MSECS_PER_DAY);
#ifdef DEBUG
	cerr << "ndays=" << ndays << endl;
#endif
	allocateRecord(ndays,syncTime);
	return true;
    }
	
    while (tt >= syncTime + 1000 ||
    	(tt < 60000 && (syncTime == MSECS_PER_DAY - 1000))) {
#ifdef DEBUG
	cerr << "distribute syncRecord, tt=" <<
		tt << " syncTime=" << syncTime << endl;
#endif

	if (doHeader) sendHeader();

	distribute(syncRecord);
	syncRecord->freeReference();
	syncRecord = 0;
	syncTime += 1000;
	if (syncTime == MSECS_PER_DAY) {
	    syncTime = 0;
	    ndays++;
	}
	allocateRecord(ndays,syncTime);
    }

    if (shortid == CLOCK_SAMPLE_ID &&
    	samp->getType() == LONG_LONG_ST) {
	long long* timep = (long long*) samp->getConstVoidDataPtr();
	int ndayCheck = (*timep / MSECS_PER_DAY);
	if (ndayCheck != ndays) cerr << "bad nday=" << ndays <<
		" ndayCheck=" << ndayCheck << endl;
	// clock samples go in the sync record stream too
	distribute(samp);
	return true;
    }

    map<unsigned long, int>::const_iterator gi =  groupIds.find(id);
    if (gi == groupIds.end()) {
        unrecognizedSamples++;
	return false;
    }
        
    int groupId = gi->second;

    int nvarsInGroup = numVarsInRateGroup[groupId];
    int sampleOffset = sampleOffsets[id];
    int groupOffset = groupOffsets[groupId];
    int msecsPerSamp = msecsPerSample[groupId];

    // rate: samples/sec
    // dt: millisec
    // 1000/rate = 1000msec/sec * sec/sample =  msec/sample

    // rate	msec/sample
    //	1000	1
    //	100	10
    //	50	20
    //  12.5	80
    //  10	100
    //	1	1000
    //
    // rate=12.5, msec/sample=80
    // rate=50


    int timeIndex = (int)rint((tt - syncTime) / msecsPerSamp);

    switch (samp->getType()) {

    case FLOAT_ST:
	{
	    const float* fp = (float*)samp->getConstVoidDataPtr();
	    int j = groupOffset + timeIndex * nvarsInGroup + sampleOffset;
	    if (sampleOffset == 1 && timeIndex == 0)
		floatPtr[j-1] = tt - syncTime;	// lag
	    for (int i = 0; i < (signed)samp->getDataLength(); i++)
		floatPtr[i + j] = fp[i];
	}
	break;
    default:
        break;
    }
    return true;
}

