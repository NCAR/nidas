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
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	input->getName().c_str(),getName().c_str());

    const list<const DSMConfig*>& dsms =  input->getDSMConfigs();
    syncRecSource.init(dsms);

    set<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
	SampleOutput* output = *oi;
	output->setDSMConfigs(input->getDSMConfigs());
	// output->setDSMService(getDSMService());
	output->requestConnection(this);
    }

    // assert(input->isRaw());

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
}
 
void SyncRecordGenerator::disconnect(SampleInput* inputarg)
	throw(atdUtil::IOException)
{
    if (!input) return;
    assert(input == inputarg);

    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	input->getName().c_str(),getName().c_str());

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
    set<SampleOutput*>::const_iterator oi;
    for (oi = outputs.begin(); oi != outputs.end(); ++oi) {
        SampleOutput* output = *oi;
        input->removeSampleClient(output);
        output->flush();
        output->close();
    }
}
 
void SyncRecordGenerator::connected(SampleOutput* output) throw()
{
    addOutput(output);
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has connected to %s",
	output->getName().c_str(),getName().c_str());

    try {
	output->init();
    }
    catch(const atdUtil::IOException& ioe) {
	atdUtil::Logger::getInstance()->log(LOG_ERR,
	    "%s: %error: %s",
	    output->getName().c_str(),ioe.what());
	return;
    }
    syncRecSource.addSampleClient(output);
}

void SyncRecordGenerator::disconnected(SampleOutput* output) throw()
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
	"%s has disconnected from %s",
	output->getName().c_str(),getName().c_str());
    syncRecSource.removeSampleClient(output);
}

void SyncRecordGenerator::sendHeader(dsm_time_t thead,IOStream* iostream)
	throw(atdUtil::IOException)
{
    SampleConnectionRequester::sendHeader(thead,iostream);
    syncRecSource.sendHeader(thead);
}

