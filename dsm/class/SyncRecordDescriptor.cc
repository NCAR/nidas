/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/SyncRecordDescripter.cc $
 ********************************************************************
*/

using namespace dsm;

SyncRecordDescripter(DSMConfig* dsm)
	throw(atdUtil::InvalidParameterException)
{
    /*
     * Format of sync record:
     * A2D sensors:
     *    variables, grouped by sampling rate
     * e.g.: 50hz, variables A,B
     *       10hz, variables C,D,E
     *  A[0],B[0],A[1],B[1],...,A[49],B[49]
     *  C[0],D[0],E[0],...,C[9],D[9],E[9]
     *
     * Arinc sensors:
     *     similar to A2D, fastest labels first
     * Serial sensors
     *
     */

    map<int,DSMSensor*> sensorMap;

template<sensorType>
void SyncRecordDescripter::scanSerialSensors()
{

    set<float> rates;
    const list<sensorType*> sensOfType;

    const list<DSMSensor*> sensors = dsm->getSensors();
    list<DSMSensor*>::const_iterator si;
    for (si = sensors.begin(); si != sensors.end(); ++si) {
        DSMSensor* sensor = *si;

	SensorType* sensort = dynamic_cast<SensorType*>(sensor);
        if (!sensort) continue;

	const std::vector<const SampleTag*>& samps = sensor->getSampleTags();
	std::vector<const SampleTag*>::const_iterator sampi;
	SampleTag* samp;

	for (sampi = samps.begin(); sampi != samps.end(); ++sampi) {
	    samp = *sampi;
	    unsigned short sampleId = samp->getId();
	    float rate = samp->getRate();
	    msecPerSample = 1000 / rate;
	    rates.insert((*vi)->getSamplingRate());

	    group = 
	    const vector<const Variable*>& vars = samp->getVariables();

	    for (vector<const Variable*>::const_iterator vi = vars.begin();	
		vi != vars.end(); ++vi)

pigeonHoler(const Sample* samp)
{
    unsigned short id = samp->getId();
    dsm_sample_time_t tt = samp->getTimeTag();

    syncGroup = syncGroups[id];
    nvarsInGroup = nvars[syncGroup];
    startOffset = startOffsets[id];

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


    timeIndex = rint((tt - synct) / msecPerSample[syncGroup]);

    switch (samp->getType()) {

    case FLOAT_ST:
	// const SampleT<float>* fsamp = dynamic_cast<SampleT<float>* >(samp);
	const SampleT<float>* fsamp = (const SampleT<float>*)samp;
	float* fp = fsamp->getConstDataPtr();
	for (int i = 0; i < samp->getDataLength(); i++) {
	    floatSyncRec[startOffset + (timeIndex * nvarsInGroup) + i] = fp[i];

    }



        }
    }
    for (si = serialsensors.begin(); si != serialsensors.end(); ++si) {
        



}
