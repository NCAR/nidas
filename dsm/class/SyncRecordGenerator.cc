/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <SyncRecordGenerator.h>
#include <SampleFileHeader.h>
#include <DSMSerialSensor.h>
#include <DSMArincSensor.h>
#include <Aircraft.h>
#include <Version.h>

#include <atdUtil/Logger.h>

#include <math.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SyncRecordGenerator);

SyncRecordGenerator::SyncRecordGenerator():
	SampleIOProcessor(),input(0)
{
    setName("SyncRecordGenerator");
}

SyncRecordGenerator::SyncRecordGenerator(const SyncRecordGenerator& x):
	SampleIOProcessor((const SampleIOProcessor&)x),input(0)
{
    setName("SyncRecordGenerator");
}

SyncRecordGenerator::~SyncRecordGenerator()
{
}

SampleIOProcessor* SyncRecordGenerator::clone() const
{
    // this shouldn't be cloned
    assert(false);
    // return new SyncRecordGenerator();
    return 0;
}

void SyncRecordGenerator::connect(SampleInput* newinput)
	throw(atdUtil::IOException)
{
    input = newinput;
    const list<const DSMConfig*>& dsms =  input->getDSMConfigs();
    syncRecSource.init(dsms);

    list<const DSMConfig*>::const_iterator di;
    for (di = dsms.begin(); di != dsms.end(); ++di) {
        const DSMConfig* dsm = *di;

	const list<DSMSensor*>& sensors = dsm->getSensors();
	list<DSMSensor*>::const_iterator si;
	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    DSMSensor* sensor = *si;
#ifdef DEBUG
	    cerr << "SyncRecordGenerator::connect, input=" <<
		    input->getName() << " sensor=" <<
			sensor->getName() << endl;
#endif
	    sensor->init();
	    input->addProcessedSampleClient(&syncRecSource,sensor);
	}
    }
    SampleIOProcessor::connect(input);
}
 
void SyncRecordGenerator::disconnect(SampleInput* oldinput)
	throw(atdUtil::IOException)
{
    if (!input) return;
    assert(input == oldinput);

    const list<const DSMConfig*>& dsms = input->getDSMConfigs();
    list<const DSMConfig*>::const_iterator di;
    for (di = dsms.begin(); di != dsms.end(); ++di) {
        const DSMConfig* dsm = *di;

	const list<DSMSensor*>& sensors = dsm->getSensors();
	list<DSMSensor*>::const_iterator si;
	for (si = sensors.begin(); si != sensors.end(); ++si) {
	    DSMSensor* sensor = *si;
	    input->removeProcessedSampleClient(&syncRecSource,sensor);
	}
    }
    syncRecSource.flush();

    list<SampleOutput*>::iterator oi;
    for (oi = conOutputs.begin(); oi != conOutputs.end(); ++oi) {
        SampleOutput* output = *oi;
	syncRecSource.removeSampleClient(output);
    }

    SampleIOProcessor::disconnect(input);
}
 
void SyncRecordGenerator::connected(SampleOutput* output) throw()
{
    SampleIOProcessor::connected(output);
    syncRecSource.addSampleClient(output);
}

void SyncRecordGenerator::disconnected(SampleOutput* output) throw()
{
    syncRecSource.removeSampleClient(output);
    SampleIOProcessor::disconnected(output);
}

void SyncRecordGenerator::sendHeader(dsm_time_t thead,IOStream* iostream)
	throw(atdUtil::IOException)
{
    SampleConnectionRequester::sendHeader(thead,iostream);
    syncRecSource.sendHeader(thead);
}

