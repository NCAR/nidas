/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

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
#include <irigclock.h>

#include <math.h>

using namespace dsm;
using namespace std;

SyncRecordGenerator::SyncRecordGenerator():
	syncRecord(0),floatNAN(nanf(""))
{
}

SyncRecordGenerator::~SyncRecordGenerator()
{
    if (syncRecord) syncRecord->freeReference();
}

void SyncRecordGenerator::setAircraft(const Aircraft* aircraft)
{

    const std::list<DSMConfig*>& dsms = aircraft->getDSMConfigs();

    std::list<DSMConfig*>::const_iterator di;

    std::list<DSMSensor*> serialSensors;
    std::list<DSMSensor*> arincSensors;
    std::list<DSMSensor*> otherSensors;
    
    for (di = dsms.begin(); di != dsms.end(); ++di) {
        const DSMConfig* dsm = *di;
	const list<DSMSensor*>& sensors = dsm->getSensors();
	list<DSMSensor*>::const_iterator si;

	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    DSMSensor* sensor = *si;

	    if (sensor->isClock()) continue;

	    if (dynamic_cast<DSMSerialSensor*>(sensor))
		serialSensors.push_back(sensor);
#ifdef HAVE_ARINC
	    else if (dynamic_cast<DSMArincSensor*>(sensor))
		arincSensors.push_back(sensor);
#endif
	    else otherSensors.push_back(sensor);
	}
    }

    scanSensors(serialSensors);
    scanSensors(arincSensors);
    scanSensors(otherSensors);

    int offset = 1;	// first float is ndays
    // iterate over the group ids
    for (int i = 0; i < (signed)numVarsInRateGroup.size(); i++) {
	groupOffsets[i] = offset;
	offset += numVarsInRateGroup[i] * (1000 / msecsPerSample[i]) + 1;
    }
    recSize = offset;
}

void SyncRecordGenerator::scanSensors(const list<DSMSensor*>& sensors)
{
    /* for a given rate, the group id */
    map<float,int> groupsByRate;

    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
        DSMSensor* sensor = *si;

	const std::vector<const SampleTag*>& tags = sensor->getSampleTags();

	std::vector<const SampleTag*>::const_iterator ti;
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

		numVarsInRateGroup.push_back(0);
		msecsPerSample.push_back(0);
		groupOffsets.push_back(0);

		variableNames.push_back(vector<string>());
	    }
	    else groupId = mi->second;

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
	    }
	}
    }
}


void SyncRecordGenerator::allocateRecord(int ndays,dsm_sample_time_t timetag)
{
    syncRecord = getSample<float>(recSize);
    syncRecord->setTimeTag(timetag);
    syncRecord->setId(0);
    floatPtr = syncRecord->getDataPtr();
    for (int i = 0; i < recSize; i++) floatPtr[i] = floatNAN;
    floatPtr[0] = ndays;
}

bool SyncRecordGenerator::receive(const Sample* samp)
        throw(SampleParseException, atdUtil::IOException)
{
    dsm_sample_time_t tt = samp->getTimeTag();
    unsigned long id = samp->getId();
    unsigned short shortid = samp->getShortId();

    if (!syncRecord) {
	if (shortid != CLOCK_SAMPLE_ID ||
		samp->getType() != LONG_LONG_ST) return false;
        syncTime = tt - (tt % 1000);
	long long* timep = (long long*) samp->getConstVoidDataPtr();
	ndays = (*timep / MSECS_PER_DAY);
	allocateRecord(ndays,syncTime);
    }
	
    while (tt >= syncTime + 1000 ||
    	(tt < 60000 && (syncTime == MSECS_PER_DAY - 1000))) {
	distribute(syncRecord);
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
	return false;
    }

    map<unsigned long, int>::const_iterator gi =  groupIds.find(id);
    if (gi == groupIds.end()) 
    	throw SampleParseException("unexpected sample id");
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
